#pragma once

#include <WebServer.h>

#include "control_panel.h"

class ConfigurationArchive
{
public:
    struct State;

    void begin(ControlPanelEventSink &events);
    void sendExport(WebServer &server);
    void receiveUpload(WebServer &server);
    void finishUpload(WebServer &server);

private:
    State *state_ = nullptr;
};
