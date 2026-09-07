#pragma once

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

bool maclock_localtime(std::time_t value, std::tm &result);
bool maclock_gmtime(std::time_t value, std::tm &result);
std::time_t maclock_timegm(std::tm value);

std::filesystem::path maclock_host_path(const std::string &utf8);
std::string maclock_host_path_utf8(const std::filesystem::path &path);
std::string maclock_default_state_directory();
std::vector<std::filesystem::path> maclock_user_roots();
std::vector<std::string> maclock_process_arguments(
    int argc, char **argv);
bool maclock_replace_file(
    const std::filesystem::path &source,
    const std::filesystem::path &destination);
bool maclock_restart_process(std::vector<std::string> arguments);
