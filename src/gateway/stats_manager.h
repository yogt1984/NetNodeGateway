#pragma once
#include "common/types.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace nng {

struct GlobalStats {
    uint64_t rx_total        = 0;
    uint64_t malformed_total = 0;
    uint64_t gap_total       = 0;
    uint64_t reorder_total   = 0;
    uint64_t duplicate_total = 0;
    uint64_t crc_fail_total  = 0;
};

struct SourceStats {
    uint16_t src_id     = 0;
    uint64_t rx_count   = 0;
    uint64_t malformed  = 0;
    uint64_t gaps       = 0;
    uint64_t reorders   = 0;
    uint64_t duplicates = 0;
    uint32_t last_seq   = 0;
    uint64_t last_ts_ns = 0;
};

enum class HealthState { OK, DEGRADED, ERROR };

class StatsManager {
public:
    void record_rx(uint16_t src_id, uint32_t seq, uint64_t ts_ns);
    void record_malformed(uint16_t src_id);
    void record_gap(uint16_t src_id, uint32_t gap_size);
    void record_reorder(uint16_t src_id);
    void record_duplicate(uint16_t src_id);
    void record_crc_fail(uint16_t src_id);

    GlobalStats get_global_stats() const;
    SourceStats get_source_stats(uint16_t src_id) const;
    std::vector<SourceStats> get_all_source_stats() const;

    HealthState get_health() const;

    void reset();

private:
    mutable std::shared_mutex mutex_;
    GlobalStats global_;
    std::unordered_map<uint16_t, SourceStats> sources_;

    SourceStats& get_or_create_source(uint16_t src_id);
};

} // namespace nng
