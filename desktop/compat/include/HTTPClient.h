#pragma once

#include <Arduino.h>
#include <NetworkClient.h>

#include <map>
#include <string>
#include <vector>

#define HTTP_CODE_OK 200
#define HTTP_CODE_NOT_MODIFIED 304

class HTTPClient
{
public:
    HTTPClient();
    ~HTTPClient();

    void setConnectTimeout(int timeout_ms);
    void setTimeout(int timeout_ms);
    void setUserAgent(const String &user_agent);
    void useHTTP10(bool enabled);
    void collectHeaders(
        const char *header_keys[], size_t count);
    void addHeader(
        const String &name, const String &value);
    String header(const String &name) const;
    bool begin(NetworkClient &client, const String &url);
    int GET();
    String getString() const;
    void end();
    static String errorToString(int error);

private:
    String url_;
    String response_;
    String user_agent_ = "Maclock-Simulator/1.0";
    int connect_timeout_ms_ = 5000;
    int timeout_ms_ = 10000;
    std::vector<std::pair<std::string, std::string>>
        request_headers_;
    std::map<std::string, std::string> response_headers_;
};
