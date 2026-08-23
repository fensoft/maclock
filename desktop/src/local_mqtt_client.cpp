#include "local_mqtt_client.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>

namespace
{
constexpr size_t kMaximumPacketSize = 1024 * 1024;

void appendString(std::vector<unsigned char> &data, const std::string &value)
{
    const size_t length = std::min(value.size(), static_cast<size_t>(0xffff));
    data.push_back(static_cast<unsigned char>(length >> 8));
    data.push_back(static_cast<unsigned char>(length));
    data.insert(data.end(), value.begin(), value.begin() + length);
}

void appendPacketId(std::vector<unsigned char> &data, unsigned short id)
{
    data.push_back(static_cast<unsigned char>(id >> 8));
    data.push_back(static_cast<unsigned char>(id));
}
}

LocalMqttClient::LocalMqttClient() = default;

LocalMqttClient::~LocalMqttClient()
{
    closeSocket();
}

void LocalMqttClient::begin(const char *host, int port)
{
    host_ = host ? host : "";
    port_ = port > 0 ? port : 1883;
}

void LocalMqttClient::setOptions(int keep_alive_seconds, bool clean_session, int)
{
    keep_alive_seconds_ = keep_alive_seconds;
    clean_session_ = clean_session;
}

void LocalMqttClient::setWill(const char *topic, const char *payload, bool retained, int qos)
{
    will_topic_ = topic ? topic : "";
    will_payload_ = payload ? payload : "";
    will_retained_ = retained;
    will_qos_ = qos;
}

void LocalMqttClient::onMessage(MessageCallback callback)
{
    message_callback_ = std::move(callback);
}

bool LocalMqttClient::connect(const char *client_id, const char *username, const char *password)
{
    disconnect();
    if (host_.empty())
    {
        fail("Broker host is empty");
        return false;
    }
    client_id_ = client_id ? client_id : "";
    username_ = username ? username : "";
    password_ = password ? password : "";
    error_.clear();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *addresses = nullptr;
    const std::string service = std::to_string(port_);
    if (getaddrinfo(host_.c_str(), service.c_str(), &hints, &addresses) != 0)
    {
        fail("Unable to resolve broker");
        return false;
    }
    for (addrinfo *address = addresses; address; address = address->ai_next)
    {
        socket_ = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket_ < 0)
            continue;
        const int flags = fcntl(socket_, F_GETFL, 0);
        if (flags < 0 || fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            closeSocket();
            continue;
        }
        if (::connect(socket_, address->ai_addr, address->ai_addrlen) == 0)
        {
            state_ = ConnectionState::AwaitingConnack;
            freeaddrinfo(addresses);
            return sendConnect();
        }
        if (errno == EINPROGRESS)
        {
            state_ = ConnectionState::TcpConnecting;
            freeaddrinfo(addresses);
            return true;
        }
        closeSocket();
    }
    freeaddrinfo(addresses);
    fail("Unable to connect to broker");
    return false;
}

bool LocalMqttClient::publish(const char *topic, const char *payload, bool retained, int qos)
{
    if (!connected() || !topic)
        return false;
    const int safe_qos = qos == 1 ? 1 : 0;
    std::vector<unsigned char> body;
    appendString(body, topic);
    if (safe_qos)
        appendPacketId(body, nextPacketId());
    const char *message = payload ? payload : "";
    body.insert(body.end(), message, message + strlen(message));
    return queuePacket(static_cast<unsigned char>(0x30 | (safe_qos << 1) | (retained ? 1 : 0)), body);
}

bool LocalMqttClient::subscribe(const char *topic, int qos)
{
    if (!connected() || !topic)
        return false;
    std::vector<unsigned char> body;
    appendPacketId(body, nextPacketId());
    appendString(body, topic);
    body.push_back(qos == 1 ? 1 : 0);
    return queuePacket(0x82, body);
}

bool LocalMqttClient::loop()
{
    if (state_ == ConnectionState::Disconnected)
        return false;
    if (state_ == ConnectionState::TcpConnecting)
    {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(socket_, &writable);
        timeval timeout{};
        const int ready = select(
            socket_ + 1, nullptr, &writable, nullptr, &timeout);
        if (ready == 0)
            return true;
        if (ready < 0)
        {
            fail("Unable to inspect broker connection");
            return false;
        }
        int socket_error = 0;
        socklen_t size = sizeof(socket_error);
        if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &socket_error, &size) != 0)
        {
            fail("Unable to inspect broker connection");
            return false;
        }
        if (socket_error == EINPROGRESS || socket_error == EALREADY)
            return true;
        if (socket_error != 0)
        {
            fail("Broker connection failed");
            return false;
        }
        state_ = ConnectionState::AwaitingConnack;
        if (!sendConnect())
            return false;
    }
    if (!flushOutput() || !receiveInput() || !processPackets())
        return false;
    if (connected() && keep_alive_seconds_ > 0 &&
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - last_activity_)
                .count() >= keep_alive_seconds_ / 2)
    {
        queuePacket(0xc0, {});
        return flushOutput();
    }
    return true;
}

void LocalMqttClient::disconnect()
{
    if (connected())
        queuePacket(0xe0, {});
    flushOutput();
    closeSocket();
    output_.clear();
    input_.clear();
    output_offset_ = 0;
    state_ = ConnectionState::Disconnected;
}

bool LocalMqttClient::connected() const { return state_ == ConnectionState::Connected; }
bool LocalMqttClient::connecting() const { return state_ == ConnectionState::TcpConnecting || state_ == ConnectionState::AwaitingConnack; }
const char *LocalMqttClient::error() const { return error_.c_str(); }

bool LocalMqttClient::queuePacket(unsigned char header, const std::vector<unsigned char> &body)
{
    if (body.size() > kMaximumPacketSize)
        return false;
    output_.push_back(header);
    size_t remaining = body.size();
    do
    {
        unsigned char encoded = static_cast<unsigned char>(remaining % 128);
        remaining /= 128;
        if (remaining)
            encoded |= 0x80;
        output_.push_back(encoded);
    } while (remaining);
    output_.insert(output_.end(), body.begin(), body.end());
    return true;
}

bool LocalMqttClient::flushOutput()
{
    while (output_offset_ < output_.size())
    {
        const ssize_t sent = send(socket_, output_.data() + output_offset_, output_.size() - output_offset_, 0);
        if (sent > 0)
        {
            output_offset_ += static_cast<size_t>(sent);
            last_activity_ = std::chrono::steady_clock::now();
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;
        fail("Broker write failed");
        return false;
    }
    output_.clear();
    output_offset_ = 0;
    return true;
}

bool LocalMqttClient::receiveInput()
{
    unsigned char buffer[4096];
    for (;;)
    {
        const ssize_t received = recv(socket_, buffer, sizeof(buffer), 0);
        if (received > 0)
        {
            input_.insert(input_.end(), buffer, buffer + received);
            last_activity_ = std::chrono::steady_clock::now();
            if (input_.size() > kMaximumPacketSize)
            {
                fail("Broker packet is too large");
                return false;
            }
            continue;
        }
        if (received == 0)
        {
            fail("Broker closed the connection");
            return false;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        fail("Broker read failed");
        return false;
    }
}

bool LocalMqttClient::processPackets()
{
    size_t offset = 0;
    while (input_.size() - offset >= 2)
    {
        size_t remaining = 0;
        size_t multiplier = 1;
        size_t position = offset + 1;
        unsigned char encoded = 0;
        bool incomplete = false;
        do
        {
            if (position >= input_.size() || multiplier > 128 * 128 * 128)
            {
                incomplete = true;
                break;
            }
            encoded = input_[position++];
            remaining += (encoded & 0x7f) * multiplier;
            multiplier *= 128;
        } while (encoded & 0x80);
        if (incomplete || remaining > kMaximumPacketSize ||
            input_.size() - position < remaining)
            break;
        const unsigned char header = input_[offset];
        const unsigned char *body = input_.data() + position;
        if ((header >> 4) == 2)
        {
            if (remaining != 2 || body[0] != 0 || body[1] != 0)
            {
                fail("Broker rejected CONNECT");
                return false;
            }
            state_ = ConnectionState::Connected;
        }
        else if ((header >> 4) == 3)
        {
            if (remaining < 2)
            {
                fail("Invalid broker PUBLISH");
                return false;
            }
            const size_t topic_length = (static_cast<size_t>(body[0]) << 8) | body[1];
            size_t payload_offset = 2 + topic_length;
            const int qos = (header >> 1) & 3;
            if (payload_offset > remaining || qos == 3)
            {
                fail("Invalid broker PUBLISH");
                return false;
            }
            unsigned short inbound_id = 0;
            if (qos)
            {
                if (payload_offset + 2 > remaining || qos != 1)
                {
                    fail("Unsupported broker PUBLISH QoS");
                    return false;
                }
                inbound_id = static_cast<unsigned short>((body[payload_offset] << 8) | body[payload_offset + 1]);
                payload_offset += 2;
            }
            String topic(std::string(reinterpret_cast<const char *>(body + 2), topic_length));
            String payload(std::string(reinterpret_cast<const char *>(body + payload_offset), remaining - payload_offset));
            if (message_callback_)
                message_callback_(topic, payload);
            if (qos == 1)
            {
                std::vector<unsigned char> ack;
                appendPacketId(ack, inbound_id);
                queuePacket(0x40, ack);
            }
        }
        offset = position + remaining;
    }
    if (offset)
        input_.erase(input_.begin(), input_.begin() + offset);
    return true;
}

bool LocalMqttClient::sendConnect()
{
    std::vector<unsigned char> body;
    appendString(body, "MQTT");
    body.push_back(4);
    unsigned char flags = clean_session_ ? 0x02 : 0;
    if (!will_topic_.empty())
        flags |= 0x04 | static_cast<unsigned char>((will_qos_ == 1 ? 1 : 0) << 3) | (will_retained_ ? 0x20 : 0);
    if (!username_.empty()) flags |= 0x80;
    if (!password_.empty()) flags |= 0x40;
    body.push_back(flags);
    appendPacketId(body, static_cast<unsigned short>(std::max(0, keep_alive_seconds_)));
    appendString(body, client_id_);
    if (!will_topic_.empty())
    {
        appendString(body, will_topic_);
        appendString(body, will_payload_);
    }
    if (!username_.empty()) appendString(body, username_);
    if (!password_.empty()) appendString(body, password_);
    return queuePacket(0x10, body);
}

void LocalMqttClient::fail(const char *message)
{
    error_ = message;
    closeSocket();
    state_ = ConnectionState::Disconnected;
}

void LocalMqttClient::closeSocket()
{
    if (socket_ >= 0)
    {
        close(socket_);
        socket_ = -1;
    }
}

unsigned short LocalMqttClient::nextPacketId()
{
    if (++packet_id_ == 0)
        ++packet_id_;
    return packet_id_;
}
