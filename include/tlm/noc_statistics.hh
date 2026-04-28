/**
 * @file noc_statistics.hh
 * @brief NoCStatistics — NoC 性能统计收集器
 *
 * 功能描述：
 * - 记录 packet 发送、flit 转发、拥塞事件
 * - 聚合计算平均延迟、平均跳数、吞吐率
 * - 使用 tlm_stats::StatGroup 层次化统计框架
 *
 * @author CppTLM Development Team
 * @date 2026-04-27
 */

#ifndef CPPTLM_NOC_STATISTICS_HH
#define CPPTLM_NOC_STATISTICS_HH

#include "metrics/stats.hh"
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>

class NoCStatistics {
public:
    NoCStatistics();

    void record_packet_sent(uint32_t src, uint32_t dst, uint64_t latency, uint64_t hops);
    void record_flit_forwarded(uint32_t router_id);
    void record_congestion(uint32_t router_id, unsigned port);

    double avg_latency() const;
    double avg_hops() const;
    double throughput_flits_per_cycle() const;
    uint64_t total_packets() const;
    uint64_t total_flits() const;
    uint64_t total_congestion_events() const;

    void set_cycle_count(uint64_t cycles);
    void reset();
    void dump(std::ostream& os) const;

private:
    struct PerRouterStats {
        tlm_stats::Scalar* flits_forwarded = nullptr;
        tlm_stats::Scalar* congestion_events = nullptr;
    };

    PerRouterStats& get_or_create_router_stats(uint32_t router_id);

    tlm_stats::StatGroup root_;

    tlm_stats::Scalar& total_packets_;
    tlm_stats::Distribution& latency_dist_;
    tlm_stats::Distribution& hops_dist_;

    tlm_stats::Scalar& total_flits_;
    tlm_stats::Scalar& cycle_count_;

    tlm_stats::Scalar& congestion_events_;

    std::unordered_map<uint32_t, PerRouterStats> router_stats_;
};

inline NoCStatistics::NoCStatistics()
    : root_("noc_stats"),
      total_packets_(root_.addScalar("packets", "Total packets sent", "count")),
      latency_dist_(root_.addDistribution("latency", "Packet latency", "cycle")),
      hops_dist_(root_.addDistribution("hops", "Hop count distribution", "hop")),
      total_flits_(root_.addScalar("flits", "Total flits forwarded", "flits")),
      cycle_count_(root_.addScalar("cycles", "Total simulation cycles", "cycle")),
      congestion_events_(root_.addScalar("congestion_events", "Total congestion events", "count")) {
}

inline NoCStatistics::PerRouterStats& NoCStatistics::get_or_create_router_stats(uint32_t router_id) {
    auto it = router_stats_.find(router_id);
    if (it != router_stats_.end()) {
        return it->second;
    }

    auto& per_router = router_stats_[router_id];
    per_router.flits_forwarded = &root_.addStat(
        "router_" + std::to_string(router_id) + "_flits",
        new tlm_stats::Scalar("Flits forwarded through router " + std::to_string(router_id), "flits"));
    per_router.congestion_events = &root_.addStat(
        "router_" + std::to_string(router_id) + "_congestion",
        new tlm_stats::Scalar("Congestion events on router " + std::to_string(router_id), "count"));
    return per_router;
}

inline void NoCStatistics::record_packet_sent(uint32_t /*src*/, uint32_t /*dst*/,
                                              uint64_t latency, uint64_t hops) {
    ++total_packets_;
    latency_dist_.sample(latency);
    hops_dist_.sample(hops);
}

inline void NoCStatistics::record_flit_forwarded(uint32_t router_id) {
    ++total_flits_;
    auto& per_router = get_or_create_router_stats(router_id);
    ++(*per_router.flits_forwarded);
}

inline void NoCStatistics::record_congestion(uint32_t router_id, unsigned /*port*/) {
    ++congestion_events_;
    auto& per_router = get_or_create_router_stats(router_id);
    ++(*per_router.congestion_events);
}

inline double NoCStatistics::avg_latency() const {
    return latency_dist_.mean();
}

inline double NoCStatistics::avg_hops() const {
    return hops_dist_.mean();
}

inline double NoCStatistics::throughput_flits_per_cycle() const {
    uint64_t cycles = cycle_count_.value();
    if (cycles == 0) return 0.0;
    return static_cast<double>(total_flits_.value()) / static_cast<double>(cycles);
}

inline uint64_t NoCStatistics::total_packets() const {
    return total_packets_.value();
}

inline uint64_t NoCStatistics::total_flits() const {
    return total_flits_.value();
}

inline uint64_t NoCStatistics::total_congestion_events() const {
    return congestion_events_.value();
}

inline void NoCStatistics::set_cycle_count(uint64_t cycles) {
    cycle_count_.reset();
    cycle_count_ += cycles;
}

inline void NoCStatistics::reset() {
    root_.reset();
}

inline void NoCStatistics::dump(std::ostream& os) const {
    root_.dump(os);
}

#endif // CPPTLM_NOC_STATISTICS_HH