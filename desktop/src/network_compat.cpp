#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

#include "maclock_hal.h"

#include <curl/curl.h>
#include <httplib.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

WiFiClass WiFi;
MDNSResponder MDNS;

IPAddress::IPAddress(
    uint8_t a, uint8_t b, uint8_t c, uint8_t d)
    : bytes_{a, b, c, d}
{
}

String IPAddress::toString() const
{
    return String(bytes_[0]) + "." +
           String(bytes_[1]) + "." +
           String(bytes_[2]) + "." +
           String(bytes_[3]);
}

wifi_mode_t WiFiClass::getMode() const
{
    return mode_;
}

bool WiFiClass::mode(wifi_mode_t mode)
{
    mode_ = mode;
    if (mode == WIFI_MODE_NULL)
        status_ = WL_DISCONNECTED;
    return true;
}

void WiFiClass::disconnect(bool wifi_off, bool)
{
    status_ = WL_DISCONNECTED;
    if (wifi_off)
        mode_ = WIFI_MODE_NULL;
}

void WiFiClass::setAutoReconnect(bool)
{
}

void WiFiClass::begin(
    const char *ssid, const char *)
{
    mode_ = WIFI_STA;
    ssid_ = ssid && *ssid
                ? ssid
                : maclock_hal().network().simulatedSsid();
    status_ = WL_CONNECTED;
}

wl_status_t WiFiClass::status() const
{
    return status_;
}

int16_t WiFiClass::scanNetworks(bool, bool)
{
    return 1;
}

void WiFiClass::scanDelete()
{
}

String WiFiClass::SSID(int index) const
{
    return index == 0
               ? String(maclock_hal().network().simulatedSsid())
               : String();
}

int32_t WiFiClass::RSSI(int index) const
{
    return index == 0 ? -42 : 0;
}

int32_t WiFiClass::RSSI() const
{
    return status_ == WL_CONNECTED ? -42 : 0;
}

int WiFiClass::encryptionType(int index) const
{
    return index == 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
}

IPAddress WiFiClass::localIP() const
{
    return IPAddress();
}

bool WiFiClass::softAP(const char *ssid)
{
    mode_ = WIFI_AP;
    ssid_ = ssid;
    status_ = WL_CONNECTED;
    return true;
}

IPAddress WiFiClass::softAPIP() const
{
    return IPAddress();
}

bool WiFiClass::softAPdisconnect(bool wifi_off)
{
    status_ = WL_DISCONNECTED;
    if (wifi_off)
        mode_ = WIFI_MODE_NULL;
    return true;
}

HTTPClient::HTTPClient()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HTTPClient::~HTTPClient()
{
    end();
}

void HTTPClient::setConnectTimeout(int timeout_ms)
{
    connect_timeout_ms_ = timeout_ms;
}

void HTTPClient::setTimeout(int timeout_ms)
{
    timeout_ms_ = timeout_ms;
}

void HTTPClient::useHTTP10(bool)
{
}

bool HTTPClient::begin(NetworkClient &, const String &url)
{
    url_ = url;
    response_.clear();
    return url.length() > 0;
}

namespace
{
size_t curl_write(
    char *data, size_t size, size_t count, void *context)
{
    auto *result = static_cast<std::string *>(context);
    result->append(data, size * count);
    return size * count;
}
}

int HTTPClient::GET()
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return -1;
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    curl_easy_setopt(
        curl, CURLOPT_CONNECTTIMEOUT_MS,
        static_cast<long>(connect_timeout_ms_));
    curl_easy_setopt(
        curl, CURLOPT_TIMEOUT_MS,
        static_cast<long>(timeout_ms_));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Maclock-Simulator/1.0");
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    if (result == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    response_ = response;
    return result == CURLE_OK
               ? static_cast<int>(status)
               : -static_cast<int>(result);
}

String HTTPClient::getString() const
{
    return response_;
}

void HTTPClient::end()
{
    url_.clear();
}

String HTTPClient::errorToString(int error)
{
    if (error >= 0)
        return String("HTTP ") + String(error);
    return curl_easy_strerror(
        static_cast<CURLcode>(-error));
}

struct WebServer::State
{
    struct Route
    {
        std::string path;
        HTTPMethod method;
        std::function<void()> handler;
    };
    struct Request
    {
        std::map<std::string, std::string> arguments;
        std::mutex mutex;
        std::condition_variable condition;
        bool completed = false;
        int status = 200;
        std::string content_type = "text/plain";
        std::string body;
        std::vector<std::pair<std::string, std::string>> headers;
    };

    explicit State(int requested_port)
        : requested_port(requested_port)
    {
    }

    int requested_port;
    uint16_t actual_port = 0;
    std::vector<Route> routes;
    std::function<void()> not_found;
    std::unique_ptr<httplib::Server> server;
    std::thread thread;
    std::mutex queue_mutex;
    std::deque<std::pair<
        std::shared_ptr<Request>, std::function<void()>>>
        queue;
    std::shared_ptr<Request> active;
    std::atomic<bool> stopping{false};

    void enqueue(
        const httplib::Request &request,
        httplib::Response &response,
        const std::function<void()> &handler)
    {
        auto pending = std::make_shared<Request>();
        for (const auto &parameter : request.params)
            pending->arguments[parameter.first] = parameter.second;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stopping)
            {
                response.status = 503;
                response.set_content(
                    "Maclock simulator is stopping",
                    "text/plain");
                return;
            }
            queue.emplace_back(pending, handler);
        }
        std::unique_lock<std::mutex> lock(pending->mutex);
        pending->condition.wait(
            lock, [&]() { return pending->completed; });
        response.status = pending->status;
        response.set_content(
            pending->body, pending->content_type);
        for (const auto &[name, value] : pending->headers)
            response.set_header(name, value);
    }
};

WebServer::WebServer(int port)
    : state_(new State(port))
{
}

WebServer::~WebServer()
{
    stop();
}

void WebServer::on(
    const char *uri, HTTPMethod method,
    std::function<void()> handler)
{
    state_->routes.push_back(
        {uri ? uri : "/", method, std::move(handler)});
}

void WebServer::onNotFound(std::function<void()> handler)
{
    state_->not_found = std::move(handler);
}

void WebServer::begin()
{
    if (state_->server)
        return;
    state_->actual_port =
        maclock_hal().network().remapServerPort(
            state_->requested_port);
    state_->stopping = false;
    state_->server.reset(new httplib::Server());
    for (const auto &route : state_->routes)
    {
        auto callback =
            [this, handler = route.handler](
                const httplib::Request &request,
                httplib::Response &response)
        {
            state_->enqueue(request, response, handler);
        };
        if (route.method == HTTP_GET)
            state_->server->Get(route.path, callback);
        else if (route.method == HTTP_POST)
            state_->server->Post(route.path, callback);
        else
        {
            state_->server->Get(route.path, callback);
            state_->server->Post(route.path, callback);
        }
    }
    state_->server->set_error_handler(
        [this](
            const httplib::Request &request,
            httplib::Response &response)
        {
            if (state_->not_found)
                state_->enqueue(
                    request, response, state_->not_found);
            else
                response.status = 404;
        });
    state_->thread = std::thread(
        [this]()
        {
            state_->server->listen(
                "127.0.0.1", state_->actual_port);
        });
}

void WebServer::stop()
{
    if (!state_->server)
        return;
    state_->stopping = true;
    std::deque<std::pair<
        std::shared_ptr<State::Request>,
        std::function<void()>>>
        abandoned;
    {
        std::lock_guard<std::mutex> lock(
            state_->queue_mutex);
        abandoned.swap(state_->queue);
    }
    for (auto &pending : abandoned)
    {
        {
            std::lock_guard<std::mutex> lock(
                pending.first->mutex);
            pending.first->status = 503;
            pending.first->body =
                "Maclock simulator is stopping";
            pending.first->completed = true;
        }
        pending.first->condition.notify_all();
    }
    state_->server->stop();
    if (state_->thread.joinable())
        state_->thread.join();
    state_->server.reset();
}

void WebServer::handleClient()
{
    for (;;)
    {
        std::pair<
            std::shared_ptr<State::Request>,
            std::function<void()>>
            pending;
        {
            std::lock_guard<std::mutex> lock(
                state_->queue_mutex);
            if (state_->queue.empty())
                break;
            pending = std::move(state_->queue.front());
            state_->queue.pop_front();
        }
        state_->active = pending.first;
        pending.second();
        {
            std::lock_guard<std::mutex> lock(
                pending.first->mutex);
            pending.first->completed = true;
        }
        pending.first->condition.notify_all();
        state_->active.reset();
    }
}

bool WebServer::hasArg(const char *name) const
{
    return state_->active &&
           state_->active->arguments.count(
               name ? name : "") != 0;
}

String WebServer::arg(const char *name) const
{
    if (!state_->active)
        return {};
    const auto found = state_->active->arguments.find(
        name ? name : "");
    return found == state_->active->arguments.end()
               ? String()
               : String(found->second);
}

void WebServer::sendHeader(
    const String &name, const String &value, bool)
{
    if (state_->active)
        state_->active->headers.emplace_back(
            name.stdString(), value.stdString());
}

void WebServer::send(
    int status, const char *content_type,
    const String &content)
{
    if (!state_->active)
        return;
    state_->active->status = status;
    state_->active->content_type =
        content_type ? content_type : "text/plain";
    state_->active->body = content.stdString();
}

void WebServer::send(
    int status, const char *content_type,
    const char *content)
{
    send(status, content_type, String(content));
}

void WebServer::send_P(
    int status, const char *content_type,
    PGM_P content, size_t length)
{
    if (!state_->active)
        return;
    state_->active->status = status;
    state_->active->content_type =
        content_type ? content_type : "application/octet-stream";
    state_->active->body.assign(content, length);
}

bool DNSServer::start(
    uint16_t, const char *, IPAddress)
{
    return true;
}

void DNSServer::processNextRequest()
{
}

void DNSServer::stop()
{
}

bool MDNSResponder::begin(const char *)
{
    return true;
}

void MDNSResponder::addService(
    const char *, const char *, uint16_t)
{
}

void MDNSResponder::end()
{
}
