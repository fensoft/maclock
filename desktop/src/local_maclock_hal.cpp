#include "local_maclock_hal.h"
#include "local_audio_output.h"
#include "local_simulator_ui.h"

#include <Arduino.h>
#include "ESP32Encoder.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
constexpr int kDisplayWidth = 320;
constexpr int kDisplayHeight = 240;
constexpr int kLogicalWidth = 304;
constexpr int kLogicalHeight = 224;
constexpr int kLogicalTop = 16;
constexpr int kDevicePanelWidth = 400;
constexpr int kWindowHorizontalPadding = 20;
constexpr int kWindowVerticalPadding = 180;
constexpr int kMinimumWindowHeight = 650;

struct LocalWindowSize
{
    int width;
    int height;
};

LocalWindowSize window_size_for_scale(uint8_t scale)
{
    return {
        kLogicalWidth * scale +
            kDevicePanelWidth +
            kWindowHorizontalPadding,
        std::max(
            kMinimumWindowHeight,
            kLogicalHeight * scale +
                kWindowVerticalPadding)};
}

uint8_t automatic_scale(const SDL_Rect &usable)
{
    const int maximum_width =
        std::max(640, usable.w - 80);
    const int maximum_height =
        std::max(520, usable.h - 80);
    for (int scale = 4; scale >= 1; --scale)
    {
        const LocalWindowSize size =
            window_size_for_scale(
                static_cast<uint8_t>(scale));
        if (size.width <= maximum_width &&
            size.height <= maximum_height)
        {
            return static_cast<uint8_t>(scale);
        }
    }
    return 1;
}

void apply_classic_mac_style(ImGuiStyle &style)
{
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 0;
    style.ChildRounding = 0;
    style.FrameRounding = 0;
    style.PopupRounding = 0;
    style.ScrollbarRounding = 0;
    style.GrabRounding = 0;
    style.TabRounding = 0;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_Text] =
        ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    colors[ImGuiCol_TextDisabled] =
        ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    colors[ImGuiCol_WindowBg] =
        ImVec4(0.82f, 0.82f, 0.82f, 1.0f);
    colors[ImGuiCol_ChildBg] =
        ImVec4(0.82f, 0.82f, 0.82f, 1.0f);
    colors[ImGuiCol_PopupBg] =
        ImVec4(0.94f, 0.94f, 0.94f, 1.0f);
    colors[ImGuiCol_Border] =
        ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
    colors[ImGuiCol_BorderShadow] =
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_FrameBg] =
        ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] =
        ImVec4(0.86f, 0.86f, 0.86f, 1.0f);
    colors[ImGuiCol_FrameBgActive] =
        ImVec4(0.72f, 0.72f, 0.72f, 1.0f);
    colors[ImGuiCol_TitleBg] =
        ImVec4(0.82f, 0.82f, 0.82f, 1.0f);
    colors[ImGuiCol_TitleBgActive] =
        ImVec4(0.82f, 0.82f, 0.82f, 1.0f);
    colors[ImGuiCol_CheckMark] =
        ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    colors[ImGuiCol_SliderGrab] =
        ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] =
        ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    colors[ImGuiCol_Button] =
        ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    colors[ImGuiCol_ButtonHovered] =
        ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
    colors[ImGuiCol_ButtonActive] =
        ImVec4(0.64f, 0.64f, 0.64f, 1.0f);
    colors[ImGuiCol_Header] =
        ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    colors[ImGuiCol_HeaderHovered] =
        ImVec4(0.78f, 0.78f, 0.78f, 1.0f);
    colors[ImGuiCol_HeaderActive] =
        ImVec4(0.64f, 0.64f, 0.64f, 1.0f);
    colors[ImGuiCol_Separator] =
        ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] =
        ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    colors[ImGuiCol_SeparatorActive] =
        ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
    colors[ImGuiCol_TableHeaderBg] =
        ImVec4(0.82f, 0.82f, 0.82f, 1.0f);
    colors[ImGuiCol_NavHighlight] =
        ImVec4(0.02f, 0.02f, 0.02f, 1.0f);
}

uint32_t local_wall_epoch()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
#if defined(__APPLE__) || defined(__linux__)
    return static_cast<uint32_t>(timegm(&local));
#else
    return static_cast<uint32_t>(std::mktime(&local));
#endif
}

uint16_t apply_backlight(
    uint16_t pixel, uint8_t pwm)
{
    if (pwm == 255)
        return pixel;
    const uint16_t red = static_cast<uint16_t>(
        (((pixel >> 11) & 0x1F) * pwm + 127) /
        255);
    const uint16_t green = static_cast<uint16_t>(
        (((pixel >> 5) & 0x3F) * pwm + 127) /
        255);
    const uint16_t blue = static_cast<uint16_t>(
        ((pixel & 0x1F) * pwm + 127) / 255);
    return static_cast<uint16_t>(
        (red << 11) | (green << 5) | blue);
}

std::string default_state_directory()
{
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
}

bool safe_reset_directory(
    const std::filesystem::path &requested,
    const std::filesystem::path &data_directory)
{
    if (requested.empty())
        return false;
    const auto path =
        std::filesystem::absolute(requested).lexically_normal();
    const auto root = path.root_path();
    const auto temporary =
        std::filesystem::temp_directory_path().lexically_normal();
    const auto current =
        std::filesystem::current_path().lexically_normal();
    const auto data =
        std::filesystem::absolute(data_directory)
            .lexically_normal();
    if (path == root || path == temporary ||
        path == current || path == data ||
        path.filename().empty())
    {
        return false;
    }
    const char *home = std::getenv("HOME");
    return !home ||
           path != std::filesystem::path(home).lexically_normal();
}
} // namespace

struct LocalMaclockHal::Impl
{
    explicit Impl(LocalMaclockOptions configured)
        : options(std::move(configured)),
          started(std::chrono::steady_clock::now()),
          framebuffer(kDisplayWidth * kDisplayHeight, 0)
    {
        pins.fill(HIGH);
        pin_modes.fill(MaclockPinMode::Input);
        i2c_register.fill(0);
        i2c_register[0x38] = 0;
        if (options.data_directory.empty())
            options.data_directory = MACLOCK_DATA_DIR;
        if (options.state_directory.empty())
            options.state_directory = default_state_directory();

        pins[GPIO_CHARGING] = LOW;
        floppy = options.floppy_inserted;
        pins[GPIO_FLOPPY] = floppy ? LOW : HIGH;
        pins[GPIO_ALARM] = HIGH;
        pins[GPIO_CLOCK] =
            options.startup == LocalStartupMode::Config
                ? LOW
                : HIGH;
        pins[GPIO_TOUCH] = HIGH;
    }

    LocalMaclockOptions options;
    std::chrono::steady_clock::time_point started;
    std::thread::id main_thread;
    std::atomic<bool> quit{false};
    bool begun = false;
    bool app_ready = false;

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    bool imgui_ready = false;
    uint64_t last_frame_ms = 0;

    mutable std::mutex framebuffer_mutex;
    std::vector<uint16_t> framebuffer;
    int window_x = 0;
    int window_y = 0;
    int window_width = kDisplayWidth;
    int window_height = kDisplayHeight;
    size_t window_position = 0;
    uint8_t rotation = 3;
    int panel_width = kDisplayWidth;
    int panel_height = kDisplayHeight;

    mutable std::mutex io_mutex;
    std::array<int, 64> pins;
    std::array<int, 64> pwm{};
    std::array<MaclockPinMode, 64> pin_modes;
    void *encoder = nullptr;
    bool floppy = false;
    bool screen_touch = false;
    uint16_t touch_raw_x = 0;
    uint16_t touch_raw_y = 0;

    uint8_t weather_kind = 0;
    uint8_t weather_address = 0x47;
    bool weather_present = true;
    float temperature = 21.0f;
    float pressure = 1013.0f;
    float humidity = 50.0f;

    bool rtc_present = true;
    bool rtc_ds1307 = false;
    int64_t rtc_offset = 0;

    uint32_t i2c_frequency = 100000;
    uint8_t tx_address = 0;
    std::vector<uint8_t> tx_data;
    std::deque<uint8_t> rx_data;
    std::array<uint8_t, 128> i2c_register;
    std::array<std::array<uint8_t, 256>, 128> device_registers{};

    uint32_t spi_frequency = 0;
    bool spi_active = false;

    LocalAudioOutput audio;

    bool i2cPresent(uint8_t address) const
    {
        if (address == 0x18 || address == 0x38)
            return true;
        if (address == 0x68)
            return rtc_present;
        if (weather_kind == 0)
            return weather_present &&
                   address == weather_address;
        if (weather_kind == 1)
            return weather_present && address == 0x40;
        return false;
    }

    uint8_t readDeviceRegister(uint8_t address, uint8_t reg)
    {
        if (address == 0x38)
        {
            switch (reg)
            {
            case 0x02:
                return screen_touch ? 1 : 0;
            case 0x03:
                return static_cast<uint8_t>(
                    (touch_raw_x >> 8) & 0x0F);
            case 0x04:
                return static_cast<uint8_t>(touch_raw_x);
            case 0x05:
                return static_cast<uint8_t>(
                    (touch_raw_y >> 8) & 0x0F);
            case 0x06:
                return static_cast<uint8_t>(touch_raw_y);
            case 0x9F:
                return 0x26;
            case 0xA0:
                return 0x01;
            case 0xA3:
                return 0x64;
            case 0xA8:
                return 0x11;
            default:
                return device_registers[address][reg];
            }
        }
        if (address == 0x68 && reg == 0x0E)
        {
            uint8_t &control = device_registers[address][reg];
            if (!rtc_ds1307)
                control &= static_cast<uint8_t>(~0x20);
            return control;
        }
        return device_registers[address][reg];
    }

    void writeDeviceRegister(
        uint8_t address, uint8_t reg, uint8_t value)
    {
        device_registers[address][reg] = value;
        if (address == 0x68 && reg == 0x0E && !rtc_ds1307)
            device_registers[address][reg] &=
                static_cast<uint8_t>(~0x20);
    }

    void setTouch(bool down, float x, float y)
    {
        std::lock_guard<std::mutex> lock(io_mutex);
        screen_touch = down;
        if (!down)
            return;
        const int logical_x = std::clamp(
            static_cast<int>(x), 0, kLogicalWidth - 1);
        const int logical_y = std::clamp(
            static_cast<int>(y) - kLogicalTop,
            0, kLogicalHeight - 1);
        const int rotated_x =
            logical_x * 320 / (kLogicalWidth - 1);
        const int rotated_y =
            logical_y * 240 / (kLogicalHeight - 1);
        touch_raw_x = static_cast<uint16_t>(rotated_y);
        touch_raw_y = static_cast<uint16_t>(
            320 - rotated_x);
    }

    void setButton(int pin, bool pressed)
    {
        std::lock_guard<std::mutex> lock(io_mutex);
        if (pin >= 0 && pin < static_cast<int>(pins.size()))
        {
            pins[pin] = pressed ? LOW : HIGH;
        }
    }

    void encoderDelta(int delta)
    {
        if (encoder)
            static_cast<ESP32Encoder *>(encoder)->localAdd(delta);
    }

    void render()
    {
        if (!imgui_ready)
            return;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            {
                quit = true;
            }
        }

        std::vector<uint16_t> snapshot;
        {
            std::lock_guard<std::mutex> lock(framebuffer_mutex);
            snapshot = framebuffer;
        }
        uint8_t backlight_pwm = 0;
        {
            std::lock_guard<std::mutex> lock(io_mutex);
            backlight_pwm = static_cast<uint8_t>(
                std::clamp(pwm[TFT_BL_VAR], 0, 255));
        }
        if (backlight_pwm != 255)
        {
            for (uint16_t &pixel : snapshot)
                pixel =
                    apply_backlight(
                        pixel, backlight_pwm);
        }
        SDL_UpdateTexture(
            texture, nullptr, snapshot.data(),
            kDisplayWidth * sizeof(uint16_t));

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        const ImGuiViewport *viewport =
            ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin(
            "Maclock Local HAL", nullptr,
            ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings);
        LocalSimulatorUiModel model;
        model.texture = texture;
        model.scale = options.scale;
        model.backlight_percent =
            static_cast<uint8_t>(
                (backlight_pwm * 100 + 127) / 255);
        {
            std::lock_guard<std::mutex> lock(io_mutex);
            model.weather_kind = weather_kind;
            model.weather_address = weather_address;
            model.weather_present = weather_present;
            model.temperature = temperature;
            model.pressure = pressure;
            model.humidity = humidity;
            model.rtc_ds1307 = rtc_ds1307;
            model.rtc_present = rtc_present;
            model.floppy = floppy;
        }
        model.set_weather =
            [this](
                uint8_t kind, uint8_t address,
                bool present, float temp,
                float pressure_value, float humidity_value)
            {
                std::lock_guard<std::mutex> lock(io_mutex);
                weather_kind = kind;
                weather_address = address;
                weather_present = present;
                temperature = temp;
                pressure = pressure_value;
                humidity = humidity_value;
            };
        model.set_rtc =
            [this](bool ds1307, bool present)
            {
                std::lock_guard<std::mutex> lock(io_mutex);
                rtc_ds1307 = ds1307;
                rtc_present = present;
            };
        model.reset_rtc =
            [this]()
            {
                std::lock_guard<std::mutex> lock(io_mutex);
                rtc_offset = 0;
            };
        const LocalAudioSnapshot audio_state =
            audio.snapshot();
        model.audio_available = audio_state.available;
        model.muted = audio_state.muted;
        model.audio_rate = audio_state.sample_rate;
        model.volume = audio_state.volume;
        model.set_volume =
            [this](uint8_t new_volume)
            {
                audio.setVolume(new_volume);
            };
        model.http_port = options.http_port;
        model.set_touch =
            [this](bool down, float x, float y)
            {
                setTouch(down, x, y);
            };
        model.encoder_delta =
            [this](int delta) { encoderDelta(delta); };
        model.set_floppy =
            [this](bool pressed)
            {
                std::lock_guard<std::mutex> lock(io_mutex);
                floppy = pressed;
                pins[GPIO_FLOPPY] =
                    pressed ? LOW : HIGH;
            };
        model.set_alarm =
            [this](bool pressed)
            {
                setButton(GPIO_ALARM, pressed);
            };
        model.set_clock =
            [this](bool pressed)
            {
                setButton(GPIO_CLOCK, pressed);
            };
        model.set_discrete_touch =
            [this](bool pressed)
            {
                setButton(GPIO_TOUCH, pressed);
            };
        model.encoder_value =
            [this]()
            {
                const auto *registered_encoder =
                    static_cast<ESP32Encoder *>(encoder);
                return registered_encoder
                           ? registered_encoder->getCount()
                           : int64_t{0};
            };
        maclock_local_draw_hardware_panel(model);
        ImGui::End();
        ImGui::Render();

        SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(
            ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
};

LocalMaclockHal::LocalMaclockHal(LocalMaclockOptions options)
    : impl_(new Impl(std::move(options)))
{
}

LocalMaclockHal::~LocalMaclockHal()
{
    impl_->audio.stop();
    if (impl_->imgui_ready)
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (impl_->texture)
        SDL_DestroyTexture(impl_->texture);
    if (impl_->renderer)
        SDL_DestroyRenderer(impl_->renderer);
    if (impl_->window)
        SDL_DestroyWindow(impl_->window);
    SDL_Quit();
}

bool LocalMaclockHal::begin()
{
    if (impl_->begun)
        return true;
    impl_->main_thread = std::this_thread::get_id();
    if (impl_->options.reset_state)
    {
        if (!safe_reset_directory(
                impl_->options.state_directory,
                impl_->options.data_directory))
        {
            Serial.println(
                "Refusing to reset an unsafe simulator state path");
            return false;
        }
        std::error_code error;
        std::filesystem::remove_all(
            impl_->options.state_directory, error);
        if (error)
            return false;
    }
    std::error_code directory_error;
    std::filesystem::create_directories(
        impl_->options.state_directory, directory_error);
    if (directory_error)
        return false;

    if (!impl_->options.headless)
    {
        if (!SDL_Init(
                SDL_INIT_VIDEO | SDL_INIT_AUDIO |
                SDL_INIT_EVENTS))
        {
            return false;
        }
        SDL_Rect usable = {0, 0, 1440, 900};
        const SDL_DisplayID primary =
            SDL_GetPrimaryDisplay();
        if (primary)
            SDL_GetDisplayUsableBounds(primary, &usable);
        if (impl_->options.scale == 0)
            impl_->options.scale =
                automatic_scale(usable);
        const LocalWindowSize window_size =
            window_size_for_scale(
                impl_->options.scale);
        impl_->window = SDL_CreateWindow(
            "Maclock Simulator",
            window_size.width,
            window_size.height,
            SDL_WINDOW_RESIZABLE);
        if (!impl_->window)
            return false;
        SDL_SetWindowMinimumSize(
            impl_->window, 760, 600);
        SDL_SetWindowPosition(
            impl_->window,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED);
        impl_->renderer =
            SDL_CreateRenderer(impl_->window, nullptr);
        if (!impl_->renderer)
            return false;
        impl_->texture = SDL_CreateTexture(
            impl_->renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            kDisplayWidth, kDisplayHeight);
        if (!impl_->texture)
            return false;
        SDL_SetTextureScaleMode(
            impl_->texture, SDL_SCALEMODE_NEAREST);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        ImFont *font = nullptr;
        const std::filesystem::path font_path =
            std::filesystem::path(
                impl_->options.data_directory) /
            "Chicago.ttf";
        if (std::filesystem::exists(font_path))
        {
            font = io.Fonts->AddFontFromFileTTF(
                font_path.string().c_str(), 18.0f);
        }
        if (!font)
        {
            ImFontConfig font_config;
            font_config.SizePixels = 17.0f;
            io.Fonts->AddFontDefault(&font_config);
        }
        ImGuiStyle &style = ImGui::GetStyle();
        apply_classic_mac_style(style);
        ImGui_ImplSDL3_InitForSDLRenderer(
            impl_->window, impl_->renderer);
        ImGui_ImplSDLRenderer3_Init(impl_->renderer);
        impl_->imgui_ready = true;
    }
    if (!impl_->audio.start(44100))
        return false;
    impl_->begun = true;
    return true;
}

void LocalMaclockHal::pump()
{
    if (impl_->options.run_for_ms &&
        millis() >= impl_->options.run_for_ms)
    {
        impl_->quit = true;
    }
    if (impl_->quit)
    {
        impl_->setButton(GPIO_CLOCK, true);
        impl_->setButton(GPIO_ALARM, true);
    }
    if (impl_->options.headless ||
        std::this_thread::get_id() != impl_->main_thread)
    {
        return;
    }
    const uint64_t now = micros() / 1000;
    if (now - impl_->last_frame_ms < 16)
        return;
    impl_->last_frame_ms = now;
    impl_->render();
}

bool LocalMaclockHal::shouldQuit() const
{
    return impl_->quit;
}

void LocalMaclockHal::requestQuit() noexcept
{
    impl_->quit = true;
}

void LocalMaclockHal::appReady()
{
    impl_->app_ready = true;
    if (impl_->options.startup == LocalStartupMode::Config)
        impl_->setButton(GPIO_CLOCK, false);
}

bool LocalMaclockHal::overrideBootEmulator(
    bool &enabled) const
{
    switch (impl_->options.startup)
    {
    case LocalStartupMode::Emulator:
        enabled = true;
        return true;
    case LocalStartupMode::Config:
    case LocalStartupMode::Clock:
        enabled = false;
        return true;
    case LocalStartupMode::Firmware:
        return false;
    }
    return false;
}

uint32_t LocalMaclockHal::millis() const
{
    return static_cast<uint32_t>(micros() / 1000);
}

uint64_t LocalMaclockHal::micros() const
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - impl_->started)
            .count());
}

void LocalMaclockHal::delay(uint32_t milliseconds)
{
    const uint64_t until =
        micros() + static_cast<uint64_t>(milliseconds) * 1000;
    do
    {
        pump();
        const uint64_t remaining =
            until > micros() ? until - micros() : 0;
        std::this_thread::sleep_for(
            std::chrono::microseconds(
                std::min<uint64_t>(remaining, 1000)));
    } while (micros() < until);
}

void LocalMaclockHal::yield()
{
    pump();
    std::this_thread::yield();
}

void LocalMaclockHal::pinMode(
    int pin, MaclockPinMode mode)
{
    if (pin < 0 || pin >= static_cast<int>(impl_->pins.size()))
        return;
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->pin_modes[pin] = mode;
    if (mode == MaclockPinMode::InputPullup &&
        pin != GPIO_FLOPPY && pin != GPIO_ALARM &&
        pin != GPIO_CLOCK && pin != GPIO_TOUCH)
    {
        impl_->pins[pin] = HIGH;
    }
    if (mode == MaclockPinMode::InputPulldown)
        impl_->pins[pin] = LOW;
}

int LocalMaclockHal::digitalRead(int pin) const
{
    if (pin < 0 || pin >= static_cast<int>(impl_->pins.size()))
        return LOW;
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->pins[pin];
}

void LocalMaclockHal::digitalWrite(int pin, int value)
{
    if (pin < 0 || pin >= static_cast<int>(impl_->pins.size()))
        return;
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->pins[pin] = value ? HIGH : LOW;
}

void LocalMaclockHal::analogWrite(int pin, int value)
{
    if (pin < 0 || pin >= static_cast<int>(impl_->pwm.size()))
        return;
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->pwm[pin] = std::clamp(value, 0, 255);
}

int LocalMaclockHal::analogRead(int) const
{
    return 0;
}

void LocalMaclockHal::begin(
    int, int, uint32_t frequency)
{
    impl_->i2c_frequency = frequency;
}

void LocalMaclockHal::setClock(uint32_t frequency)
{
    impl_->i2c_frequency = frequency;
}

void LocalMaclockHal::beginTransmission(uint8_t address)
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->tx_address = address;
    impl_->tx_data.clear();
}

size_t LocalMaclockHal::write(uint8_t value)
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->tx_data.push_back(value);
    return 1;
}

uint8_t LocalMaclockHal::endTransmission(bool)
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    const uint8_t address = impl_->tx_address;
    if (!impl_->i2cPresent(address))
        return 4;
    if (!impl_->tx_data.empty())
    {
        uint8_t reg = impl_->tx_data.front();
        impl_->i2c_register[address] = reg;
        for (size_t i = 1; i < impl_->tx_data.size(); ++i)
            impl_->writeDeviceRegister(
                address, reg++,
                impl_->tx_data[i]);
    }
    return 0;
}

size_t LocalMaclockHal::requestFrom(
    uint8_t address, size_t count)
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->rx_data.clear();
    if (!impl_->i2cPresent(address))
        return 0;
    uint8_t reg = impl_->i2c_register[address];
    for (size_t i = 0; i < count; ++i)
        impl_->rx_data.push_back(
            impl_->readDeviceRegister(address, reg++));
    impl_->i2c_register[address] = reg;
    return impl_->rx_data.size();
}

int LocalMaclockHal::available() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return static_cast<int>(impl_->rx_data.size());
}

int LocalMaclockHal::read()
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    if (impl_->rx_data.empty())
        return -1;
    const int value = impl_->rx_data.front();
    impl_->rx_data.pop_front();
    return value;
}

void LocalMaclockHal::beginBus()
{
}

void LocalMaclockHal::beginTransaction(uint32_t frequency)
{
    impl_->spi_frequency = frequency;
    impl_->spi_active = true;
}

uint8_t LocalMaclockHal::transfer(uint8_t value)
{
    return value;
}

void LocalMaclockHal::endTransaction()
{
    impl_->spi_active = false;
}

void LocalMaclockHal::initialize(int width, int height)
{
    impl_->panel_width = width;
    impl_->panel_height = height;
    fill(0);
}

void LocalMaclockHal::setRotation(uint8_t rotation)
{
    impl_->rotation = rotation;
}

uint8_t LocalMaclockHal::rotation() const
{
    return impl_->rotation;
}

int LocalMaclockHal::width() const
{
    return impl_->panel_width;
}

int LocalMaclockHal::height() const
{
    return impl_->panel_height;
}

void LocalMaclockHal::setAddressWindow(
    int x, int y, int width, int height)
{
    std::lock_guard<std::mutex> lock(
        impl_->framebuffer_mutex);
    impl_->window_x = x;
    impl_->window_y = y;
    impl_->window_width = width;
    impl_->window_height = height;
    impl_->window_position = 0;
}

void LocalMaclockHal::writePixels(
    const uint16_t *pixels, size_t count, bool swap_bytes)
{
    if (!pixels)
        return;
    (void)swap_bytes;
    std::lock_guard<std::mutex> lock(
        impl_->framebuffer_mutex);
    for (size_t i = 0; i < count; ++i)
    {
        const size_t offset = impl_->window_position++;
        const int x = impl_->window_x +
                      static_cast<int>(
                          offset % impl_->window_width);
        const int y = impl_->window_y +
                      static_cast<int>(
                          offset / impl_->window_width);
        if (x >= 0 && x < kDisplayWidth &&
            y >= 0 && y < kDisplayHeight)
        {
            impl_->framebuffer[
                y * kDisplayWidth + x] =
                pixels[i];
        }
    }
}

void LocalMaclockHal::fill(uint16_t color)
{
    std::lock_guard<std::mutex> lock(
        impl_->framebuffer_mutex);
    std::fill(
        impl_->framebuffer.begin(),
        impl_->framebuffer.end(), color);
}

void LocalMaclockHal::fillRect(
    int x, int y, int width, int height, uint16_t color)
{
    std::lock_guard<std::mutex> lock(
        impl_->framebuffer_mutex);
    for (int row = std::max(0, y);
         row < std::min(kDisplayHeight, y + height); ++row)
    {
        for (int column = std::max(0, x);
             column < std::min(kDisplayWidth, x + width);
             ++column)
        {
            impl_->framebuffer[
                row * kDisplayWidth + column] = color;
        }
    }
}

void LocalMaclockHal::drawRect(
    int x, int y, int width, int height, uint16_t color)
{
    fillRect(x, y, width, 1, color);
    fillRect(x, y + height - 1, width, 1, color);
    fillRect(x, y, 1, height, color);
    fillRect(x + width - 1, y, 1, height, color);
}

bool LocalMaclockHal::begin(
    uint32_t sample_rate, uint8_t channels)
{
    return impl_->audio.start(sample_rate, channels);
}

void LocalMaclockHal::stop()
{
    impl_->audio.stop();
}

size_t LocalMaclockHal::write(
    const int16_t *samples, size_t frame_count,
    uint8_t channels)
{
    return impl_->audio.write(
        samples, frame_count, channels);
}

void LocalMaclockHal::drain()
{
    impl_->audio.drain();
}

void LocalMaclockHal::setVolume(uint8_t volume)
{
    impl_->audio.setVolume(volume);
}

void LocalMaclockHal::setMuted(bool muted)
{
    impl_->audio.setMuted(muted);
}

const char *LocalMaclockHal::dataDirectory() const
{
    return impl_->options.data_directory.c_str();
}

const char *LocalMaclockHal::stateDirectory() const
{
    return impl_->options.state_directory.c_str();
}

uint16_t LocalMaclockHal::remapServerPort(uint16_t requested) const
{
    return requested == 80
               ? impl_->options.http_port
               : requested;
}

const char *LocalMaclockHal::simulatedSsid() const
{
    return "Mac Host Network";
}

void LocalMaclockHal::registerEncoder(void *encoder)
{
    impl_->encoder = encoder;
}

bool LocalMaclockHal::rtcPresent() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->rtc_present;
}

bool LocalMaclockHal::rtcIsDs1307() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->rtc_ds1307;
}

uint32_t LocalMaclockHal::rtcEpoch() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return static_cast<uint32_t>(
        static_cast<int64_t>(local_wall_epoch()) +
        impl_->rtc_offset);
}

void LocalMaclockHal::adjustRtc(uint32_t epoch)
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->rtc_offset =
        static_cast<int64_t>(epoch) - local_wall_epoch();
}

void LocalMaclockHal::resetRtc()
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    impl_->rtc_offset = 0;
}

uint8_t LocalMaclockHal::weatherKind() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->weather_present
               ? impl_->weather_kind
               : 2;
}

uint8_t LocalMaclockHal::weatherAddress() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->weather_address;
}

float LocalMaclockHal::temperature() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->temperature;
}

float LocalMaclockHal::pressure() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->pressure;
}

float LocalMaclockHal::humidity() const
{
    std::lock_guard<std::mutex> lock(impl_->io_mutex);
    return impl_->humidity;
}

bool LocalMaclockHal::saveFramebuffer(
    const std::string &path) const
{
    std::ofstream output(
        path, std::ios::binary | std::ios::trunc);
    if (!output)
        return false;
    output << "P6\n" << kDisplayWidth << " "
           << kDisplayHeight << "\n255\n";
    std::lock_guard<std::mutex> lock(
        impl_->framebuffer_mutex);
    for (const uint16_t pixel : impl_->framebuffer)
    {
        const unsigned char rgb[3] = {
            static_cast<unsigned char>(
                ((pixel >> 11) & 0x1F) * 255 / 31),
            static_cast<unsigned char>(
                ((pixel >> 5) & 0x3F) * 255 / 63),
            static_cast<unsigned char>(
                (pixel & 0x1F) * 255 / 31)};
        output.write(
            reinterpret_cast<const char *>(rgb), 3);
    }
    return static_cast<bool>(output);
}

LocalMaclockHal &local_maclock_hal()
{
    return static_cast<LocalMaclockHal &>(maclock_hal());
}
