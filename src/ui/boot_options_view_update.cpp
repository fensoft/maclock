#ifdef MACLOCK_COMBINED_SOURCE
void BootOptionsView::refreshUpdate()
{
    if (!boot_options_view.update_status)
        return;

    const UpdateSnapshot update = update_service.snapshot();
    const bool show_progress =
        update.stage == UpdateStage::DownloadingAssets ||
        update.stage == UpdateStage::InstallingAssets ||
        update.stage == UpdateStage::DownloadingFirmware ||
        update.stage == UpdateStage::UploadingFirmware;
    uint8_t overall_progress = update.progress;
    if (update.stage == UpdateStage::DownloadingAssets ||
        update.stage == UpdateStage::InstallingAssets)
    {
        overall_progress =
            static_cast<uint8_t>(update.progress / 2);
    }
    else if (update.stage == UpdateStage::DownloadingFirmware)
    {
        overall_progress = static_cast<uint8_t>(
            50 + update.progress / 2);
    }
    char status[256];
    char current_version[80];
    char latest_version[80];
    snprintf(
        current_version, sizeof(current_version),
        tr("Current version: %s"),
        update.current_version);
    snprintf(
        latest_version, sizeof(latest_version),
        tr("Latest version: %s"),
        update.latest_version[0]
            ? update.latest_version
            : "-");
    switch (update.stage)
    {
    case UpdateStage::Checking:
        snprintf(
            status, sizeof(status), "%s\n%s",
            tr("Checking for updates..."),
            current_version);
        break;
    case UpdateStage::UpToDate:
        snprintf(
            status, sizeof(status),
            "%s\n%s",
            tr("Maclock is up to date."),
            current_version);
        break;
    case UpdateStage::Available:
        snprintf(
            status, sizeof(status),
            "%s\n%s\n%s",
            current_version, latest_version,
            tr("Update is ready."));
        break;
    case UpdateStage::DownloadingAssets:
    case UpdateStage::InstallingAssets:
    case UpdateStage::DownloadingFirmware:
    case UpdateStage::UploadingFirmware:
        snprintf(
            status, sizeof(status),
            "%s", tr("Installing update..."));
        break;
    case UpdateStage::ReadyToReboot:
        snprintf(
            status, sizeof(status), "%s\n%s",
            tr("Update is ready."),
            latest_version);
        break;
    case UpdateStage::Error:
        snprintf(
            status, sizeof(status), "%s\n%s\n%s",
            tr("Update failed."), current_version,
            update.message);
        break;
    case UpdateStage::Unsupported:
        snprintf(
            status, sizeof(status), "%s",
            tr("Update unavailable."));
        break;
    case UpdateStage::Idle:
    default:
        snprintf(
            status, sizeof(status), "%s",
            current_version);
        break;
    }
    lv_label_set_text(
        boot_options_view.update_status, status);
    lv_bar_set_value(
        boot_options_view.update_progress,
        overall_progress, LV_ANIM_OFF);
    if (show_progress)
        lv_obj_clear_flag(
            boot_options_view.update_progress,
            LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(
            boot_options_view.update_progress,
            LV_OBJ_FLAG_HIDDEN);

    const bool busy = update.busy;
    const bool available =
        update.update_available &&
        update.stage != UpdateStage::ReadyToReboot;
    const bool ready =
        update.stage == UpdateStage::ReadyToReboot;
    lv_label_set_text(
        boot_options_view.update_primary_label,
        ready
            ? tr("Reboot")
            : (available ? tr("Update") : tr("Check Now")));
    lv_label_set_text(
        boot_options_view.update_later_label,
        tr("Later"));
    lv_label_set_text(
        boot_options_view.update_ignore_label,
        tr("Ignore"));

    if (busy)
        lv_obj_add_state(
            boot_options_view.update_primary,
            LV_STATE_DISABLED);
    else
        lv_obj_remove_state(
            boot_options_view.update_primary,
            LV_STATE_DISABLED);

    if (available)
    {
        lv_obj_align(
            boot_options_view.update_primary,
            LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_clear_flag(
            boot_options_view.update_later,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(
            boot_options_view.update_ignore,
            LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_align(
            boot_options_view.update_primary,
            LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_add_flag(
            boot_options_view.update_later,
            LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(
            boot_options_view.update_ignore,
            LV_OBJ_FLAG_HIDDEN);
    }
}
#endif
