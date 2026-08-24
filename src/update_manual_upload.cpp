#ifdef MACLOCK_UPDATE_COMBINED_SOURCE
bool UpdateService::beginManualFirmware(const char *filename)
{
    const size_t filename_length =
        filename ? strlen(filename) : 0;
    if (!state_ || filename_length < 4 ||
        strcasecmp(
            filename + filename_length - 4, ".bin") != 0)
    {
        return false;
    }
#ifdef MACLOCK_LOCAL
    set_error(
        *state_,
        "Firmware upload is unavailable in the simulator");
    return false;
#else
    portENTER_CRITICAL(&state_->mux);
    if (state_->snapshot.busy || state_->manual_upload)
    {
        portEXIT_CRITICAL(&state_->mux);
        return false;
    }
    state_->manual_upload = true;
    state_->manual_written = 0;
    state_->manual_update_started = false;
    state_->manual_prefix_length = 0;
    portEXIT_CRITICAL(&state_->mux);
    String partition_error;
    if (!validate_ota_partitions(1, partition_error))
    {
        state_->manual_upload = false;
        set_error(*state_, partition_error);
        return false;
    }
    set_stage(
        *state_, UpdateStage::UploadingFirmware,
        "Uploading firmware");
    return true;
#endif
}

bool UpdateService::writeManualFirmware(
    const uint8_t *data, size_t length)
{
#ifdef MACLOCK_LOCAL
    (void)data;
    (void)length;
    return false;
#else
    if (!state_ || !state_->manual_upload ||
        !data || !length)
    {
        return false;
    }
    if (!state_->manual_update_started)
    {
        const size_t required =
            kFirmwarePrefixSize - state_->manual_prefix_length;
        const size_t copied = length < required
                                  ? length
                                  : required;
        memcpy(
            state_->manual_prefix +
                state_->manual_prefix_length,
            data, copied);
        state_->manual_prefix_length += copied;
        data += copied;
        length -= copied;
        if (state_->manual_prefix_length <
            kFirmwarePrefixSize)
        {
            return true;
        }

        String error;
        if (!validate_firmware_prefix(
                state_->manual_prefix,
                state_->manual_prefix_length, error) ||
            !Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH))
        {
            state_->manual_upload = false;
            set_error(
                *state_,
                error.length() ? error : Update.errorString());
            return false;
        }
        state_->manual_update_started = true;
        const size_t prefix_written = Update.write(
            state_->manual_prefix,
            state_->manual_prefix_length);
        state_->manual_written = prefix_written;
        if (prefix_written != state_->manual_prefix_length)
            return false;
    }
    if (!length)
        return true;
    const size_t written =
        Update.write(const_cast<uint8_t *>(data), length);
    state_->manual_written += written;
    return written == length &&
           state_->manual_written <= kAppSlotSize;
#endif
}

bool UpdateService::finishManualFirmware()
{
#ifdef MACLOCK_LOCAL
    return false;
#else
    if (!state_ || !state_->manual_upload ||
        !state_->manual_update_started ||
        !state_->manual_written)
    {
        return false;
    }
    const bool finished = Update.end(true);
    state_->manual_upload = false;
    if (!finished)
    {
        set_error(*state_, Update.errorString());
        return false;
    }
    portENTER_CRITICAL(&state_->mux);
    state_->snapshot.stage = UpdateStage::ReadyToReboot;
    state_->snapshot.busy = false;
    state_->snapshot.progress = 100;
    state_->snapshot.reboot_required = true;
    copy_text(
        state_->snapshot.message,
        sizeof(state_->snapshot.message),
        "Firmware uploaded; reboot to finish");
    portEXIT_CRITICAL(&state_->mux);
    return true;
#endif
}

void UpdateService::abortManualFirmware()
{
#ifndef MACLOCK_LOCAL
    if (!state_ || !state_->manual_upload)
        return;
    if (state_->manual_update_started)
        Update.abort();
    state_->manual_upload = false;
    set_error(*state_, "Firmware upload was cancelled");
#endif
}

bool UpdateService::reboot()
{
    if (!state_)
        return false;
#ifdef MACLOCK_LOCAL
    return false;
#else
    if (!snapshot().reboot_required)
        return false;
    delay(100);
    ESP.restart();
    return true;
#endif
}
#endif
