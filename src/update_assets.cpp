#ifdef MACLOCK_UPDATE_INSTALL_FRAGMENT
bool install_assets(
    UpdateService::State &state,
    JsonObjectConst assets, const String &assets_url,
    bool prune_obsolete, String &error)
{
    JsonArrayConst files = assets["files"].as<JsonArrayConst>();
    size_t largest_temporary_file = 0;
    uint16_t changed_before_download = 0;
    for (JsonObjectConst file : files)
    {
        const char *path = file["path"] | "";
        String installed_digest;
        const bool unchanged =
            LittleFS.exists(path) &&
            hash_file(path, installed_digest) &&
            equals_digest(
                installed_digest, file["sha256"] | "");
        if (!unchanged)
        {
            ++changed_before_download;
            const size_t size = file["size"] | 0U;
            if (size > largest_temporary_file)
                largest_temporary_file = size;
        }
    }
    const size_t free_bytes =
        LittleFS.totalBytes() - LittleFS.usedBytes();
    if (largest_temporary_file > free_bytes)
    {
        error =
            "Not enough LittleFS space. Remove downloaded sounds "
            "through Sound Manager and try again.";
        return false;
    }
    portENTER_CRITICAL(&state.mux);
    state.snapshot.changed_assets = changed_before_download;
    portEXIT_CRITICAL(&state.mux);

    String download_url;
    if (!resolve_download_url(
            assets_url, download_url, error))
        return false;
    GithubNetworkClientSecure client;
    client.useSystemCertificateBundle();
    client.setHandshakeTimeout(30);
    HTTPClient http;
    if (!begin_http(http, client, download_url, false))
    {
        error = "Could not open the asset download";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        if (response < 0)
        {
            set_http_connection_error(
                error, "Asset download connection failed",
                response, client);
        }
        else
        {
            error = String("Asset download returned HTTP ") +
                    String(response);
        }
        http.end();
        return false;
    }
    const int content_length = http.getSize();
    const size_t expected_length = assets["size"] | 0U;
    if (content_length <= 0 ||
        static_cast<size_t>(content_length) != expected_length)
    {
        error = "The asset ZIP size does not match its manifest";
        http.end();
        return false;
    }
    NetworkClient *stream = http.getStreamPtr();
    if (!stream)
    {
        error = "The asset ZIP stream is unavailable";
        http.end();
        return false;
    }

    HashedStream input(
        *stream, expected_length,
        [](void *context, uint8_t progress)
        {
            auto &update_state =
                *static_cast<UpdateService::State *>(context);
            set_progress(
                update_state, UpdateStage::DownloadingAssets,
                progress, "Downloading LittleFS assets");
        },
        &state);
    size_t processed = 0;
    uint16_t changed = 0;
    bool central_directory = false;
    std::vector<String> archive_paths;
    LittleFS.remove(kAssetTemporaryPath);

    while (input.remaining() >= 4)
    {
        uint8_t signature_bytes[4];
        if (!input.readExact(signature_bytes, sizeof(signature_bytes)))
        {
            error = "The asset ZIP ended unexpectedly";
            break;
        }
        const uint32_t signature = read_u32(signature_bytes);
        if (signature == 0x02014b50UL)
        {
            uint32_t central_signature = signature;
            size_t central_entries = 0;
            while (central_signature == 0x02014b50UL)
            {
                uint8_t central[42];
                if (!input.readExact(
                        central, sizeof(central)))
                {
                    error =
                        "The ZIP central directory was truncated";
                    break;
                }
                const uint16_t made_by =
                    read_u16(central);
                const uint16_t flags =
                    read_u16(central + 4);
                const uint16_t method =
                    read_u16(central + 6);
                const uint32_t compressed_size =
                    read_u32(central + 16);
                const uint32_t uncompressed_size =
                    read_u32(central + 20);
                const uint16_t name_length =
                    read_u16(central + 24);
                const uint16_t extra_length =
                    read_u16(central + 26);
                const uint16_t comment_length =
                    read_u16(central + 28);
                const uint32_t attributes =
                    read_u32(central + 34);
                if ((flags & 0x0009U) ||
                    (method != 0 && method != 8) ||
                    name_length == 0 ||
                    name_length >= 191 ||
                    extra_length > 4096 ||
                    comment_length > 4096 ||
                    compressed_size == UINT32_MAX ||
                    uncompressed_size == UINT32_MAX ||
                    (((made_by >> 8) == 3) &&
                     (((attributes >> 16) & 0xF000U) ==
                      0xA000U)))
                {
                    error =
                        "The ZIP central directory is unsafe";
                    break;
                }
                char central_name[192] = {};
                if (!input.readExact(
                        reinterpret_cast<uint8_t *>(
                            central_name),
                        name_length) ||
                    !input.discard(
                        extra_length + comment_length))
                {
                    error =
                        "The ZIP central directory was truncated";
                    break;
                }
                central_name[name_length] = '\0';
                String central_path =
                    central_name[0] == '/'
                        ? String(central_name)
                        : String("/") + central_name;
                JsonObjectConst central_file =
                    find_manifest_file(
                        files, central_path.c_str());
                if (central_file.isNull() ||
                    !valid_asset_path(
                        central_path.c_str()) ||
                    static_cast<uint16_t>(
                        central_file["method"] | 99) !=
                        method ||
                    static_cast<uint32_t>(
                        central_file["compressedSize"] |
                        UINT32_MAX) != compressed_size ||
                    static_cast<uint32_t>(
                        central_file["size"] | UINT32_MAX) !=
                        uncompressed_size)
                {
                    error =
                        "The ZIP central directory and manifest differ";
                    break;
                }
                ++central_entries;
                if (!input.readExact(
                        signature_bytes,
                        sizeof(signature_bytes)))
                {
                    error =
                        "The ZIP central directory was truncated";
                    break;
                }
                central_signature =
                    read_u32(signature_bytes);
            }
            if (!error.length() &&
                central_signature != 0x06054b50UL)
            {
                error =
                    "The ZIP end record is missing";
            }
            if (!error.length() &&
                !input.discard(input.remaining()))
            {
                error =
                    "The ZIP end record was truncated";
            }
            central_directory =
                !error.length() &&
                central_entries == files.size();
            break;
        }
        if (signature == 0x06054b50UL)
        {
            error = "The ZIP contains no central directory";
            break;
        }
        if (signature != 0x04034b50UL)
        {
            error = "The asset ZIP contains an invalid record";
            break;
        }

        uint8_t header[26];
        if (!input.readExact(header, sizeof(header)))
        {
            error = "The asset ZIP header was truncated";
            break;
        }
        const uint16_t flags = read_u16(header + 2);
        const uint16_t method = read_u16(header + 4);
        const uint32_t compressed_size = read_u32(header + 14);
        const uint32_t uncompressed_size = read_u32(header + 18);
        const uint16_t name_length = read_u16(header + 22);
        const uint16_t extra_length = read_u16(header + 24);
        if (read_u16(header) > 20 ||
            (flags & 0x0009U) || name_length == 0 ||
            name_length >= 191 || extra_length > 4096 ||
            compressed_size == UINT32_MAX ||
            uncompressed_size == UINT32_MAX)
        {
            error = "The asset ZIP uses an unsupported feature";
            break;
        }

        char archive_name[192] = {};
        if (!input.readExact(
                reinterpret_cast<uint8_t *>(archive_name),
                name_length) ||
            !input.discard(extra_length))
        {
            error = "The asset ZIP filename was truncated";
            break;
        }
        archive_name[name_length] = '\0';
        String path = archive_name[0] == '/'
                          ? String(archive_name)
                          : String("/") + archive_name;
        JsonObjectConst file =
            find_manifest_file(files, path.c_str());
        if (file.isNull() || !valid_asset_path(path.c_str()) ||
            static_cast<uint16_t>(file["method"] | 99) != method ||
            static_cast<uint32_t>(
                file["compressedSize"] | UINT32_MAX) !=
                compressed_size ||
            static_cast<uint32_t>(
                file["size"] | UINT32_MAX) !=
                uncompressed_size)
        {
            error = "The ZIP and asset manifest do not match";
            break;
        }
        for (const String &seen : archive_paths)
        {
            if (seen == path)
            {
                error = "The ZIP contains a duplicate asset";
                break;
            }
        }
        if (error.length())
            break;
        archive_paths.push_back(path);

        String installed_digest;
        const bool unchanged =
            LittleFS.exists(path.c_str()) &&
            hash_file(path.c_str(), installed_digest) &&
            equals_digest(
                installed_digest, file["sha256"] | "");
        if (unchanged)
        {
            if (!input.discard(compressed_size))
            {
                error = "An unchanged ZIP entry was truncated";
                break;
            }
        }
        else
        {
            ++changed;
            if (!ensure_parent_directories(path.c_str()))
            {
                error = "Could not create an asset directory";
                break;
            }
            LittleFS.remove(kAssetTemporaryPath);
            fs::File output =
                LittleFS.open(kAssetTemporaryPath, "w");
            if (!output)
            {
                error =
                    "LittleFS has insufficient space for the update";
                break;
            }
            SHA256Builder hash;
            hash.begin();
            size_t written = 0;
            const bool copied =
                method == 0
                    ? copy_stored_entry(
                          input, output, compressed_size,
                          hash, written)
                    : inflate_entry(
                          input, output, compressed_size,
                          hash, written);
            output.close();
            hash.calculate();
            const String digest = hash.toString();
            if (!copied || written != uncompressed_size ||
                !equals_digest(digest, file["sha256"] | ""))
            {
                LittleFS.remove(kAssetTemporaryPath);
                error =
                    "An asset failed decompression or verification";
                break;
            }
            if (!LittleFS.rename(
                    kAssetTemporaryPath, path.c_str()))
            {
                LittleFS.remove(kAssetTemporaryPath);
                error = "Could not install a verified asset";
                break;
            }
        }

        ++processed;
    }

    const String zip_digest = input.finish();
    http.end();
    LittleFS.remove(kAssetTemporaryPath);
    if (error.length())
        return false;
    if (!central_directory || processed != files.size() ||
        !equals_digest(zip_digest, assets["sha256"] | ""))
    {
        error = "The complete asset ZIP failed verification";
        return false;
    }

    if (prune_obsolete)
    {
        std::vector<String> installed;
        std::vector<String> directories;
        collect_files("/", installed, directories);
        for (const String &path : installed)
        {
            if (!manifest_contains(files, path.c_str()) &&
                !protected_user_path(path.c_str()))
            {
                LittleFS.remove(path.c_str());
            }
        }
        for (auto item = directories.rbegin();
             item != directories.rend(); ++item)
        {
            if (!protected_user_path(item->c_str()))
                LittleFS.rmdir(item->c_str());
        }
    }

    set_progress(
        state, UpdateStage::InstallingAssets, 100,
        "LittleFS assets installed");

    portENTER_CRITICAL(&state.mux);
    state.snapshot.changed_assets = changed;
    portEXIT_CRITICAL(&state.mux);
    return true;
}

#endif
