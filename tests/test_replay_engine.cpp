#include "replay/replay_engine.h"
#include "gateway/frame_recorder.h"
#include <gtest/gtest.h>
#include <cstdio>

using namespace nng;

class ReplayEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "/tmp/test_replay_engine_" + std::to_string(rand()) + ".bin";
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }

    std::string test_file_;
};

TEST_F(ReplayEngineTest, RecordAndReplaySameFrames) {
    // Record 10 frames
    std::vector<std::vector<uint8_t>> original_frames;
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));

        for (int i = 0; i < 10; ++i) {
            std::vector<uint8_t> frame = {
                static_cast<uint8_t>(i),
                static_cast<uint8_t>(i * 2),
                static_cast<uint8_t>(i * 3)
            };
            original_frames.push_back(frame);
            recorder.record(i * 1000000, frame.data(), frame.size());
        }
        recorder.close();
    }

    // Replay and verify
    ReplayFrameSource replay;
    ASSERT_TRUE(replay.open(test_file_));
    replay.set_speed(0.0); // As fast as possible

    std::vector<uint8_t> buf;
    int count = 0;
    while (!replay.is_done()) {
        if (!replay.receive(buf))
            break;

        EXPECT_EQ(buf, original_frames[count]);
        count++;
    }

    EXPECT_EQ(count, 10);
    EXPECT_TRUE(replay.is_done());
    EXPECT_EQ(replay.frames_replayed(), 10u);
}

TEST_F(ReplayEngineTest, IsDoneAfterAllFrames) {
    // Record 3 frames
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));
        uint8_t frame[] = {0x01};
        recorder.record(1000, frame, 1);
        recorder.record(2000, frame, 1);
        recorder.record(3000, frame, 1);
        recorder.close();
    }

    ReplayFrameSource replay;
    ASSERT_TRUE(replay.open(test_file_));
    replay.set_speed(0.0);

    EXPECT_FALSE(replay.is_done());

    std::vector<uint8_t> buf;
    replay.receive(buf);
    EXPECT_FALSE(replay.is_done());

    replay.receive(buf);
    EXPECT_FALSE(replay.is_done());

    replay.receive(buf);
    EXPECT_TRUE(replay.is_done());
}

TEST_F(ReplayEngineTest, FramesReplayedCount) {
    // Record 5 frames
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));
        uint8_t frame[] = {0x01};
        for (int i = 0; i < 5; ++i) {
            recorder.record(i * 1000, frame, 1);
        }
        recorder.close();
    }

    ReplayFrameSource replay;
    ASSERT_TRUE(replay.open(test_file_));
    replay.set_speed(0.0);

    EXPECT_EQ(replay.frames_replayed(), 0u);

    std::vector<uint8_t> buf;
    replay.receive(buf);
    EXPECT_EQ(replay.frames_replayed(), 1u);

    replay.receive(buf);
    replay.receive(buf);
    EXPECT_EQ(replay.frames_replayed(), 3u);

    replay.receive(buf);
    replay.receive(buf);
    EXPECT_EQ(replay.frames_replayed(), 5u);
}

TEST_F(ReplayEngineTest, SpeedZeroReturnsFast) {
    // Record 100 frames
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));
        uint8_t frame[] = {0x01};
        for (int i = 0; i < 100; ++i) {
            // Timestamps 100ms apart (real-time would take 10 seconds)
            recorder.record(i * 100000000ULL, frame, 1);
        }
        recorder.close();
    }

    ReplayFrameSource replay;
    ASSERT_TRUE(replay.open(test_file_));
    replay.set_speed(0.0); // As fast as possible

    auto start = std::chrono::steady_clock::now();

    std::vector<uint8_t> buf;
    while (!replay.is_done()) {
        replay.receive(buf);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(replay.frames_replayed(), 100u);
    // Should complete in less than 1 second (not 10 seconds of real-time)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(ReplayEngineTest, OpenNonexistentFile) {
    ReplayFrameSource replay;
    EXPECT_FALSE(replay.open("/nonexistent/path/to/file.bin"));
    EXPECT_FALSE(replay.is_open());
}

TEST_F(ReplayEngineTest, ByteIdenticalFrames) {
    // Record frames with varying content
    std::vector<std::vector<uint8_t>> original_frames;
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));

        for (int i = 0; i < 5; ++i) {
            std::vector<uint8_t> frame;
            for (int j = 0; j <= i; ++j) {
                frame.push_back(static_cast<uint8_t>((i * 17 + j * 31) & 0xFF));
            }
            original_frames.push_back(frame);
            recorder.record(i * 1000, frame.data(), frame.size());
        }
        recorder.close();
    }

    // Replay and verify byte-by-byte
    ReplayFrameSource replay;
    ASSERT_TRUE(replay.open(test_file_));
    replay.set_speed(0.0);

    std::vector<uint8_t> buf;
    for (size_t i = 0; i < original_frames.size(); ++i) {
        ASSERT_TRUE(replay.receive(buf));
        ASSERT_EQ(buf.size(), original_frames[i].size());
        for (size_t j = 0; j < buf.size(); ++j) {
            EXPECT_EQ(buf[j], original_frames[i][j])
                << "Mismatch at frame " << i << " byte " << j;
        }
    }
}
