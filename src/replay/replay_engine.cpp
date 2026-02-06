#include "replay/replay_engine.h"
#include <thread>

namespace nng {

ReplayFrameSource::~ReplayFrameSource() {
    close();
}

bool ReplayFrameSource::open(const std::string& path) {
    close();
    file_.open(path, std::ios::binary);
    if (!file_.is_open())
        return false;

    frames_replayed_ = 0;
    done_ = false;
    first_frame_ = true;
    return true;
}

void ReplayFrameSource::set_speed(double multiplier) {
    speed_multiplier_ = multiplier;
}

bool ReplayFrameSource::receive(std::vector<uint8_t>& buf) {
    buf.clear();

    if (!file_.is_open() || done_)
        return false;

    // Read timestamp
    uint64_t ts_ns = 0;
    file_.read(reinterpret_cast<char*>(&ts_ns), sizeof(ts_ns));
    if (!file_.good()) {
        done_ = true;
        return false;
    }

    // Read frame length
    uint32_t len = 0;
    file_.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!file_.good()) {
        done_ = true;
        return false;
    }

    // Read frame data
    buf.resize(len);
    if (len > 0) {
        file_.read(reinterpret_cast<char*>(buf.data()), len);
        if (!file_.good()) {
            done_ = true;
            buf.clear();
            return false;
        }
    }

    // Handle timing for real-time playback
    if (speed_multiplier_ > 0.0) {
        if (first_frame_) {
            first_frame_ = false;
            first_frame_ts_ns_ = ts_ns;
            replay_start_time_ = std::chrono::steady_clock::now();
        } else {
            // Calculate how long to wait
            uint64_t frame_offset_ns = ts_ns - first_frame_ts_ns_;
            auto target_offset = std::chrono::nanoseconds(
                static_cast<uint64_t>(frame_offset_ns / speed_multiplier_));

            auto elapsed = std::chrono::steady_clock::now() - replay_start_time_;
            auto wait_time = target_offset - elapsed;

            if (wait_time.count() > 0) {
                std::this_thread::sleep_for(wait_time);
            }
        }
    }

    ++frames_replayed_;

    // Check if more data
    if (file_.peek() == EOF) {
        done_ = true;
    }

    return true;
}

void ReplayFrameSource::close() {
    if (file_.is_open()) {
        file_.close();
    }
    done_ = true;
}

} // namespace nng
