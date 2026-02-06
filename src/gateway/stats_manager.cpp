#include "gateway/stats_manager.h"

namespace nng {

SourceStats& StatsManager::get_or_create_source(uint16_t src_id) {
    auto it = sources_.find(src_id);
    if (it == sources_.end()) {
        auto& s = sources_[src_id];
        s.src_id = src_id;
        return s;
    }
    return it->second;
}

void StatsManager::record_rx(uint16_t src_id, uint32_t seq, uint64_t ts_ns) {
    std::unique_lock lock(mutex_);
    global_.rx_total++;
    auto& s = get_or_create_source(src_id);
    s.rx_count++;
    s.last_seq = seq;
    s.last_ts_ns = ts_ns;
}

void StatsManager::record_malformed(uint16_t src_id) {
    std::unique_lock lock(mutex_);
    global_.malformed_total++;
    get_or_create_source(src_id).malformed++;
}

void StatsManager::record_gap(uint16_t src_id, uint32_t gap_size) {
    std::unique_lock lock(mutex_);
    global_.gap_total += gap_size;
    get_or_create_source(src_id).gaps += gap_size;
}

void StatsManager::record_reorder(uint16_t src_id) {
    std::unique_lock lock(mutex_);
    global_.reorder_total++;
    get_or_create_source(src_id).reorders++;
}

void StatsManager::record_duplicate(uint16_t src_id) {
    std::unique_lock lock(mutex_);
    global_.duplicate_total++;
    get_or_create_source(src_id).duplicates++;
}

void StatsManager::record_crc_fail(uint16_t src_id) {
    std::unique_lock lock(mutex_);
    global_.crc_fail_total++;
    // CRC failures also count as malformed
    get_or_create_source(src_id).malformed++;
}

GlobalStats StatsManager::get_global_stats() const {
    std::shared_lock lock(mutex_);
    return global_;
}

SourceStats StatsManager::get_source_stats(uint16_t src_id) const {
    std::shared_lock lock(mutex_);
    auto it = sources_.find(src_id);
    if (it == sources_.end())
        return SourceStats{};
    return it->second;
}

std::vector<SourceStats> StatsManager::get_all_source_stats() const {
    std::shared_lock lock(mutex_);
    std::vector<SourceStats> result;
    result.reserve(sources_.size());
    for (const auto& [id, s] : sources_)
        result.push_back(s);
    return result;
}

HealthState StatsManager::get_health() const {
    std::shared_lock lock(mutex_);
    if (global_.malformed_total > 0 || global_.crc_fail_total > 0)
        return HealthState::ERROR;
    if (global_.gap_total > 0 || global_.reorder_total > 0)
        return HealthState::DEGRADED;
    return HealthState::OK;
}

void StatsManager::reset() {
    std::unique_lock lock(mutex_);
    global_ = GlobalStats{};
    sources_.clear();
}

} // namespace nng
