#pragma once
#include "common/protocol.h"
#include "common/types.h"
#include <cstdint>
#include <cstddef>

namespace nng {

enum class ParseError {
    OK = 0,
    TOO_SHORT,
    BAD_VERSION,
    BAD_MSG_TYPE,
    PAYLOAD_TOO_LONG,
    TRUNCATED,
    CRC_MISMATCH,
};

struct ParsedFrame {
    TelemetryHeader header;
    const uint8_t*  payload_ptr = nullptr;
    uint32_t        crc         = 0;
    bool            has_crc     = false;
};

// Parse a raw UDP datagram into a validated frame.
// crc_enabled: if true, expect 4-byte CRC32 after payload.
ParseError parse_frame(const uint8_t* buf, std::size_t len,
                       bool crc_enabled, ParsedFrame& out);

} // namespace nng
