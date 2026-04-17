/**
 * @file stress_patterns.hh
 * @brief TrafficGenTLM 6种压力模式策略接口及实现
 * 
 * 提供 6 种地址生成策略：SEQUENTIAL / RANDOM / HOTSPOT / STRIDED / NEIGHBOR / TORNADO
 * 用于 TLM 流量压力测试，模拟真实片上网络通信模式
 * 
 * @author CppTLM Development Team
 * @date 2026-04-16
 */

#ifndef TLM_STRESS_PATTERNS_HH
#define TLM_STRESS_PATTERNS_HH

#include <cstdint>
#include <vector>
#include <random>
#include <memory>
#include <numeric>

// 压力模式枚举
enum class StressPattern {
    SEQUENTIAL,   // 线性递增
    RANDOM,       // 均匀随机
    HOTSPOT,      // 80% 流量集中 20% 地址
    STRIDED,      // 固定步长（模拟缓存行冲突）
    NEIGHBOR,     // 局部性访问（80% 相邻，20% 随机）
    TORNADO       // 对角流量（mesh NoC）
};

// 压力模式策略基类
class StressPatternStrategy {
public:
    virtual ~StressPatternStrategy() = default;
    virtual uint64_t next_address(uint64_t base, uint64_t range) = 0;
    virtual void reset() = 0;
};

// SequentialStrategy — 线性递增，回绕
class SequentialStrategy : public StressPatternStrategy {
public:
    inline uint64_t next_address(uint64_t base, uint64_t range) override;
    inline void reset() override;
private:
    uint64_t current_ = 0;
};

// RandomStrategy — 均匀随机
class RandomStrategy : public StressPatternStrategy {
public:
    inline uint64_t next_address(uint64_t base, uint64_t range) override;
    inline void reset() override;
private:
    std::mt19937_64 rng_{42};
    std::uniform_int_distribution<uint64_t> dist_{0, 0};  // 通过 param() 动态更新 range
};

// HotspotStrategy — 权重离散分布
class HotspotStrategy : public StressPatternStrategy {
public:
    inline void set_hotspot_config(const std::vector<uint64_t>& addrs,
                             const std::vector<double>& weights);
    inline uint64_t next_address(uint64_t base, uint64_t range) override;
    inline void reset() override;
private:
    std::vector<uint64_t> hotspot_addrs_;
    std::discrete_distribution<size_t> hotspot_dist_;
    std::mt19937_64 rng_{42};
};

// StridedStrategy — 固定步长
class StridedStrategy : public StressPatternStrategy {
public:
    inline void set_stride(uint64_t s) { stride_ = s; }
    inline uint64_t next_address(uint64_t base, uint64_t range) override;
    inline void reset() override;
private:
    uint64_t stride_ = 64;
    uint64_t offset_ = 0;
};

// NeighborStrategy — 80% 相邻 + 20% 随机
class NeighborStrategy : public StressPatternStrategy {
public:
    inline uint64_t next_address(uint64_t base, uint64_t range) override;
    inline void reset() override;
private:
    uint64_t last_addr_ = 0;
    std::mt19937_64 rng_{42};
    std::uniform_int_distribution<int> pct_dist_{0, 99};
    std::uniform_int_distribution<uint64_t> random_dist_{0, 1000000};
    std::uniform_int_distribution<uint64_t> fallback_dist_{0, 0};  // 20% 随机分支的 distribution
};

// TornadoStrategy — mesh 对角流量
class TornadoStrategy : public StressPatternStrategy {
public:
    inline void set_mesh_config(uint64_t w, uint64_t h) { mesh_width_ = w; mesh_height_ = h; }
    inline uint64_t next_address(uint64_t base, uint64_t range) override;
    inline void reset() override;
private:
    uint64_t mesh_width_ = 4;
    uint64_t mesh_height_ = 4;
    uint64_t current_node_ = 0;
    uint64_t step_ = 0;
};

// 工厂函数
inline std::unique_ptr<StressPatternStrategy> create_strategy(StressPattern pattern);

// ==================== 实现 ====================

// SequentialStrategy 实现
inline uint64_t SequentialStrategy::next_address(uint64_t base, uint64_t range) {
    if (range == 0) return base;
    uint64_t ret = base + current_;
    current_ = (current_ + 1) % range;
    return ret;
}

inline void SequentialStrategy::reset() {
    current_ = 0;
}

// RandomStrategy 实现
inline uint64_t RandomStrategy::next_address(uint64_t base, uint64_t range) {
    if (range == 0) return base;
    dist_.param(std::uniform_int_distribution<uint64_t>::param_type(0, range - 1));
    return base + dist_(rng_);
}

inline void RandomStrategy::reset() {
    rng_.seed(42);
}

// HotspotStrategy 实现
inline void HotspotStrategy::set_hotspot_config(const std::vector<uint64_t>& addrs,
                                           const std::vector<double>& weights) {
    hotspot_addrs_ = addrs;
    hotspot_dist_ = std::discrete_distribution<size_t>(weights.begin(), weights.end());
}

inline uint64_t HotspotStrategy::next_address(uint64_t /*base*/, uint64_t /*range*/) {
    if (hotspot_addrs_.empty()) return 0;
    return hotspot_addrs_[hotspot_dist_(rng_)];
}

inline void HotspotStrategy::reset() {
    rng_.seed(42);
}

// StridedStrategy 实现
inline uint64_t StridedStrategy::next_address(uint64_t base, uint64_t range) {
    if (range == 0) return base;
    uint64_t ret = base + offset_;
    offset_ = (offset_ + stride_) % range;
    return ret;
}

inline void StridedStrategy::reset() {
    offset_ = 0;
}

// NeighborStrategy 实现
inline uint64_t NeighborStrategy::next_address(uint64_t base, uint64_t range) {
    if (range == 0) return base;
    if (range <= 1) return base;
    if (pct_dist_(rng_) < 80) {
        uint64_t delta = random_dist_(rng_) % 2 == 0 ? 1 : (uint64_t)-1;
        uint64_t next = last_addr_;
        if (delta == 1 && last_addr_ - base < range - 1) {
            next = last_addr_ + 1;
        } else if (delta == (uint64_t)-1 && last_addr_ > base) {
            next = last_addr_ - 1;
        } else if (last_addr_ - base >= range - 1) {
            next = base;
        } else {
            next = last_addr_ + 1;
        }
        last_addr_ = next;
        return last_addr_;
    }
    fallback_dist_.param(std::uniform_int_distribution<uint64_t>::param_type(0, range - 1));
    last_addr_ = base + fallback_dist_(rng_);
    return last_addr_;
}

inline void NeighborStrategy::reset() {
    last_addr_ = 0;
    rng_.seed(42);
}

// TornadoStrategy 实现 — mesh 对角 tornado 模式
// 遍历所有节点，每轮按对角线顺序访问
// range 超出 mesh 节点数时自动 wrap（取模），确保地址不越界
inline uint64_t TornadoStrategy::next_address(uint64_t base, uint64_t range) {
    uint64_t total = mesh_width_ * mesh_height_;
    uint64_t diagonal_step = step_ % mesh_width_;  // 0,1,2,3,0,1,2,3,...
    uint64_t base_node = (step_ / mesh_width_) % mesh_height_;  // 起始行
    
    current_node_ = (base_node * mesh_width_ + ((base_node + diagonal_step) % mesh_width_)) % total;
    step_++;
    
    // range 校验：total 可能大于 range，此时取模确保地址在 [base, base+range) 内
    if (range > 0 && current_node_ >= range) {
        current_node_ = current_node_ % range;
    }
    return base + current_node_;
}

inline void TornadoStrategy::reset() {
    current_node_ = 0;
    step_ = 0;
}

// 工厂函数实现
inline std::unique_ptr<StressPatternStrategy> create_strategy(StressPattern pattern) {
    switch (pattern) {
        case StressPattern::SEQUENTIAL:
            return std::make_unique<SequentialStrategy>();
        case StressPattern::RANDOM:
            return std::make_unique<RandomStrategy>();
        case StressPattern::HOTSPOT:
            return std::make_unique<HotspotStrategy>();
        case StressPattern::STRIDED:
            return std::make_unique<StridedStrategy>();
        case StressPattern::NEIGHBOR:
            return std::make_unique<NeighborStrategy>();
        case StressPattern::TORNADO:
            return std::make_unique<TornadoStrategy>();
    }
    return std::make_unique<SequentialStrategy>();
}

#endif // TLM_STRESS_PATTERNS_HH