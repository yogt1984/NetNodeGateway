#pragma once
#include "gateway/frame_source.h"
#include <string>
#include <cstdint>

namespace nng {

class UdpFrameSource : public IFrameSource {
public:
    UdpFrameSource() = default;
    ~UdpFrameSource() override;

    // Bind to a UDP port
    bool bind(uint16_t port);

    // Receive one datagram (blocks up to timeout)
    bool receive(std::vector<uint8_t>& buf) override;

    // Close socket
    void close();

    // Set receive timeout in milliseconds (0 = blocking)
    void set_timeout_ms(int ms);

    bool is_open() const { return sockfd_ >= 0; }

private:
    int sockfd_ = -1;
    int timeout_ms_ = 100;
};

class UdpFrameSink : public IFrameSink {
public:
    UdpFrameSink() = default;
    ~UdpFrameSink() override;

    // Connect to a remote host:port (sets default destination)
    bool connect(const std::string& host, uint16_t port);

    // Send one datagram
    bool send(const std::vector<uint8_t>& buf) override;

    // Close socket
    void close();

    bool is_open() const { return sockfd_ >= 0; }

private:
    int sockfd_ = -1;
};

} // namespace nng
