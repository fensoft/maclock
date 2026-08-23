#ifdef MACLOCK_COMBINED_SOURCE
void ClockView::init(lv_obj_t *screen)
{
    clock_view.initScreensavers(screen);
}

void ClockView::show(const ClockRenderSnapshot &snapshot)
{
    ui_shell.hideAll();
    clock_view.screensaver_active = false;
    if (clock_view.custom_face)
        lv_obj_add_flag(clock_view.custom_face, LV_OBJ_FLAG_HIDDEN);
    clock_view.showCustomFace(snapshot);
}

void ClockView::update(const ClockRenderSnapshot &snapshot)
{
    clock_view.updateCustomFace(snapshot);
}
#endif
