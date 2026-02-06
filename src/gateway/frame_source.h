#pragma once
#include <vector>
#include <cstdint>

namespace nng {

class IFrameSource {
public:
    virtual ~IFrameSource() = default;

    // Receive one frame into buf. Returns true if a frame was received.
    virtual bool receive(std::vector<uint8_t>& buf) = 0;
};

class IFrameSink {
public:
    virtual ~IFrameSink() = default;

    // Send one frame. Returns true on success.
    virtual bool send(const std::vector<uint8_t>& buf) = 0;
};

} // namespace nng
