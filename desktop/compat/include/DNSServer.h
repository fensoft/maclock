#pragma once

#include <WiFi.h>

class DNSServer
{
public:
    bool start(
        uint16_t port, const char *domain, IPAddress address);
    void processNextRequest();
    void stop();
};
