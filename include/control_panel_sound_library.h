#pragma once

#include <ArduinoJson.h>
#include <FS.h>
#include <WebServer.h>

#include "control_panel.h"

class ControlPanelSoundLibrary
{
public:
    void appendSnapshot(
        JsonArray sounds,
        const ControlPanelSnapshot &snapshot) const;
    void receiveUpload(WebServer &server);
    void finishUpload(WebServer &server);
    void importFromUrl(WebServer &server);
    void remove(WebServer &server, ControlPanelEventSink &events);

private:
    void resetUpload();

    fs::File upload_file_;
    String upload_path_;
    String upload_error_;
    size_t upload_size_ = 0;
    bool upload_finished_ = false;
};
