#include "control_node/control_node.h"
#include "control_node/tcp_framer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>

namespace nng {

ControlNode::ControlNode(uint16_t port, StatsManager& stats, Logger& logger)
    : port_(port), stats_(stats), logger_(logger), handler_(stats, logger) {}

ControlNode::~ControlNode() {
    stop();
}

bool ControlNode::start() {
    if (running_.load())
        return true;

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
        return false;

    int optval = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (::listen(listen_fd_, 8) < 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    should_stop_.store(false);
    running_.store(true);
    accept_thread_ = std::thread(&ControlNode::accept_loop, this);

    return true;
}

void ControlNode::stop() {
    if (!running_.load())
        return;

    should_stop_.store(true);

    // Close listen socket to unblock accept
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }

    // Close all client sockets
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (int fd : client_fds_) {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
        client_fds_.clear();
    }

    if (accept_thread_.joinable())
        accept_thread_.join();

    // Join all client threads
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& t : client_threads_) {
            if (t.joinable())
                t.join();
        }
        client_threads_.clear();
    }

    running_.store(false);
}

void ControlNode::accept_loop() {
    while (!should_stop_.load()) {
        struct pollfd pfd{};
        pfd.fd = listen_fd_;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100); // 100ms timeout
        if (ret <= 0 || should_stop_.load())
            continue;

        if (!(pfd.revents & POLLIN))
            continue;

        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd_,
            reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

        if (client_fd < 0)
            continue;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_fds_.push_back(client_fd);
            client_threads_.emplace_back(&ControlNode::client_loop, this, client_fd);
        }
    }
}

void ControlNode::client_loop(int client_fd) {
    TcpFramer framer;
    uint8_t buf[4096];

    while (!should_stop_.load()) {
        struct pollfd pfd{};
        pfd.fd = client_fd;
        pfd.events = POLLIN;

        int ret = ::poll(&pfd, 1, 100);
        if (ret < 0)
            break;
        if (ret == 0)
            continue;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            break;

        if (!(pfd.revents & POLLIN))
            continue;

        ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
        if (n <= 0)
            break;

        framer.feed(buf, static_cast<std::size_t>(n));

        while (framer.has_frame()) {
            std::string command = framer.pop_frame();
            std::string response = handler_.handle(command);

            auto encoded = TcpFramer::encode(response);
            ssize_t sent = ::send(client_fd, encoded.data(), encoded.size(), 0);
            if (sent < 0)
                break;
        }
    }

    ::close(client_fd);

    // Remove from client list
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_fds_.erase(
            std::remove(client_fds_.begin(), client_fds_.end(), client_fd),
            client_fds_.end());
    }
}

} // namespace nng
