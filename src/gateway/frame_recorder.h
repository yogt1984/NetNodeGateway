#pragma once
#include <string>
#include <fstream>
#include <cstdint>

namespace nng {

class FrameRecorder {
public:
    FrameRecorder() = default;
    ~FrameRecorder();

    // Open file for writing
    bool open(const std::string& path);

    // Record one frame with its receive timestamp
    bool record(uint64_t rx_timestamp_ns,
                const uint8_t* frame_data, std::size_t frame_len);

    // Close file
    void close();

    // How many frames recorded so far
    uint64_t frame_count() const { return frame_count_; }

    bool is_open() const { return file_.is_open(); }

private:
    std::ofstream file_;
    uint64_t frame_count_ = 0;
};

} // namespace nng
