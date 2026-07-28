#pragma once

#include <Arduino.h>
#include <NetworkClient.h>

#define HTTP_CODE_OK 200

class HTTPClient
{
public:
    HTTPClient();
    ~HTTPClient();

    void setConnectTimeout(int timeout_ms);
    void setTimeout(int timeout_ms);
    void useHTTP10(bool enabled);
    bool begin(NetworkClient &client, const String &url);
    int GET();
    String getString() const;
    void end();
    static String errorToString(int error);

private:
    String url_;
    String response_;
    int connect_timeout_ms_ = 5000;
    int timeout_ms_ = 10000;
};
