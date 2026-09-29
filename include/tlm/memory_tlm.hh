// include/tlm/memory_tlm.hh
// MemoryTLM：基于 ch_stream 语义的简化 Memory 模块（v2.1 新式模型）
// 功能描述：作为 CacheTLM 下游模块，接收请求后返回模拟响应
//           用于验证 StreamAdapter 完整数据通路
//           v2.2: 新增 backing-store 注入 + 零时 memcpy 路径
//           (per openspec/changes/cpptlm-minimal-dgpu-soc-v1 A1)
//           v0.3 (Phase 4 cpptlm-dgpu-soc-timing-mvp T1): cycle-approximate 扩展
//             - defer_response() priority_queue (per M1 + 七轮 P0-3 落盘)
//             - use_zero_delay_for_test_ 默认 true 保 functional 零回归 (per M5)
//             - T4.0 protected 化 (req_in_/resp_out_/stats_/backing_*/size_cap_)
//               + friend class VramControllerTLM (per M10)
// 作者 CppTLM Team
// 日期 2026-04-12 (v2.2: 2027-02-09, v0.3 timing: 2027-02-10)
#ifndef TLM_MEMORY_TLM_HH
#define TLM_MEMORY_TLM_HH

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>
#include <cstring>
#include <queue>
#include <vector>

class MemoryTLM : public ChStreamModuleBase {
    // T4.0 (per M10): VramControllerTLM 需要访问 protected 成员 (继承 + friend 双保险)
    friend class VramControllerTLM;
protected:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // backing-store 注入 (v2.2, per spec/memory-tlm-backing-store)
    // nullptr = legacy 路径 (0xDEADBEEF + latency 采样); 非空 = 零时 memcpy 路径
    // ADR-DGPU-10: backing_ptr_ → backing_view_ (injected-ChStream)
    uint8_t* backing_view_ = nullptr;
    uint64_t backing_view_size_ = 0;
    uint64_t size_cap_ = 0;  // set_size_bytes 上限 (0 = 用 backing_view_size_)

    // 性能统计
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_read_;
    tlm_stats::Scalar& stats_requests_write_;
    tlm_stats::Scalar& stats_row_hits_;
    tlm_stats::Scalar& stats_row_misses_;
    tlm_stats::Distribution& stats_latency_read_;
    tlm_stats::Distribution& stats_latency_write_;
    tlm_stats::Formula& stats_row_buffer_hit_rate_;

    // ── v0.3 timing-mode 扩展 (per design.md §2) ──
    // PendingResp 延迟响应条目, 按 ready_cycle 升序 (priority_queue per M1)
    struct PendingResp {
        bundles::CacheRespBundle resp;
        uint64_t ready_cycle;  // resp 可发出时间 = current_cycle_ + latency
        bool operator>(const PendingResp& o) const noexcept {
            return ready_cycle > o.ready_cycle;
        }
    };
    // priority_queue (std::greater → operator>), 顶部必为 ready_cycle 最小者
    std::priority_queue<PendingResp, std::vector<PendingResp>, std::greater<PendingResp>>
        pending_resps_;

    // cycle 跟踪 (board 注入, 不自行 advance per TInv-3)
    uint64_t current_cycle_ = 0;

    // 5 个时序参数 (design.md §2.2)
    uint64_t read_latency_hit_cycles_  = 100;
    uint64_t read_latency_miss_cycles_ = 200;
    uint64_t write_latency_cycles_     = 120;
    bool     use_zero_delay_for_test_  = true;  // 默认 true 保 functional 零回归 (per M5)

public:
    MemoryTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq), stats_("memory"),
          stats_requests_read_(stats_.addScalar("requests_read", "Memory read requests", "count")),
          stats_requests_write_(
              stats_.addScalar("requests_write", "Memory write requests", "count")),
          stats_row_hits_(stats_.addScalar("row_hits", "Row buffer hits", "count")),
          stats_row_misses_(stats_.addScalar("row_misses", "Row buffer misses", "count")),
          stats_latency_read_(
              stats_.addDistribution("latency_read", "Memory read latency", "cycle")),
          stats_latency_write_(
              stats_.addDistribution("latency_write", "Memory write latency", "cycle")),
          stats_row_buffer_hit_rate_(
              stats_.addFormula("row_buffer_hit_rate", "Row buffer hit rate", "ratio", [this]() {
                  auto hits = stats_row_hits_.value();
                  auto misses = stats_row_misses_.value();
                  return (hits + misses) > 0 ? static_cast<double>(hits) / (hits + misses) : 0.0;
              })) {
    }

    ~MemoryTLM() override = default;

    std::string get_module_type() const override {
        return "MemoryTLM";
    }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // ChStreamModuleBase 统计接口
    tlm_stats::StatGroup* get_stats_group() override {
        return &stats_;
    }
    std::string get_stats_path() const override {
        return "system.memory";
    }

    // v2.2: backing-store API (per spec/memory-tlm-backing-store)
    // backing 已注入: tick() 走零时 memcpy 路径
    // backing == nullptr: 保留 v2.1 legacy 行为 (0xDEADBEEF + latency 采样)
    // ADR-DGPU-10: 新命名 set_backing_view (旧 set_backing_store 保留 deprecation wrapper)
    void set_backing_view(uint8_t* ptr, uint64_t size_bytes) noexcept {
        backing_view_ = ptr;
        backing_view_size_ = size_bytes;
        if (size_cap_ == 0) size_cap_ = size_bytes;
    }
    [[deprecated("use set_backing_view")]]
    void set_backing_store(uint8_t* ptr, uint64_t size_bytes) noexcept {
        set_backing_view(ptr, size_bytes);
    }
    void set_size_bytes(uint64_t sz) noexcept { size_cap_ = sz; }
    uint8_t* host_addr(uint64_t offset) const noexcept {
        return backing_view_ ? (backing_view_ + offset) : nullptr;
    }
    uint64_t backing_size() const noexcept { return backing_view_size_; }
    bool has_backing() const noexcept { return backing_view_ != nullptr; }

    // ── v0.3 timing-mode API (per design.md §2.1, DGpuBoard::tick 调用) ──
    // cycle advance: 由 DGpuBoard 统一推进 (TInv-3), 模块不可自行 ++cycle
    void advance_cycle() noexcept { ++current_cycle_; }
    uint64_t current_cycle() const noexcept { return current_cycle_; }

    // 时序参数 setter (DGpuBoard::init_timing_mode 调用)
    void set_timing_params(uint64_t read_hit, uint64_t read_miss, uint64_t write,
                           bool use_zero_delay) noexcept {
        read_latency_hit_cycles_  = read_hit;
        read_latency_miss_cycles_ = read_miss;
        write_latency_cycles_     = write;
        use_zero_delay_for_test_  = use_zero_delay;
    }

    // 显式门控 zero-delay (per M5): functional 默认 true; timing-mode 必须显式置 false
    void set_use_zero_delay_for_test(bool b) noexcept { use_zero_delay_for_test_ = b; }
    bool use_zero_delay_for_test() const noexcept { return use_zero_delay_for_test_; }

    // 暴露 pending 队列深度 (测试 + 统计)
    uint64_t inflight_resp_count() const noexcept {
        return static_cast<uint64_t>(pending_resps_.size());
    }

    // 4K row 简化命中模型 (v0.3): 与既有 v2.1 legacy 路径 (addr & 0xF000) == 0 一致;
    // VramControllerTLM 覆盖此方法使用真实行缓冲 (T4.1: virtual per design.md §5.1)
    virtual bool row_hit(uint64_t addr) const noexcept {
        return (addr & 0xF000ULL) == 0;
    }

    // D-AXI v1.5 Phase 0.3 (B18): 真实接线 cfg 中的 capacity_gb
    // 消除 v1.4 §13 "1GB cap 不存在" 谎言 (Oracle/Metis 三轮敌对审查发现)
    void on_config_loaded() override {
        const auto& cfg = get_config();
        if (cfg.contains("capacity_gb") && cfg["capacity_gb"].is_number()) {
            const uint64_t gb = cfg["capacity_gb"].get<uint64_t>();
            set_size_bytes(gb * (1ULL << 30));
        }
    }

    void tick() override {
        // ── v0.3 阶段 1: 处理新 req (按 arrival 顺序) ──
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();
            bool is_write = req.is_write.read();
            uint64_t addr = req.address.read();
            uint64_t tid = req.transaction_id.read();
            uint64_t sz = req.size.read();

            bundles::CacheRespBundle resp;
            resp.transaction_id.write(tid);

            if (use_zero_delay_for_test_) {
                // ── functional-mode v2.2 既有路径 (字节级零 diff, per M5) ──
                if (backing_view_ != nullptr) {
                    // backing 零时路径 (per spec/memory-tlm-backing-store)
                    const uint64_t cap = size_cap_ ? size_cap_ : backing_view_size_;
                    if (addr + sz > cap) {
                        resp.error_code.write(1);  // OUT_OF_RANGE
                        resp.is_hit.write(0);
                        resp.data.write(0);
                    } else if (is_write) {
                        uint64_t val = req.data.read();
                        std::memcpy(backing_view_ + addr, &val, std::min<size_t>(sz, 8));
                        resp.error_code.write(0);
                        resp.is_hit.write(1);
                        resp.data.write(0);
                        ++stats_requests_write_;
                    } else {
                        uint64_t val = 0;
                        std::memcpy(&val, backing_view_ + addr, std::min<size_t>(sz, 8));
                        resp.error_code.write(0);
                        resp.is_hit.write(1);
                        resp.data.write(val);
                        ++stats_requests_read_;
                    }
                    resp_out_.write(resp);
                    req_in_.consume();
                } else {
                    // v2.1 legacy 路径 (逐字节保留)
                    if (is_write) {
                        ++stats_requests_write_;
                        stats_latency_write_.sample(120);
                    } else {
                        ++stats_requests_read_;
                        stats_latency_read_.sample(100);
                    }
                    bool row_hit = (addr & 0xF000) == 0;
                    if (row_hit) ++stats_row_hits_;
                    else ++stats_row_misses_;
                    resp.data.write(0xDEADBEEF);
                    resp.is_hit.write(row_hit ? 1 : 0);
                    resp.error_code.write(0);
                    resp_out_.write(resp);
                    req_in_.consume();
                }
            } else {
                // ── timing-mode defer_response 路径 (per design.md §2.3) ──
                uint64_t latency = 0;
                if (backing_view_ != nullptr) {
                    const uint64_t cap = size_cap_ ? size_cap_ : backing_view_size_;
                    if (addr + sz > cap) {
                        resp.error_code.write(1);  // OUT_OF_RANGE
                        resp.is_hit.write(0);
                        resp.data.write(0);
                        latency = 1;  // 错误立即 (per design §2.3 + spec Scenario)
                    } else if (is_write) {
                        // 写: 立即写 backing (TInv-2 强 ordering), resp 延迟发
                        uint64_t val = req.data.read();
                        std::memcpy(backing_view_ + addr, &val, std::min<size_t>(sz, 8));
                        resp.error_code.write(0);
                        resp.is_hit.write(1);
                        resp.data.write(0);
                        latency = write_latency_cycles_;
                        ++stats_requests_write_;
                    } else {
                        uint64_t val = 0;
                        std::memcpy(&val, backing_view_ + addr, std::min<size_t>(sz, 8));
                        resp.error_code.write(0);
                        resp.is_hit.write(1);
                        resp.data.write(val);
                        latency = row_hit(addr) ? read_latency_hit_cycles_
                                                : read_latency_miss_cycles_;
                        ++stats_requests_read_;
                    }
                } else {
                    // legacy 路径 (兼容 functional 测试): 仅延迟, 不计数
                    latency = is_write ? write_latency_cycles_
                                       : (row_hit(addr) ? read_latency_hit_cycles_
                                                        : read_latency_miss_cycles_);
                }

                // sample latency stats (always 启用)
                if (is_write) stats_latency_write_.sample(latency);
                else          stats_latency_read_.sample(latency);

                // schedule to pending_resps_ (priority_queue per M1)
                pending_resps_.push({resp, current_cycle_ + latency});
                req_in_.consume();
            }
        }

        // ── v0.3 阶段 2: drain pending_resps_ (top 必为 ready_cycle 最小者) ──
        while (!pending_resps_.empty() && pending_resps_.top().ready_cycle <= current_cycle_) {
            resp_out_.write(pending_resps_.top().resp);
            pending_resps_.pop();
        }

        if (adapter_)
            adapter_->tick();
    }

    void do_reset(const ResetConfig& /*config*/) override {
        req_in_.reset();
        resp_out_.reset();
        stats_.reset();
        // v0.3: 清空 pending 队列 + 重置 cycle (per tasks.md T1.1)
        while (!pending_resps_.empty()) pending_resps_.pop();
        current_cycle_ = 0;
    }

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        return req_in_;
    }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        return resp_out_;
    }
    // P0-5b: 被动响应模块,无 req 输出/resp 输入。返回静态 dummy 供 StreamAdapter 统一调用。
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() {
        static cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() {
        static cpptlm::InputStreamAdapter<bundles::CacheRespBundle> dummy;
        return dummy;
    }
    cpptlm::StreamAdapterBase* get_adapter() const {
        return adapter_;
    }

    // 统计访问器
    tlm_stats::StatGroup& stats() {
        return stats_;
    }
    const tlm_stats::StatGroup& stats() const {
        return stats_;
    }

    void dumpStats(std::ostream& os) const {
        stats_.dump(os);
    }
};

#endif // TLM_MEMORY_TLM_HH
