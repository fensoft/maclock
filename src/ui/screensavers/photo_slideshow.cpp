#ifdef MACLOCK_COMBINED_SOURCE
static void reset_photo_slideshow(ClockView &view, ScreensaverMode mode) { if (mode == ScreensaverMode::PhotoSlideshow) view.screensaver_photo_loaded = load_next_photo(view); }
static bool update_photo_slideshow(ClockView &view, uint32_t frame) { if (millis() >= view.screensaver_photo_due_ms) { view.screensaver_photo_loaded = load_next_photo(view); view.screensaver_photo_transition = 0; view.screensaver_photo_due_ms = millis() + 10000; } if (view.screensaver_photo_transition < 16 && !(frame & 1)) ++view.screensaver_photo_transition; if (!view.screensaver_photo_loaded) lv_obj_clear_flag(view.screensaver_canvas, LV_OBJ_FLAG_HIDDEN); draw_photo(view); return true; }
#endif
