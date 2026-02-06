#include "sensor_sim/fault_injector.h"
#include <algorithm>

namespace nng {

FaultInjector::FaultInjector(const FaultConfig& config, uint32_t seed)
    : config_(config), rng_(seed) {}

void FaultInjector::apply(std::vector<std::vector<uint8_t>>& frames) {
    last_stats_ = FaultStats{};

    if (frames.empty())
        return;

    std::uniform_real_distribution<double> pct(0.0, 100.0);

    // 1. Corruption (before loss, so corrupted frames may also be dropped)
    if (config_.corrupt_pct > 0.0) {
        for (auto& frame : frames) {
            if (pct(rng_) < config_.corrupt_pct && !frame.empty()) {
                std::uniform_int_distribution<size_t> byte_dist(0, frame.size() - 1);
                frame[byte_dist(rng_)] ^= 0xFF;
                last_stats_.corrupted++;
            }
        }
    }

    // 2. Duplication (before loss, so duplicates may be dropped)
    if (config_.duplicate_pct > 0.0) {
        std::vector<std::vector<uint8_t>> extras;
        for (size_t i = 0; i < frames.size(); ++i) {
            if (pct(rng_) < config_.duplicate_pct) {
                extras.push_back(frames[i]);
                last_stats_.duplicated++;
            }
        }
        // Insert duplicates at random positions
        for (auto& dup : extras) {
            std::uniform_int_distribution<size_t> pos_dist(0, frames.size());
            auto pos = frames.begin() + static_cast<ptrdiff_t>(pos_dist(rng_));
            frames.insert(pos, std::move(dup));
        }
    }

    // 3. Loss
    if (config_.loss_pct > 0.0) {
        auto new_end = std::remove_if(frames.begin(), frames.end(),
            [&](const std::vector<uint8_t>&) {
                if (pct(rng_) < config_.loss_pct) {
                    last_stats_.dropped++;
                    return true;
                }
                return false;
            });
        frames.erase(new_end, frames.end());
    }

    // 4. Reorder (swap adjacent pairs)
    if (config_.reorder_pct > 0.0 && frames.size() >= 2) {
        for (size_t i = 0; i + 1 < frames.size(); ++i) {
            if (pct(rng_) < config_.reorder_pct) {
                std::swap(frames[i], frames[i + 1]);
                last_stats_.reordered++;
                ++i; // skip the swapped pair
            }
        }
    }
}

} // namespace nng
