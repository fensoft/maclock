#pragma once

#include <functional>
#include <chrono>
#include <string>
#include <vector>

#include <Arduino.h>

// Small MQTT 3.1.1 client for the local simulator. All socket progress occurs
// from loop(), so the simulator's UI thread is never blocked on broker I/O.
class LocalMqttClient
{
public:
    using MessageCallback = std::function<void(String &, String &)>;

    LocalMqttClient();
    ~LocalMqttClient();

    void begin(const char *host, int port);
    void setOptions(int keep_alive_seconds, bool clean_session, int);
    void setWill(const char *topic, const char *payload, bool retained, int qos);
    void onMessage(MessageCallback callback);
    bool connect(const char *client_id, const char *username, const char *password);
    bool publish(const char *topic, const char *payload, bool retained, int qos);
    bool publish(const char *topic, const String &payload, bool retained, int qos)
    {
        return publish(topic, payload.c_str(), retained, qos);
    }
    bool subscribe(const char *topic, int qos);
    bool loop();
    void disconnect();

    bool connected() const;
    bool connecting() const;
    const char *error() const;

private:
    enum class ConnectionState { Disconnected, TcpConnecting, AwaitingConnack, Connected };

    bool queuePacket(unsigned char header, const std::vector<unsigned char> &body);
    bool flushOutput();
    bool receiveInput();
    bool processPackets();
    bool sendConnect();
    void fail(const char *message);
    void closeSocket();
    unsigned short nextPacketId();

    int socket_ = -1;
    ConnectionState state_ = ConnectionState::Disconnected;
    std::string host_;
    int port_ = 1883;
    int keep_alive_seconds_ = 30;
    bool clean_session_ = true;
    std::string will_topic_;
    std::string will_payload_;
    bool will_retained_ = false;
    int will_qos_ = 0;
    std::string client_id_;
    std::string username_;
    std::string password_;
    MessageCallback message_callback_;
    std::vector<unsigned char> output_;
    size_t output_offset_ = 0;
    std::vector<unsigned char> input_;
    unsigned short packet_id_ = 0;
    std::string error_;
    std::chrono::steady_clock::time_point last_activity_;
};
