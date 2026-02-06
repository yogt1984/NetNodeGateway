#include "gateway/udp_socket.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace nng;

TEST(UdpLoopbackTest, BindAndClose) {
    UdpFrameSource source;

    EXPECT_FALSE(source.is_open());
    EXPECT_TRUE(source.bind(0));  // Bind to any available port
    EXPECT_TRUE(source.is_open());

    source.close();
    EXPECT_FALSE(source.is_open());
}

TEST(UdpLoopbackTest, SendAndReceive) {
    UdpFrameSource source;
    UdpFrameSink sink;

    // Bind source to a known port
    uint16_t port = 19876;
    ASSERT_TRUE(source.bind(port));
    source.set_timeout_ms(1000);

    // Connect sink to same port
    ASSERT_TRUE(sink.connect("127.0.0.1", port));

    // Send data
    std::vector<uint8_t> send_buf = {'H', 'E', 'L', 'L', 'O'};
    EXPECT_TRUE(sink.send(send_buf));

    // Receive data
    std::vector<uint8_t> recv_buf;
    EXPECT_TRUE(source.receive(recv_buf));
    EXPECT_EQ(recv_buf, send_buf);

    source.close();
    sink.close();
}

TEST(UdpLoopbackTest, SendMultipleDatagrams) {
    UdpFrameSource source;
    UdpFrameSink sink;

    uint16_t port = 19877;
    ASSERT_TRUE(source.bind(port));
    source.set_timeout_ms(1000);
    ASSERT_TRUE(sink.connect("127.0.0.1", port));

    // Send multiple datagrams
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> send_buf = {static_cast<uint8_t>('A' + i)};
        EXPECT_TRUE(sink.send(send_buf));
    }

    // Small delay to ensure all datagrams arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Receive all
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> recv_buf;
        EXPECT_TRUE(source.receive(recv_buf));
        EXPECT_EQ(recv_buf.size(), 1u);
        EXPECT_EQ(recv_buf[0], static_cast<uint8_t>('A' + i));
    }

    source.close();
    sink.close();
}

TEST(UdpLoopbackTest, ReceiveTimeout) {
    UdpFrameSource source;

    uint16_t port = 19878;
    ASSERT_TRUE(source.bind(port));
    source.set_timeout_ms(100);

    // Try to receive without any sender
    std::vector<uint8_t> recv_buf;
    auto start = std::chrono::steady_clock::now();
    bool received = source.receive(recv_buf);
    auto end = std::chrono::steady_clock::now();

    EXPECT_FALSE(received);
    EXPECT_TRUE(recv_buf.empty());

    // Should have taken approximately timeout_ms
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed, 50);  // At least 50ms
    EXPECT_LE(elapsed, 500); // Not too long

    source.close();
}

TEST(UdpLoopbackTest, LargePayload) {
    UdpFrameSource source;
    UdpFrameSink sink;

    uint16_t port = 19879;
    ASSERT_TRUE(source.bind(port));
    source.set_timeout_ms(1000);
    ASSERT_TRUE(sink.connect("127.0.0.1", port));

    // Send a larger payload (but under MTU)
    std::vector<uint8_t> send_buf(1400, 0xAB);
    EXPECT_TRUE(sink.send(send_buf));

    std::vector<uint8_t> recv_buf;
    EXPECT_TRUE(source.receive(recv_buf));
    EXPECT_EQ(recv_buf, send_buf);

    source.close();
    sink.close();
}

TEST(UdpLoopbackTest, ClosedSocketSend) {
    UdpFrameSink sink;

    // Don't connect
    std::vector<uint8_t> send_buf = {'X'};
    EXPECT_FALSE(sink.send(send_buf));
}

TEST(UdpLoopbackTest, ClosedSocketReceive) {
    UdpFrameSource source;

    // Don't bind
    std::vector<uint8_t> recv_buf;
    EXPECT_FALSE(source.receive(recv_buf));
}

TEST(UdpLoopbackTest, SinkConnectAndClose) {
    UdpFrameSink sink;

    EXPECT_FALSE(sink.is_open());
    EXPECT_TRUE(sink.connect("127.0.0.1", 12345));
    EXPECT_TRUE(sink.is_open());

    sink.close();
    EXPECT_FALSE(sink.is_open());
}
