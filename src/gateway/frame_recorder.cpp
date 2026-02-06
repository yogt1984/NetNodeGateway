#include "gateway/frame_recorder.h"

namespace nng {

FrameRecorder::~FrameRecorder() {
    close();
}

bool FrameRecorder::open(const std::string& path) {
    close();
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_.is_open())
        return false;
    frame_count_ = 0;
    return true;
}

bool FrameRecorder::record(uint64_t rx_timestamp_ns,
                           const uint8_t* frame_data, std::size_t frame_len) {
    if (!file_.is_open())
        return false;

    // Write timestamp (8 bytes)
    file_.write(reinterpret_cast<const char*>(&rx_timestamp_ns), sizeof(rx_timestamp_ns));

    // Write frame length (4 bytes)
    uint32_t len = static_cast<uint32_t>(frame_len);
    file_.write(reinterpret_cast<const char*>(&len), sizeof(len));

    // Write frame data
    if (frame_len > 0 && frame_data != nullptr) {
        file_.write(reinterpret_cast<const char*>(frame_data), frame_len);
    }

    if (!file_.good())
        return false;

    ++frame_count_;
    return true;
}

void FrameRecorder::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

} // namespace nng
