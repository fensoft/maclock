#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
class RestoreParser
{
public:
    bool begin()
    {
        abort();
        remove_tree(kRestoreRoot);
        if (!LittleFS.mkdir(kRestoreRoot) ||
            !LittleFS.mkdir(kRestoreDownloaded) ||
            !LittleFS.mkdir(kRestoreFloppies) ||
            !LittleFS.mkdir(kRestoreLoading))
        {
            fail("Could not create restore staging folders");
            return false;
        }
        state_ = ParserState::Signature;
        return true;
    }

    bool feed(const uint8_t *data, size_t length)
    {
        if (!data || state_ == ParserState::Failed)
            return false;
        while (length && state_ != ParserState::Failed)
        {
            if (state_ == ParserState::Trailing)
            {
                for (size_t index = 0; index < length; ++index)
                    push_tail(data[index]);
                stream_offset_ += length;
                return true;
            }
            if (state_ == ParserState::Data)
            {
                const size_t count =
                    std::min<size_t>(
                        length, data_remaining_);
                if (output_ &&
                    output_.write(data, count) != count)
                {
                    fail("Could not write a restored file");
                    return false;
                }
                if (current_is_configuration_)
                {
                    for (size_t index = 0;
                         index < count; ++index)
                    {
                        configuration_ +=
                            static_cast<char>(data[index]);
                    }
                }
                current_crc_ =
                    crc32_update(current_crc_, data, count);
                data += count;
                length -= count;
                stream_offset_ += count;
                data_remaining_ -= count;
                if (!data_remaining_ && !finish_entry())
                    return false;
                continue;
            }

            const uint8_t byte = *data++;
            --length;
            ++stream_offset_;
            if (state_ == ParserState::Signature)
            {
                field_[field_length_++] = byte;
                if (field_length_ != 4)
                    continue;
                const uint32_t signature =
                    read_u32(field_);
                field_length_ = 0;
                if (signature == 0x04034B50UL)
                {
                    record_offset_ = stream_offset_ - 4;
                    state_ = ParserState::Header;
                }
                else if (signature == 0x02014B50UL)
                {
                    central_offset_ = stream_offset_ - 4;
                    state_ = ParserState::Trailing;
                    push_tail(0x50);
                    push_tail(0x4B);
                    push_tail(0x01);
                    push_tail(0x02);
                }
                else
                {
                    fail("The ZIP archive has an invalid record");
                }
                continue;
            }
            if (state_ == ParserState::Header)
            {
                field_[field_length_++] = byte;
                if (field_length_ == 26)
                {
                    if (!start_header())
                        return false;
                }
                continue;
            }
            if (state_ == ParserState::Name)
            {
                name_ += static_cast<char>(byte);
                if (--name_remaining_ == 0)
                {
                    state_ = extra_remaining_
                                 ? ParserState::Extra
                                 : ParserState::Data;
                    if (!extra_remaining_ &&
                        !open_entry())
                    {
                        return false;
                    }
                }
                continue;
            }
            if (state_ == ParserState::Extra)
            {
                if (--extra_remaining_ == 0)
                {
                    state_ = ParserState::Data;
                    if (!open_entry())
                        return false;
                }
            }
        }
        return state_ != ParserState::Failed;
    }

    bool finish()
    {
        if (output_)
            output_.close();
        if (state_ != ParserState::Trailing ||
            tail_length_ != sizeof(tail_) ||
            read_u32(tail_) != 0x06054B50UL ||
            read_u16(tail_ + 4) != 0 ||
            read_u16(tail_ + 6) != 0 ||
            read_u16(tail_ + 8) != entry_count_ ||
            read_u16(tail_ + 10) != entry_count_ ||
            read_u32(tail_ + 16) != central_offset_ ||
            read_u16(tail_ + 20) != 0 ||
            static_cast<uint64_t>(
                read_u32(tail_ + 12)) +
                    central_offset_ + sizeof(tail_) !=
                stream_offset_ ||
            !configuration_seen_)
        {
            fail("The ZIP archive is incomplete or invalid");
            return false;
        }
        return true;
    }

    void abort()
    {
        if (output_)
            output_.close();
        remove_tree(kRestoreRoot);
        state_ = ParserState::Failed;
        error_.clear();
        configuration_.clear();
        names_.clear();
        field_length_ = 0;
        tail_length_ = 0;
        stream_offset_ = 0;
        central_offset_ = 0;
        entry_count_ = 0;
        configuration_seen_ = false;
        loading_files_seen_ = false;
    }

    const String &configuration() const
    {
        return configuration_;
    }

    const String &error() const
    {
        return error_;
    }

    bool loadingFilesSeen() const { return loading_files_seen_; }

private:
    enum class ParserState : uint8_t
    {
        Signature,
        Header,
        Name,
        Extra,
        Data,
        Trailing,
        Failed
    };

    void fail(const char *message)
    {
        if (output_)
            output_.close();
        error_ = message;
        state_ = ParserState::Failed;
    }

    bool start_header()
    {
        const uint16_t flags = read_u16(field_ + 2);
        const uint16_t method = read_u16(field_ + 4);
        expected_crc_ = read_u32(field_ + 10);
        const uint32_t compressed_size =
            read_u32(field_ + 14);
        data_remaining_ = read_u32(field_ + 18);
        name_remaining_ = read_u16(field_ + 22);
        extra_remaining_ = read_u16(field_ + 24);
        field_length_ = 0;
        if ((flags & ~0x0800U) != 0 ||
            method != 0 ||
            compressed_size != data_remaining_ ||
            data_remaining_ > kMaxArchiveEntryBytes ||
            name_remaining_ == 0 ||
            name_remaining_ >= 160 ||
            extra_remaining_ > 1024 ||
            entry_count_ >= kMaxArchiveEntries)
        {
            fail("The ZIP archive uses unsupported features");
            return false;
        }
        name_.clear();
        name_.reserve(name_remaining_);
        state_ = ParserState::Name;
        return true;
    }

    bool duplicate_name() const
    {
        for (const String &existing : names_)
        {
            if (existing == name_)
                return true;
        }
        return false;
    }

    bool open_entry()
    {
        if (duplicate_name())
        {
            fail("The ZIP archive contains duplicate files");
            return false;
        }
        names_.push_back(name_);
        current_crc_ = 0xFFFFFFFFUL;
        current_is_configuration_ = false;
        if (name_ == kConfigurationEntry)
        {
            if (configuration_seen_ ||
                data_remaining_ > kMaxConfigurationBytes)
            {
                fail("configuration.json is invalid");
                return false;
            }
            configuration_seen_ = true;
            current_is_configuration_ = true;
            configuration_.clear();
            configuration_.reserve(data_remaining_);
        }
        else if (name_.startsWith(kDownloadedPrefix))
        {
            const String relative =
                name_.substring(strlen(kDownloadedPrefix));
            if (!valid_relative_path(relative.c_str()))
            {
                fail("The backup contains an invalid downloaded path");
                return false;
            }
            const String path =
                String(kRestoreDownloaded) + "/" + relative;
            if (!ensure_parent_directories(path.c_str()))
            {
                fail("Could not create restore folders");
                return false;
            }
            output_ = LittleFS.open(path.c_str(), "w");
            if (!output_)
            {
                fail("Could not stage a downloaded file");
                return false;
            }
        }
        else if (name_.startsWith(kFloppyPrefix))
        {
            const String relative =
                name_.substring(strlen(kFloppyPrefix));
            if (!valid_relative_path(relative.c_str()) ||
                strchr(relative.c_str(), '/') ||
                !is_floppy_name(relative.c_str()))
            {
                fail("The backup contains an invalid floppy path");
                return false;
            }
            const String path =
                String(kRestoreFloppies) + "/" + relative;
            output_ = LittleFS.open(path.c_str(), "w");
            if (!output_)
            {
                fail("Could not stage a floppy image");
                return false;
            }
        }
        else if (name_.startsWith(kLoadingPrefix))
        {
            const String relative = name_.substring(strlen(kLoadingPrefix));
            if (!valid_relative_path(relative.c_str()) ||
                !strchr(relative.c_str(), '/'))
            {
                fail("The backup contains an invalid loading path");
                return false;
            }
            const String path = String(kRestoreLoading) + "/" + relative;
            if (!ensure_parent_directories(path.c_str()))
            {
                fail("Could not create loading restore folders");
                return false;
            }
            output_ = LittleFS.open(path.c_str(), "w");
            if (!output_)
            {
                fail("Could not stage a loading screen file");
                return false;
            }
            loading_files_seen_ = true;
        }
        else if (name_ == kRomEntry)
        {
            output_ = LittleFS.open(kRestoreRom, "w");
            if (!output_)
            {
                fail("Could not stage the Macintosh ROM");
                return false;
            }
        }
        else
        {
            fail("The ZIP archive contains an unexpected file");
            return false;
        }
        return data_remaining_ ? true : finish_entry();
    }

    bool finish_entry()
    {
        if (output_)
            output_.close();
        const uint32_t actual_crc =
            current_crc_ ^ 0xFFFFFFFFUL;
        if (actual_crc != expected_crc_)
        {
            fail("A ZIP entry failed its CRC check");
            return false;
        }
        ++entry_count_;
        current_is_configuration_ = false;
        state_ = ParserState::Signature;
        return true;
    }

    void push_tail(uint8_t byte)
    {
        if (tail_length_ < sizeof(tail_))
        {
            tail_[tail_length_++] = byte;
            return;
        }
        memmove(tail_, tail_ + 1, sizeof(tail_) - 1);
        tail_[sizeof(tail_) - 1] = byte;
    }

    ParserState state_ = ParserState::Failed;
    fs::File output_;
    String configuration_;
    String error_;
    String name_;
    std::vector<String> names_;
    uint8_t field_[26] = {};
    size_t field_length_ = 0;
    uint8_t tail_[22] = {};
    size_t tail_length_ = 0;
    uint32_t stream_offset_ = 0;
    uint32_t record_offset_ = 0;
    uint32_t central_offset_ = 0;
    uint32_t expected_crc_ = 0;
    uint32_t current_crc_ = 0;
    uint32_t data_remaining_ = 0;
    uint16_t name_remaining_ = 0;
    uint16_t extra_remaining_ = 0;
    uint16_t entry_count_ = 0;
    bool current_is_configuration_ = false;
    bool configuration_seen_ = false;
    bool loading_files_seen_ = false;
};
#endif
