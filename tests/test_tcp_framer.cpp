#include "control_node/tcp_framer.h"
#include <gtest/gtest.h>

using namespace nng;

TEST(TcpFramerTest, EncodeSingleFrame) {
    std::string payload = "HELLO";
    auto encoded = TcpFramer::encode(payload);

    // 4 bytes length + 5 bytes payload
    EXPECT_EQ(encoded.size(), 9u);

    // Length is big-endian
    uint32_t len = (encoded[0] << 24) | (encoded[1] << 16) | (encoded[2] << 8) | encoded[3];
    EXPECT_EQ(len, 5u);

    // Payload matches
    std::string decoded(encoded.begin() + 4, encoded.end());
    EXPECT_EQ(decoded, "HELLO");
}

TEST(TcpFramerTest, EncodeEmptyPayload) {
    std::string payload = "";
    auto encoded = TcpFramer::encode(payload);

    EXPECT_EQ(encoded.size(), 4u);
    uint32_t len = (encoded[0] << 24) | (encoded[1] << 16) | (encoded[2] << 8) | encoded[3];
    EXPECT_EQ(len, 0u);
}

TEST(TcpFramerTest, DecodeSingleFrame) {
    TcpFramer framer;

    std::string payload = "WORLD";
    auto encoded = TcpFramer::encode(payload);

    framer.feed(encoded.data(), encoded.size());

    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "WORLD");
    EXPECT_FALSE(framer.has_frame());
}

TEST(TcpFramerTest, DecodeFragmentedFrame) {
    TcpFramer framer;

    std::string payload = "FRAGMENTED";
    auto encoded = TcpFramer::encode(payload);

    // Feed in 3-byte chunks
    for (std::size_t i = 0; i < encoded.size(); i += 3) {
        std::size_t chunk = std::min<std::size_t>(3, encoded.size() - i);
        framer.feed(encoded.data() + i, chunk);
    }

    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "FRAGMENTED");
}

TEST(TcpFramerTest, DecodeMultipleFrames) {
    TcpFramer framer;

    auto frame1 = TcpFramer::encode("FIRST");
    auto frame2 = TcpFramer::encode("SECOND");
    auto frame3 = TcpFramer::encode("THIRD");

    // Combine all frames
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), frame1.begin(), frame1.end());
    combined.insert(combined.end(), frame2.begin(), frame2.end());
    combined.insert(combined.end(), frame3.begin(), frame3.end());

    framer.feed(combined.data(), combined.size());

    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "FIRST");

    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "SECOND");

    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "THIRD");

    EXPECT_FALSE(framer.has_frame());
}

TEST(TcpFramerTest, PartialLengthHeader) {
    TcpFramer framer;

    std::string payload = "TEST";
    auto encoded = TcpFramer::encode(payload);

    // Feed only 2 bytes of header
    framer.feed(encoded.data(), 2);
    EXPECT_FALSE(framer.has_frame());

    // Feed rest
    framer.feed(encoded.data() + 2, encoded.size() - 2);
    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "TEST");
}

TEST(TcpFramerTest, Reset) {
    TcpFramer framer;

    std::string payload = "RESET";
    auto encoded = TcpFramer::encode(payload);

    // Feed partial
    framer.feed(encoded.data(), 5);
    framer.reset();

    // Should not have frame after reset
    EXPECT_FALSE(framer.has_frame());

    // Feed complete frame after reset
    framer.feed(encoded.data(), encoded.size());
    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), "RESET");
}

TEST(TcpFramerTest, LargePayload) {
    TcpFramer framer;

    std::string payload(10000, 'X');
    auto encoded = TcpFramer::encode(payload);

    framer.feed(encoded.data(), encoded.size());
    EXPECT_TRUE(framer.has_frame());
    EXPECT_EQ(framer.pop_frame(), payload);
}
