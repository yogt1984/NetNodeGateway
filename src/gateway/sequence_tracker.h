#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <bitset>

namespace nng {

enum class SeqResult {
    FIRST,
    OK,
    GAP,
    REORDER,
    DUPLICATE,
};

struct SeqEvent {
    SeqResult result;
    uint16_t  src_id;
    uint32_t  expected_seq;
    uint32_t  actual_seq;
    uint32_t  gap_size;
};

class SequenceTracker {
public:
    SeqEvent track(uint16_t src_id, uint32_t seq);
    void reset(uint16_t src_id);
    void reset_all();
    std::size_t source_count() const;

private:
    static constexpr std::size_t WINDOW_SIZE = 64;

    struct SourceState {
        uint32_t next_expected = 0;
        bool     initialized   = false;
        // Sliding window: bit i = 1 means (next_expected - WINDOW_SIZE + i) was seen
        std::bitset<WINDOW_SIZE> seen_window;
    };

    std::unordered_map<uint16_t, SourceState> sources_;
};

} // namespace nng
