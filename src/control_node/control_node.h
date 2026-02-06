#pragma once
#include "gateway/stats_manager.h"
#include "common/logger.h"
#include "control_node/command_handler.h"
#include <cstdint>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

namespace nng {

class ControlNode {
public:
    ControlNode(uint16_t port, StatsManager& stats, Logger& logger);
    ~ControlNode();

    // Start listening (spawns accept thread)
    bool start();

    // Stop (closes all connections, joins thread)
    void stop();

    bool is_running() const { return running_.load(); }

    // Access command handler (for testing)
    CommandHandler& handler() { return handler_; }

private:
    void accept_loop();
    void client_loop(int client_fd);

    uint16_t port_;
    StatsManager& stats_;
    Logger& logger_;
    CommandHandler handler_;

    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::thread accept_thread_;

    std::mutex clients_mutex_;
    std::vector<std::thread> client_threads_;
    std::vector<int> client_fds_;
};

} // namespace nng
