#pragma once

#include <stddef.h>
#include <stdint.h>

#include <Preferences.h>
#include <lvgl.h>

#include "app_event_sink.h"
#include "sound_selector.h"

class TimerService
{
public:
    struct State;

    void begin(Preferences &preferences);
    void update(uint32_t now_ms);
    bool takeFinished();
    bool active() const;
    uint32_t remainingSeconds(uint32_t now_ms) const;
    void formatRemaining(
        uint32_t now_ms, char *text, size_t text_size) const;
    void start(uint16_t minutes);
    void cancel();
    bool configure(
        uint16_t minutes, const char *sound_path, uint8_t volume);
    uint16_t selectedMinutes() const;
    const char *soundPath() const;
    uint8_t volume() const;
    uint8_t volumeIndex() const;

    State &state();

private:
    State *state_ = nullptr;
};

class TimerView
{
public:
    explicit TimerView(TimerService &service)
        : service_(service) {}

    void begin(lv_obj_t *screen, AppEventSink &events);
    void hide();
    void enter(uint32_t now_ms);
    void show(uint32_t now_ms);
    void showFinished();
    void refreshLanguage();

private:
    TimerService &service_;
};
