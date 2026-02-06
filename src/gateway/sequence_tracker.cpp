#include "gateway/sequence_tracker.h"

namespace nng {

SeqEvent SequenceTracker::track(uint16_t src_id, uint32_t seq) {
    auto& s = sources_[src_id];

    if (!s.initialized) {
        s.initialized = true;
        s.next_expected = seq + 1;
        s.seen_window.reset();
        return SeqEvent{SeqResult::FIRST, src_id, 0, seq, 0};
    }

    if (seq == s.next_expected) {
        // Normal: exactly what we expected
        s.seen_window <<= 1;
        s.seen_window.set(WINDOW_SIZE - 1);
        s.next_expected = seq + 1;
        return SeqEvent{SeqResult::OK, src_id, seq, seq, 0};
    }

    if (seq > s.next_expected) {
        // Gap: we skipped some sequence numbers
        uint32_t gap = seq - s.next_expected;
        // Shift window by (gap + 1) positions
        if (gap + 1 >= WINDOW_SIZE) {
            s.seen_window.reset();
        } else {
            s.seen_window <<= (gap + 1);
        }
        s.seen_window.set(WINDOW_SIZE - 1);
        s.next_expected = seq + 1;
        return SeqEvent{SeqResult::GAP, src_id, seq - gap, seq, gap};
    }

    // seq < next_expected: either reorder or duplicate
    uint32_t age = s.next_expected - seq;
    if (age <= WINDOW_SIZE) {
        std::size_t bit_idx = WINDOW_SIZE - age;
        if (s.seen_window.test(bit_idx)) {
            return SeqEvent{SeqResult::DUPLICATE, src_id, s.next_expected, seq, 0};
        }
        // Not seen before -> reorder
        s.seen_window.set(bit_idx);
        return SeqEvent{SeqResult::REORDER, src_id, s.next_expected, seq, 0};
    }

    // Very old packet (older than window) — treat as reorder
    return SeqEvent{SeqResult::REORDER, src_id, s.next_expected, seq, 0};
}

void SequenceTracker::reset(uint16_t src_id) {
    sources_.erase(src_id);
}

void SequenceTracker::reset_all() {
    sources_.clear();
}

std::size_t SequenceTracker::source_count() const {
    return sources_.size();
}

} // namespace nng
