#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lvgl.h>

#include "app_event_sink.h"

class TimerService
{
public:
    struct State;

    void update(uint32_t now_ms);
    bool takeFinished();
    bool active() const;
    uint32_t remainingSeconds(uint32_t now_ms) const;
    void formatRemaining(
        uint32_t now_ms, char *text, size_t text_size) const;
    void cancel();
    const char *soundPath() const;
    uint8_t volume() const;

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
