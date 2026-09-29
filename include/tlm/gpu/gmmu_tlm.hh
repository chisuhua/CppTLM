// include/tlm/gpu/gmmu_tlm.hh
// GmmuTLM: 一级页表翻译 (per openspec/changes/cpptlm-driver-visible-minimal-soc v1.8)
// 镜像 gem5 PhysicalMemory/AbstractMemory 角色: SDMA 通过 translate_cb 注入,
// backing 是同一份 framebuffer_, 直读直写页表项。
//
// Phase 3 T2 (v1.8): 从 SimModule → ChStreamModuleBase
//   - 异步状态机: translate() 在 mem_view_==nullptr 时经 MasterPort (PcieTlpBundle MEM_READ)
//     发起 PTE walk, 返回 -EAGAIN; 多 tick 后收 CPLD → COMPLETE
//   - N2 iova 匹配: WAIT/COMPLETE 入口检查 pending_iova_/pending_size_ 匹配 (乱序重试防御)
//   - dual-mode legacy: mem_view_ != nullptr → translate_sync (v1.0 行为逐字节一致)
//   - 1 MasterPort (req_out_) + 1 SlavePort (resp_in_) 访问器 (单端口模板要求)
//   - wire-format: PcieTlpBundle (v1.8 H1)
//   - B22: pte_addr 越界 → -EIO (async 路径不触发 MasterPort)
//   - max_retry=16 (B22): 防无限 EAGAIN 重试
//
// Phase 4 T2 (cpptlm-dgpu-soc-timing-mvp): TLB + page walk (v0.2 简化)
//   - translate_timing() 同步 TLB 查询 + 一级 page walk (per design.md §3)
//   - 32-entry 直接映射 TLB (无 LRU, per R3); set_tlb_size() 可配置
//   - translate() 内部转调 translate_timing() + 丢弃 latency (零 functional 回归)
//   - do_reset() 清 TLB + stats
// 作者: CppTLM Team · 日期: 2027-02-09 (Phase 3 2027-02-10, Phase 4 T2 2027-02-11)
#ifndef CPPTLM_GMMU_TLM_H
#define CPPTLM_GMMU_TLM_H

#include "bundles/pcie_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/sim_object.hh"
#include "framework/stream_adapter.hh"

#include <array>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <vector>

namespace tlm::gpu {

class GmmuTLM : public ChStreamModuleBase {
public:
    explicit GmmuTLM(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq) {
        // Phase 4 T2: 默认 32-entry 直接映射 TLB (per design.md §3.1)
        tlb_entries_.assign(32, TlbEntry{});
    }
    ~GmmuTLM() override = default;

    std::string get_module_type() const override { return "GmmuTLM"; }

    // ── ChStream MasterPort 访问器 (单端口模板要求) ──
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle>& req_out() { return req_out_; }
    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle>&  resp_in() { return resp_in_; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { adapter_ = a; }

    // Initiator 不接收请求, 空适配器 (满足 StreamAdapter 接口, 同 CPUTLM 模式)
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle>& resp_out() {
        static cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle>& req_in() {
        static cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> dummy;
        return dummy;
    }

    // ── 寄存器接口 (DGpuBoard BAR0 hook 调用) ──
    void set_pt_base_lo(uint32_t lo) noexcept { pt_base_lo_ = lo; }
    void set_pt_base_hi(uint32_t hi) noexcept { pt_base_hi_ = hi; }
    void set_enabled(bool en) noexcept { enabled_ = en; }
    uint64_t pt_base() const noexcept {
        return (static_cast<uint64_t>(pt_base_hi_) << 32) |
               static_cast<uint64_t>(pt_base_lo_);
    }

    // ── backing 注入 (DGpuBoard::bind_memory_backings 调用)
    // ADR-DGPU-10: 新命名 set_mem_view (旧 set_backing 保留 deprecation wrapper)
    void set_mem_view(uint8_t* ptr, uint64_t sz) noexcept {
        mem_view_ = ptr;
        mem_view_size_ = sz;
    }
    [[deprecated("use set_mem_view / mem_view_")]]
    void set_backing(uint8_t* ptr, uint64_t sz) noexcept {
        set_mem_view(ptr, sz);
    }

    // ── 翻译 API (DmaTranslateCb 签名严格匹配) ──
    // 返回 0 成功, -EIO 失败, -EAGAIN 异步 pending; out_paddr 仅在成功时写入
    // v1.8 H1: wire-format 统一 PcieTlpBundle (替代 v1.3 AxiMemBundle)
    int translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        if (!enabled_ || pt_base() == 0) return -EIO;

        // legacy 同步路径 (mem_view_ 注入 → 直读页表项)
        // Phase 4 T2: 转调 translate_timing (TLB + page walk), 丢弃 latency (零 functional 回归)
        if (mem_view_) {
            uint64_t lat = 0;
            return translate_timing(iova, size, out_paddr, lat);
        }

        // 跨页检查 (v1.0 不支持跨页描述符, per design D7)
        constexpr uint64_t page_mask = 4095;
        if ((iova & ~page_mask) != ((iova + size - 1) & ~page_mask)) return -EIO;
        const uint64_t pte_addr = pt_base() + (iova >> 12) * 8;

        // 异步路径: IDLE → 发 PTE MEM_READ → WAIT
        // B22 注: 越界检查由下游 PcieMemoryDevice handle_slave_port 负责
        // (PTE read 经 MasterPort 发到 pcie_memory, SLVERR 回 CPLD data=0xDEAD)
        if (state_ == State::IDLE) {
            bundles::PcieTlpBundle req;
            req.kind.write(bundles::PcieTlpBundle::MEM_READ);
            req.offset.write(pte_addr);
            req.size.write(8);
            req.trans_id.write(next_req_id_++);
            req_out_.write(req);
            state_ = State::WAIT;
            pending_iova_ = iova;
            pending_size_ = size;
            retry_count_ = 0;
            return -EAGAIN;
        }

        // N2 关键: WAIT/COMPLETE 检查 iova 匹配 (乱序重试防御)
        if (state_ == State::WAIT || state_ == State::COMPLETE) {
            if (iova != pending_iova_ || size != pending_size_) {
                return -EAGAIN;  // 不消费，pending_iova_ 仍在等待
            }
        }

        if (state_ == State::WAIT) {
            // B22 max_retry: 防无限 EAGAIN 重试
            if (++retry_count_ > kMaxRetry) {
                state_ = State::IDLE;
                return -EIO;
            }
            return -EAGAIN;
        }

        // COMPLETE (iova 匹配)
        state_ = State::IDLE;
        uint64_t pte = 0;
        std::memcpy(&pte, pte_buf_.data(), 8);
        if (!(pte & 1ULL)) return -EIO;
        out_paddr = (pte & ~page_mask) | (iova & page_mask);
        return 0;
    }

    // ── Phase 4 T2: timing-mode 翻译 API (per design.md §3) ──
    // 同步返回 + 累加 latency: TLB hit → lat=0; TLB miss → page walk (lat=tlb_miss_latency_cycles_)
    // 仅 sync 路径 (mem_view_ 注入); async 路径保持 translate() 既有行为
    int translate_timing(uint64_t iova, uint32_t size, uint64_t& out_paddr,
                         uint64_t& out_latency_cycles) {
        out_latency_cycles = 0;
        if (!enabled_ || !mem_view_ || pt_base() == 0) return -EIO;

        constexpr uint64_t page_size = 4096;
        constexpr uint64_t page_mask = page_size - 1;

        // 页跨界检查 (沿用 minimal_v1 §5.1)
        if ((iova & ~page_mask) != ((iova + size - 1) & ~page_mask)) return -EIO;

        const uint64_t page_num = iova >> 12;

        // 1. TLB lookup (直接映射 idx = page_num % tlb_size)
        if (!tlb_entries_.empty()) {
            const size_t idx = page_num % tlb_entries_.size();
            TlbEntry& entry = tlb_entries_[idx];
            if (entry.valid && entry.iova_page == page_num) {
                ++translate_stats_.hits;
                out_paddr = entry.paddr_page | (iova & page_mask);
                out_latency_cycles = 0;
                return 0;
            }
        }

        // 2. TLB miss → page walk (1 级 walk, 累加 tlb_miss_latency_cycles_)
        const uint64_t pte_addr = pt_base() + page_num * 8;
        if (pte_addr + 8 > mem_view_size_) return -EIO;

        uint64_t pte = 0;
        std::memcpy(&pte, mem_view_ + pte_addr, sizeof(pte));
        if (!(pte & 1ULL)) return -EIO;  // invalid PTE

        // 3. 累加 walk cycles
        out_latency_cycles = tlb_miss_latency_cycles_;
        ++translate_stats_.misses;
        translate_stats_.total_walk_cycles += out_latency_cycles;

        // 4. TLB fill (直接映射 idx = page_num % tlb_size)
        if (!tlb_entries_.empty()) {
            const size_t idx = page_num % tlb_entries_.size();
            tlb_entries_[idx] = TlbEntry{page_num, pte & ~page_mask, true};
        }

        out_paddr = (pte & ~page_mask) | (iova & page_mask);
        return 0;
    }

    // ── Phase 4 T2: TLB 统计 + 时序参数 (per design.md §3.1) ──
    struct TranslateStats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t total_walk_cycles = 0;
    };
    const TranslateStats& stats() const noexcept { return translate_stats_; }
    void reset_stats() noexcept { translate_stats_ = TranslateStats{}; }

    // TLB miss latency (page walk 时间)
    void set_timing_params(uint64_t tlb_miss_latency) noexcept {
        tlb_miss_latency_cycles_ = tlb_miss_latency;
    }
    // TLB 大小配置 (default 32-entry 直接映射; 0 = 禁用 TLB, 每次走 page walk)
    void set_tlb_size(size_t entries) noexcept {
        tlb_entries_.assign(entries, TlbEntry{});
        if (entries > 0) {
            tlb_entries_.resize(entries);
        }
    }
    uint64_t tlb_miss_latency() const noexcept { return tlb_miss_latency_cycles_; }
    size_t tlb_size() const noexcept { return tlb_entries_.size(); }

    // do_reset (T2.4): 清 TLB + stats (PT_BASE 切换防 stale entry, per design §3.4)
    void do_reset(const ResetConfig& /*config*/) override {
        tlb_entries_.clear();
        tlb_entries_.resize(32);  // 恢复默认 32-entry
        translate_stats_ = TranslateStats{};
        state_ = State::IDLE;
    }

    // on_config_loaded: v1.0 读 page_size_bytes (固定 4096); v1.1 扩展支持 2MB/1GB
    void on_config_loaded() override {
        const auto& cfg = get_config();
        if (cfg.is_object() && cfg.contains("page_size_bytes")) {
            // 当前实现: 固定 4096, 参数保留以备 v1.1 扩展
        }
    }

    void tick() override {
        if (state_ == State::WAIT && resp_in_.valid()) {
            const auto& r = resp_in_.data();
            if (r.kind.read() == bundles::PcieTlpBundle::CPLD) {
                uint64_t pte = r.data.read();
                std::memcpy(pte_buf_.data(), &pte, 8);
                state_ = State::COMPLETE;
            } else {
                state_ = State::IDLE;  // 非 CPLD = 错误
            }
            resp_in_.consume();
        }
        if (adapter_) adapter_->tick();
    }

    // 测试断言 helper
    uint64_t pending_iova() const noexcept { return pending_iova_; }
    bool is_async_pending() const noexcept { return state_ != State::IDLE; }

private:
    enum class State : uint8_t { IDLE = 0, WAIT = 1, COMPLETE = 2 };
    static constexpr uint32_t kMaxRetry = 16;

    // ── Phase 4 T2: TLB (32-entry 直接映射, per design.md §3.4) ──
    struct TlbEntry {
        uint64_t iova_page = 0;
        uint64_t paddr_page = 0;
        bool valid = false;
    };
    std::vector<TlbEntry> tlb_entries_;   // default 32 (ctor 初始化)
    TranslateStats translate_stats_{};
    uint64_t tlb_miss_latency_cycles_ = 50;  // default per design.md §3.1

    int translate_sync(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        if (pt_base() == 0) return -EIO;
        constexpr uint64_t page_size = 4096;
        constexpr uint64_t page_mask = page_size - 1;

        if ((iova & ~page_mask) != ((iova + size - 1) & ~page_mask)) {
            return -EIO;
        }

        const uint64_t idx = iova >> 12;
        const uint64_t pte_addr = pt_base() + idx * 8;
        if (pte_addr + 8 > mem_view_size_) return -EIO;

        uint64_t pte = 0;
        std::memcpy(&pte, mem_view_ + pte_addr, sizeof(pte));
        if (!(pte & 1ULL)) return -EIO;
        out_paddr = (pte & ~page_mask) | (iova & page_mask);
        return 0;
    }

    uint32_t pt_base_lo_ = 0;
    uint32_t pt_base_hi_ = 0;
    bool enabled_ = false;
    uint8_t* mem_view_ = nullptr;
    uint64_t mem_view_size_ = 0;

    // 异步状态机
    State state_ = State::IDLE;
    uint64_t pending_iova_ = 0;
    uint32_t pending_size_ = 0;
    uint32_t next_req_id_ = 1;
    uint32_t retry_count_ = 0;
    std::array<uint8_t, 8> pte_buf_{};
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> req_out_;
    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle>  resp_in_;
};

} // namespace tlm::gpu

#endif // CPPTLM_GMMU_TLM_H
