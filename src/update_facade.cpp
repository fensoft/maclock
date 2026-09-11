#ifdef MACLOCK_UPDATE_COMBINED_SOURCE
void UpdateService::begin(Preferences &preferences)
{
#ifndef MACLOCK_LOCAL
    static bool tls_allocator_configured = false;
    if (!tls_allocator_configured)
    {
        tls_allocator_configured =
            mbedtls_platform_set_calloc_free(
                tls_psram_calloc, tls_psram_free) == 0;
    }
#endif
    if (!state_)
        state_ = new State();
    state_->preferences = &preferences;
    copy_text(
        state_->snapshot.current_version,
        sizeof(state_->snapshot.current_version),
        MACLOCK_VERSION);
    const String asset_version =
        preferences.getString("assetVer", "");
    copy_text(
        state_->snapshot.asset_version,
        sizeof(state_->snapshot.asset_version),
        asset_version);
    state_->ignored_version =
        preferences.getString("otaIgnored", "");
    state_->release_etag =
        preferences.getString("otaEtag", "");
    state_->install_requested =
        preferences.getBool("assetWork", false);
    state_->asset_refresh_requested =
        !LittleFS.exists(kAssetProbePath);
    if (state_->install_requested)
    {
        state_->snapshot.stage = UpdateStage::InstallingAssets;
        copy_text(
            state_->snapshot.message,
            sizeof(state_->snapshot.message),
            "Interrupted update will resume after Wi-Fi connects");
    }
#ifdef MACLOCK_LOCAL
    state_->snapshot.supported = false;
    state_->snapshot.stage = UpdateStage::Idle;
#else
    const esp_partition_t *running =
        esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
    if (running &&
        esp_ota_get_state_partition(running, &ota_state) ==
            ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        state_->pending_verify = true;
        state_->validation_started_ms = millis();
    }
#endif
}

void UpdateService::tick(
    const WifiModeSnapshot &wifi,
    bool allow_device_prompt,
    bool allow_network_check)
{
    if (!state_)
        return;
#ifndef MACLOCK_LOCAL
    if (state_->pending_verify &&
        millis() - state_->validation_started_ms >=
            kFirstBootValidationMs)
    {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
        {
            state_->pending_verify = false;
            if (state_->preferences)
                state_->preferences->putBool(
                    "otaPending", false);
        }
    }
#endif

    const uint32_t now = millis();
    if (allow_network_check && needsNetworkCheck(wifi))
    {
        if (start_worker(*state_, WorkerAction::Check))
        {
            state_->check_requested = false;
            state_->first_check_complete = true;
            state_->last_check_ms = now;
        }
    }
    if (state_->install_requested &&
        !state_->snapshot.busy &&
        state_->snapshot.update_available &&
        state_->manifest_url.length() &&
        state_->firmware_url.length() &&
        state_->assets_url.length())
    {
        if (start_worker(*state_, WorkerAction::Install))
            state_->install_requested = false;
    }

    portENTER_CRITICAL(&state_->mux);
    if (!allow_device_prompt ||
        static_cast<int32_t>(
            state_->prompt_snoozed_until_ms - now) > 0)
    {
        state_->snapshot.prompt_pending = false;
    }
    else if (state_->snapshot.update_available &&
             !String(state_->snapshot.latest_version)
                  .equalsIgnoreCase(state_->ignored_version))
    {
        state_->snapshot.prompt_pending = true;
    }
    portEXIT_CRITICAL(&state_->mux);
}

bool UpdateService::needsNetworkCheck(
    const WifiModeSnapshot &wifi) const
{
    if (!state_ || !wifi.enabled || !wifi.connected ||
        wifi.portal_active)
    {
        return false;
    }

    const uint32_t now = millis();
    portENTER_CRITICAL(&state_->mux);
    const bool needed =
        !state_->worker && !state_->snapshot.busy &&
        !state_->snapshot.reboot_required &&
        (state_->check_requested ||
         !state_->first_check_complete ||
         now - state_->last_check_ms >= kCheckIntervalMs);
    portEXIT_CRITICAL(&state_->mux);
    return needed;
}

bool UpdateService::needsAssetRefresh(
    const WifiModeSnapshot &wifi) const
{
    if (!state_ || !wifi.enabled || !wifi.connected ||
        wifi.portal_active)
        return false;
    portENTER_CRITICAL(&state_->mux);
    const bool needed = state_->snapshot.supported &&
        state_->asset_refresh_requested &&
        !state_->worker && !state_->snapshot.busy &&
        !state_->snapshot.reboot_required;
    portEXIT_CRITICAL(&state_->mux);
    return needed;
}

bool UpdateService::networkOperationActive() const
{
    if (!state_)
        return false;

    portENTER_CRITICAL(&state_->mux);
    const bool active =
        state_->worker || state_->snapshot.busy;
    portEXIT_CRITICAL(&state_->mux);
    return active;
}

UpdateSnapshot UpdateService::snapshot() const
{
    UpdateSnapshot result;
    if (!state_)
        return result;
    portENTER_CRITICAL(&state_->mux);
    result = state_->snapshot;
    portEXIT_CRITICAL(&state_->mux);
    return result;
}

bool UpdateService::requestCheck()
{
    if (!state_)
        return false;
    portENTER_CRITICAL(&state_->mux);
    state_->snapshot.assets_only = false;
    state_->check_requested = true;
    portEXIT_CRITICAL(&state_->mux);
    return true;
}

bool UpdateService::requestInstall()
{
    if (!state_)
        return false;
    portENTER_CRITICAL(&state_->mux);
    state_->snapshot.assets_only = false;
    const bool available =
        state_->snapshot.update_available &&
        state_->manifest_url.length() &&
        state_->firmware_url.length() &&
        state_->assets_url.length();
    portEXIT_CRITICAL(&state_->mux);
    return available &&
           start_worker(*state_, WorkerAction::Install);
}

bool UpdateService::requestAssetRefresh()
{
    if (!state_)
        return false;
    portENTER_CRITICAL(&state_->mux);
    const bool available = state_->snapshot.supported &&
        !state_->worker && !state_->snapshot.busy &&
        !state_->snapshot.reboot_required;
    if (available)
        state_->snapshot.assets_only = true;
    portEXIT_CRITICAL(&state_->mux);
    if (!available)
        return false;
    if (!start_worker(*state_, WorkerAction::RefreshAssets))
    {
        portENTER_CRITICAL(&state_->mux);
        state_->snapshot.assets_only = false;
        portEXIT_CRITICAL(&state_->mux);
        return false;
    }
    portENTER_CRITICAL(&state_->mux);
    state_->asset_refresh_requested = false;
    portEXIT_CRITICAL(&state_->mux);
    return true;
}

void UpdateService::dismiss(bool ignore_version)
{
    if (!state_)
        return;
    portENTER_CRITICAL(&state_->mux);
    state_->snapshot.prompt_pending = false;
    if (ignore_version)
    {
        state_->ignored_version =
            state_->snapshot.latest_version;
        if (state_->preferences)
            state_->preferences->putString(
                "otaIgnored", state_->ignored_version);
    }
    else
    {
        state_->prompt_snoozed_until_ms =
            millis() + kLaterIntervalMs;
    }
    portEXIT_CRITICAL(&state_->mux);
}

bool UpdateService::consumePrompt()
{
    if (!state_)
        return false;
    portENTER_CRITICAL(&state_->mux);
    const bool pending = state_->snapshot.prompt_pending;
    state_->snapshot.prompt_pending = false;
    portEXIT_CRITICAL(&state_->mux);
    return pending;
}

#endif
