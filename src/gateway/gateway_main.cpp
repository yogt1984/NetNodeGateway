#include "gateway/gateway.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_shutdown{false};
static nng::Gateway* g_gateway = nullptr;

void signal_handler(int /*signum*/) {
    g_shutdown.store(true);
    if (g_gateway) {
        g_gateway->stop();
    }
}

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --port <port>       UDP port to listen on (default: 5000)\n"
              << "  --crc               Enable CRC validation (default)\n"
              << "  --no-crc            Disable CRC validation\n"
              << "  --record <path>     Record frames to file\n"
              << "  --replay <path>     Replay frames from file instead of UDP\n"
              << "  --log-level <level> Log level: DEBUG, INFO, WARN, ALARM, ERROR, FATAL\n"
              << "  --help              Show this help\n";
}

nng::Severity parse_log_level(const std::string& level) {
    if (level == "DEBUG") return nng::Severity::DEBUG;
    if (level == "INFO")  return nng::Severity::INFO;
    if (level == "WARN")  return nng::Severity::WARN;
    if (level == "ALARM") return nng::Severity::ALARM;
    if (level == "ERROR") return nng::Severity::ERROR;
    if (level == "FATAL") return nng::Severity::FATAL;
    return nng::Severity::INFO;
}

int main(int argc, char* argv[]) {
    nng::GatewayConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.udp_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--crc") {
            config.crc_enabled = true;
        } else if (arg == "--no-crc") {
            config.crc_enabled = false;
        } else if (arg == "--record" && i + 1 < argc) {
            config.record_enabled = true;
            config.record_path = argv[++i];
        } else if (arg == "--replay" && i + 1 < argc) {
            config.replay_path = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = parse_log_level(argv[++i]);
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set up signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    nng::Gateway gateway(config);
    g_gateway = &gateway;

    std::cout << "Starting gateway on UDP port " << config.udp_port << "\n";
    if (config.record_enabled) {
        std::cout << "Recording to: " << config.record_path << "\n";
    }
    if (!config.replay_path.empty()) {
        std::cout << "Replaying from: " << config.replay_path << "\n";
    }
    std::cout << "CRC validation: " << (config.crc_enabled ? "enabled" : "disabled") << "\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    gateway.run();

    // Print final stats
    auto stats = gateway.stats().get_global_stats();
    std::cout << "\n=== Final Statistics ===\n"
              << "Frames received: " << stats.rx_total << "\n"
              << "Malformed:       " << stats.malformed_total << "\n"
              << "CRC failures:    " << stats.crc_fail_total << "\n"
              << "Sequence gaps:   " << stats.gap_total << "\n"
              << "Reorders:        " << stats.reorder_total << "\n"
              << "Duplicates:      " << stats.duplicate_total << "\n";

    g_gateway = nullptr;
    return 0;
}
