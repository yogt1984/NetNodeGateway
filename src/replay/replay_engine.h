#pragma once
#include "gateway/frame_source.h"
#include <string>
#include <fstream>
#include <cstdint>
#include <chrono>

namespace nng {

class ReplayFrameSource : public IFrameSource {
public:
    ReplayFrameSource() = default;
    ~ReplayFrameSource() override;

    // Open recorded file
    bool open(const std::string& path);

    // Set playback speed multiplier (1.0 = real-time, 0.0 = as fast as possible)
    void set_speed(double multiplier);

    // IFrameSource interface
    bool receive(std::vector<uint8_t>& buf) override;

    // Is there more data?
    bool is_done() const { return done_; }

    // How many frames replayed so far
    uint64_t frames_replayed() const { return frames_replayed_; }

    void close();

    bool is_open() const { return file_.is_open(); }

private:
    std::ifstream file_;
    double speed_multiplier_ = 1.0;
    uint64_t frames_replayed_ = 0;
    bool done_ = false;

    // For timing
    bool first_frame_ = true;
    uint64_t first_frame_ts_ns_ = 0;
    std::chrono::steady_clock::time_point replay_start_time_;
};

} // namespace nng
