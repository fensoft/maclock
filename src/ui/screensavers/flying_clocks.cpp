#ifdef MACLOCK_COMBINED_SOURCE

static bool update_flying_clocks(ClockView &view)
{
    if (view.screensaver_active_mode != ScreensaverMode::FlyingClocks)
        return false;
    const int16_t max_x = kScreensaverWidth - 82, max_y = kScreensaverHeight - 28;
    for (size_t i = 0; i < kScreensaverFlyingClockCount; ++i)
    {
        int16_t x = view.screensaver_flying_x[i] + view.screensaver_flying_dx[i];
        int16_t y = view.screensaver_flying_y[i] + view.screensaver_flying_dy[i];
        if (x <= 0 || x >= max_x) { view.screensaver_flying_dx[i] = -view.screensaver_flying_dx[i]; x = constrain(x, 0, max_x); }
        if (y <= 0 || y >= max_y) { view.screensaver_flying_dy[i] = -view.screensaver_flying_dy[i]; y = constrain(y, 0, max_y); }
        view.screensaver_flying_x[i] = x; view.screensaver_flying_y[i] = y;
        lv_obj_set_pos(view.screensaver_flying_clocks[i], x, y);
    }
    return true;
}

#endif
