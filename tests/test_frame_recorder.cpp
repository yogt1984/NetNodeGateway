#include "gateway/frame_recorder.h"
#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <vector>

using namespace nng;

class FrameRecorderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "/tmp/test_frame_recorder_" + std::to_string(rand()) + ".bin";
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }

    std::string test_file_;
};

TEST_F(FrameRecorderTest, RecordFrames) {
    FrameRecorder recorder;
    ASSERT_TRUE(recorder.open(test_file_));

    uint8_t frame1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t frame2[] = {0xAA, 0xBB, 0xCC};
    uint8_t frame3[] = {0xFF};
    uint8_t frame4[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t frame5[] = {0xDE, 0xAD, 0xBE, 0xEF};

    EXPECT_TRUE(recorder.record(1000, frame1, sizeof(frame1)));
    EXPECT_TRUE(recorder.record(2000, frame2, sizeof(frame2)));
    EXPECT_TRUE(recorder.record(3000, frame3, sizeof(frame3)));
    EXPECT_TRUE(recorder.record(4000, frame4, sizeof(frame4)));
    EXPECT_TRUE(recorder.record(5000, frame5, sizeof(frame5)));

    EXPECT_EQ(recorder.frame_count(), 5u);

    recorder.close();

    // Verify file exists and has size > 0
    std::ifstream check(test_file_, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(check.is_open());
    EXPECT_GT(check.tellg(), 0);
}

TEST_F(FrameRecorderTest, FrameCountAfterRecording) {
    FrameRecorder recorder;
    ASSERT_TRUE(recorder.open(test_file_));

    EXPECT_EQ(recorder.frame_count(), 0u);

    uint8_t frame[] = {0x01};
    recorder.record(1000, frame, 1);
    EXPECT_EQ(recorder.frame_count(), 1u);

    recorder.record(2000, frame, 1);
    recorder.record(3000, frame, 1);
    recorder.record(4000, frame, 1);
    recorder.record(5000, frame, 1);

    EXPECT_EQ(recorder.frame_count(), 5u);
    recorder.close();
}

TEST_F(FrameRecorderTest, ReadBackRecordedFrames) {
    // Record frames
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));

        uint8_t frame1[] = {0x01, 0x02, 0x03};
        uint8_t frame2[] = {0xAA, 0xBB, 0xCC, 0xDD};
        uint8_t frame3[] = {0xFF, 0xFE};

        recorder.record(1000, frame1, sizeof(frame1));
        recorder.record(2000, frame2, sizeof(frame2));
        recorder.record(3000, frame3, sizeof(frame3));
        recorder.close();
    }

    // Read back and verify
    std::ifstream file(test_file_, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    int frame_count = 0;
    while (file.good()) {
        uint64_t ts;
        file.read(reinterpret_cast<char*>(&ts), sizeof(ts));
        if (!file.good()) break;

        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (!file.good()) break;

        std::vector<uint8_t> data(len);
        if (len > 0) {
            file.read(reinterpret_cast<char*>(data.data()), len);
            if (!file.good()) break;
        }

        frame_count++;

        if (frame_count == 1) {
            EXPECT_EQ(ts, 1000u);
            EXPECT_EQ(len, 3u);
            EXPECT_EQ(data[0], 0x01);
            EXPECT_EQ(data[1], 0x02);
            EXPECT_EQ(data[2], 0x03);
        } else if (frame_count == 2) {
            EXPECT_EQ(ts, 2000u);
            EXPECT_EQ(len, 4u);
        } else if (frame_count == 3) {
            EXPECT_EQ(ts, 3000u);
            EXPECT_EQ(len, 2u);
        }
    }

    EXPECT_EQ(frame_count, 3);
}

TEST_F(FrameRecorderTest, RecordWithClosedFile) {
    FrameRecorder recorder;
    // Don't open the file

    uint8_t frame[] = {0x01};
    EXPECT_FALSE(recorder.record(1000, frame, 1));
    EXPECT_EQ(recorder.frame_count(), 0u);
}

TEST_F(FrameRecorderTest, RecordEmptyFrame) {
    FrameRecorder recorder;
    ASSERT_TRUE(recorder.open(test_file_));

    // Record empty frame (len=0)
    EXPECT_TRUE(recorder.record(1000, nullptr, 0));
    EXPECT_EQ(recorder.frame_count(), 1u);

    recorder.close();

    // Read back
    std::ifstream file(test_file_, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    uint64_t ts;
    file.read(reinterpret_cast<char*>(&ts), sizeof(ts));
    EXPECT_EQ(ts, 1000u);

    uint32_t len;
    file.read(reinterpret_cast<char*>(&len), sizeof(len));
    EXPECT_EQ(len, 0u);
}

TEST_F(FrameRecorderTest, OpenOverwritesExisting) {
    // Create file with some content
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));
        uint8_t frame[] = {0x01, 0x02, 0x03};
        recorder.record(1000, frame, 3);
        recorder.record(2000, frame, 3);
        recorder.record(3000, frame, 3);
        recorder.close();
    }

    // Reopen and record new content
    {
        FrameRecorder recorder;
        ASSERT_TRUE(recorder.open(test_file_));
        uint8_t frame[] = {0xAA};
        recorder.record(9999, frame, 1);
        EXPECT_EQ(recorder.frame_count(), 1u);
        recorder.close();
    }

    // Verify only one frame in file
    std::ifstream file(test_file_, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    uint64_t ts;
    file.read(reinterpret_cast<char*>(&ts), sizeof(ts));
    EXPECT_EQ(ts, 9999u);

    uint32_t len;
    file.read(reinterpret_cast<char*>(&len), sizeof(len));
    EXPECT_EQ(len, 1u);

    uint8_t byte;
    file.read(reinterpret_cast<char*>(&byte), 1);
    EXPECT_EQ(byte, 0xAA);

    // No more frames
    file.read(reinterpret_cast<char*>(&ts), sizeof(ts));
    EXPECT_FALSE(file.good());
}
