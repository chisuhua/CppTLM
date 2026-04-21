/**
 * @file mock_stats_manager.hh
 * @brief Mock StatsManager — 用于 Phase 5 早期开发测试
 *
 * 提供与真实 StatsManager 兼容的 API，但不依赖全局单例。
 * 用于在没有完整 StatsManager 单例的情况下测试框架逻辑。
 *
 * 设计原则：
 * - 与 StatsManager API 完全兼容
 * - 零外部依赖
 * - 可独立测试框架逻辑
 *
 * @author CppTLM Development Team
 * @date 2026-04-18
 */

#ifndef CPPTLM_TEST_MOCK_STATS_MANAGER_HH
#define CPPTLM_TEST_MOCK_STATS_MANAGER_HH

#include "metrics/stats.hh"
#include <map>
#include <mutex>
#include <string>

namespace tlm_stats_test {

// ============================================================================
// MockStatsManager — 与 StatsManager API 兼容的 Mock 实现
// ============================================================================

class MockStatsManager {
public:
    // 单例访问（Mock 版本）
    static MockStatsManager& instance() {
        static MockStatsManager manager;
        return manager;
    }

    // =========================================================================
    // 注册与注销
    // =========================================================================

    void register_group(StatGroup* group, const std::string& path) {
        if (!group || path.empty()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        groups_[path] = group;
    }

    void unregister_group(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        groups_.erase(path);
    }

    // =========================================================================
    // 查找
    // =========================================================================

    StatGroup* find_group(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = groups_.find(path);
        return (it != groups_.end()) ? it->second : nullptr;
    }

    // =========================================================================
    // 输出
    // =========================================================================

    void dump_all(std::ostream& os, int width = 50) const {
        std::lock_guard<std::mutex> lock(mutex_);
        os << "---------- Begin Simulation Statistics (Mock) ----------\n";
        for (const auto& kv : groups_) {
            if (kv.second) {
                kv.second->dump(os, kv.first, -1);  // -1 means path is already complete
            }
        }
        os << "---------- End Simulation Statistics (Mock) ----------\n";
    }

    std::string dump_text(int width = 50) const {
        std::ostringstream oss;
        dump_all(oss, width);
        return oss.str();
    }

    // =========================================================================
    // 重置
    // =========================================================================

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

    size_t num_groups() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return groups_.empty();
    }

    const std::map<std::string, StatGroup*>& groups() const {
        return groups_;
    }

    // =========================================================================
    // Mock 专用方法
    // =========================================================================

    /// 重置所有状态（测试用）
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        groups_.clear();
    }

private:
    MockStatsManager() = default;
    ~MockStatsManager() = default;

    MockStatsManager(const MockStatsManager&) = delete;
    MockStatsManager& operator=(const MockStatsManager&) = delete;
    MockStatsManager(MockStatsManager&&) = delete;
    MockStatsManager& operator=(MockStatsManager&&) = delete;

    mutable std::mutex mutex_;
    std::map<std::string, StatGroup*> groups_;
};

} // namespace tlm_stats_test

#endif // CPPTLM_TEST_MOCK_STATS_MANAGER_HH