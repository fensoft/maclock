#ifdef MACLOCK_CONFIGURATION_ARCHIVE_COMBINED_SOURCE
struct ConfigurationArchive::State
{
    ControlPanelEventSink *events = nullptr;
    RestoreParser restore;
    bool upload_started = false;
    bool upload_finished = false;
    bool transfer_active = false;
    String upload_error;
};

void ConfigurationArchive::begin(
    ControlPanelEventSink &events)
{
    if (!state_)
        state_ = new State();
    state_->events = &events;
}

void ConfigurationArchive::sendExport(WebServer &server)
{
    if (!state_ || !state_->events)
    {
        std::vector<String> warnings;
        send_result(
            server, false,
            "Control service is unavailable",
            warnings, false, 503);
        return;
    }

    state_->events->beginControlPanelNetworkTransfer();
    std::vector<ExportEntry> entries;
    String error;
    if (!build_export_entries(
            *state_->events, entries, error))
    {
        state_->events->endControlPanelNetworkTransfer();
        std::vector<String> warnings;
        send_result(
            server, false, error.c_str(),
            warnings, false, 500);
        return;
    }

    char timestamp[24] = {};
    const time_t now = time(nullptr);
    struct tm local_time = {};
    if (localtime_r(&now, &local_time))
    {
        strftime(
            timestamp, sizeof(timestamp),
            "%Y-%m-%d_%H-%M-%S", &local_time);
    }
    const String filename =
        String("attachment; filename=\"maclock-backup-") +
        (timestamp[0] ? timestamp : "unknown-time") +
        ".zip\"";

    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader(
        "Content-Disposition", filename);
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/zip", "");
    stream_zip(server, entries);
    server.sendContent("");
    state_->events->endControlPanelNetworkTransfer();
}

void ConfigurationArchive::receiveUpload(
    WebServer &server)
{
    if (!state_ || !state_->events)
        return;
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        state_->upload_error.clear();
        state_->upload_finished = false;
        state_->events->beginControlPanelNetworkTransfer();
        state_->transfer_active = true;
        state_->upload_started = state_->restore.begin();
        if (!state_->upload_started)
            state_->upload_error =
                state_->restore.error();
        return;
    }
    if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (state_->upload_started &&
            !state_->restore.feed(
                upload.buf, upload.currentSize))
        {
            state_->upload_error =
                state_->restore.error();
            state_->upload_started = false;
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED)
    {
        state_->restore.abort();
        state_->upload_started = false;
        state_->upload_finished = true;
        state_->upload_error =
            "Backup upload was cancelled";
        return;
    }
    if (upload.status == UPLOAD_FILE_END)
    {
        if (state_->upload_started &&
            !state_->restore.finish())
        {
            state_->upload_error =
                state_->restore.error();
        }
        state_->upload_started = false;
        state_->upload_finished = true;
    }
}

void ConfigurationArchive::finishUpload(
    WebServer &server)
{
    std::vector<String> warnings;
    bool network_changed = false;
    if (!state_ || !state_->events)
    {
        send_result(
            server, false,
            "Control service is unavailable",
            warnings, false, 503);
        return;
    }
    if (!state_->upload_finished ||
        state_->upload_error.length())
    {
        const String message =
            state_->upload_error.length()
                ? state_->upload_error
                : String("No backup was uploaded");
        state_->restore.abort();
        if (state_->transfer_active)
        {
            state_->events->endControlPanelNetworkTransfer();
            state_->transfer_active = false;
        }
        send_result(
            server, false, message.c_str(),
            warnings, false, 400);
        return;
    }

    ControlPanelConfiguration configuration;
    String error;
    if (!deserialize_configuration(
            state_->restore.configuration(),
            configuration, error) ||
        !replace_restored_files())
    {
        if (!error.length())
            error = "Could not install restored files";
        state_->restore.abort();
        if (state_->transfer_active)
        {
            state_->events->endControlPanelNetworkTransfer();
            state_->transfer_active = false;
        }
        send_result(
            server, false, error.c_str(),
            warnings, false, 400);
        return;
    }

    resolve_restored_sounds(configuration, warnings);
    const String previous_ssid =
        state_->events->controlPanelConfiguration()
            .wifi.ssid;
    if (!state_->events->applyControlConfiguration(
            configuration, network_changed))
    {
        state_->restore.abort();
        if (state_->transfer_active)
        {
            state_->events->endControlPanelNetworkTransfer();
            state_->transfer_active = false;
        }
        send_result(
            server, false,
            "Configuration could not be restored",
            warnings, false, 500);
        return;
    }
    if (previous_ssid != configuration.wifi.ssid)
    {
        warnings.push_back(
            "The Wi-Fi password was not restored; "
            "Maclock kept its current password");
    }

    state_->restore.abort();
    if (state_->transfer_active)
    {
        state_->events->endControlPanelNetworkTransfer();
        state_->transfer_active = false;
    }
    send_result(
        server, true,
        network_changed
            ? "Backup restored; reconnecting to Wi-Fi"
            : "Backup restored",
        warnings, network_changed, 200);
}
#endif
