#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <deque>

namespace nng {

class TcpFramer {
public:
    // Encode: prepend 4-byte big-endian length to payload
    static std::vector<uint8_t> encode(const std::string& payload);
    static std::vector<uint8_t> encode(const uint8_t* data, std::size_t len);

    // Decode: feed bytes incrementally, extract complete frames
    void feed(const uint8_t* data, std::size_t len);

    // Check if a complete frame is available
    bool has_frame() const;

    // Pop the next complete frame (payload only, no length prefix)
    std::string pop_frame();

    // Reset internal buffer
    void reset();

    // How many bytes are buffered
    std::size_t buffered_bytes() const { return buffer_.size(); }

private:
    std::deque<uint8_t> buffer_;
    std::deque<std::string> ready_frames_;

    void try_extract_frames();
};

} // namespace nng
