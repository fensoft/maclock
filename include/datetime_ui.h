#pragma once

#include <lvgl.h>
#include <RTClib.h>

#include "regional_settings.h"
#include "app_event_sink.h"

class DateTimeEditor
{
public:
    struct State;

    void begin(lv_obj_t *screen, AppEventSink &events);
    void hide();
    void show();
    void enter(const DateTime &current);
    void setDateFormat(UiDateFormat format);
    void refreshLanguage();

    State &state();

private:
    State *state_ = nullptr;
};
