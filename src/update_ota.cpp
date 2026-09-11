#ifdef MACLOCK_UPDATE_INSTALL_FRAGMENT
bool install_firmware(
    UpdateService::State &state,
    JsonObjectConst firmware, String &error)
{
    const size_t expected_size = firmware["size"] | 0U;
    if (!validate_ota_partitions(expected_size, error))
        return false;
    String download_url;
    if (!resolve_download_url(
            state.firmware_url, download_url, error))
        return false;
    GithubNetworkClientSecure client;
    client.useSystemCertificateBundle();
    client.setHandshakeTimeout(30);
    HTTPClient http;
    if (!begin_http(http, client, download_url, false))
    {
        error = "Could not open the firmware download";
        return false;
    }
    const int response = http.GET();
    if (response != HTTP_CODE_OK)
    {
        if (response < 0)
        {
            set_http_connection_error(
                error, "Firmware download connection failed",
                response, client);
        }
        else
        {
            error = String("Firmware download returned HTTP ") +
                    String(response);
        }
        http.end();
        return false;
    }
    if (http.getSize() != static_cast<int>(expected_size))
    {
        error = "The firmware download size is invalid";
        http.end();
        return false;
    }
    NetworkClient *stream = http.getStreamPtr();
    if (!stream)
    {
        error = "The firmware download stream is unavailable";
        http.end();
        return false;
    }

    SHA256Builder hash;
    hash.begin();
    uint8_t buffer[4096];
    size_t received = 0;
    uint32_t idle_since = millis();
    while (received < kFirmwarePrefixSize)
    {
        const int count = stream->read(
            buffer + received, kFirmwarePrefixSize - received);
        if (count > 0)
        {
            received += static_cast<size_t>(count);
            idle_since = millis();
            continue;
        }
        if (millis() - idle_since > kNetworkTimeoutMs)
        {
            http.end();
            error = "The firmware header download timed out";
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!validate_firmware_prefix(buffer, received, error) ||
        !Update.begin(expected_size, U_FLASH))
    {
        if (!error.length())
            error = "The inactive firmware partition is unavailable";
        http.end();
        return false;
    }
    hash.add(buffer, received);
    if (Update.write(buffer, received) != received)
    {
        Update.abort();
        http.end();
        error = String("Firmware write failed: ") +
                Update.errorString();
        return false;
    }
    while (received < expected_size)
    {
        const size_t wanted =
            min(expected_size - received, sizeof(buffer));
        const int count = stream->read(buffer, wanted);
        if (count <= 0)
        {
            if (millis() - idle_since <= kNetworkTimeoutMs)
            {
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }
            Update.abort();
            http.end();
            error = "The firmware download timed out";
            return false;
        }
        idle_since = millis();
        hash.add(buffer, static_cast<size_t>(count));
        if (Update.write(buffer, static_cast<size_t>(count)) !=
            static_cast<size_t>(count))
        {
            Update.abort();
            http.end();
            error = String("Firmware write failed: ") +
                    Update.errorString();
            return false;
        }
        received += static_cast<size_t>(count);
        set_progress(
            state, UpdateStage::DownloadingFirmware,
            static_cast<uint8_t>(
                min<size_t>(
                    99, received * 100 / expected_size)),
            "Installing firmware");
    }
    http.end();
    hash.calculate();
    if (!equals_digest(
            hash.toString(), firmware["sha256"] | ""))
    {
        Update.abort();
        error = "The firmware SHA-256 does not match";
        return false;
    }
    if (!Update.end(false))
    {
        error = String("Firmware validation failed: ") +
                Update.errorString();
        return false;
    }
    return true;
}

bool perform_install(UpdateService::State &state)
{
    set_stage(
        state, UpdateStage::DownloadingAssets,
        "Reading the release manifest");
    String payload;
    String error;
    if (!fetch_text(state.manifest_url, payload, error))
    {
        set_error(state, error);
        return false;
    }
    JsonDocument manifest;
    if (deserializeJson(manifest, payload))
    {
        set_error(state, "The update manifest is invalid");
        return false;
    }
    char target_version[32] = {};
    portENTER_CRITICAL(&state.mux);
    strlcpy(
        target_version, state.snapshot.latest_version,
        sizeof(target_version));
    portEXIT_CRITICAL(&state.mux);
    if (!validate_manifest(manifest, target_version, error))
    {
        set_error(state, error);
        return false;
    }
    if (state.preferences)
    {
        state.preferences->putBool("assetWork", true);
        state.preferences->putString(
            "assetTarget", target_version);
    }
    if (!install_assets(
            state, manifest["assets"].as<JsonObjectConst>(),
            state.assets_url, true, error))
    {
        set_error(state, error);
        return false;
    }
    if (state.preferences)
    {
        state.preferences->putBool("assetWork", false);
        state.preferences->remove("assetTarget");
    }
    set_progress(
        state, UpdateStage::DownloadingFirmware, 0,
        "Downloading firmware");
    if (!install_firmware(
            state, manifest["firmware"].as<JsonObjectConst>(),
            error))
    {
        set_error(state, error);
        return false;
    }

    if (state.preferences)
    {
        state.preferences->putString("assetVer", target_version);
        state.preferences->putBool("otaPending", true);
    }
    portENTER_CRITICAL(&state.mux);
    copy_text(
        state.snapshot.asset_version,
        sizeof(state.snapshot.asset_version), target_version);
    state.snapshot.stage = UpdateStage::ReadyToReboot;
    state.snapshot.busy = false;
    state.snapshot.progress = 100;
    state.snapshot.reboot_required = true;
    copy_text(
        state.snapshot.message,
        sizeof(state.snapshot.message),
        "Update installed; reboot to finish");
    portEXIT_CRITICAL(&state.mux);
    return true;
}

bool perform_asset_refresh(UpdateService::State &state)
{
    portENTER_CRITICAL(&state.mux);
    state.snapshot.assets_only = true;
    portEXIT_CRITICAL(&state.mux);

    if (!perform_check(state))
        return false;

    String manifest_url;
    String assets_url;
    char target_version[32] = {};
    portENTER_CRITICAL(&state.mux);
    manifest_url = state.manifest_url;
    assets_url = state.assets_url;
    strlcpy(target_version, state.snapshot.latest_version,
        sizeof(target_version));
    portEXIT_CRITICAL(&state.mux);
    if (!manifest_url.length() || !assets_url.length() || !target_version[0])
    {
        set_error(state, "The latest release has no refreshable assets");
        return false;
    }

    set_stage(state, UpdateStage::DownloadingAssets,
        "Reading the latest asset manifest");
    String payload;
    String error;
    if (!fetch_text(manifest_url, payload, error))
    {
        set_error(state, error);
        return false;
    }
    JsonDocument manifest;
    if (deserializeJson(manifest, payload))
    {
        set_error(state, "The update manifest is invalid");
        return false;
    }
    if (!validate_manifest(manifest, target_version, error))
    {
        set_error(state, error);
        return false;
    }
    if (!install_assets(state,
            manifest["assets"].as<JsonObjectConst>(),
            assets_url, false, error))
    {
        set_error(state, error);
        return false;
    }

    if (state.preferences)
        state.preferences->putString("assetVer", target_version);
    portENTER_CRITICAL(&state.mux);
    copy_text(state.snapshot.asset_version,
        sizeof(state.snapshot.asset_version), target_version);
    state.snapshot.stage = UpdateStage::ReadyToReboot;
    state.snapshot.busy = false;
    state.snapshot.progress = 100;
    state.snapshot.reboot_required = true;
    copy_text(state.snapshot.message,
        sizeof(state.snapshot.message),
        "Assets refreshed; reboot to load them");
    portEXIT_CRITICAL(&state.mux);
    return true;
}
#endif
