#include "common/crc32.h"

namespace nng {
namespace {

// Precomputed CRC32 lookup table (polynomial 0xEDB88320, reflected)
constexpr uint32_t make_crc_entry(uint32_t idx) {
    uint32_t crc = idx;
    for (int j = 0; j < 8; ++j) {
        if (crc & 1)
            crc = (crc >> 1) ^ 0xEDB88320u;
        else
            crc >>= 1;
    }
    return crc;
}

struct CrcTable {
    uint32_t entries[256];
    constexpr CrcTable() : entries{} {
        for (uint32_t i = 0; i < 256; ++i)
            entries[i] = make_crc_entry(i);
    }
};

constexpr CrcTable table{};

} // anonymous namespace

uint32_t crc32_update(uint32_t crc, const uint8_t* data, std::size_t len) {
    crc = ~crc;
    for (std::size_t i = 0; i < len; ++i) {
        uint8_t idx = static_cast<uint8_t>(crc ^ data[i]);
        crc = (crc >> 8) ^ table.entries[idx];
    }
    return ~crc;
}

uint32_t crc32(const uint8_t* data, std::size_t len) {
    return crc32_update(0, data, len);
}

} // namespace nng
