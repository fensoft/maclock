#pragma once

class MDNSResponder
{
public:
    bool begin(const char *host);
    void addService(
        const char *service, const char *protocol, uint16_t port);
    void end();
};

extern MDNSResponder MDNS;
