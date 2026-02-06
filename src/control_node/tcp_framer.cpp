#include "control_node/tcp_framer.h"
#include <cstring>

namespace nng {

std::vector<uint8_t> TcpFramer::encode(const std::string& payload) {
    return encode(reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

std::vector<uint8_t> TcpFramer::encode(const uint8_t* data, std::size_t len) {
    std::vector<uint8_t> result(4 + len);
    // Big-endian length prefix
    uint32_t be_len = static_cast<uint32_t>(len);
    result[0] = static_cast<uint8_t>((be_len >> 24) & 0xFF);
    result[1] = static_cast<uint8_t>((be_len >> 16) & 0xFF);
    result[2] = static_cast<uint8_t>((be_len >> 8) & 0xFF);
    result[3] = static_cast<uint8_t>(be_len & 0xFF);
    if (data && len > 0)
        std::memcpy(result.data() + 4, data, len);
    return result;
}

void TcpFramer::feed(const uint8_t* data, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i)
        buffer_.push_back(data[i]);
    try_extract_frames();
}

void TcpFramer::try_extract_frames() {
    while (buffer_.size() >= 4) {
        // Read length (big-endian)
        uint32_t frame_len = (static_cast<uint32_t>(buffer_[0]) << 24) |
                             (static_cast<uint32_t>(buffer_[1]) << 16) |
                             (static_cast<uint32_t>(buffer_[2]) << 8) |
                             static_cast<uint32_t>(buffer_[3]);

        // Sanity check to prevent memory exhaustion
        if (frame_len > 10 * 1024 * 1024) {
            // Malformed: reset and discard
            buffer_.clear();
            return;
        }

        if (buffer_.size() < 4 + frame_len)
            return; // Not enough data yet

        // Extract frame payload
        std::string payload;
        payload.reserve(frame_len);
        for (std::size_t i = 0; i < frame_len; ++i)
            payload.push_back(static_cast<char>(buffer_[4 + i]));

        // Remove frame from buffer
        for (std::size_t i = 0; i < 4 + frame_len; ++i)
            buffer_.pop_front();

        ready_frames_.push_back(std::move(payload));
    }
}

bool TcpFramer::has_frame() const {
    return !ready_frames_.empty();
}

std::string TcpFramer::pop_frame() {
    if (ready_frames_.empty())
        return {};
    std::string frame = std::move(ready_frames_.front());
    ready_frames_.pop_front();
    return frame;
}

void TcpFramer::reset() {
    buffer_.clear();
    ready_frames_.clear();
}

} // namespace nng
