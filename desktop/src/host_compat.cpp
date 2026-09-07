#include "host_compat.h"

#include <cstdlib>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

namespace
{
#ifdef _WIN32
std::wstring utf8_to_wide(const std::string &value)
{
    if (value.empty())
        return {};
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0)
        return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length) != length)
    {
        return {};
    }
    return result;
}

std::string wide_to_utf8(const std::wstring &value)
{
    if (value.empty())
        return {};
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0)
        return {};
    std::string result(static_cast<size_t>(length), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), length,
            nullptr, nullptr) != length)
    {
        return {};
    }
    return result;
}
#endif
} // namespace

bool maclock_localtime(std::time_t value, std::tm &result)
{
#ifdef _WIN32
    return localtime_s(&result, &value) == 0;
#else
    return localtime_r(&value, &result) != nullptr;
#endif
}

bool maclock_gmtime(std::time_t value, std::tm &result)
{
#ifdef _WIN32
    return gmtime_s(&result, &value) == 0;
#else
    return gmtime_r(&value, &result) != nullptr;
#endif
}

std::time_t maclock_timegm(std::tm value)
{
#ifdef _WIN32
    return static_cast<std::time_t>(_mkgmtime64(&value));
#else
    return timegm(&value);
#endif
}

std::filesystem::path maclock_host_path(const std::string &utf8)
{
#ifdef _WIN32
    return std::filesystem::path(utf8_to_wide(utf8));
#else
    return std::filesystem::path(utf8);
#endif
}

std::string maclock_host_path_utf8(const std::filesystem::path &path)
{
#ifdef _WIN32
    return wide_to_utf8(path.native());
#else
    return path.string();
#endif
}

std::string maclock_default_state_directory()
{
#ifdef _WIN32
    const DWORD required = GetEnvironmentVariableW(
        L"LOCALAPPDATA", nullptr, 0);
    if (required > 1)
    {
        std::wstring value(required, L'\0');
        const DWORD written = GetEnvironmentVariableW(
            L"LOCALAPPDATA", value.data(), required);
        if (written > 0 && written < required)
        {
            value.resize(written);
            return maclock_host_path_utf8(
                std::filesystem::path(value) / L"Fensoft" /
                L"Maclock Simulator");
        }
    }
    return "Maclock Simulator State";
#else
    const char *home = std::getenv("HOME");
    if (!home)
        return ".maclock-simulator";
#ifdef __APPLE__
    return std::string(home) +
           "/Library/Application Support/Maclock Simulator";
#else
    return std::string(home) +
           "/.local/share/maclock-simulator";
#endif
#endif
}

std::vector<std::filesystem::path> maclock_user_roots()
{
    std::vector<std::filesystem::path> roots;
#ifdef _WIN32
    for (const wchar_t *name : {L"USERPROFILE", L"LOCALAPPDATA"})
    {
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required <= 1)
            continue;
        std::wstring value(required, L'\0');
        const DWORD written = GetEnvironmentVariableW(
            name, value.data(), required);
        if (written > 0 && written < required)
        {
            value.resize(written);
            roots.emplace_back(value);
        }
    }
#else
    const char *home = std::getenv("HOME");
    if (home)
        roots.emplace_back(home);
#endif
    return roots;
}

std::vector<std::string> maclock_process_arguments(
    int argc, char **argv)
{
    std::vector<std::string> arguments;
#ifdef _WIN32
    int wide_count = 0;
    wchar_t **wide_arguments = CommandLineToArgvW(
        GetCommandLineW(), &wide_count);
    if (!wide_arguments)
        return arguments;
    arguments.reserve(static_cast<size_t>(wide_count));
    for (int index = 0; index < wide_count; ++index)
        arguments.push_back(wide_to_utf8(wide_arguments[index]));
    LocalFree(wide_arguments);
#else
    arguments.reserve(static_cast<size_t>(argc));
    for (int index = 0; index < argc; ++index)
        arguments.emplace_back(argv[index] ? argv[index] : "");
#endif
    return arguments;
}

bool maclock_replace_file(
    const std::filesystem::path &source,
    const std::filesystem::path &destination)
{
#ifdef _WIN32
    return MoveFileExW(
               source.c_str(), destination.c_str(),
               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    return !error;
#endif
}

bool maclock_restart_process(std::vector<std::string> arguments)
{
    if (arguments.empty())
        return false;
#ifdef _WIN32
    std::vector<std::wstring> wide_arguments;
    wide_arguments.reserve(arguments.size());
    for (const std::string &argument : arguments)
    {
        std::wstring wide = utf8_to_wide(argument);
        if (wide.empty() && !argument.empty())
            return false;
        wide_arguments.push_back(std::move(wide));
    }
    std::vector<const wchar_t *> native;
    native.reserve(wide_arguments.size() + 1);
    for (const std::wstring &argument : wide_arguments)
        native.push_back(argument.c_str());
    native.push_back(nullptr);
    _wexecv(native.front(), native.data());
#else
    std::vector<char *> native;
    native.reserve(arguments.size() + 1);
    for (std::string &argument : arguments)
        native.push_back(argument.data());
    native.push_back(nullptr);
    execv(native.front(), native.data());
#endif
    return false;
}
