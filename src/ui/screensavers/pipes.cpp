#ifdef MACLOCK_COMBINED_SOURCE

static bool update_pipes(ClockView &view)
{
    if (view.screensaver_active_mode != ScreensaverMode::Pipes)
        return false;
    static constexpr lv_color_t kPipeColors[] = {
        LV_COLOR_MAKE(0x24, 0xD9, 0x65), LV_COLOR_MAKE(0x21, 0xC7, 0xD9),
        LV_COLOR_MAKE(0xF0, 0xD2, 0x32), LV_COLOR_MAKE(0xD6, 0x43, 0xC7)};
    lv_obj_t *segment = view.screensaver_pipe_segments[view.screensaver_pipe_index];
    const bool horizontal = (view.screensaver_pipe_direction & 1) == 0;
    lv_obj_set_size(segment, horizontal ? 16 : 4, horizontal ? 4 : 16);
    lv_obj_set_pos(segment, view.screensaver_pipe_x, view.screensaver_pipe_y);
    lv_obj_set_style_bg_color(segment, kPipeColors[view.screensaver_pipe_color % 4], 0);
    lv_obj_clear_flag(segment, LV_OBJ_FLAG_HIDDEN);
    view.screensaver_pipe_index = static_cast<uint8_t>((view.screensaver_pipe_index + 1) % kScreensaverPipeSegmentCount);
    int16_t next_x = view.screensaver_pipe_x, next_y = view.screensaver_pipe_y;
    switch (view.screensaver_pipe_direction) { case 0: next_x += 12; break; case 1: next_y += 12; break; case 2: next_x -= 12; break; default: next_y -= 12; break; }
    const bool boundary = next_x < 4 || next_x > kScreensaverWidth - 18 || next_y < 4 || next_y > kScreensaverHeight - 18;
    if (boundary || random(0, 6) == 0)
    {
        view.screensaver_pipe_direction = static_cast<int8_t>((view.screensaver_pipe_direction + (random(0, 2) ? 1 : 3)) % 4);
        ++view.screensaver_pipe_color;
    }
    else { view.screensaver_pipe_x = next_x; view.screensaver_pipe_y = next_y; }
    return true;
}

#endif
