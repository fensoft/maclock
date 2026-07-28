#pragma once

#include <Arduino.h>

#include <functional>
#include <memory>

enum HTTPMethod
{
    HTTP_ANY,
    HTTP_GET,
    HTTP_POST
};

class WebServer
{
public:
    explicit WebServer(int port = 80);
    ~WebServer();

    void on(
        const char *uri, HTTPMethod method,
        std::function<void()> handler);
    void onNotFound(std::function<void()> handler);
    void begin();
    void stop();
    void handleClient();

    bool hasArg(const char *name) const;
    String arg(const char *name) const;
    void sendHeader(
        const String &name, const String &value,
        bool first = false);
    void send(
        int status, const char *content_type,
        const String &content);
    void send(
        int status, const char *content_type,
        const char *content);
    void send_P(
        int status, const char *content_type,
        PGM_P content, size_t length);

private:
    struct State;
    std::unique_ptr<State> state_;
};
