#ifdef MACLOCK_COMBINED_SOURCE
static bool update_digital_rain_clock(ClockView &view, const ClockRenderSnapshot &snapshot) { for (uint8_t i = 0; i < 38; ++i) { view.screensaver_y[i] += 2 + (i % 4); if (view.screensaver_y[i] > kExtHeight + 20) view.screensaver_y[i] = -random(10, 100); for (uint8_t tail = 0; tail < 5; ++tail) put_pixel(view, i * 8 + 2, view.screensaver_y[i] - tail * 4); } draw_time(view, snapshot.current, 43, 83, 6); return true; }
#endif
