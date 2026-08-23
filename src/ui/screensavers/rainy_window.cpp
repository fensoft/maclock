#ifdef MACLOCK_COMBINED_SOURCE
static bool update_rainy_window(ClockView &view, const ClockRenderSnapshot &snapshot) { for (uint8_t i = 0; i < 48; ++i) { view.screensaver_y[i] += 2 + (i % 5); if (view.screensaver_y[i] >= kExtHeight) { view.screensaver_y[i] = -random(1, 80); view.screensaver_x[i] = random(0, kExtWidth); } line(view, view.screensaver_x[i], view.screensaver_y[i], view.screensaver_x[i] - 2, view.screensaver_y[i] + 7); } draw_time(view, snapshot.current, 67, 91, 5); return true; }
#endif
