#ifdef MACLOCK_COMBINED_SOURCE

static bool update_matrix_rain(ClockView &view)
{
    if (view.screensaver_active_mode != ScreensaverMode::MatrixRain)
        return false;
    for (size_t i = 0; i < kScreensaverMatrixColumnCount; ++i)
    {
        int16_t y = view.screensaver_matrix_y[i] +
            view.screensaver_matrix_speed[i] * 2;
        if (y >= kScreensaverHeight)
        {
            y = static_cast<int16_t>(-random(60, 190));
            fill_matrix_column(view.screensaver_matrix_text[i],
                               sizeof(view.screensaver_matrix_text[i]));
            lv_label_set_text(view.screensaver_matrix_columns[i],
                              view.screensaver_matrix_text[i]);
        }
        view.screensaver_matrix_y[i] = y;
        lv_obj_set_y(view.screensaver_matrix_columns[i], y);
    }
    return true;
}

#endif
