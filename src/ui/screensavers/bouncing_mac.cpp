#ifdef MACLOCK_COMBINED_SOURCE

static bool update_bouncing_mac(ClockView &view)
{
    if (view.screensaver_active_mode != ScreensaverMode::BouncingMac)
        return false;
    int16_t x = view.screensaver_logo_x + view.screensaver_logo_dx;
    int16_t y = view.screensaver_logo_y + view.screensaver_logo_dy;
    const int16_t max_x = kScreensaverWidth - 58;
    const int16_t max_y = kScreensaverHeight - 70;
    if (x <= 0 || x >= max_x)
    {
        view.screensaver_logo_dx = -view.screensaver_logo_dx;
        x = constrain(x, 0, max_x);
    }
    if (y <= 0 || y >= max_y)
    {
        view.screensaver_logo_dy = -view.screensaver_logo_dy;
        y = constrain(y, 0, max_y);
    }
    view.screensaver_logo_x = x;
    view.screensaver_logo_y = y;
    lv_obj_set_pos(view.screensaver_logo, x, y);
    return true;
}

#endif
