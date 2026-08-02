#include "local_maclock_hal.h"
#include "local_hal_self_test.h"
#include "maclock_app.h"

#include <Arduino.h>

#include <cstdlib>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

namespace
{
enum class SelfTestMode
{
    None,
    Write,
    Verify
};

volatile std::sig_atomic_t interrupted = 0;
LocalMaclockHal *signal_hal = nullptr;

void handle_signal(int)
{
    interrupted = 1;
    if (signal_hal)
        signal_hal->requestQuit();
}

void print_usage(const char *program)
{
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --startup config|clock|emulator|firmware\n"
        << "  --data-dir PATH\n"
        << "  --state-dir PATH\n"
        << "  --reset-state\n"
        << "  --floppy-inserted\n"
        << "  --touch-disconnected\n"
        << "  --http-port PORT\n"
        << "  --scale auto|1|2|3|4\n"
        << "  --headless\n"
        << "  --run-for-ms MILLISECONDS\n"
        << "  --framebuffer-out PATH\n"
        << "  --self-test write|verify\n";
}

bool parse_startup(
    const std::string &value, LocalStartupMode &mode)
{
    if (value == "config")
        mode = LocalStartupMode::Config;
    else if (value == "clock")
        mode = LocalStartupMode::Clock;
    else if (value == "emulator")
        mode = LocalStartupMode::Emulator;
    else if (value == "firmware")
        mode = LocalStartupMode::Firmware;
    else
        return false;
    return true;
}

bool parse_options(
    int argc, char **argv, LocalMaclockOptions &options,
    SelfTestMode &self_test)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        auto next = [&]() -> const char *
        {
            return index + 1 < argc ? argv[++index] : nullptr;
        };
        if (argument == "--startup")
        {
            const char *value = next();
            if (!value || !parse_startup(value, options.startup))
                return false;
        }
        else if (argument == "--data-dir")
        {
            const char *value = next();
            if (!value)
                return false;
            options.data_directory = value;
        }
        else if (argument == "--state-dir")
        {
            const char *value = next();
            if (!value)
                return false;
            options.state_directory = value;
        }
        else if (argument == "--http-port")
        {
            const char *value = next();
            const long port = value
                                  ? std::strtol(value, nullptr, 10)
                                  : 0;
            if (port < 1 || port > 65535)
                return false;
            options.http_port = static_cast<uint16_t>(port);
        }
        else if (argument == "--scale")
        {
            const char *value = next();
            if (value && std::strcmp(value, "auto") == 0)
            {
                options.scale = 0;
                continue;
            }
            const long scale = value
                                   ? std::strtol(value, nullptr, 10)
                                   : 0;
            if (scale < 1 || scale > 4)
                return false;
            options.scale = static_cast<uint8_t>(scale);
        }
        else if (argument == "--reset-state")
        {
            options.reset_state = true;
        }
        else if (argument == "--floppy-inserted")
        {
            options.floppy_inserted = true;
        }
        else if (argument == "--touch-disconnected")
        {
            options.touch_disconnected = true;
            options.touch_option_set = true;
        }
        else if (argument == "--headless")
        {
            options.headless = true;
        }
        else if (argument == "--run-for-ms")
        {
            const char *value = next();
            const long duration = value
                                      ? std::strtol(value, nullptr, 10)
                                      : 0;
            if (duration < 1)
                return false;
            options.run_for_ms =
                static_cast<uint32_t>(duration);
        }
        else if (argument == "--framebuffer-out")
        {
            const char *value = next();
            if (!value)
                return false;
            options.framebuffer_output = value;
        }
        else if (argument == "--self-test")
        {
            const char *value = next();
            if (!value)
                return false;
            if (std::strcmp(value, "write") == 0)
                self_test = SelfTestMode::Write;
            else if (std::strcmp(value, "verify") == 0)
                self_test = SelfTestMode::Verify;
            else
                return false;
            options.headless = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            print_usage(argv[0]);
            std::exit(0);
        }
        else
        {
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    LocalMaclockOptions options;
    SelfTestMode self_test = SelfTestMode::None;
    if (!parse_options(argc, argv, options, self_test))
    {
        print_usage(argv[0]);
        return 2;
    }

    const std::string framebuffer_output =
        options.framebuffer_output;
    LocalMaclockHal hal(std::move(options));
    signal_hal = &hal;
    maclock_install_hal(hal);
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
    if (!hal.begin())
    {
        std::cerr << "Could not initialize LocalMaclockHal\n";
        return 1;
    }
    if (self_test != SelfTestMode::None)
    {
        const bool passed = maclock_local_run_self_test(
            self_test == SelfTestMode::Write
                ? LocalHalSelfTestMode::Write
                : LocalHalSelfTestMode::Verify);
        return passed ? 0 : 1;
    }
    MaclockApp app(hal);
    app.begin();
    while (!hal.shouldQuit() && !interrupted)
    {
        app.tick();
        delay(1);
    }
    if (!framebuffer_output.empty() &&
        !hal.saveFramebuffer(framebuffer_output))
    {
        std::cerr << "Could not write framebuffer to "
                  << framebuffer_output << "\n";
    }
    maclock_local_freertos_shutdown();
    if (hal.restartRequested())
    {
        std::vector<std::string> arguments;
        arguments.emplace_back(argv[0]);
        for (int i = 1; i < argc; ++i)
        {
            const std::string value = argv[i];
            if (value == "--startup")
            {
                ++i;
                continue;
            }
            if (value == "--reset-state" ||
                value == "--touch-disconnected")
                continue;
            arguments.push_back(value);
        }
        arguments.emplace_back("--startup");
        switch (hal.restartStartup())
        {
        case LocalStartupMode::Emulator:
            arguments.emplace_back("emulator");
            break;
        case LocalStartupMode::Clock:
            arguments.emplace_back("clock");
            break;
        default:
            arguments.emplace_back("config");
            break;
        }
        if (!hal.touchscreenPresent())
            arguments.emplace_back("--touch-disconnected");
        std::vector<char *> native;
        for (std::string &value : arguments)
            native.push_back(value.data());
        native.push_back(nullptr);
        execv(native[0], native.data());
        std::perror("Could not reset Maclock");
        return 1;
    }
    return 0;
}
