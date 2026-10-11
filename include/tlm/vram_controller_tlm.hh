// include/tlm/vram_controller_tlm.hh
// VramControllerTLM: VRAM 控制器 (继承 MemoryTLM + 行缓冲 + bandwidth 上限)
// Phase 4 cpptlm-dgpu-soc-timing-mvp T4 (per design.md §5 + ADR-DGPU-07 D3)
//   - 继承 MemoryTLM: 复用 pending_resps_ / current_cycle_ / stats_ (per M10)
//   - 覆盖 row_hit(): 真实行缓冲 (4K row) vs MemoryTLM 简化模型
//   - bandwidth 上限: 128B req → 累加 wait cycles
//   - on_config_loaded(): 读 vram_size_bytes
//
// ── 时序模型 (Change D D.4 文档化, 2026-10-08) ──────────────────────────
//   - 假定操作频率: 1 GHz (per dgpu-realip-pin-contract Change D D.4)
//   - `bandwidth_gbps_` 字段语义实为 bytes/cycle (而非 Gbps) — 命名沿用
//     design.md §5.2 历史用法避免破坏 ABI
//   - 真实带宽换算: bandwidth_GBps = bandwidth_gbps_ * 1 GHz = bandwidth_gbps_ GB/s
//     (例: 默认 32 → 32 GB/s 上限, 注入 128 → 128 GB/s 上限)
//   - row_hit_cycles_ / row_miss_cycles_ 字段语义为 cycle 数 (latency),
//     直接作为父类 MemoryTLM read_latency_{hit,miss}_cycles_ 注入 (per M1)
//   - 仿真时间到 wall-clock 换算: 1 sim_cycle = 1 ns @ 1 GHz 假设
//   - 假定变更触发点: DGpuBoard::init() 注入 vram_params (per bind_memory_backings)
// 作者: CppTLM Team · 日期: 2027-02-11
#ifndef TLM_VRAM_CONTROLLER_TLM_HH
#define TLM_VRAM_CONTROLLER_TLM_HH

#include "tlm/memory_tlm.hh"
#include <cerrno>
#include <cstdint>
#include <cstring>

// A.4 (cpptlm-driver-visible-minimal-soc 2026-10-08): forward-decl
// tlm::cluster::GpuInternalFabric instead of #include to avoid circular
// include with tlm/cluster/gpu_internal_fabric.hh which itself needs
// VramControllerTLM pointer. Forward-decl in the outer tlm::cluster
// namespace keeps the qualifier tlm::cluster::GpuInternalFabric usable
// for the setter signature below.
namespace tlm::cluster {
class GpuInternalFabric;
}

// ── Change D D.4: 操作频率常量 (2026-10-08 文档化) ─────────────────────────
namespace tlm::vram {
// 假定操作频率 (1 GHz)。变更此处必须同步更新 dgpu-realip-pin-contract D.4 docs
// 与 timing-mode.md §2.4;实际芯片 HBM PHY 频率由 platform config 注入 (D3 seam)。
constexpr uint64_t kOperatingFrequencyHz = 1'000'000'000ULL;  // 1 GHz
constexpr uint64_t kSimCycleNs = 1;                            // 1 sim_cycle = 1 ns @ 1 GHz

// ── Change A (cpptlm-vram-controller-functional-integration, 2026-10-09) ──
// VramController 工作模式 (per docs/designs/dgpu-soc/architecture.md §0.1 原则 4 +
// ADR-DGPU-11 H2 互斥约束):
//   FunctionalZeroDelay     — functional-mode: zero-delay, 协议真实; 同步
//                             handle_functional_request() 走控制器协议路径, latency=0
//   TimingCycleApproximate  — timing-mode: cycle-approximate + bandwidth accounting;
//                             既有 ChStream tick() 路径 (默认, backward compat)
enum class VramControllerMode : uint8_t {
    FunctionalZeroDelay    = 0,  // functional-mode (minimal_v1)
    TimingCycleApproximate = 1,  // timing-mode (dgpu_soc_timing_v1)
};
} // namespace tlm::vram

class VramControllerTLM : public MemoryTLM {
public:
    explicit VramControllerTLM(const std::string& name, EventQueue* eq)
        : MemoryTLM(name, eq),
          stats_row_hits_(stats_.addScalar("vram_row_hits", "VRAM row buffer hits", "count")),
          stats_row_misses_(stats_.addScalar("vram_row_misses", "VRAM row buffer misses", "count")),
          stats_bandwidth_limit_waits_(
              stats_.addScalar("bandwidth_limit_waits", "Cycles waiting for bandwidth", "count")) {}

    ~VramControllerTLM() override = default;

    std::string get_module_type() const override { return "VramControllerTLM"; }

    // ── Change A (cpptlm-vram-controller-functional-integration, 2026-10-09): 工作模式 API ──
    // 默认 TimingCycleApproximate 保证 backward compat (既有 timing-mode 行为 100% 不变).
    // functional-mode 由 DGpuBoard::bind_memory_backings() 依 simulation_mode 自动设置.
    void set_mode(tlm::vram::VramControllerMode m) noexcept { mode_ = m; }
    tlm::vram::VramControllerMode mode() const noexcept { return mode_; }

    // ── Change A T4 (2026-10-09): functional-mode 同步入口 ──
    // per design.md A-B1 (模仿 gem5 AbstractMemory::functionalAccess()):
    // 同步服务 API, 仅 functional-mode 走; timing-mode 仍走既有 ChStream tick() 路径.
    // 范围检查 + 字节级 memcpy 到 backing (zero-delay latency=0).
    struct FunctionalResult {
        int rc;                  // 0=OK, -EINVAL/-ENODEV=SLVERR 源
        uint64_t latency_cycles; // functional=0; timing 计算字段 (保留)
    };
    FunctionalResult handle_functional_request(uint64_t addr, uint32_t size,
                                               bool is_write,
                                               const void* data_ptr) noexcept {
        // functional-mode zero-delay; timing-mode 也走此同步入口时返回计算 latency
        const uint64_t latency =
            (mode_ == tlm::vram::VramControllerMode::FunctionalZeroDelay)
                ? 0
                : (size + bandwidth_gbps_ - 1) / bandwidth_gbps_;
        if (backing_view_ == nullptr || backing_view_size_ == 0) {
            return {-ENODEV, latency};
        }
        if (size == 0 || addr >= backing_view_size_ ||
            static_cast<uint64_t>(size) > backing_view_size_ - addr) {
            return {-EINVAL, latency};
        }
        if (data_ptr != nullptr) {
            if (is_write) {
                std::memcpy(backing_view_ + addr, data_ptr, size);
            } else {
                std::memcpy(const_cast<void*>(data_ptr), backing_view_ + addr, size);
            }
        }
        return {0, latency};
    }

    // ── Phase 4 T4: 行缓冲 + bandwidth 参数 setter (per design.md §5.1) ──
    void set_vram_params(uint64_t row_hit_cycles, uint64_t row_miss_cycles,
                         uint64_t bandwidth_gbps) noexcept {
        row_hit_cycles_ = row_hit_cycles;
        row_miss_cycles_ = row_miss_cycles;
        bandwidth_gbps_ = bandwidth_gbps;
        // 同步父类门限延迟 (per design §5.2): MemoryTLM::tick() 按 row_hit() 返回值
        // 选 read_latency_hit_/miss_cyces_, 让 VRAM 行缓冲参数生效
        read_latency_hit_cycles_ = row_hit_cycles;
        read_latency_miss_cycles_ = row_miss_cycles;
    }

    // ── A.4 (cpptlm-driver-visible-minimal-soc 2026-10-08): programmatic bridge source ──
    // per Phase 8 HB/RC precedent + cpptlm-p2-integration-unblock D7.
    // Momus 修订: 不加 ChStreamTargetPort<Bundle> 成员(类型不存在),改 setter-only 模式。
    void set_fabric_source(tlm::cluster::GpuInternalFabric* src) noexcept { fabric_ = src; }
    tlm::cluster::GpuInternalFabric* fabric() const noexcept { return fabric_; }

    // ── 行缓冲 (per design.md §5.2): 单 row buffer, 4K row ──
    void invalidate_row_buffer() noexcept { row_buffer_valid_ = false; }

    // 覆盖 MemoryTLM::row_hit (virtual, T4.1): 使用 VRAM 真实行缓冲
    // 当前请求处理中 (processing_req_) 返回该请求的预计算命中决策 (per design §5.2:
    //   miss 请求必须先判定再更新 row buffer, 父类 tick() 才能选对 read_latency_miss_cycles_);
    // 否则返回实时状态 (供外部测试断言)
    bool row_hit(uint64_t addr) const noexcept override {
        if (processing_req_) return pending_req_row_hit_;
        if (!row_buffer_valid_) return false;
        constexpr uint64_t kRowSize = 4096;
        return (addr & ~(kRowSize - 1)) == row_buffer_base_;
    }

    // ── 统计访问器 (测试 + 报告) ──
    uint64_t vram_row_hits() const noexcept { return vram_row_hits_; }
    uint64_t vram_row_misses() const noexcept { return vram_row_misses_; }
    uint64_t bandwidth_limit_waits() const noexcept { return bandwidth_limit_waits_; }

    // on_config_loaded: 读 vram_size_bytes (per ADR-DGPU-05 单一真源)
    void on_config_loaded() override {
        const auto& cfg = get_config();
        if (cfg.contains("vram_size_bytes") && cfg["vram_size_bytes"].is_number()) {
            set_size_bytes(cfg["vram_size_bytes"].get<uint64_t>());
        }
        MemoryTLM::on_config_loaded();
    }

    // tick(): 行缓冲 hit/miss + bandwidth 累加 (per design.md §5.2)
    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();
            const uint64_t addr = req.address.read();

            // 1. 行缓冲 hit/miss 判定 (预计算, 父类 tick() 经 row_hit() 读取)
            constexpr uint64_t kRowSize = 4096;
            const bool hit = row_buffer_valid_ && (addr & ~(kRowSize - 1)) == row_buffer_base_;
            processing_req_ = true;
            pending_req_row_hit_ = hit;
            if (hit) {
                ++stats_row_hits_;
                ++vram_row_hits_;
            } else {
                ++stats_row_misses_;
                ++vram_row_misses_;
                row_buffer_base_ = addr & ~(kRowSize - 1);
                row_buffer_valid_ = true;
            }

            // 2. bandwidth 限制: cycles_needed = ceil(bytes / bandwidth_gbps_)
            //    简化: 超过 1 cyc 的 req 累加 wait (per design §5.2, D4 加专门调度)
            const uint64_t bytes_this_req = req.size.read();
            const uint64_t cycles_needed = (bytes_this_req + bandwidth_gbps_ - 1) / bandwidth_gbps_;
            if (cycles_needed > 1) {
                ++stats_bandwidth_limit_waits_;
                ++bandwidth_limit_waits_;
            }
        }

        // 复用父类 pending_resps_ 处理 (per M1 priority_queue)
        MemoryTLM::tick();
        processing_req_ = false;
    }

    void do_reset(const ResetConfig& config) override {
        MemoryTLM::do_reset(config);
        row_buffer_base_ = 0;
        row_buffer_valid_ = false;
        processing_req_ = false;
        pending_req_row_hit_ = false;
        vram_row_hits_ = 0;
        vram_row_misses_ = 0;
        bandwidth_limit_waits_ = 0;
    }

private:
    uint64_t row_buffer_base_ = 0;
    bool row_buffer_valid_ = false;

    // 当前请求处理中的命中决策 (per design §5.2: miss 先判定再更新 buffer)
    bool processing_req_ = false;
    bool pending_req_row_hit_ = false;

    uint64_t row_hit_cycles_ = 30;     // per tasks.md T4 (design 默认 100, T5 JSON 注入 30)
    uint64_t row_miss_cycles_ = 80;    // per tasks.md T4
    uint64_t bandwidth_gbps_ = 32;     // 32 GB/s 默认上限

    // A.4: cached programmatic bridge source (non-owning)
    tlm::cluster::GpuInternalFabric* fabric_ = nullptr;

    // Change A: 工作模式 (默认 timing-mode, backward compat per H-B2)
    tlm::vram::VramControllerMode mode_ = tlm::vram::VramControllerMode::TimingCycleApproximate;

    // 额外 stats (继承自 MemoryTLM::stats_, 附加标量)
    tlm_stats::Scalar& stats_row_hits_;
    tlm_stats::Scalar& stats_row_misses_;
    tlm_stats::Scalar& stats_bandwidth_limit_waits_;
    // 简单计数 (测试断言用, 不经 tlm_stats 便于直接读)
    uint64_t vram_row_hits_ = 0;
    uint64_t vram_row_misses_ = 0;
    uint64_t bandwidth_limit_waits_ = 0;
};

#endif // TLM_VRAM_CONTROLLER_TLM_HH
