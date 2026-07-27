#pragma once

#include <lvgl.h>
#include <RTClib.h>

#include "regional_settings.h"

void datetime_ui_init(lv_obj_t *scr);
void datetime_ui_hide();
void datetime_ui_show();
void datetime_ui_enter(const DateTime &current);
void datetime_ui_set_date_format(UiDateFormat format);
void datetime_ui_refresh_language();
