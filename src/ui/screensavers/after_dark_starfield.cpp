#ifdef MACLOCK_COMBINED_SOURCE

static bool update_after_dark_starfield(
    ClockView &view, ScreensaverMode mode)
{
    if (mode != ScreensaverMode::AfterDark &&
        mode != ScreensaverMode::Starfield)
        return false;
    for (size_t i = 0; i < kScreensaverStarCount; ++i)
    {
        int16_t y = view.screensaver_star_y[i] +
            view.screensaver_star_speed[i];
        if (y >= kScreensaverHeight)
        {
            y = 0;
            view.screensaver_star_x[i] = static_cast<int16_t>(
                (view.screensaver_star_x[i] + 73 + i * 11) %
                kScreensaverWidth);
        }
        view.screensaver_star_y[i] = y;
        lv_obj_set_pos(view.screensaver_stars[i],
                       view.screensaver_star_x[i], y);
    }
    if (mode == ScreensaverMode::Starfield)
        return true;

    int16_t x = view.screensaver_clock_x + view.screensaver_clock_dx;
    int16_t y = view.screensaver_clock_y + view.screensaver_clock_dy;
    if (x <= 0 || x >= 160)
    {
        view.screensaver_clock_dx = -view.screensaver_clock_dx;
        x = constrain(x, 0, 160);
    }
    if (y <= 0 || y >= 158)
    {
        view.screensaver_clock_dy = -view.screensaver_clock_dy;
        y = constrain(y, 0, 158);
    }
    view.screensaver_clock_x = x;
    view.screensaver_clock_y = y;
    lv_obj_set_pos(view.screensaver_clock, x, y);
    return true;
}

#endif
