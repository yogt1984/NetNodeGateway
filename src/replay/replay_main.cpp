#include "replay/replay_engine.h"
#include "gateway/udp_socket.h"
#include "common/protocol.h"
#include <iostream>
#include <string>
#include <chrono>
#include <cstring>

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " --file <path> [options]\n"
              << "Options:\n"
              << "  --file <path>     Recorded file to replay (required)\n"
              << "  --speed <mult>    Playback speed (1.0 = real-time, 0.0 = fast)\n"
              << "  --host <ip>       Target host (default: 127.0.0.1)\n"
              << "  --port <port>     Target UDP port (default: 5000)\n"
              << "  --dry-run         Print frame summaries without sending\n"
              << "  --help            Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string file_path;
    std::string host = "127.0.0.1";
    uint16_t port = 5000;
    double speed = 1.0;
    bool dry_run = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--file" && i + 1 < argc) {
            file_path = argv[++i];
        } else if (arg == "--speed" && i + 1 < argc) {
            speed = std::stod(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--dry-run") {
            dry_run = true;
        } else if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (file_path.empty()) {
        std::cerr << "Error: --file is required\n";
        print_usage(argv[0]);
        return 1;
    }

    nng::ReplayFrameSource replay;
    if (!replay.open(file_path)) {
        std::cerr << "Error: Could not open file: " << file_path << "\n";
        return 1;
    }

    replay.set_speed(speed);

    nng::UdpFrameSink sink;
    if (!dry_run) {
        if (!sink.connect(host, port)) {
            std::cerr << "Error: Could not connect to " << host << ":" << port << "\n";
            return 1;
        }
    }

    auto start_time = std::chrono::steady_clock::now();
    std::vector<uint8_t> buf;

    while (!replay.is_done()) {
        if (!replay.receive(buf))
            break;

        if (dry_run) {
            // Print frame summary
            if (buf.size() >= nng::FRAME_HEADER_SIZE) {
                nng::TelemetryHeader hdr = nng::deserialize_header(buf.data());
                const char* msg_type_str = "UNKNOWN";
                switch (static_cast<nng::MsgType>(hdr.msg_type)) {
                    case nng::MsgType::PLOT:       msg_type_str = "PLOT"; break;
                    case nng::MsgType::TRACK:      msg_type_str = "TRACK"; break;
                    case nng::MsgType::HEARTBEAT:  msg_type_str = "HEARTBEAT"; break;
                    case nng::MsgType::ENGAGEMENT: msg_type_str = "ENGAGEMENT"; break;
                }
                std::cout << "Frame " << replay.frames_replayed()
                          << ": src_id=" << hdr.src_id
                          << " seq=" << hdr.seq
                          << " type=" << msg_type_str
                          << " len=" << buf.size() << "\n";
            } else {
                std::cout << "Frame " << replay.frames_replayed()
                          << ": len=" << buf.size() << " (too short for header)\n";
            }
        } else {
            sink.send(buf);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\n=== Replay Summary ===\n"
              << "Frames replayed: " << replay.frames_replayed() << "\n"
              << "Duration: " << duration.count() << " ms\n";

    if (duration.count() > 0 && replay.frames_replayed() > 0) {
        double rate = static_cast<double>(replay.frames_replayed()) * 1000.0 / duration.count();
        std::cout << "Effective rate: " << rate << " frames/sec\n";
    }

    replay.close();
    if (!dry_run) {
        sink.close();
    }

    return 0;
}
