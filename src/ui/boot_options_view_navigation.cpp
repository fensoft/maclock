#ifdef MACLOCK_COMBINED_SOURCE
static const BootOptionsPage
    g_boot_options_section_first[BOOT_OPTIONS_SECTION_COUNT] = {
        BOOT_OPTIONS_LANGUAGE,
        BOOT_OPTIONS_CLOCK_FACE,
        BOOT_OPTIONS_CHIME,
        BOOT_OPTIONS_START};

static const BootOptionsPage
    g_boot_options_section_last[BOOT_OPTIONS_SECTION_COUNT] = {
        BOOT_OPTIONS_DATETIME,
        BOOT_OPTIONS_NIGHT_SCREEN,
        BOOT_OPTIONS_CHIME_QUIET,
        BOOT_OPTIONS_ABOUT};

static bool boot_options_page_position(
    BootOptionsPage page,
    BootOptionsSection *section,
    uint8_t *position,
    uint8_t *page_count)
{
    for (uint8_t i = 0; i < BOOT_OPTIONS_SECTION_COUNT; ++i)
    {
        const BootOptionsPage first =
            g_boot_options_section_first[i];
        const BootOptionsPage last =
            g_boot_options_section_last[i];
        if (page < first || page > last)
            continue;

        if (section)
            *section = static_cast<BootOptionsSection>(i);
        if (position)
            *position = static_cast<uint8_t>(page - first);
        if (page_count)
            *page_count =
                static_cast<uint8_t>(last - first + 1);
        return true;
    }
    return false;
}

static void update_boot_mode_button()
{
    if (!boot_options_view.boot_mode_button_label)
        return;
    lv_label_set_text(
        boot_options_view.boot_mode_button_label,
        (active_app && !active_app->touchscreenAvailable())
            ? tr("Boot: Clock")
            : g_boot_floppy_emulator
            ? tr("Boot: Emulator")
            : tr("Boot: Clock"));
}

static void boot_mode_toggle_event(lv_event_t *event)
{
    (void)event;
    g_boot_floppy_emulator = !g_boot_floppy_emulator;
    settings_store.saveBootMode(g_boot_floppy_emulator);
    update_boot_mode_button();
}

static void boot_start_clock_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_EMPTY_SCREEN);
}

static void boot_start_emulator_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_EMULATOR);
}

static void boot_diagnostics_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.page_on_show = BOOT_OPTIONS_TOOLS;
    request_state(UI_STATE_DIAGNOSTICS);
}

static void boot_wifi_setup_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.page_on_show = BOOT_OPTIONS_WIFI;
    request_state(UI_STATE_WIFI_SETUP);
}

static void boot_update_primary_event(lv_event_t *event)
{
    (void)event;
    const UpdateSnapshot update = update_service.snapshot();
    if (update.busy)
        return;

    if (update.stage == UpdateStage::ReadyToReboot)
        active_app->rebootAfterControlUpdate();
    else if (update.update_available)
        active_app->requestControlUpdateInstall();
    else
        active_app->requestControlUpdateCheck();
    boot_options_view.refreshUpdate();
}

static void boot_update_refresh_assets_event(lv_event_t *event)
{
    (void)event;
    const UpdateSnapshot update = update_service.snapshot();
    if (update.busy || update.reboot_required)
        return;
    active_app->requestAssetRefresh();
    boot_options_view.refreshUpdate();
}

static void boot_update_later_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.standalone_update_prompt = false;
    update_service.dismiss(false);
    request_state(UI_STATE_NORMAL);
}

static void boot_update_ignore_event(lv_event_t *event)
{
    (void)event;
    boot_options_view.standalone_update_prompt = false;
    update_service.dismiss(true);
    request_state(UI_STATE_NORMAL);
}

static void wifi_setup_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

static void boot_exit_event(lv_event_t *event)
{
    (void)event;
    if (boot_options_view.page == BOOT_OPTIONS_HOME)
        request_state(UI_STATE_NORMAL);
    else
        boot_options_view.setPage(BOOT_OPTIONS_HOME);
}

static void diagnostics_back_event(lv_event_t *event)
{
    (void)event;
    request_state(UI_STATE_BOOT_OPTIONS);
}

void BootOptionsView::tick(uint32_t now)
{
    if (boot_options_view.page == BOOT_OPTIONS_DATETIME &&
        (!boot_options_view.datetime_last_refresh_ms ||
         now - boot_options_view.datetime_last_refresh_ms >= 250))
    {
        boot_options_view.datetime_last_refresh_ms = now;
        boot_options_view.refreshDateTime();
    }
    else if (
        boot_options_view.page == BOOT_OPTIONS_UPDATE &&
        (!boot_options_view.update_last_refresh_ms ||
         now - boot_options_view.update_last_refresh_ms >= 250))
    {
        boot_options_view.update_last_refresh_ms = now;
        boot_options_view.refreshUpdate();
    }
}

void BootOptionsView::setPage(BootOptionsPage page)
{
    if (page >= BOOT_OPTIONS_PAGE_COUNT)
        return;
    if (page != BOOT_OPTIONS_UPDATE)
        boot_options_view.standalone_update_prompt = false;

    const char *page_names[BOOT_OPTIONS_PAGE_COUNT] = {
        tr("Configuration"), tr("Language"),
        tr("Regional"), tr("Date / Time"), tr("Clock Face"),
        tr("Face Settings"), tr("Screensaver"),
        tr("Night Schedule"), tr("Night Screen"), tr("Chime"),
        tr("Chime Sound"), tr("Chime Volume"), tr("Quiet Hours"),
        tr("Start"), tr("Preferences"), tr("Wi-Fi"),
        tr("Tools"), tr("Software Update"), tr("About")};
    const char *section_names[BOOT_OPTIONS_SECTION_COUNT] = {
        tr("General"), tr("Display"), tr("Sound"), tr("System")};
    boot_options_view.page = page;
    for (size_t i = 0; i < BOOT_OPTIONS_PAGE_COUNT; ++i)
        lv_obj_add_flag(
            boot_options_view.pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(
        boot_options_view.pages[page], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(
        boot_options_view.pages[BOOT_OPTIONS_UPDATE], 130);
    lv_obj_clear_flag(
        boot_options_view.exit, LV_OBJ_FLAG_HIDDEN);
    if (page == BOOT_OPTIONS_DATETIME)
    {
        boot_options_view.datetime_last_refresh_ms = 0;
        boot_options_view.refreshDateTime();
    }
    else if (page == BOOT_OPTIONS_UPDATE)
    {
        boot_options_view.update_last_refresh_ms = 0;
        boot_options_view.refreshUpdate();
        if (boot_options_view.standalone_update_prompt)
        {
            lv_obj_set_height(
                boot_options_view.pages[BOOT_OPTIONS_UPDATE],
                168);
            lv_label_set_text(
                boot_options_view.title,
                tr("Software Update"));
            lv_obj_add_flag(
                boot_options_view.previous,
                LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(
                boot_options_view.exit,
                LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(
                boot_options_view.next,
                LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }
    else if (page == BOOT_OPTIONS_CHIME_SOUND)
    {
        boot_options_view.chime_sound_selector.reload(
            g_chime_sound_path);
    }

    if (page == BOOT_OPTIONS_HOME)
    {
        lv_label_set_text(
            boot_options_view.title, tr("Configuration"));
        lv_obj_add_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(
            boot_options_view.exit_label, tr("Exit"));
        return;
    }

    BootOptionsSection section = BOOT_OPTIONS_SECTION_GENERAL;
    uint8_t position = 0;
    uint8_t page_count = 0;
    if (!boot_options_page_position(
            page, &section, &position, &page_count))
    {
        return;
    }

    char title[72];
    snprintf(
        title, sizeof(title), "%s - %s (%u/%u)",
        section_names[section], page_names[page],
        (unsigned)position + 1, (unsigned)page_count);
    lv_label_set_text(boot_options_view.title, title);
    lv_label_set_text(
        boot_options_view.exit_label, tr("Sections"));

    if (position == 0)
        lv_obj_add_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            boot_options_view.previous, LV_OBJ_FLAG_HIDDEN);

    if (position + 1 >= page_count)
        lv_obj_add_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(
            boot_options_view.next, LV_OBJ_FLAG_HIDDEN);
}

static void boot_options_previous_event(lv_event_t *event)
{
    (void)event;
    uint8_t position = 0;
    if (boot_options_page_position(
            boot_options_view.page, nullptr, &position, nullptr) &&
        position > 0)
        boot_options_view.setPage(static_cast<BootOptionsPage>(
            boot_options_view.page - 1));
}

static void boot_options_next_event(lv_event_t *event)
{
    (void)event;
    uint8_t position = 0;
    uint8_t page_count = 0;
    if (boot_options_page_position(
            boot_options_view.page, nullptr,
            &position, &page_count) &&
        position + 1 < page_count)
        boot_options_view.setPage(static_cast<BootOptionsPage>(
            boot_options_view.page + 1));
}

static void boot_options_section_event(lv_event_t *event)
{
    lv_obj_t *button =
        (lv_obj_t *)lv_event_get_target(event);
    const uint32_t section =
        (uint32_t)(uintptr_t)lv_obj_get_user_data(button);
    if (section >= BOOT_OPTIONS_SECTION_COUNT)
        return;
    boot_options_view.setPage(
        g_boot_options_section_first[section]);
}
#endif
