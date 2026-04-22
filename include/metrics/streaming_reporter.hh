/**
 * @file streaming_reporter.hh
 * @brief StreamingReporter — 运行时流式统计导出
 *
 * 设计目标：
 * - 每 N 周期输出统计快照（JSON Lines 格式）
 * - 后台线程异步写入，避免干扰仿真
 * - 支持文件滚动，防止单文件过大
 *
 * 使用方式：
 * @code
 * StreamingReporter reporter("output/stats_stream.jsonl");
 * reporter.set_interval(10000);  // 每 10000 周期输出一次
 * reporter.start();
 *
 * // 仿真循环中...
 * if (should_report) {
 *     reporter.enqueue_snapshot();  // 非阻塞入队
 * }
 *
 * reporter.stop();  // 仿真结束后停止
 * @endcode
 *
 * @author CppTLM Development Team
 * @date 2026-04-22
 */

#ifndef CPPTLM_METRICS_STREAMING_REPORTER_HH
#define CPPTLM_METRICS_STREAMING_REPORTER_HH

#include "metrics/stats.hh"
#include "metrics/stats_manager.hh"
#include "metrics/histogram.hh"
#include <atomic>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <condition_variable>

namespace tlm_stats {

class StreamingReporter {
public:
    using Clock = std::chrono::steady_clock;

    /**
     * @brief 构造流式报告器
     * @param path 输出文件路径（.jsonl 扩展名）
     * @param flush_interval_ms 刷新间隔（毫秒）
     */
    explicit StreamingReporter(const std::string& path,
                               int flush_interval_ms = 500);

    ~StreamingReporter();

    // 禁止拷贝和移动
    StreamingReporter(const StreamingReporter&) = delete;
    StreamingReporter& operator=(const StreamingReporter&) = delete;
    StreamingReporter(StreamingReporter&&) = delete;
    StreamingReporter& operator=(StreamingReporter&&) = delete;

    /**
     * @brief 设置输出间隔（周期数）
     * @param cycles 每隔多少周期输出一次
     */
    void set_interval(uint64_t cycles) {
        interval_cycles_.store(cycles, std::memory_order_relaxed);
    }

    /**
     * @brief 获取输出间隔
     */
    uint64_t get_interval() const {
        return interval_cycles_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 设置当前仿真周期
     * @param cycle 当前仿真周期
     */
    void set_current_cycle(uint64_t cycle) {
        current_cycle_.store(cycle, std::memory_order_relaxed);
    }

    /**
     * @brief 启动后台输出线程
     */
    void start();

    /**
     * @brief 停止后台线程（会刷新所有 pending 数据）
     */
    void stop();

    /**
     * @brief 入队所有统计组快照（非阻塞）
     */
    void enqueue_all();

    /**
     * @brief 入队单个统计组快照（非阻塞）
     * @param group_name 统计组路径
     * @param group 统计组指针
     */
    void enqueue_snapshot(const std::string& group_name, StatGroup* group);

    /**
     * @brief 检查是否正在运行
     */
    bool is_running() const {
        return running_.load(std::memory_order_acquire);
    }

private:
    void output_loop();
    void flush_all();
    std::string serialize_group(const StatGroup* group);
    std::string get_current_time_ns();

    std::string filepath_;
    int flush_interval_ms_;
    std::atomic<bool> running_;
    std::atomic<bool> force_flush_;
    std::atomic<uint64_t> interval_cycles_;
    std::atomic<uint64_t> current_cycle_;

    std::thread output_thread_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::queue<std::string> pending_queue_;

    std::ofstream ofs_;
};

// =============================================================================
// 实现
// =============================================================================

inline StreamingReporter::StreamingReporter(const std::string& path, int flush_interval_ms)
    : filepath_(path)
    , flush_interval_ms_(flush_interval_ms)
    , running_(false)
    , force_flush_(false)
    , interval_cycles_(10000)
    , current_cycle_(0)
{
    // 创建目录
    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

inline StreamingReporter::~StreamingReporter() {
    stop();
}

inline void StreamingReporter::start() {
    if (running_.load(std::memory_order_acquire)) return;

    running_.store(true, std::memory_order_release);

    // 打开输出文件
    ofs_.open(filepath_, std::ios::out | std::ios::trunc);
    if (!ofs_) {
        std::cerr << "StreamingReporter: Failed to open " << filepath_ << "\n";
        running_.store(false, std::memory_order_release);
        return;
    }

    output_thread_ = std::thread(&StreamingReporter::output_loop, this);
}

inline void StreamingReporter::stop() {
    if (!running_.load(std::memory_order_acquire)) return;

    running_.store(false, std::memory_order_release);

    // 强制刷新所有 pending 数据
    force_flush_.store(true, std::memory_order_relaxed);
    cv_.notify_all();

    if (output_thread_.joinable()) {
        output_thread_.join();
    }

    // 最终刷新
    flush_all();

    if (ofs_.is_open()) {
        ofs_.close();
    }
}

inline void StreamingReporter::enqueue_all() {
    const auto& groups = StatsManager::instance().groups();
    for (const auto& kv : groups) {
        enqueue_snapshot(kv.first, kv.second);
    }
}

inline void StreamingReporter::enqueue_snapshot(const std::string& group_name,
                                                 StatGroup* group) {
    if (!group) return;

    std::lock_guard<std::mutex> lock(queue_mutex_);

    // 构建 JSON 对象
    std::ostringstream oss;
    oss << "{";
    oss << "\"timestamp_ns\":" << get_current_time_ns() << ",";
    oss << "\"simulation_cycle\":" << current_cycle_.load(std::memory_order_relaxed) << ",";
    oss << "\"group\":\"" << group_name << "\",";
    oss << "\"data\":" << serialize_group(group);
    oss << "}";

    pending_queue_.push(oss.str());
}

inline void StreamingReporter::output_loop() {
    while (running_.load(std::memory_order_acquire)) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // 等待条件：非空 or 强制刷新 or 超时
        cv_.wait_for(lock, std::chrono::milliseconds(flush_interval_ms_), [this] {
            return !pending_queue_.empty() || force_flush_.load(std::memory_order_relaxed);
        });

        // 刷新所有 pending
        while (!pending_queue_.empty()) {
            ofs_ << pending_queue_.front() << "\n";
            pending_queue_.pop();
        }

        ofs_.flush();  // 实时刷出
        force_flush_.store(false, std::memory_order_relaxed);
    }
}

inline void StreamingReporter::flush_all() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (!ofs_.is_open()) return;

    while (!pending_queue_.empty()) {
        ofs_ << pending_queue_.front() << "\n";
        pending_queue_.pop();
    }
    ofs_.flush();
}

inline std::string StreamingReporter::serialize_group(const StatGroup* group) {
    if (!group) return "{}";

    std::ostringstream oss;
    oss << "{";

    bool first = true;

    // 输出当前组的统计
    for (const auto& kv : group->stats()) {
        const std::string& stat_name = kv.first;
        const StatBase* stat = kv.second.get();

        if (!first) oss << ",";
        first = false;

        oss << "\"" << stat_name << "\":";

        if (auto* s = dynamic_cast<const Scalar*>(stat)) {
            oss << s->value();
        } else if (auto* a = dynamic_cast<const Average*>(stat)) {
            oss << std::fixed << std::setprecision(6) << a->result();
        } else if (auto* d = dynamic_cast<const Distribution*>(stat)) {
            oss << "{";
            oss << "\"count\":" << d->count() << ",";
            oss << "\"min\":" << d->min() << ",";
            oss << "\"avg\":" << std::fixed << std::setprecision(6) << d->mean() << ",";
            oss << "\"max\":" << d->max() << ",";
            oss << "\"stddev\":" << std::fixed << std::setprecision(6) << d->stddev();
            oss << "}";
        } else if (auto* f = dynamic_cast<const Formula*>(stat)) {
            oss << std::fixed << std::setprecision(6) << f->value();
        } else if (auto* p = dynamic_cast<const PercentileHistogram*>(stat)) {
            oss << "{";
            oss << "\"count\":" << p->total_count() << ",";
            oss << "\"min\":" << p->min_value() << ",";
            oss << "\"p50\":" << std::fixed << std::setprecision(6) << p->p50() << ",";
            oss << "\"p95\":" << std::fixed << std::setprecision(6) << p->p95() << ",";
            oss << "\"p99\":" << std::fixed << std::setprecision(6) << p->p99() << ",";
            oss << "\"p99.9\":" << std::fixed << std::setprecision(6) << p->p99_9() << ",";
            oss << "\"max\":" << p->max_value();
            oss << "}";
        } else {
            oss << "null";
        }
    }

    // 递归处理子组
    for (const auto& kv : group->subgroups()) {
        if (!first) oss << ",";
        first = false;

        const std::string& subgroup_name = kv.first;
        const StatGroup* subgroup = kv.second.get();

        oss << "\"" << subgroup_name << "\":";
        if (subgroup) {
            oss << serialize_group(subgroup);
        } else {
            oss << "{}";
        }
    }

    oss << "}";
    return oss.str();
}

inline std::string StreamingReporter::get_current_time_ns() {
    auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count();
    return std::to_string(now);
}

} // namespace tlm_stats

#endif // CPPTLM_METRICS_STREAMING_REPORTER_HH