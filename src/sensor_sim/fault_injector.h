#pragma once
#include <vector>
#include <random>
#include <cstdint>

namespace nng {

struct FaultConfig {
    double loss_pct      = 0.0;
    double reorder_pct   = 0.0;
    double duplicate_pct = 0.0;
    double corrupt_pct   = 0.0;
};

class FaultInjector {
public:
    explicit FaultInjector(const FaultConfig& config, uint32_t seed = 99);

    // Apply faults to a batch of frames IN PLACE.
    void apply(std::vector<std::vector<uint8_t>>& frames);

    struct FaultStats {
        uint32_t dropped    = 0;
        uint32_t reordered  = 0;
        uint32_t duplicated = 0;
        uint32_t corrupted  = 0;
    };

    FaultStats last_stats() const { return last_stats_; }

private:
    FaultConfig config_;
    std::mt19937 rng_;
    FaultStats last_stats_;
};

} // namespace nng
