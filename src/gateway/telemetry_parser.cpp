#include "gateway/telemetry_parser.h"
#include "common/crc32.h"
#include <cstring>

namespace nng {

ParseError parse_frame(const uint8_t* buf, std::size_t len,
                       bool crc_enabled, ParsedFrame& out) {
    // Minimum size check
    if (len < FRAME_HEADER_SIZE)
        return ParseError::TOO_SHORT;

    // Deserialize header
    out.header = deserialize_header(buf);

    // Version check
    if (out.header.version != PROTOCOL_VERSION)
        return ParseError::BAD_VERSION;

    // Message type check
    uint8_t mt = out.header.msg_type;
    if (mt < static_cast<uint8_t>(MsgType::PLOT) ||
        mt > static_cast<uint8_t>(MsgType::ENGAGEMENT))
        return ParseError::BAD_MSG_TYPE;

    // Payload length check
    if (out.header.payload_len > MAX_PAYLOAD_SIZE)
        return ParseError::PAYLOAD_TOO_LONG;

    // Total expected size
    std::size_t expected = FRAME_HEADER_SIZE + out.header.payload_len;
    if (crc_enabled)
        expected += FRAME_CRC_SIZE;

    if (len < expected)
        return ParseError::TRUNCATED;

    // Payload pointer
    out.payload_ptr = buf + FRAME_HEADER_SIZE;

    // CRC validation
    out.has_crc = crc_enabled;
    if (crc_enabled) {
        // CRC is stored after the payload
        std::size_t crc_offset = FRAME_HEADER_SIZE + out.header.payload_len;
        std::memcpy(&out.crc, buf + crc_offset, FRAME_CRC_SIZE);

        // Compute CRC over header + payload
        uint32_t computed = crc32(buf, FRAME_HEADER_SIZE + out.header.payload_len);
        if (computed != out.crc)
            return ParseError::CRC_MISMATCH;
    } else {
        out.crc = 0;
    }

    return ParseError::OK;
}

} // namespace nng
