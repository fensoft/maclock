#pragma once

#include <stddef.h>
#include <stdint.h>

#include <lvgl.h>

void timer_update(uint32_t now_ms);
bool timer_take_finished();
bool timer_is_active();
uint32_t timer_remaining_seconds(uint32_t now_ms);
void timer_format_remaining(uint32_t now_ms, char *text, size_t text_size);
void timer_cancel();
const char *timer_sound_path();
uint8_t timer_volume();

void timer_ui_init(lv_obj_t *screen);
void timer_ui_hide();
void timer_ui_enter(uint32_t now_ms);
void timer_ui_show(uint32_t now_ms);
void timer_ui_show_finished();
