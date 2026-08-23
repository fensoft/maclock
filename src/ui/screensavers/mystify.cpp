#ifdef MACLOCK_COMBINED_SOURCE
static bool update_mystify(ClockView &view) { for (uint8_t i = 0; i < 4; ++i) { view.screensaver_x[i] += view.screensaver_dx[i] * 2; view.screensaver_y[i] += view.screensaver_dy[i] * 2; if (view.screensaver_x[i] <= 2 || view.screensaver_x[i] >= 301) view.screensaver_dx[i] = -view.screensaver_dx[i]; if (view.screensaver_y[i] <= 2 || view.screensaver_y[i] >= 221) view.screensaver_dy[i] = -view.screensaver_dy[i]; line(view, view.screensaver_x[i], view.screensaver_y[i], view.screensaver_x[(i + 1) & 3], view.screensaver_y[(i + 1) & 3]); } return true; }
#endif
