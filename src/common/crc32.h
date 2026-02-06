#pragma once
#include <cstdint>
#include <cstddef>

namespace nng {

// Compute CRC32 (ISO 3309 / ITU-T V.42, polynomial 0xEDB88320)
uint32_t crc32(const uint8_t* data, std::size_t len);

// Incremental CRC32: feed chunks, start with crc=0
uint32_t crc32_update(uint32_t crc, const uint8_t* data, std::size_t len);

} // namespace nng
