#include <EEPROM.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "maclock_hal.h"
#include "host_compat.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

EEPROMClass EEPROM;
LittleFSFS LittleFS;

namespace
{
std::mutex storage_mutex;
enum class PreferenceType : uint8_t
{
    Raw,
    Bool,
    UChar,
    UShort,
    Int,
    Double,
    String,
    Bytes
};

struct PreferenceValue
{
    PreferenceType type = PreferenceType::Raw;
    std::vector<uint8_t> bytes;
};

std::map<std::string, PreferenceValue> preferences;
bool preferences_loaded = false;
std::vector<uint8_t> eeprom;

std::filesystem::path state_path(const char *name)
{
    return maclock_host_path(
               maclock_hal().storage().stateDirectory()) /
           name;
}

void load_preferences()
{
    if (preferences_loaded)
        return;
    preferences_loaded = true;
    std::ifstream input(
        state_path("preferences.bin"), std::ios::binary);
    char magic[8]{};
    input.read(magic, sizeof(magic));
    const bool typed =
        input && std::memcmp(magic, "MLPREF2", 7) == 0;
    const bool legacy =
        input && std::memcmp(magic, "MLPREF1", 7) == 0;
    if (!typed && !legacy)
        return;
    uint32_t count = 0;
    input.read(reinterpret_cast<char *>(&count), sizeof(count));
    for (uint32_t index = 0; input && index < count; ++index)
    {
        uint32_t key_length = 0;
        uint32_t value_length = 0;
        input.read(
            reinterpret_cast<char *>(&key_length),
            sizeof(key_length));
        input.read(
            reinterpret_cast<char *>(&value_length),
            sizeof(value_length));
        uint8_t type =
            static_cast<uint8_t>(PreferenceType::Raw);
        if (typed)
            input.read(reinterpret_cast<char *>(&type), 1);
        if (!input || key_length > 4096 ||
            value_length > 16 * 1024 * 1024 ||
            type > static_cast<uint8_t>(
                       PreferenceType::Bytes))
        {
            preferences.clear();
            return;
        }
        std::string key(key_length, '\0');
        PreferenceValue value;
        value.type = static_cast<PreferenceType>(type);
        value.bytes.resize(value_length);
        input.read(key.data(), key.size());
        input.read(
            reinterpret_cast<char *>(value.bytes.data()),
            value.bytes.size());
        if (input)
            preferences.emplace(std::move(key), std::move(value));
    }
}

bool save_preferences()
{
    const auto path = state_path("preferences.bin");
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream output(
        temporary, std::ios::binary | std::ios::trunc);
    const char magic[8] = "MLPREF2";
    output.write(magic, sizeof(magic));
    const uint32_t count =
        static_cast<uint32_t>(preferences.size());
    output.write(
        reinterpret_cast<const char *>(&count), sizeof(count));
    for (const auto &[key, value] : preferences)
    {
        const uint32_t key_length =
            static_cast<uint32_t>(key.size());
        const uint32_t value_length =
            static_cast<uint32_t>(value.bytes.size());
        output.write(
            reinterpret_cast<const char *>(&key_length),
            sizeof(key_length));
        output.write(
            reinterpret_cast<const char *>(&value_length),
            sizeof(value_length));
        const uint8_t type =
            static_cast<uint8_t>(value.type);
        output.write(
            reinterpret_cast<const char *>(&type), 1);
        output.write(key.data(), key.size());
        output.write(
            reinterpret_cast<const char *>(value.bytes.data()),
            value.bytes.size());
    }
    output.close();
    if (!output)
        return false;
    return maclock_replace_file(temporary, path);
}

std::string preference_key(
    const String &name_space, const char *key)
{
    return name_space.stdString() + "/" +
           (key ? key : "");
}

template <typename T>
T get_number(
    const String &name_space, const char *key, T fallback,
    PreferenceType expected)
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    const auto found =
        preferences.find(preference_key(name_space, key));
    if (found == preferences.end() ||
        (found->second.type != expected &&
         found->second.type != PreferenceType::Raw) ||
        found->second.bytes.size() != sizeof(T))
    {
        return fallback;
    }
    T value;
    std::memcpy(
        &value, found->second.bytes.data(), sizeof(value));
    return value;
}

size_t put_value(
    const String &name_space, bool read_only,
    const char *key, PreferenceType type,
    const void *value, size_t length)
{
    if (read_only || (!value && length))
        return 0;
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    PreferenceValue entry;
    entry.type = type;
    entry.bytes.resize(length);
    if (length)
        std::memcpy(entry.bytes.data(), value, length);
    preferences[preference_key(name_space, key)] =
        std::move(entry);
    return save_preferences() ? length : 0;
}

template <typename T>
size_t put_number(
    const String &name_space, bool read_only,
    const char *key, T value, PreferenceType type)
{
    return put_value(
        name_space, read_only, key, type,
        &value, sizeof(value));
}

bool windows_reserved_name(const std::string &component)
{
#ifdef _WIN32
    std::string base = component.substr(0, component.find('.'));
    std::transform(
        base.begin(), base.end(), base.begin(),
        [](unsigned char value)
        { return static_cast<char>(std::toupper(value)); });
    if (base == "CON" || base == "PRN" || base == "AUX" ||
        base == "NUL")
        return true;
    return base.size() == 4 &&
           (base.compare(0, 3, "COM") == 0 ||
            base.compare(0, 3, "LPT") == 0) &&
           base[3] >= '1' && base[3] <= '9';
#else
    (void)component;
    return false;
#endif
}

std::optional<std::filesystem::path> normalize_relative(
    const char *path)
{
    const std::string value = path ? path : "";
    if (value.empty())
        return std::nullopt;
    if (value == "/")
        return std::filesystem::path{};
    size_t offset = value.front() == '/' ? 1 : 0;
    if (offset < value.size() && value[offset] == '/')
        return std::nullopt;

    std::filesystem::path relative;
    while (offset < value.size())
    {
        const size_t separator = value.find('/', offset);
        const std::string component = value.substr(
            offset, separator == std::string::npos
                        ? std::string::npos
                        : separator - offset);
        if (component.empty() || component == "." ||
            component == ".." ||
            component.find('\\') != std::string::npos ||
            component.find(':') != std::string::npos ||
            component.back() == ' ' || component.back() == '.' ||
            windows_reserved_name(component))
        {
            return std::nullopt;
        }
        relative /= maclock_host_path(component);
        if (separator == std::string::npos)
            break;
        offset = separator + 1;
    }
    return relative;
}

std::filesystem::path source_root()
{
    return maclock_host_path(
        maclock_hal().storage().dataDirectory());
}

std::filesystem::path overlay_root()
{
    return state_path("littlefs");
}

#ifdef _WIN32
std::wstring folded_component(const std::filesystem::path &value)
{
    std::wstring folded = value.filename().native();
    std::transform(
        folded.begin(), folded.end(), folded.begin(),
        [](wchar_t character)
        { return static_cast<wchar_t>(std::towlower(character)); });
    return folded;
}
#endif

bool exact_path_exists(
    const std::filesystem::path &root,
    const std::filesystem::path &relative)
{
    std::filesystem::path current = root;
    if (relative.empty())
        return std::filesystem::is_directory(current);
#ifdef _WIN32
    for (const auto &component : relative)
    {
        std::error_code error;
        bool found = false;
        for (const auto &entry :
             std::filesystem::directory_iterator(current, error))
        {
            if (entry.path().filename().native() == component.native())
            {
                current = entry.path();
                found = true;
                break;
            }
        }
        if (error || !found)
            return false;
    }
    return true;
#else
    return std::filesystem::exists(current / relative);
#endif
}

bool has_case_conflict(
    const std::filesystem::path &root,
    const std::filesystem::path &relative)
{
#ifdef _WIN32
    std::filesystem::path current = root;
    for (const auto &component : relative)
    {
        std::error_code error;
        if (!std::filesystem::is_directory(current, error))
            return false;
        bool exact = false;
        for (const auto &entry :
             std::filesystem::directory_iterator(current, error))
        {
            const auto filename = entry.path().filename();
            if (filename.native() == component.native())
            {
                current = entry.path();
                exact = true;
                break;
            }
            if (folded_component(filename) == folded_component(component))
                return true;
        }
        if (error || !exact)
            return false;
    }
#else
    (void)root;
    (void)relative;
#endif
    return false;
}

std::string case_key(const std::filesystem::path &relative)
{
#ifdef _WIN32
    std::filesystem::path folded;
    for (const auto &component : relative)
    {
        std::wstring value = component.native();
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](wchar_t character)
            { return static_cast<wchar_t>(std::towlower(character)); });
        folded /= value;
    }
    return maclock_host_path_utf8(folded);
#else
    return relative.generic_string();
#endif
}

void merge_path(
    std::map<std::string, std::filesystem::path> &paths,
    std::set<std::string> &ambiguous,
    const std::filesystem::path &relative)
{
    const std::string key = case_key(relative);
    if (ambiguous.count(key))
        return;
    const auto found = paths.find(key);
    if (found == paths.end())
    {
        paths.emplace(key, relative);
        return;
    }
    if (found->second.native() != relative.native())
    {
        paths.erase(found);
        ambiguous.insert(key);
    }
}

std::filesystem::path deletion_marker(
    const std::filesystem::path &relative)
{
    return overlay_root() / ".deleted" / relative;
}

bool is_deleted(const std::filesystem::path &relative)
{
    if (relative.empty() || relative == ".")
        return false;
    return exact_path_exists(
        overlay_root() / ".deleted", relative);
}

void clear_deletion_markers(const std::filesystem::path &relative)
{
    std::error_code error;
    for (auto current = relative;
         !current.empty() && current != ".";
         current = current.parent_path())
    {
        std::filesystem::remove_all(deletion_marker(current), error);
        error.clear();
    }
}

std::filesystem::path resolve_read(
    const std::filesystem::path &relative)
{
    if (is_deleted(relative))
        return {};
    const auto overlay = overlay_root() / relative;
    if (exact_path_exists(overlay_root(), relative))
        return overlay;
    return exact_path_exists(source_root(), relative)
               ? source_root() / relative
               : std::filesystem::path{};
}

std::string virtual_path(
    const std::filesystem::path &relative)
{
    if (relative.empty())
        return "/";
    std::string value;
    for (const auto &component : relative)
    {
        if (!value.empty())
            value += '/';
        value += maclock_host_path_utf8(component);
    }
    return "/" + value;
}

std::ios::openmode file_mode(const char *mode)
{
    const std::string text = mode ? mode : "r";
    std::ios::openmode result = std::ios::binary;
    if (text.find('r') != std::string::npos)
        result |= std::ios::in;
    if (text.find('w') != std::string::npos)
        result |= std::ios::out | std::ios::trunc;
    if (text.find('a') != std::string::npos)
        result |= std::ios::out | std::ios::app;
    if (text.find('+') != std::string::npos)
        result |= std::ios::in | std::ios::out;
    return result;
}
} // namespace

bool Preferences::begin(
    const char *name, bool read_only, const char *)
{
    namespace_ = name ? name : "";
    read_only_ = read_only;
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    return namespace_.length() > 0;
}

void Preferences::end()
{
}

bool Preferences::isKey(const char *key) const
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    return preferences.count(
               preference_key(namespace_, key)) != 0;
}

bool Preferences::getBool(
    const char *key, bool fallback) const
{
    return get_number(
        namespace_, key, fallback, PreferenceType::Bool);
}

uint8_t Preferences::getUChar(
    const char *key, uint8_t fallback) const
{
    return get_number(
        namespace_, key, fallback, PreferenceType::UChar);
}

uint16_t Preferences::getUShort(
    const char *key, uint16_t fallback) const
{
    return get_number(
        namespace_, key, fallback, PreferenceType::UShort);
}

int32_t Preferences::getInt(
    const char *key, int32_t fallback) const
{
    return get_number(
        namespace_, key, fallback, PreferenceType::Int);
}

double Preferences::getDouble(
    const char *key, double fallback) const
{
    return get_number(
        namespace_, key, fallback, PreferenceType::Double);
}

String Preferences::getString(
    const char *key, const String &fallback) const
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    const auto found =
        preferences.find(preference_key(namespace_, key));
    if (found == preferences.end() ||
        (found->second.type != PreferenceType::String &&
         found->second.type != PreferenceType::Raw))
        return fallback;
    return std::string(
        reinterpret_cast<const char *>(
            found->second.bytes.data()),
        found->second.bytes.size());
}

size_t Preferences::getBytesLength(const char *key) const
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    const auto found =
        preferences.find(preference_key(namespace_, key));
    return found == preferences.end()
               ? 0
               : found->second.bytes.size();
}

size_t Preferences::getBytes(
    const char *key, void *buffer, size_t maximum) const
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    load_preferences();
    const auto found =
        preferences.find(preference_key(namespace_, key));
    if (found == preferences.end() || !buffer)
        return 0;
    const size_t copied =
        std::min(maximum, found->second.bytes.size());
    std::memcpy(
        buffer, found->second.bytes.data(), copied);
    return copied;
}

size_t Preferences::putBool(const char *key, bool value)
{
    return put_number(
        namespace_, read_only_, key, value,
        PreferenceType::Bool);
}

size_t Preferences::putUChar(
    const char *key, uint8_t value)
{
    return put_number(
        namespace_, read_only_, key, value,
        PreferenceType::UChar);
}

size_t Preferences::putUShort(
    const char *key, uint16_t value)
{
    return put_number(
        namespace_, read_only_, key, value,
        PreferenceType::UShort);
}

size_t Preferences::putInt(
    const char *key, int32_t value)
{
    return put_number(
        namespace_, read_only_, key, value,
        PreferenceType::Int);
}

size_t Preferences::putDouble(
    const char *key, double value)
{
    return put_number(
        namespace_, read_only_, key, value,
        PreferenceType::Double);
}

size_t Preferences::putString(
    const char *key, const String &value)
{
    return put_value(
        namespace_, read_only_, key, PreferenceType::String,
        value.c_str(), value.length());
}

size_t Preferences::putString(
    const char *key, const char *value)
{
    return putString(key, String(value));
}

size_t Preferences::putBytes(
    const char *key, const void *value, size_t length)
{
    return put_value(
        namespace_, read_only_, key, PreferenceType::Bytes,
        value, length);
}

bool EEPROMClass::begin(size_t size)
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    eeprom.assign(size, 0xFF);
    std::ifstream input(
        state_path("eeprom.bin"), std::ios::binary);
    if (input)
        input.read(
            reinterpret_cast<char *>(eeprom.data()),
            eeprom.size());
    return true;
}

bool EEPROMClass::commit()
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    const auto path = state_path("eeprom.bin");
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(
        path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char *>(eeprom.data()),
        eeprom.size());
    return static_cast<bool>(output);
}

uint8_t EEPROMClass::read(int address) const
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    return address >= 0 &&
                   static_cast<size_t>(address) < eeprom.size()
               ? eeprom[address]
               : 0xFF;
}

void EEPROMClass::write(int address, uint8_t value)
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    if (address >= 0 &&
        static_cast<size_t>(address) < eeprom.size())
    {
        eeprom[address] = value;
    }
}

void EEPROMClass::readBlock(
    int address, void *value, size_t size) const
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    if (!value || address < 0 ||
        static_cast<size_t>(address) + size > eeprom.size())
    {
        if (value)
            std::memset(value, 0xFF, size);
        return;
    }
    std::memcpy(value, eeprom.data() + address, size);
}

void EEPROMClass::writeBlock(
    int address, const void *value, size_t size)
{
    std::lock_guard<std::mutex> lock(storage_mutex);
    if (!value || address < 0 ||
        static_cast<size_t>(address) + size > eeprom.size())
        return;
    std::memcpy(eeprom.data() + address, value, size);
}

struct fs::File::State
{
    bool directory = false;
    bool readable = false;
    bool writable = false;
    std::filesystem::path real_path;
    std::string virtual_name;
    std::fstream stream;
    std::vector<std::filesystem::path> entries;
    size_t entry_index = 0;
};

fs::File::File() = default;

fs::File::File(std::shared_ptr<State> state)
    : state_(std::move(state))
{
}

fs::File::operator bool() const
{
    return state_ &&
           (state_->directory || state_->stream.is_open());
}

bool fs::File::isDirectory() const
{
    return state_ && state_->directory;
}

const char *fs::File::path() const
{
    return state_ ? state_->virtual_name.c_str() : "";
}

const char *fs::File::name() const
{
    return path();
}

fs::File fs::File::openNextFile()
{
    if (!state_ || !state_->directory ||
        state_->entry_index >= state_->entries.size())
    {
        return {};
    }
    const auto relative =
        state_->entries[state_->entry_index++];
    return LittleFS.open(virtual_path(relative).c_str(), "r");
}

void fs::File::close()
{
    if (state_ && state_->stream.is_open())
        state_->stream.close();
    state_.reset();
}

int fs::File::read()
{
    if (!state_ || !state_->stream.is_open() ||
        !state_->readable)
        return -1;
    return state_->stream.get();
}

size_t fs::File::read(uint8_t *buffer, size_t size)
{
    if (!state_ || !state_->stream.is_open() ||
        !state_->readable || !buffer)
        return 0;
    state_->stream.read(
        reinterpret_cast<char *>(buffer), size);
    return static_cast<size_t>(state_->stream.gcount());
}

size_t fs::File::write(
    const uint8_t *buffer, size_t size)
{
    if (!state_ || !state_->stream.is_open() ||
        !state_->writable || !buffer)
        return 0;
    state_->stream.write(
        reinterpret_cast<const char *>(buffer), size);
    state_->stream.flush();
    return state_->stream ? size : 0;
}

bool fs::File::seek(uint32_t position, SeekMode mode)
{
    if (!state_ || !state_->stream.is_open())
        return false;
    std::ios::seekdir direction = std::ios::beg;
    if (mode == SeekCur)
        direction = std::ios::cur;
    else if (mode == SeekEnd)
        direction = std::ios::end;
    state_->stream.clear();
    if (state_->readable)
        state_->stream.seekg(position, direction);
    if (state_->writable)
        state_->stream.seekp(position, direction);
    return static_cast<bool>(state_->stream);
}

size_t fs::File::position() const
{
    if (!state_ || !state_->stream.is_open())
        return 0;
    if (state_->readable)
    {
        const auto position = state_->stream.tellg();
        if (position >= 0)
            return static_cast<size_t>(position);
        state_->stream.clear();
    }
    if (state_->writable)
    {
        const auto output_position = state_->stream.tellp();
        if (output_position >= 0)
            return static_cast<size_t>(output_position);
        state_->stream.clear();
    }
    return 0;
}

size_t fs::File::size() const
{
    if (!state_)
        return 0;
    std::error_code error;
    const auto bytes = std::filesystem::file_size(
        state_->real_path, error);
    return error ? 0 : static_cast<size_t>(bytes);
}

bool LittleFSFS::begin(bool)
{
    std::filesystem::create_directories(overlay_root());
    return std::filesystem::exists(source_root()) ||
           std::filesystem::exists(overlay_root());
}

size_t LittleFSFS::totalBytes() const
{
    return 0x9F0000U;
}

size_t LittleFSFS::usedBytes() const
{
    std::map<std::string, std::filesystem::path> files;
    std::set<std::string> ambiguous;
    for (const auto &root : {source_root(), overlay_root()})
    {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error))
            continue;
        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(
                 root, error))
        {
            if (error)
                break;
            const auto relative =
                entry.path().lexically_relative(root);
            if (relative.empty() ||
                *relative.begin() == ".deleted" ||
                !entry.is_regular_file(error) ||
                is_deleted(relative))
            {
                continue;
            }
            merge_path(files, ambiguous, relative);
        }
    }

    size_t total = 0;
    for (const auto &[key, relative] : files)
    {
        (void)key;
        std::error_code error;
        const auto size = std::filesystem::file_size(
            resolve_read(relative), error);
        if (!error)
            total += static_cast<size_t>(size);
    }
    return total;
}

bool LittleFSFS::exists(const char *path) const
{
    const auto relative = normalize_relative(path);
    return relative && !is_deleted(*relative) &&
           !resolve_read(*relative).empty();
}

fs::File LittleFSFS::open(
    const char *path, const char *mode)
{
    const auto relative = normalize_relative(path);
    if (!relative)
        return {};
    const std::string mode_text = mode ? mode : "r";
    const bool writing =
        mode_text.find('w') != std::string::npos ||
        mode_text.find('a') != std::string::npos ||
        mode_text.find('+') != std::string::npos;
    if (writing &&
        (has_case_conflict(source_root(), *relative) ||
         has_case_conflict(overlay_root(), *relative)))
    {
        return {};
    }
    if (writing)
    {
        std::error_code marker_error;
        std::filesystem::remove(
            deletion_marker(*relative), marker_error);
    }

    auto state = std::make_shared<fs::File::State>();
    state->virtual_name = virtual_path(*relative);
    state->readable =
        mode_text.find('r') != std::string::npos ||
        mode_text.find('+') != std::string::npos;
    state->writable = writing;

    const auto selected = resolve_read(*relative);
    if (!writing && !selected.empty() &&
        std::filesystem::is_directory(selected))
    {
        state->directory = true;
        std::map<std::string, std::filesystem::path> entries;
        std::set<std::string> ambiguous;
        for (const auto &root : {source_root(), overlay_root()})
        {
            const auto directory = root / *relative;
            std::error_code error;
            if (!std::filesystem::is_directory(directory, error))
                continue;
            for (const auto &entry :
                 std::filesystem::directory_iterator(
                     directory, error))
            {
                const auto child =
                    *relative / entry.path().filename();
                if (child == ".deleted" ||
                    is_deleted(child))
                {
                    continue;
                }
                merge_path(entries, ambiguous, child);
            }
        }
        for (const auto &[key, entry] : entries)
        {
            (void)key;
            state->entries.push_back(entry);
        }
        return fs::File(state);
    }

    std::filesystem::path real_path = selected;
    if (writing)
    {
        real_path = overlay_root() / *relative;
        std::filesystem::create_directories(
            real_path.parent_path());
        if (!std::filesystem::exists(real_path) &&
            exact_path_exists(source_root(), *relative) &&
            mode_text.find('w') == std::string::npos)
        {
            std::filesystem::copy_file(
                source_root() / *relative, real_path);
        }
    }
    state->real_path = real_path;
    state->stream.open(real_path, file_mode(mode));
    return state->stream.is_open()
               ? fs::File(state)
               : fs::File();
}

bool LittleFSFS::remove(const char *path)
{
    const auto relative = normalize_relative(path);
    if (!relative || relative->empty())
        return false;

    std::error_code error;
    const bool overlay_removed =
        exact_path_exists(overlay_root(), *relative) &&
        std::filesystem::remove(
            overlay_root() / *relative, error);
    const bool source_exists =
        exact_path_exists(source_root(), *relative);
    if (!source_exists)
        return overlay_removed;

    const auto marker = deletion_marker(*relative);
    std::filesystem::create_directories(
        marker.parent_path(), error);
    error.clear();
    std::ofstream output(
        marker, std::ios::binary | std::ios::trunc);
    output.close();
    return static_cast<bool>(output);
}

bool LittleFSFS::rename(const char *from, const char *to)
{
    const auto source = normalize_relative(from);
    const auto destination = normalize_relative(to);
    if (!source || !destination || source->empty() ||
        destination->empty() ||
        has_case_conflict(source_root(), *destination) ||
        has_case_conflict(overlay_root(), *destination))
        return false;

    const auto source_path = overlay_root() / *source;
    const auto destination_path = overlay_root() / *destination;
    std::error_code error;
    if (!exact_path_exists(overlay_root(), *source))
        return false;
    std::filesystem::remove(
        deletion_marker(*destination), error);
    error.clear();
    std::filesystem::create_directories(
        destination_path.parent_path(), error);
    error.clear();
    std::filesystem::rename(
        source_path, destination_path, error);
    return !error;
}

bool LittleFSFS::mkdir(const char *path)
{
    const auto relative = normalize_relative(path);
    if (!relative || relative->empty() ||
        has_case_conflict(source_root(), *relative) ||
        has_case_conflict(overlay_root(), *relative))
        return false;
    clear_deletion_markers(*relative);
    std::error_code error;
    return std::filesystem::create_directories(
               overlay_root() / *relative, error) ||
           (!error &&
            std::filesystem::is_directory(
                 overlay_root() / *relative));
}

bool LittleFSFS::rmdir(const char *path)
{
    const auto relative = normalize_relative(path);
    if (!relative || relative->empty())
        return false;
    std::error_code error;
    return exact_path_exists(overlay_root(), *relative) &&
           std::filesystem::remove(
               overlay_root() / *relative, error);
}
