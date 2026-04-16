/**
 * @file stats.hh
 * @brief 核心统计类型实现 — Phase 8 性能指标收集框架
 * 
 * 提供四种基础统计类型：
 * - Scalar: 简单计数器，支持原子操作
 * - Average: 时间加权平均
 * - Distribution: 分布统计 (min/max/mean/stddev)
 * - StatGroup: 层次化统计组管理
 * 
 * 设计原则：
 * - 零外部依赖（仅使用 C++17 标准库）
 * - 在线聚合（不存储原始样本，防止内存爆炸）
 * - 线程安全（Scalar 使用原子操作）
 * 
 * @author CppTLM Development Team
 * @date 2026-04-15
 */

#ifndef CPPTLM_METRICS_STATS_HH
#define CPPTLM_METRICS_STATS_HH

#include <atomic>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace tlm_stats {

// ============================================================================
// 类型别名
// ============================================================================

using Counter = uint64_t;
using Result = double;

// ============================================================================
// StatBase — 所有统计类型的抽象基类
// ============================================================================

class StatBase {
public:
    virtual ~StatBase() = default;
    
    virtual void reset() = 0;
    virtual void dump(std::ostream& os, const std::string& path, int width) const = 0;
    virtual std::string unit() const = 0;
    virtual std::string description() const = 0;
};

// ============================================================================
// Scalar — 简单计数器
// ============================================================================

class Scalar : public StatBase {
public:
    explicit Scalar(const std::string& desc = "", const std::string& u = "count")
        : description_(desc), unit_(u) {}
    
    // 后置自增
    Scalar& operator++() {
        value_.fetch_add(1, std::memory_order_relaxed);
        return *this;
    }
    
    // 前置自增
    Counter operator++(int) {
        Counter old = value_.fetch_add(1, std::memory_order_relaxed);
        return old;
    }
    
    // 累加
    Scalar& operator+=(Counter delta) {
        value_.fetch_add(delta, std::memory_order_relaxed);
        return *this;
    }
    
    // 读取当前值
    Counter value() const {
        return value_.load(std::memory_order_relaxed);
    }
    
    void reset() override {
        value_.store(0, std::memory_order_relaxed);
    }
    
    void dump(std::ostream& os, const std::string& path, int width) const override {
        os << std::left << std::setw(width) << path 
           << std::right << std::setw(15) << value()
           << "  # " << description_ << " (" << unit_ << ")\n";
    }
    
    std::string unit() const override { return unit_; }
    std::string description() const override { return description_; }

private:
    std::atomic<Counter> value_{0};
    std::string description_;
    std::string unit_;
};

// ============================================================================
// Average — 时间加权平均
// ============================================================================

class Average : public StatBase {
public:
    Average(const std::string& desc = "", const std::string& u = "count")
        : description_(desc), unit_(u) {}
    
    /**
     * @brief 采样一个值及其持续时间
     * @param value 当前值
     * @param cycles 持续周期数
     * 
     * 时间加权平均 = Σ(value_i × duration_i) / Σ(duration_i)
     */
    void sample(double value, Counter cycles = 1) {
        if (cycles == 0) return;
        
        sum_.fetch_add(static_cast<Counter>(value * cycles), std::memory_order_relaxed);
        total_cycles_.fetch_add(cycles, std::memory_order_relaxed);
    }
    
    /**
     * @brief 获取时间加权平均值
     * @return sum / total_cycles，如果 total_cycles == 0 返回 0
     */
    Result result() const {
        Counter tc = total_cycles_.load(std::memory_order_relaxed);
        if (tc == 0) return 0.0;
        
        Counter s = sum_.load(std::memory_order_relaxed);
        return static_cast<Result>(s) / static_cast<Result>(tc);
    }
    
    void reset() override {
        sum_.store(0, std::memory_order_relaxed);
        total_cycles_.store(0, std::memory_order_relaxed);
    }
    
    void dump(std::ostream& os, const std::string& path, int width) const override {
        os << std::left << std::setw(width) << path 
           << std::right << std::setw(15) << std::fixed << std::setprecision(3) << result()
           << "  # " << description_ << " (" << unit_ << ")\n";
    }
    
    std::string unit() const override { return unit_; }
    std::string description() const override { return description_; }
    
    Counter total_cycles() const { return total_cycles_.load(std::memory_order_relaxed); }

private:
    std::atomic<Counter> sum_{0};           // Σ(value × cycles)
    std::atomic<Counter> total_cycles_{0};  // Σ(cycles)
    std::string description_;
    std::string unit_;
};

// ============================================================================
// Distribution — 分布统计
// ============================================================================

class Distribution : public StatBase {
public:
    Distribution(const std::string& desc = "", const std::string& u = "cycle")
        : description_(desc), unit_(u) {}
    
    /**
     * @brief 采样一个值
     * @param value 采样的值
     * 
     * 更新 min/max/sum/sum_sq/count
     * 使用 Kahan 求和提高精度（仅限非并发场景；
     * 多线程下 sample() 为近似精度，无保证）
     */
    void sample(Counter value) {
        count_.fetch_add(1, std::memory_order_relaxed);
        
        // Update min (CAS spin-lock 保证线程安全)
        Counter current_min = min_.load(std::memory_order_relaxed);
        while (value < current_min) {
            if (min_.compare_exchange_weak(current_min, value, std::memory_order_relaxed))
                break;
        }
        
        // Update max (CAS spin-lock 保证线程安全)
        Counter current_max = max_.load(std::memory_order_relaxed);
        while (value > current_max) {
            if (max_.compare_exchange_weak(current_max, value, std::memory_order_relaxed))
                break;
        }
        
        // Kahan 求和修正版：sum_ 和 sum_sq_ 均为 atomic<double>
        double val = static_cast<double>(value);
        
        // sum += val with Kahan compensation
        double old_sum = sum_.load(std::memory_order_relaxed);
        double corrected_sum = val - correction_.load(std::memory_order_relaxed);
        double new_sum = old_sum + corrected_sum;
        correction_.store((new_sum - old_sum) - corrected_sum, std::memory_order_relaxed);
        sum_.store(new_sum, std::memory_order_relaxed);
        
        // sum_sq += val*val with Kahan compensation
        double val_sq = val * val;
        double old_sum_sq = sum_sq_.load(std::memory_order_relaxed);
        double corrected_sq = val_sq - correction_sq_.load(std::memory_order_relaxed);
        double new_sum_sq = old_sum_sq + corrected_sq;
        correction_sq_.store((new_sum_sq - old_sum_sq) - corrected_sq, std::memory_order_relaxed);
        sum_sq_.store(new_sum_sq, std::memory_order_relaxed);
    }
    
    Counter count() const { return count_.load(std::memory_order_relaxed); }
    
    Counter min() const {
        Counter c = count_.load(std::memory_order_relaxed);
        return c == 0 ? 0 : min_.load(std::memory_order_relaxed);
    }
    
    Counter max() const {
        Counter c = count_.load(std::memory_order_relaxed);
        return c == 0 ? 0 : max_.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief 获取平均值
     * @return sum / count，如果 count == 0 返回 0
     */
    Result mean() const {
        Counter c = count_.load(std::memory_order_relaxed);
        if (c == 0) return 0.0;
        return sum_.load(std::memory_order_relaxed) / static_cast<Result>(c);
    }
    
    /**
     * @brief 获取标准差
     * @return sqrt(E[X²] - E[X]²)，如果 count < 2 返回 0
     */
    Result stddev() const {
        Counter c = count_.load(std::memory_order_relaxed);
        if (c < 2) return 0.0;
        
        Result m = mean();
        Result e_x2 = sum_sq_.load(std::memory_order_relaxed) / static_cast<Result>(c);
        Result variance = e_x2 - m * m;
        
        // 防止负数（由于浮点精度）
        if (variance < 0) variance = 0;
        return std::sqrt(variance);
    }
    
    void reset() override {
        count_.store(0, std::memory_order_relaxed);
        min_.store(UINT64_MAX, std::memory_order_relaxed);
        max_.store(0, std::memory_order_relaxed);
        sum_.store(0.0, std::memory_order_relaxed);
        sum_sq_.store(0.0, std::memory_order_relaxed);
        correction_.store(0.0, std::memory_order_relaxed);
        correction_sq_.store(0.0, std::memory_order_relaxed);
    }
    
    void dump(std::ostream& os, const std::string& path, int width) const override {
        // 输出多个子指标
        std::string base = path;
        
        // .count
        os << std::left << std::setw(width) << base + ".count"
           << std::right << std::setw(15) << count()
           << "  # " << description_ << " sample count (" << unit_ << ")\n";
        
        // .min
        os << std::left << std::setw(width) << base + ".min"
           << std::right << std::setw(15) << min()
           << "  # " << description_ << " minimum (" << unit_ << ")\n";
        
        // .avg
        os << std::left << std::setw(width) << base + ".avg"
           << std::right << std::setw(15) << std::fixed << std::setprecision(3) << mean()
           << "  # " << description_ << " average (" << unit_ << ")\n";
        
        // .max
        os << std::left << std::setw(width) << base + ".max"
           << std::right << std::setw(15) << max()
           << "  # " << description_ << " maximum (" << unit_ << ")\n";
        
        // .stddev
        os << std::left << std::setw(width) << base + ".stddev"
           << std::right << std::setw(15) << std::fixed << std::setprecision(3) << stddev()
           << "  # " << description_ << " std deviation (" << unit_ << ")\n";
    }
    
    std::string unit() const override { return unit_; }
    std::string description() const override { return description_; }

private:
    std::atomic<Counter> count_{0};
    std::atomic<Counter> min_{UINT64_MAX};
    std::atomic<Counter> max_{0};
    std::atomic<double> sum_{0.0};       // double 存储以支持 Kahan 求和
    std::atomic<double> sum_sq_{0.0};    // 避免 uint64_t 溢出 (sum_sq 可超过 2^64)
    std::atomic<double> correction_{0};   // Kahan 校正项
    std::atomic<double> correction_sq_{0};
    std::string description_;
    std::string unit_;
};

// ============================================================================
// StatGroup — 层次化统计组
// ============================================================================

// 前向声明
class Formula;

class StatGroup : public StatBase {
public:
    explicit StatGroup(const std::string& name, StatGroup* parent = nullptr)
        : name_(name), parent_(parent) {}
    
    const std::string& name() const { return name_; }
    StatGroup* parent() const { return parent_; }
    
    // 添加子统计
    template<typename StatType>
    StatType& addStat(const std::string& stat_name, StatType* stat) {
        auto result = stats_.emplace(stat_name, std::unique_ptr<StatBase>(stat));
        return *static_cast<StatType*>(result.first->second.get());
    }
    
    // 添加 Scalar
    Scalar& addScalar(const std::string& name, const std::string& desc = "", 
                       const std::string& unit = "count") {
        return addStat(name, new Scalar(desc, unit));
    }
    
    // 添加 Average
    Average& addAverage(const std::string& name, const std::string& desc = "",
                        const std::string& unit = "count") {
        return addStat(name, new Average(desc, unit));
    }
    
    // 添加 Distribution
    Distribution& addDistribution(const std::string& name, const std::string& desc = "",
                                  const std::string& unit = "cycle") {
        return addStat(name, new Distribution(desc, unit));
    }
    
    // 添加 Formula（计算型指标）— 实现定义在后
    Formula& addFormula(const std::string& name, const std::string& desc,
                        const std::string& unit, std::function<Result()> calc);
    
    // 添加子组
    StatGroup& addSubgroup(const std::string& sub_name) {
        auto it = subgroups_.find(sub_name);
        if (it != subgroups_.end()) {
            return *static_cast<StatGroup*>(it->second.get());
        }
        auto subgroup = std::make_unique<StatGroup>(sub_name);
        subgroup->parent_ = this;
        auto result = subgroups_.emplace(sub_name, std::move(subgroup));
        return *result.first->second;
    }
    
    StatGroup* addSubgroup(StatGroup* subgroup) {
        if (!subgroup) return nullptr;
        
        subgroup->parent_ = this;
        auto it = subgroups_.find(subgroup->name_);
        if (it != subgroups_.end()) {
            // 已存在同名子组，不插入（避免 double-free）
            return it->second.get();
        }
        subgroups_.emplace(subgroup->name_, std::unique_ptr<StatGroup>(subgroup));
        return subgroup;
    }
    
    // 查找子组（支持路径语法，如 "system.cache"）
    StatGroup* findSubgroup(const std::string& path) {
        if (path.empty()) return this;
        
        size_t dot = path.find('.');
        std::string first = (dot == std::string::npos) ? path : path.substr(0, dot);
        std::string rest = (dot == std::string::npos) ? "" : path.substr(dot + 1);
        
        auto it = subgroups_.find(first);
        if (it == subgroups_.end()) return nullptr;
        
        if (rest.empty()) return it->second.get();
        
        return it->second->findSubgroup(rest);
    }
    
    // 查找统计
    StatBase* findStat(const std::string& path) {
        size_t dot = path.rfind('.');
        if (dot == std::string::npos) {
            auto it = stats_.find(path);
            return (it != stats_.end()) ? it->second.get() : nullptr;
        }
        
        StatGroup* subgroup = findSubgroup(path.substr(0, dot));
        if (!subgroup) return nullptr;
        
        return subgroup->findStat(path.substr(dot + 1));
    }
    
    // 获取完整路径
    std::string fullPath() const {
        if (!parent_) return name_;
        return parent_->fullPath() + "." + name_;
    }
    
    void reset() override {
        for (auto& [name, stat] : stats_) {
            stat->reset();
        }
        for (auto& [name, subgroup] : subgroups_) {
            subgroup->reset();
        }
    }
    
    void dump(std::ostream& os, const std::string& path = "", int width = 50) const {
        std::string full;
        if (path.empty()) {
            full = name_;
        } else {
            full = path + "." + name_;
        }
        
        // 输出自己的统计
        for (const auto& [stat_name, stat] : stats_) {
            stat->dump(os, full + "." + stat_name, width);
        }
        
        // 递归输出子组
        for (const auto& [subgroup_name, subgroup] : subgroups_) {
            subgroup->dump(os, full, width);
        }
    }
    
    std::string unit() const override { return ""; }
    std::string description() const override { return ""; }
    
    // 遍历子组
    auto& subgroups() const { return subgroups_; }
    auto& stats() const { return stats_; }

private:
    std::string name_;
    StatGroup* parent_;
    std::map<std::string, std::unique_ptr<StatBase>> stats_;
    std::map<std::string, std::unique_ptr<StatGroup>> subgroups_;
};

// ============================================================================
// Formula — 计算型指标
// ============================================================================

class Formula : public StatBase {
public:
    using CalcFunc = std::function<Result()>;
    
    Formula(const std::string& name, const std::string& desc,
             const std::string& unit, CalcFunc calc)
        : StatBase(), name_(name), description_(desc), unit_(unit), calc_(calc) {}
    
    Result value() const { return calc_ ? calc_() : 0.0; }
    
    void reset() override {
        // Formula 通常不存储状态，无需重置
    }
    
    void dump(std::ostream& os, const std::string& path, int width) const override {
        os << std::left << std::setw(width) << path 
           << std::right << std::setw(15) << std::fixed << std::setprecision(6) << value()
           << "  # " << description_ << " (" << unit_ << ")\n";
    }
    
    std::string unit() const override { return unit_; }
    std::string description() const override { return description_; }

private:
    std::string name_;
    std::string description_;
    std::string unit_;
    CalcFunc calc_;
};

// ============================================================================
// StatGroup::addFormula — 必须在 Formula 完全定义后
// ============================================================================

inline Formula& StatGroup::addFormula(const std::string& name, const std::string& desc,
                                       const std::string& unit, std::function<Result()> calc) {
    return addStat(name, new Formula(name, desc, unit, calc));
}

} // namespace tlm_stats

#endif // CPPTLM_METRICS_STATS_HH
