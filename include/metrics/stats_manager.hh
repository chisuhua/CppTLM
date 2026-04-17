/**
 * @file stats_manager.hh
 * @brief StatsManager 单例 — 全局统计管理中枢
 *
 * 提供全局唯一的统计管理器，负责：
 * - 以层次路径注册 StatGroup（"system.cache.latency"）
 * - 按路径查找统计组
 * - 统一 dump/reset 所有已注册统计
 *
 * 设计原则：
 * - 单例模式（C++11 magic static 保证线程安全）
 * - 零外部依赖（仅使用 C++11 标准库 + mutex）
 *
 * @author CppTLM Development Team
 * @date 2026-04-17
 */

#ifndef CPPTLM_METRICS_STATS_MANAGER_HH
#define CPPTLM_METRICS_STATS_MANAGER_HH

#include "metrics/stats.hh"
#include <map>
#include <mutex>
#include <sstream>
#include <string>

namespace tlm_stats {

// ============================================================================
// StatsManager — 单例模式全局统计中枢
// ============================================================================

class StatsManager {
public:
    // =========================================================================
    // 单例访问 — C++11 magic static 保证线程安全
    // =========================================================================
    static StatsManager& instance() {
        static StatsManager manager;
        return manager;
    }

    // =========================================================================
    // 注册与注销
    // =========================================================================

    /**
     * @brief 注册一个 StatGroup 到指定路径
     * @param group StatGroup 指针（外部拥有，StatsManager 不负责释放）
     * @param path 层次路径，如 "system.cache" 或 "system.memory.latency"
     */
    void register_group(StatGroup* group, const std::string& path) {
        if (!group || path.empty()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        groups_[path] = group;
    }

    /**
     * @brief 注销指定路径的 StatGroup
     * @param path 层次路径
     */
    void unregister_group(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        groups_.erase(path);
    }

    // =========================================================================
    // 查找
    // =========================================================================

    /**
     * @brief 按路径查找已注册的 StatGroup
     * @param path 层次路径
     * @return 找到返回指针，否则返回 nullptr
     */
    StatGroup* find_group(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = groups_.find(path);
        return (it != groups_.end()) ? it->second : nullptr;
    }

    // =========================================================================
    // 输出
    // =========================================================================

    /**
     * @brief 将所有已注册统计输出到流
     * @param os 输出流
     * @param width 字段宽度（默认 50）
     */
    void dump_all(std::ostream& os, int width = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        os << "---------- Begin Simulation Statistics ----------\n";
        for (const auto& kv : groups_) {
            if (kv.second) {
                kv.second->dump(os, kv.first, width);
            }
        }
        os << "---------- End Simulation Statistics ----------\n";
    }

    /**
     * @brief 生成 Text 格式字符串
     * @param width 字段宽度（默认 50）
     * @return 格式化文本
     */
    std::string dump_text(int width = 50) const {
        std::ostringstream oss;
        dump_all(oss, width);
        return oss.str();
    }

    // =========================================================================
    // 重置
    // =========================================================================

    /// 重置所有已注册的统计
    void reset_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& kv : groups_) {
            if (kv.second) {
                kv.second->reset();
            }
        }
    }

    // =========================================================================
    // 查询
    // =========================================================================

    /// 获取已注册组数量
    size_t num_groups() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.size();
    }

    /// 判断是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.empty();
    }

    // =====================================================================
    // 内部访问（供 MetricsReporter 使用）
    // =====================================================================

    /// 获取所有已注册组的引用（const，只读）
    const std::map<std::string, StatGroup*>& groups() const {
        return groups_;
    }

private:
    // 私有构造函数（单例）
    StatsManager() = default;

    // 禁止拷贝和移动
    StatsManager(const StatsManager&) = delete;
    StatsManager& operator=(const StatsManager&) = delete;
    StatsManager(StatsManager&&) = delete;
    StatsManager& operator=(StatsManager&&) = delete;

    mutable std::mutex mutex_;
    std::map<std::string, StatGroup*> groups_;  // path → StatGroup*
};

} // namespace tlm_stats

#endif // CPPTLM_METRICS_STATS_MANAGER_HH
