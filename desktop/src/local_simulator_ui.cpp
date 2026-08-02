#include "local_simulator_ui.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{
constexpr int kDisplayWidth = 320;
constexpr int kDisplayHeight = 240;
constexpr int kLogicalWidth = 304;
constexpr int kLogicalTop = 16;

bool draw_labeled_combo(
    const char *table_id,
    const char *label,
    const char *combo_id,
    int *current_item,
    const char *const items[],
    int item_count)
{
    bool changed = false;
    if (ImGui::BeginTable(
            table_id, 2,
            ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(
            "label",
            ImGuiTableColumnFlags_WidthFixed,
            ImGui::CalcTextSize(label).x + 8.0f);
        ImGui::TableSetupColumn("value");
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        changed = ImGui::Combo(
            combo_id, current_item,
            items, item_count);
        ImGui::EndTable();
    }
    return changed;
}
}

void maclock_local_draw_hardware_panel(
    LocalSimulatorUiModel &model)
{
    static bool framebuffer_right_touch_active = false;
    static bool framebuffer_left_touch_active = false;
    static float wheel_accumulator = 0.0f;
    const uint8_t framebuffer_scale =
        model.scale > 1
            ? static_cast<uint8_t>(model.scale - 1)
            : model.scale;

    if (ImGui::BeginTable(
            "hardware-layout", 2,
            ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(
            "display", ImGuiTableColumnFlags_WidthFixed,
            static_cast<float>(
                kLogicalWidth * framebuffer_scale + 16));
        ImGui::TableSetupColumn("devices");
        ImGui::TableNextColumn();

        ImGui::TextUnformatted("Maclock framebuffer");
        ImGui::SameLine();
        ImGui::TextDisabled(
            "Backlight: %u%%",
            model.backlight_percent);
        ImGui::Image(
            reinterpret_cast<ImTextureID>(model.texture),
            ImVec2(
                kLogicalWidth * framebuffer_scale,
                (kDisplayHeight - kLogicalTop) *
                    framebuffer_scale),
            ImVec2(
                0.0f,
                static_cast<float>(kLogicalTop) /
                    kDisplayHeight),
            ImVec2(
                static_cast<float>(kLogicalWidth) /
                    kDisplayWidth,
                1.0f));
        const ImVec2 image_min = ImGui::GetItemRectMin();
        const ImVec2 image_max = ImGui::GetItemRectMax();
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool down =
            hovered &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left);
        framebuffer_left_touch_active = down;
        if (down)
        {
            const float x =
                (mouse.x - image_min.x) /
                (image_max.x - image_min.x) *
                kLogicalWidth;
            const float y =
                (mouse.y - image_min.y) /
                (image_max.y - image_min.y) *
                (kDisplayHeight - kLogicalTop);
            model.set_touch(
                x >= 0 && x < kLogicalWidth &&
                    y >= 0 &&
                    y <
                        kDisplayHeight -
                            kLogicalTop,
                x, y + kLogicalTop);
        }
        else
        {
            model.set_touch(false, 0, 0);
        }
        if (hovered &&
            ImGui::IsMouseClicked(
                ImGuiMouseButton_Right))
        {
            framebuffer_right_touch_active = true;
        }
        if (framebuffer_right_touch_active &&
            !ImGui::IsMouseDown(
                ImGuiMouseButton_Right))
        {
            framebuffer_right_touch_active = false;
        }
        if (hovered && ImGui::GetIO().MouseWheel != 0)
        {
            wheel_accumulator += ImGui::GetIO().MouseWheel;
            while (wheel_accumulator >= 6.0f)
            {
                model.encoder_delta(1);
                wheel_accumulator -= 6.0f;
            }
            while (wheel_accumulator <= -6.0f)
            {
                model.encoder_delta(-1);
                wheel_accumulator += 6.0f;
            }
        }

        ImGui::TableNextColumn();
        ImGui::SeparatorText("I2C devices");
        ImGui::TextUnformatted("Touchscreen");
        ImGui::SameLine();
        if (ImGui::Button(
                model.touchscreen_present
                    ? "Connected##touchscreen"
                    : "Disconnected##touchscreen",
                ImVec2(-1.0f, 38.0f)))
        {
            model.touchscreen_present =
                !model.touchscreen_present;
            model.set_touchscreen_present(
                model.touchscreen_present);
        }
        ImGui::TextDisabled(
            "Reset Maclock to apply boot-time touch detection.");
        const char *sensor_names[] = {
            "BMP5xx", "HTU2x", "Disconnected"};
        bool weather_changed = false;
        int sensor = model.weather_kind;
        if (draw_labeled_combo(
                "weather-sensor-row",
                "Weather sensor",
                "##weather-sensor",
                &sensor, sensor_names, 3))
        {
            model.weather_kind =
                static_cast<uint8_t>(sensor);
            model.weather_present = sensor != 2;
            weather_changed = true;
        }
        if (model.weather_kind == 0)
        {
            int address_index =
                model.weather_address == 0x50 ? 1 : 0;
            const char *addresses[] = {"0x47", "0x50"};
            if (draw_labeled_combo(
                    "bmp-address-row",
                    "BMP address",
                    "##bmp-address",
                    &address_index, addresses, 2))
            {
                model.weather_address =
                    address_index ? 0x50 : 0x47;
                weather_changed = true;
            }
            if (ImGui::Button("Temp -0.5"))
            {
                model.temperature -= 0.5f;
                weather_changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Temp +0.5"))
            {
                model.temperature += 0.5f;
                weather_changed = true;
            }
            if (ImGui::Button("Pressure -1"))
            {
                model.pressure -= 1.0f;
                weather_changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Pressure +1"))
            {
                model.pressure += 1.0f;
                weather_changed = true;
            }
            ImGui::Text(
                "%.1f C, %.1f hPa",
                model.temperature, model.pressure);
        }
        else if (model.weather_kind == 1)
        {
            if (ImGui::Button("Temp -0.5"))
            {
                model.temperature -= 0.5f;
                weather_changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Temp +0.5"))
            {
                model.temperature += 0.5f;
                weather_changed = true;
            }
            if (ImGui::Button("Humidity -1"))
            {
                model.humidity =
                    std::max(0.0f, model.humidity - 1);
                weather_changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Humidity +1"))
            {
                model.humidity =
                    std::min(100.0f, model.humidity + 1);
                weather_changed = true;
            }
            ImGui::Text(
                "%.1f C, %.0f%%",
                model.temperature, model.humidity);
        }
        if (weather_changed)
        {
            model.set_weather(
                model.weather_kind,
                model.weather_address,
                model.weather_present,
                model.temperature,
                model.pressure,
                model.humidity);
        }

        const char *rtc_names[] = {"DS3231", "DS1307"};
        int rtc_type = model.rtc_ds1307 ? 1 : 0;
        bool rtc_changed = false;
        if (ImGui::BeginTable(
                "rtc-row", 3,
                ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(
                "label",
                ImGuiTableColumnFlags_WidthFixed,
                ImGui::CalcTextSize("RTC").x + 8.0f);
            ImGui::TableSetupColumn("model");
            ImGui::TableSetupColumn(
                "present",
                ImGuiTableColumnFlags_WidthFixed,
                ImGui::CalcTextSize("Present").x + 34.0f);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("RTC");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo(
                    "##rtc-model",
                    &rtc_type, rtc_names, 2))
            {
                model.rtc_ds1307 = rtc_type == 1;
                rtc_changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Checkbox(
                    "Present##rtc-present",
                    &model.rtc_present))
            {
                rtc_changed = true;
            }
            ImGui::EndTable();
        }
        if (rtc_changed)
            model.set_rtc(
                model.rtc_ds1307, model.rtc_present);
        if (ImGui::Button("Reset RTC to OS time"))
            model.reset_rtc();

        ImGui::SeparatorText("Codec");
        int shown_volume = model.volume;
        ImGui::Text(
            "Application volume: %d%%",
            shown_volume);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderInt(
                "##application-volume",
                &shown_volume, 0, 100))
        {
            model.set_volume(
                static_cast<uint8_t>(shown_volume));
        }
        ImGui::Text(
            "Output: %s  |  %u Hz",
            !model.audio_available
                ? "Unavailable"
                : model.muted ? "Muted" : "Playing",
            model.audio_rate);

        ImGui::SeparatorText("Network");
        ImGui::TextUnformatted(
            "Wi-Fi: Mac Host Network (-42 dBm)");
        const std::string url =
            "http://127.0.0.1:" +
            std::to_string(model.http_port) + "/";
        ImGui::TextUnformatted(url.c_str());
        if (ImGui::Button("Open Portal"))
            SDL_OpenURL(url.c_str());
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Physical controls");
    const float reset_width =
        (ImGui::GetContentRegionAvail().x -
         ImGui::GetStyle().ItemSpacing.x * 2.0f) /
        3.0f;
    if (ImGui::Button(
            "Reset to Emulator",
            ImVec2(reset_width, 46.0f)))
        model.reset_maclock(2);
    ImGui::SameLine();
    if (ImGui::Button(
            "Reset to Clock",
            ImVec2(reset_width, 46.0f)))
        model.reset_maclock(1);
    ImGui::SameLine();
    if (ImGui::Button(
            "Reset to Settings",
            ImVec2(reset_width, 46.0f)))
        model.reset_maclock(0);
    const ImGuiStyle &style = ImGui::GetStyle();
    const float available_width =
        ImGui::GetContentRegionAvail().x;
    const float primary_width =
        std::max(
            100.0f,
            (available_width -
             style.ItemSpacing.x * 4.0f) /
                5.0f);
    constexpr float kControlHeight = 46.0f;

    if (model.floppy)
    {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImGui::GetStyleColorVec4(
                ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(
            model.floppy
                ? "Floppy: inserted"
                : "Floppy: ejected",
            ImVec2(primary_width, kControlHeight)))
    {
        model.set_floppy(!model.floppy);
    }
    if (model.floppy)
        ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::Button(
        "Alarm",
        ImVec2(primary_width, kControlHeight));
    const bool alarm_down = ImGui::IsItemActive();

    ImGui::SameLine();
    ImGui::Button(
        "Clock",
        ImVec2(primary_width, kControlHeight));
    const bool clock_down = ImGui::IsItemActive();

    ImGui::SameLine();
    ImGui::Button(
        "Alarm + Clock",
        ImVec2(primary_width, kControlHeight));
    const bool alarm_clock_down =
        ImGui::IsItemActive();

    ImGui::SameLine();
    ImGui::Button(
        "Discrete touch",
        ImVec2(primary_width, kControlHeight));
    const bool touch_down = ImGui::IsItemActive();

    model.set_alarm(
        alarm_down || alarm_clock_down);
    model.set_clock(
        clock_down || alarm_clock_down);
    model.set_discrete_touch(
        framebuffer_right_touch_active ||
        ((model.emulator_active ||
          !model.touchscreen_present) &&
         framebuffer_left_touch_active) ||
        touch_down);

    const float secondary_width =
        std::max(
            150.0f,
            (available_width -
             style.ItemSpacing.x * 2.0f) /
                3.0f);
    if (ImGui::Button(
            "Encoder -",
            ImVec2(
                secondary_width,
                kControlHeight)))
    {
        model.encoder_delta(-1);
    }
    ImGui::SameLine();
    if (ImGui::Button(
            "Encoder +",
            ImVec2(
                secondary_width,
                kControlHeight)))
    {
        model.encoder_delta(1);
    }
    ImGui::SameLine();
    char encoder_value[48];
    snprintf(
        encoder_value, sizeof(encoder_value),
        "Encoder value: %lld",
        static_cast<long long>(
            model.encoder_value()));
    ImGui::BeginDisabled();
    ImGui::Button(
        encoder_value,
        ImVec2(
            secondary_width,
            kControlHeight));
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}
