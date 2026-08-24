#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
struct ExportEntry
{
    String archive_name;
    String source_path;
    String content;
    uint32_t size = 0;
    uint32_t crc = 0;
    uint32_t local_offset = 0;
    bool memory = false;
};

static bool calculate_file_crc(
    ExportEntry &entry, String &error)
{
    fs::File file =
        LittleFS.open(entry.source_path.c_str(), "r");
    if (!file || file.isDirectory() ||
        file.size() > UINT32_MAX)
    {
        file.close();
        error = String("Could not read ") +
                entry.source_path;
        return false;
    }
    entry.size = static_cast<uint32_t>(file.size());
    uint8_t buffer[kIoBufferSize];
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t remaining = entry.size;
    while (remaining)
    {
        const size_t requested =
            std::min<size_t>(
                sizeof(buffer), remaining);
        const size_t count =
            file.read(buffer, requested);
        if (count == 0)
        {
            file.close();
            error = String("Could not read ") +
                    entry.source_path;
            return false;
        }
        crc = crc32_update(crc, buffer, count);
        remaining -= count;
    }
    file.close();
    entry.crc = crc ^ 0xFFFFFFFFUL;
    return true;
}

static bool build_export_entries(
    ControlPanelEventSink &events,
    std::vector<ExportEntry> &entries,
    String &error)
{
    ExportEntry configuration;
    configuration.archive_name = kConfigurationEntry;
    configuration.memory = true;
    serialize_configuration(
        events.controlPanelConfiguration(),
        configuration.content);
    configuration.size =
        static_cast<uint32_t>(
            configuration.content.length());
    configuration.crc =
        crc32_update(
            0xFFFFFFFFUL,
            reinterpret_cast<const uint8_t *>(
                configuration.content.c_str()),
            configuration.content.length()) ^
        0xFFFFFFFFUL;
    entries.push_back(configuration);

    std::vector<String> downloaded;
    collect_files("/downloaded", downloaded);
    for (const String &path : downloaded)
    {
        ExportEntry entry;
        entry.source_path = path;
        entry.archive_name =
            String(kDownloadedPrefix) +
            path.substring(strlen("/downloaded/"));
        if (!calculate_file_crc(entry, error))
            return false;
        entries.push_back(entry);
    }

    std::vector<String> floppies;
    collect_root_floppies(floppies);
    for (const String &path : floppies)
    {
        ExportEntry entry;
        entry.source_path = path;
        const char *name =
            strrchr(path.c_str(), '/');
        entry.archive_name =
            String(kFloppyPrefix) +
            (name ? name + 1 : path.c_str());
        if (!calculate_file_crc(entry, error))
            return false;
        entries.push_back(entry);
    }

    if (LittleFS.exists(kRomPath))
    {
        ExportEntry entry;
        entry.source_path = kRomPath;
        entry.archive_name = kRomEntry;
        if (!calculate_file_crc(entry, error))
            return false;
        entries.push_back(entry);
    }

    if (entries.size() > kMaxArchiveEntries)
    {
        error = "Too many files to back up";
        return false;
    }
    return true;
}

static void send_binary(
    WebServer &server, const uint8_t *data,
    size_t length, uint32_t &offset)
{
    server.sendContent(
        reinterpret_cast<const char *>(data), length);
    offset += static_cast<uint32_t>(length);
}

static void stream_zip(
    WebServer &server, std::vector<ExportEntry> &entries)
{
    uint32_t offset = 0;
    uint8_t header[46] = {};
    uint8_t buffer[kIoBufferSize];

    for (ExportEntry &entry : entries)
    {
        entry.local_offset = offset;
        memset(header, 0, 30);
        write_u32(header, 0x04034B50UL);
        write_u16(header + 4, 20);
        write_u16(header + 6, 0x0800);
        write_u16(header + 8, 0);
        write_u32(header + 14, entry.crc);
        write_u32(header + 18, entry.size);
        write_u32(header + 22, entry.size);
        write_u16(
            header + 26,
            static_cast<uint16_t>(
                entry.archive_name.length()));
        send_binary(server, header, 30, offset);
        send_binary(
            server,
            reinterpret_cast<const uint8_t *>(
                entry.archive_name.c_str()),
            entry.archive_name.length(), offset);

        if (entry.memory)
        {
            send_binary(
                server,
                reinterpret_cast<const uint8_t *>(
                    entry.content.c_str()),
                entry.content.length(), offset);
            continue;
        }

        fs::File file =
            LittleFS.open(entry.source_path.c_str(), "r");
        uint32_t remaining = entry.size;
        while (file && remaining)
        {
            const size_t count = file.read(
                buffer,
                std::min<size_t>(
                    sizeof(buffer), remaining));
            if (!count)
                break;
            send_binary(server, buffer, count, offset);
            remaining -= count;
        }
        file.close();
    }

    const uint32_t central_offset = offset;
    for (const ExportEntry &entry : entries)
    {
        memset(header, 0, sizeof(header));
        write_u32(header, 0x02014B50UL);
        write_u16(header + 4, 20);
        write_u16(header + 6, 20);
        write_u16(header + 8, 0x0800);
        write_u16(header + 10, 0);
        write_u32(header + 16, entry.crc);
        write_u32(header + 20, entry.size);
        write_u32(header + 24, entry.size);
        write_u16(
            header + 28,
            static_cast<uint16_t>(
                entry.archive_name.length()));
        write_u32(header + 42, entry.local_offset);
        send_binary(server, header, sizeof(header), offset);
        send_binary(
            server,
            reinterpret_cast<const uint8_t *>(
                entry.archive_name.c_str()),
            entry.archive_name.length(), offset);
    }

    const uint32_t central_size = offset - central_offset;
    uint8_t end[22] = {};
    write_u32(end, 0x06054B50UL);
    write_u16(
        end + 8,
        static_cast<uint16_t>(entries.size()));
    write_u16(
        end + 10,
        static_cast<uint16_t>(entries.size()));
    write_u32(end + 12, central_size);
    write_u32(end + 16, central_offset);
    send_binary(server, end, sizeof(end), offset);
}

#endif
