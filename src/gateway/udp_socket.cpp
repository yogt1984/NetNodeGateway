#include "gateway/udp_socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>

namespace nng {

// --- UdpFrameSource ---

UdpFrameSource::~UdpFrameSource() {
    close();
}

bool UdpFrameSource::bind(uint16_t port) {
    close();

    sockfd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0)
        return false;

    // Allow address reuse
    int optval = 1;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(sockfd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }

    return true;
}

bool UdpFrameSource::receive(std::vector<uint8_t>& buf) {
    if (sockfd_ < 0)
        return false;

    // Poll with timeout
    struct pollfd pfd{};
    pfd.fd = sockfd_;
    pfd.events = POLLIN;

    int ret = ::poll(&pfd, 1, timeout_ms_);
    if (ret <= 0)
        return false; // Timeout or error

    if (!(pfd.revents & POLLIN))
        return false;

    // Receive datagram
    buf.resize(65536); // Max UDP datagram size
    struct sockaddr_in src_addr{};
    socklen_t src_len = sizeof(src_addr);

    ssize_t n = ::recvfrom(sockfd_, buf.data(), buf.size(), 0,
                           reinterpret_cast<struct sockaddr*>(&src_addr), &src_len);
    if (n <= 0) {
        buf.clear();
        return false;
    }

    buf.resize(static_cast<std::size_t>(n));
    return true;
}

void UdpFrameSource::close() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

void UdpFrameSource::set_timeout_ms(int ms) {
    timeout_ms_ = ms;
}

// --- UdpFrameSink ---

UdpFrameSink::~UdpFrameSink() {
    close();
}

bool UdpFrameSink::connect(const std::string& host, uint16_t port) {
    close();

    sockfd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ < 0)
        return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close();
        return false;
    }

    // "Connect" sets the default destination for send()
    if (::connect(sockfd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }

    return true;
}

bool UdpFrameSink::send(const std::vector<uint8_t>& buf) {
    if (sockfd_ < 0 || buf.empty())
        return false;

    ssize_t n = ::send(sockfd_, buf.data(), buf.size(), 0);
    return n == static_cast<ssize_t>(buf.size());
}

void UdpFrameSink::close() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

} // namespace nng
