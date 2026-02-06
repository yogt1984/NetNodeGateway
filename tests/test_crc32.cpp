#include <gtest/gtest.h>
#include "common/crc32.h"
#include <cstring>
#include <vector>

TEST(Crc32, EmptyInput) {
    EXPECT_EQ(nng::crc32(nullptr, 0), 0x00000000u);
}

TEST(Crc32, StandardCheckValue) {
    // "123456789" -> CRC32 = 0xCBF43926 (standard test vector)
    const char* input = "123456789";
    auto crc = nng::crc32(reinterpret_cast<const uint8_t*>(input), 9);
    EXPECT_EQ(crc, 0xCBF43926u);
}

TEST(Crc32, KnownBuffer) {
    // 100 bytes of value 0xAA
    std::vector<uint8_t> buf(100, 0xAA);
    auto crc = nng::crc32(buf.data(), buf.size());
    // Verify it's deterministic by computing twice
    auto crc2 = nng::crc32(buf.data(), buf.size());
    EXPECT_EQ(crc, crc2);
    // Non-zero for non-trivial input
    EXPECT_NE(crc, 0u);
}

TEST(Crc32, IncrementalMatchesSingleCall) {
    const char* input = "123456789";
    auto full = nng::crc32(reinterpret_cast<const uint8_t*>(input), 9);

    // Feed in two chunks: "12345" then "6789"
    auto partial = nng::crc32_update(0,
        reinterpret_cast<const uint8_t*>(input), 5);
    auto final_crc = nng::crc32_update(partial,
        reinterpret_cast<const uint8_t*>(input + 5), 4);

    EXPECT_EQ(full, final_crc);
}
