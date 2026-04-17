/**
 * @file histogram.hh
 * @brief PercentileHistogram — 指数桶百分位直方图
 * 
 * @author CppTLM Development Team
 * @date 2026-04-16
 */

#ifndef CPPTLM_METRICS_HISTOGRAM_HH
#define CPPTLM_METRICS_HISTOGRAM_HH

#include "metrics/stats.hh"
#include <algorithm>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace tlm_stats {

class PercentileHistogram : public StatBase {
public:
    explicit PercentileHistogram(const std::string& desc = "",
                                  const std::string& unit = "cycle")
        : description_(desc), unit_(unit) {
        static_assert(NUM_BUCKETS == 32, "Requires exactly 32 buckets");
    }

    void record(int64_t value) {
        if (value <= 0) return;

        int bucket_idx = value_to_bucket(value);
        buckets_[bucket_idx].fetch_add(1, std::memory_order_relaxed);
        total_count_.fetch_add(1, std::memory_order_relaxed);

        int64_t current_min = min_value_.load(std::memory_order_relaxed);
        while (value < current_min) {
            if (min_value_.compare_exchange_weak(current_min, value,
                                                  std::memory_order_relaxed))
                break;
        }

        int64_t current_max = max_value_.load(std::memory_order_relaxed);
        while (value > current_max) {
            if (max_value_.compare_exchange_weak(current_max, value,
                                                  std::memory_order_relaxed))
                break;
        }
    }

    int64_t percentile(double p) const {
        if (p < 0.0) p = 0.0;
        if (p > 100.0) p = 100.0;

        Counter total = total_count_.load(std::memory_order_relaxed);
        if (total == 0) return 0;

        double target = (p / 100.0) * static_cast<double>(total);
        Counter cumulative = 0;

        for (int i = 0; i < NUM_BUCKETS; ++i) {
            Counter bucket_count = buckets_[i].load(std::memory_order_relaxed);
            if (cumulative + bucket_count >= target) {
                double position = bucket_count > 0
                    ? (target - cumulative) / static_cast<double>(bucket_count)
                    : 0.0;
                int64_t lo = bucket_lower_bound(i);
                int64_t hi = bucket_upper_bound(i);
                int64_t result = lo + static_cast<int64_t>(
                    position * static_cast<double>(hi - lo));
                return std::clamp(result, lo, hi);
            }
            cumulative += bucket_count;
        }
        return max_value_.load(std::memory_order_relaxed);
    }

    int64_t p50() const { return percentile(50.0); }
    int64_t p95() const { return percentile(95.0); }
    int64_t p99() const { return percentile(99.0); }
    int64_t p99_9() const { return percentile(99.9); }

    void reset() override {
        for (int i = 0; i < NUM_BUCKETS; ++i) {
            buckets_[i].store(0, std::memory_order_relaxed);
        }
        total_count_.store(0, std::memory_order_relaxed);
        min_value_.store(INT64_MAX, std::memory_order_relaxed);
        max_value_.store(0, std::memory_order_relaxed);
    }

    void dump(std::ostream& os, const std::string& path, int width) const override {
        os << std::left << std::setw(width) << path + ".count"
           << std::right << std::setw(15) << total_count()
           << "  # " << description_ << " sample count (" << unit_ << ")\n";
        os << std::left << std::setw(width) << path + ".min"
           << std::right << std::setw(15) << min_value()
           << "  # " << description_ << " minimum (" << unit_ << ")\n";
        os << std::left << std::setw(width) << path + ".p50"
           << std::right << std::setw(15) << p50()
           << "  # " << description_ << " 50th percentile (" << unit_ << ")\n";
        os << std::left << std::setw(width) << path + ".p95"
           << std::right << std::setw(15) << p95()
           << "  # " << description_ << " 95th percentile (" << unit_ << ")\n";
        os << std::left << std::setw(width) << path + ".p99"
           << std::right << std::setw(15) << p99()
           << "  # " << description_ << " 99th percentile (" << unit_ << ")\n";
        os << std::left << std::setw(width) << path + ".p99.9"
           << std::right << std::setw(15) << p99_9()
           << "  # " << description_ << " 99.9th percentile (" << unit_ << ")\n";
        os << std::left << std::setw(width) << path + ".max"
           << std::right << std::setw(15) << max_value()
           << "  # " << description_ << " maximum (" << unit_ << ")\n";
    }

    std::string unit() const override { return unit_; }
    std::string description() const override { return description_; }

    Counter total_count() const { return total_count_.load(std::memory_order_relaxed); }

    int64_t min_value() const {
        Counter c = total_count_.load(std::memory_order_relaxed);
        return (c == 0) ? 0 : min_value_.load(std::memory_order_relaxed);
    }

    int64_t max_value() const {
        Counter c = total_count_.load(std::memory_order_relaxed);
        return (c == 0) ? 0 : max_value_.load(std::memory_order_relaxed);
    }

    size_t memory_usage() const { return NUM_BUCKETS * sizeof(Counter); }

private:
    static constexpr int NUM_BUCKETS = 32;

    int value_to_bucket(int64_t value) const {
        if (value <= 0) return 0;
        uint64_t v = static_cast<uint64_t>(value);
#if defined(__GNUC__) || defined(__clang__)
        int bit = 63 - __builtin_clzll(v);
#else
        // MSVC / 其他编译器：逐位查找最高有效位
        int bit = 0;
        uint64_t x = v;
        if (x > 0xFFFFFFFFULL) { bit += 32; x >>= 32; }
        if (x > 0xFFFFULL)     { bit += 16; x >>= 16; }
        if (x > 0xFFULL)       { bit += 8;  x >>= 8;  }
        if (x > 0xFULL)         { bit += 4;  x >>= 4;  }
        if (x > 0x3ULL)        { bit += 2;  x >>= 2;  }
        if (x > 0x1ULL)        { bit += 1; }
#endif
        if (bit < 0) bit = 0;
        if (bit >= NUM_BUCKETS) bit = NUM_BUCKETS - 1;
        return bit;
    }

    static int64_t bucket_lower_bound(int i) {
        if (i == 0) return 1;
        return static_cast<int64_t>(1ULL << i);
    }

    static int64_t bucket_upper_bound(int i) {
        if (i < 31) return static_cast<int64_t>(1ULL << (i + 1));
        return static_cast<int64_t>(1ULL << 32);
    }

    std::atomic<Counter> buckets_[NUM_BUCKETS] = {};
    std::atomic<Counter> total_count_{0};
    std::atomic<int64_t> min_value_{INT64_MAX};
    std::atomic<int64_t> max_value_{0};
    std::string description_;
    std::string unit_;
};

inline PercentileHistogram& StatGroup::addPercentileHistogram(const std::string& name,
                                                              const std::string& desc,
                                                              const std::string& unit) {
    return addStat(name, new PercentileHistogram(desc, unit));
}

} // namespace tlm_stats

#endif // CPPTLM_METRICS_HISTOGRAM_HH
