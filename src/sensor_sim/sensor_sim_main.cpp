#include "sensor_sim/object_generator.h"
#include "sensor_sim/world_model.h"
#include "sensor_sim/measurement_generator.h"
#include "sensor_sim/fault_injector.h"
#include "sensor_sim/scenario_loader.h"
#include "gateway/udp_socket.h"
#include "common/logger.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_shutdown{false};

void signal_handler(int /*signum*/) {
    g_shutdown.store(true);
}

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --profile <name>    Scenario profile: idle, patrol, raid, stress (default: patrol)\n"
              << "  --profile-file <f>  Load profile from JSON file\n"
              << "  --host <ip>         Target host (default: 127.0.0.1)\n"
              << "  --port <port>       Target UDP port (default: 5000)\n"
              << "  --rate <hz>         Tick rate in Hz (default: 50)\n"
              << "  --duration <sec>    Duration in seconds (default: 10)\n"
              << "  --seed <int>        Random seed (default: 42)\n"
              << "  --loss <pct>        Packet loss percentage (default: 0)\n"
              << "  --reorder <pct>     Reorder percentage (default: 0)\n"
              << "  --duplicate <pct>   Duplicate percentage (default: 0)\n"
              << "  --corrupt <pct>     Corruption percentage (default: 0)\n"
              << "  --help              Show this help\n";
}

int main(int argc, char* argv[]) {
    std::string profile_name = "patrol";
    std::string profile_file;
    std::string host = "127.0.0.1";
    uint16_t port = 5000;
    double rate_hz = 50.0;
    double duration_s = 10.0;
    uint32_t seed = 42;
    double loss_pct = 0.0;
    double reorder_pct = 0.0;
    double duplicate_pct = 0.0;
    double corrupt_pct = 0.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--profile" && i + 1 < argc) {
            profile_name = argv[++i];
        } else if (arg == "--profile-file" && i + 1 < argc) {
            profile_file = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--rate" && i + 1 < argc) {
            rate_hz = std::stod(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration_s = std::stod(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--loss" && i + 1 < argc) {
            loss_pct = std::stod(argv[++i]);
        } else if (arg == "--reorder" && i + 1 < argc) {
            reorder_pct = std::stod(argv[++i]);
        } else if (arg == "--duplicate" && i + 1 < argc) {
            duplicate_pct = std::stod(argv[++i]);
        } else if (arg == "--corrupt" && i + 1 < argc) {
            corrupt_pct = std::stod(argv[++i]);
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

    // Load profile
    nng::ScenarioProfile profile;
    if (!profile_file.empty()) {
        try {
            profile = nng::load_scenario(profile_file);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load profile: " << e.what() << "\n";
            return 1;
        }
    } else if (profile_name == "idle") {
        profile = nng::profile_idle();
    } else if (profile_name == "patrol") {
        profile = nng::profile_patrol();
    } else if (profile_name == "raid") {
        profile = nng::profile_raid();
    } else if (profile_name == "stress") {
        profile = nng::profile_stress();
    } else {
        std::cerr << "Unknown profile: " << profile_name << "\n";
        return 1;
    }

    std::cout << "=== Sensor Simulator ===\n"
              << "Profile:   " << profile.name << "\n"
              << "Target:    " << host << ":" << port << "\n"
              << "Rate:      " << rate_hz << " Hz\n"
              << "Duration:  " << duration_s << " s\n"
              << "Seed:      " << seed << "\n"
              << "Faults:    loss=" << loss_pct << "% reorder=" << reorder_pct
              << "% dup=" << duplicate_pct << "% corrupt=" << corrupt_pct << "%\n\n";

    // Create components
    nng::ObjectGenerator generator(profile, seed);
    nng::WorldModel world;
    nng::MeasurementGenerator measurer(1, seed + 100);

    nng::FaultConfig fault_config;
    fault_config.loss_pct = loss_pct;
    fault_config.reorder_pct = reorder_pct;
    fault_config.duplicate_pct = duplicate_pct;
    fault_config.corrupt_pct = corrupt_pct;
    nng::FaultInjector injector(fault_config, seed + 200);

    // Connect to gateway
    nng::UdpFrameSink sink;
    if (!sink.connect(host, port)) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        return 1;
    }

    // Initialize world
    auto initial_objects = generator.generate_initial();
    for (auto& obj : initial_objects) {
        world.add_object(obj);
    }

    std::cout << "Initial objects: " << world.active_count() << "\n";
    std::cout << "Starting simulation...\n\n";

    const double dt = 1.0 / rate_hz;
    const int total_ticks = static_cast<int>(duration_s * rate_hz);
    int tick = 0;

    uint64_t frames_sent = 0;
    uint64_t frames_dropped = 0;
    uint64_t frames_reordered = 0;
    uint64_t frames_duplicated = 0;
    uint64_t frames_corrupted = 0;

    auto start_time = std::chrono::steady_clock::now();
    auto next_tick_time = start_time;

    while (tick < total_ticks && !g_shutdown.load()) {
        double current_time_s = tick * dt;
        uint64_t timestamp_ns = static_cast<uint64_t>(current_time_s * 1e9);

        // Maybe spawn new object
        auto spawned = generator.maybe_spawn(current_time_s);
        if (spawned) {
            world.add_object(*spawned);
        }

        // Tick world
        world.tick(dt, current_time_s);

        // Generate frames
        auto tracks = measurer.generate_tracks(world.objects(), timestamp_ns);
        auto plots = measurer.generate_plots(world.objects(), timestamp_ns);

        // Combine frames
        std::vector<std::vector<uint8_t>> frames;
        frames.insert(frames.end(), tracks.begin(), tracks.end());
        frames.insert(frames.end(), plots.begin(), plots.end());

        // Generate heartbeat every 50 ticks
        if (tick % 50 == 0) {
            auto hb = measurer.generate_heartbeat(timestamp_ns);
            frames.push_back(hb);
        }

        // Apply faults
        injector.apply(frames);

        auto fault_stats = injector.last_stats();
        frames_dropped += fault_stats.dropped;
        frames_reordered += fault_stats.reordered;
        frames_duplicated += fault_stats.duplicated;
        frames_corrupted += fault_stats.corrupted;

        // Send frames
        for (const auto& frame : frames) {
            if (sink.send(frame)) {
                frames_sent++;
            }
        }

        tick++;

        // Rate limiting
        next_tick_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(dt));
        auto now = std::chrono::steady_clock::now();
        if (next_tick_time > now) {
            std::this_thread::sleep_until(next_tick_time);
        }

        // Progress update every second
        if (tick % static_cast<int>(rate_hz) == 0) {
            std::cout << "Progress: " << tick << "/" << total_ticks
                      << " ticks, " << frames_sent << " frames sent\r" << std::flush;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    sink.close();

    std::cout << "\n\n=== Summary ===\n"
              << "Ticks:           " << tick << "\n"
              << "Frames sent:     " << frames_sent << "\n"
              << "Frames dropped:  " << frames_dropped << "\n"
              << "Frames reordered:" << frames_reordered << "\n"
              << "Frames duped:    " << frames_duplicated << "\n"
              << "Frames corrupted:" << frames_corrupted << "\n"
              << "Duration:        " << elapsed.count() << " ms\n";

    if (elapsed.count() > 0) {
        double rate = static_cast<double>(frames_sent) * 1000.0 / elapsed.count();
        std::cout << "Effective rate:  " << rate << " frames/sec\n";
    }

    return 0;
}
