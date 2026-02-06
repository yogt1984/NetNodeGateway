#pragma once
#include <string>
#include <cstdint>

namespace nng {

class CliClient {
public:
    CliClient() = default;
    ~CliClient();

    // Connect to a control node
    bool connect(const std::string& host, uint16_t port);

    // Send a command and receive the response
    std::string send_command(const std::string& cmd);

    // Close connection
    void close();

    bool is_connected() const { return sockfd_ >= 0; }

private:
    int sockfd_ = -1;
};

} // namespace nng
