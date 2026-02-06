#include "cli/cli_client.h"
#include "control_node/tcp_framer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>

namespace nng {

CliClient::~CliClient() {
    close();
}

bool CliClient::connect(const std::string& host, uint16_t port) {
    close();

    sockfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0)
        return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close();
        return false;
    }

    if (::connect(sockfd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }

    return true;
}

std::string CliClient::send_command(const std::string& cmd) {
    if (sockfd_ < 0)
        return "";

    // Encode and send
    auto encoded = TcpFramer::encode(cmd);
    ssize_t sent = ::send(sockfd_, encoded.data(), encoded.size(), 0);
    if (sent != static_cast<ssize_t>(encoded.size()))
        return "";

    // Receive response with framing
    TcpFramer framer;
    uint8_t buf[4096];

    while (!framer.has_frame()) {
        struct pollfd pfd{};
        pfd.fd = sockfd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 5000); // 5 second timeout
        if (ret <= 0)
            return "";

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            return "";

        if (!(pfd.revents & POLLIN))
            continue;

        ssize_t n = ::recv(sockfd_, buf, sizeof(buf), 0);
        if (n <= 0)
            return "";

        framer.feed(buf, static_cast<std::size_t>(n));
    }

    return framer.pop_frame();
}

void CliClient::close() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

} // namespace nng
