# cpptlm-dgpu-soc-timing-mvp: Design (实施指导)

> **来源**: [proposal.md](./proposal.md) (v0.1) + [docs/designs/dgpu-soc/timing-mode.md](../../docs/designs/dgpu-soc/timing-mode.md) (第 1 层架构视图)
> **范围**: T1-T5 实施阶段的代码改动详细说明
> **状态**: 📋 提案草案,与 proposal.md 同步,Oracle 评审后定稿

## 1. 实施总览

### 1.1 模块改动分类

| 模块 | 改动类型 | 实施任务 (tasks.md) | 工作量 |
|------|----------|---------------------|--------|
| `MemoryTLM` | 🔧 扩展 | T1 | 1d |
| `GmmuTLM` | 🔧 扩展 | T2 | 1d |
| `SdmaEngineTLM` | 🔧 扩展 | T3 | **0.5d** |
| `VramControllerTLM` | 🆕 新建 (含 T4.0 MemoryTLM protected 化 0.5d) | T4 | **1.5d** |
| `CrossbarTLM` + `DGpuBoard` | 🔧 扩展 | T5 | **2d** |

### 1.2 共享基线约束

- **单一 vram_storage_ 真源** (per ADR-DGPU-05): timing-mode **不**重新分配;沿用 `DGpuBoard::vram_storage_`
- **AxiMemBundle 边界** (per ADR-DGPU-06): chip-internal bundle 类型不变
- **Phase 6 AXI4Mapper pattern 借鉴, v0.2 已删除 SDMA outstanding** (per M8 + H3): 设计模式作参考, 不复用类, 不调用任何 outstanding API; v0.2 SDMA **不**带 outstanding 表
- **Phase 5 PcieAxiAdapter 直接复用**: PCIe TLP 时序已建模(2 cyc TLP propagation)

---

## 2. MemoryTLM cycle-approximate 扩展

### 2.1 API 新增

```cpp
// include/tlm/memory_tlm.hh (扩展,144 → ~210 行)
class MemoryTLM : public ChStreamModuleBase {
public:
    // ... 既有 API (set_backing_view / on_config_loaded / host_addr / backing_view_size) 不变 ...

    // 🆕 v0.1: cycle advance (DGpuBoard::tick 调用)
    void advance_cycle() noexcept { ++current_cycle_; }
    uint64_t current_cycle() const noexcept { return current_cycle_; }

    // 🆕 v0.1: 时序参数 setter (DGpuBoard::init_timing_mode 调用)
    void set_timing_params(uint64_t read_hit, uint64_t read_miss, uint64_t write,
                            bool use_zero_delay_for_test) noexcept;

    // 🆕 v0.1: 暴露给测试 + stats
    uint64_t inflight_resp_count() const noexcept { return pending_resps_.size(); }
};
```

### 2.2 内部数据结构

```cpp
private:
    // ... 既有 backing_view_ / size_cap_ / stats_ 不变 ...

    // 🆕 v0.1: 延迟响应队列
    struct PendingResp {
        bundles::CacheRespBundle resp;
        uint64_t ready_cycle;   // resp 可发出时间 = current_cycle_ + latency
    };
    // 🆕 v0.2 (per M1 + 七轮 P0-3): priority_queue 按 ready_cycle 升序 (避免 head-of-line blocking)
    // 与 timing-mode.md §3.3 / spec.md:163-170 决策一致
    struct PendingResp {
        bundles::CacheRespBundle resp;
        uint64_t ready_cycle;
        bool operator>(const PendingResp& o) const noexcept { return ready_cycle > o.ready_cycle; }
    };
    std::priority_queue<PendingResp, std::vector<PendingResp>, std::greater<PendingResp>> pending_resps_;

    // 🆕 v0.1: cycle 跟踪 (board 注入,不自行 advance)
    uint64_t current_cycle_ = 0;

    // 🆕 v0.1: 5 个时序参数
    uint64_t read_latency_hit_cycles_  = 100;
    uint64_t read_latency_miss_cycles_ = 200;
    uint64_t write_latency_cycles_     = 120;
    bool     use_zero_delay_for_test_  = true;  // 默认 true 保 functional 零回归 (per M5 Oracle 反馈)
```

### 2.3 tick() 时序逻辑

```cpp
void MemoryTLM::tick() override {
    // 🆕 v0.1 阶段 1: 处理新 req (按 arrival 顺序)
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        const uint64_t tid = req.transaction_id.read();
        const uint64_t addr = req.address.read();
        const uint64_t sz = req.size.read();
        const bool is_write = req.is_write.read();

        bundles::CacheRespBundle resp;
        resp.transaction_id.write(tid);
        uint64_t latency = 0;

        if (use_zero_delay_for_test_) {
            // 旁路: 用于 backdoor sanity (functional 兼容)
            // ... 既有 v2.2 行为 ...
            latency = 0;
        } else if (backing_view_ != nullptr) {
            // ⚠️ 数据粒度限制 (per M12 Oracle 反馈): CacheReqBundle.data 是 ch_uint<64>,
            // 单 req memcpy max 8 字节 (std::min<size_t>(sz, 8))。SDMA 4KB sub-req 必须由
            // SDMA 端拆分为 512×8B req (per Phase 5 PcieAxiAdapter 已实现该模式);
            // v0.1 timing-mode E2E 验证范围为"cycle accounting + 8B chunk 数据正确性",
            // **不**验证 4KB 单 req 整块搬运
            // timing-mode: 立即计算 resp payload,延迟发出
            const uint64_t cap = size_cap_ ? size_cap_ : backing_view_size_;
            if (addr + sz > cap) {
                resp.error_code.write(1);
                resp.is_hit.write(0);
                resp.data.write(0);
                latency = 1;
            } else if (is_write) {
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
                latency = row_hit(addr) ? read_latency_hit_cycles_ : read_latency_miss_cycles_;
                ++stats_requests_read_;
            }
        } else {
            // legacy 路径 (兼容 functional 测试)
            latency = is_write ? write_latency_cycles_
                                : (row_hit(addr) ? read_latency_hit_cycles_ : read_latency_miss_cycles_);
        }

        // sample latency stats (always 启用,不论 backing 是否注入)
        if (is_write) stats_latency_write_.sample(latency);
        else          stats_latency_read_.sample(latency);

        // 🆕 v0.1: schedule to pending_resps_
        pending_resps_.push_back({resp, current_cycle_ + latency});
        req_in_.consume();
    }

    // 🆕 v0.2 (per M1 + 七轮 P0-3 落盘): drain priority_queue (top 必为 ready_cycle 最小者, 避免 FIFO head-of-line blocking)
    while (!pending_resps_.empty() && pending_resps_.top().ready_cycle <= current_cycle_) {
        resp_out_.write(pending_resps_.top().resp);
        pending_resps_.pop();
    }

    if (adapter_) adapter_->tick();
}

bool MemoryTLM::row_hit(uint64_t addr) const noexcept {
    // 行命中假设: 4K row,简化模型
    // addr bit[15:12] == (addr+1) bit[15:12] 算 hit
    return (addr & 0xF000ULL) == ((addr >> 16) & 0xF000ULL);
}
```

### 2.4 设计要点

- **cycle advance 不可见**: `current_cycle_` 仅 setter `advance_cycle()` 暴露,内部 `tick()` 不可自增(per TInv-3)
- **pending_resps_ 顺序**: 按 `ready_cycle` 单调排序(FIFO),保证 cycle 内 ordering(per TInv-2)
- **functional-mode 零 diff**: `use_zero_delay_for_test_=true` 走 v2.2 既有路径,**不**触发 pending_resps_

---

## 3. GmmuTLM TLB miss + page walk

### 3.1 API 新增

```cpp
// include/tlm/gpu/gmmu_tlm.hh (扩展,89 → ~145 行)
class GmmuTLM : public ::SimModule {
public:
    // ... 既有 minimal_v1 API (set_pt_base_lo/hi/enabled + set_backing + translate) 不变 ...

    // 🆕 v0.1: timing-mode 翻译 (同步返回 + 累加 cycle)
    int translate_timing(uint64_t iova, uint32_t size,
                          uint64_t& out_paddr, uint64_t& out_latency_cycles);

    // 🆕 v0.1: 统计
    struct TranslateStats {
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t total_walk_cycles = 0;
    };
    const TranslateStats& stats() const noexcept { return translate_stats_; }
    void reset_stats() noexcept { translate_stats_ = {}; }

    // 🆕 v0.1: 时序参数
    void set_timing_params(uint64_t tlb_miss_latency) noexcept;
    void set_tlb_size(size_t entries) noexcept;

private:
    // ... 既有 pt_base_lo_/hi_/enabled_/mem_view_ 不变 ...

    // 🆕 v0.1: TLB (32-entry default, 直接映射)
    struct TlbEntry {
        uint64_t iova_page = 0;
        uint64_t paddr = 0;
        bool valid = false;
    };
    std::vector<TlbEntry> tlb_entries_;

    TranslateStats translate_stats_;
    uint64_t tlb_miss_latency_cycles_ = 50;
};
```

### 3.2 translate_timing() 实现

```cpp
int GmmuTLM::translate_timing(uint64_t iova, uint32_t size,
                                uint64_t& out_paddr, uint64_t& out_latency_cycles) {
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
            // TLB hit
            ++translate_stats_.hits;
            out_paddr = entry.paddr | (iova & page_mask);
            out_latency_cycles = 0;
            return 0;
        }
    }

    // 2. TLB miss → page walk (4 级,本版本 1 级 walk,累加 N cycles)
    const uint64_t pte_addr = pt_base() + page_num * 8;
    if (pte_addr + 8 > mem_view_size_) return -EIO;

    uint64_t pte = 0;
    std::memcpy(&pte, mem_view_ + pte_addr, sizeof(pte));
    if (!(pte & 1ULL)) return -EIO;  // invalid PTE

    // 3. 累加 walk cycles
    out_latency_cycles = tlb_miss_latency_cycles_;
    ++translate_stats_.misses;
    translate_stats_.total_walk_cycles += out_latency_cycles;

    // 4. TLB fill
    if (!tlb_entries_.empty()) {
        const size_t idx = page_num % tlb_entries_.size();
        tlb_entries_[idx] = {page_num, pte & ~page_mask, true};
    }

    out_paddr = (pte & ~page_mask) | (iova & page_mask);
    return 0;
}

// 既有 translate() 内部转调 translate_timing,latency 0 (向后兼容)
int GmmuTLM::translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
    uint64_t lat = 0;
    return translate_timing(iova, size, out_paddr, lat);
}
```

### 3.3 set_translate_cb 兼容性

`SdmaEngineTLM::translate_cb_` 签名严格保持 minimal_v1 不变:
```cpp
using DmaTranslateCb = std::function<int(uint64_t iova, uint32_t size, uint64_t& phys)>;
```

timing-mode 通过 `SDMA::translate_cb_(iova, size, &phys)` 调用 `GmmuTLM::translate()`,**不**破坏 ABI。`translate_timing()` 的 latency 输出通过 SDMA 内部额外参数传递(per TInv-6)。

### 3.4 设计要点

- **TLB 32-entry 简化**: 直接映射,无 LRU;v0.1 文档明示"简化模型",D4 后续加 LRU
- **TLB 可配置**: `set_tlb_size()` 允许测试场景用更大 TLB 验证 hit rate
- **reset 时清空**: `gmmu_->reset()` 必须清 tlb_entries_,否则 PT_BASE 切换有 stale entry

---

## 4. SdmaEngineTLM cycle accounting (per v0.2 简化, **不**新增端口)

> **🔧 v0.2 重构 (per Oracle H2/H3)**:
> 原 v0.1 提案的"SDMA outstanding + AXI master port + rid 关联"**不可实施**:
> - `CacheReqBundle` 无 `rid`/`user` 字段 (`cache_bundles_tlm.hh:38-69` 实际字段只有 `transaction_id`/`parent_id`/`fragment_id`/`fragment_total`/`address`/`size`/`is_write`/`data`)
> - `size` 是 `ch_uint<8>` 上限 255 字节,4KB sub-req 溢出
> - "Phase 5 PcieAxiAdapter 已实现模式" 伪引用,`pcie_axi_adapter_tlm.cc` 无 chunk/split 逻辑
> - `CrossbarTLM` 无 `req_out`/`resp_in` 下游端口,无 multi-hop cascade 拓扑
>
> **v0.2 简化策略**: SDMA 沿用既有 5-port + `set_vram_backdoor()` 路径 (functional-mode 流程),**不**新增端口、**不**拆分 descriptor、**不**接 Crossbar 仲裁、**不**接 VramCtrl cascade。Cycle accounting 仅在 `translate_cb_` 回调中累加 GMMU TLB miss latency。

### 4.1 公共 API (沿用 minimal_v1, **不**新增)

```cpp
// include/tlm/gpu/sdma_engine_tlm.hh (337 行,**不**改)
// 既有 5 端口 (desc_in/done_out/host_out/mem_in/mem_out) + set_vram_backdoor + set_translate_cb 不变
class SdmaEngineTLM : public ChStreamModuleBase {
public:
    // 既有 API, **不**新增:
    // - submit_descriptor(DmaDescriptor desc)
    // - set_vram_backdoor(uint8_t* ptr, uint64_t size)
    // - set_translate_cb(DmaTranslateCb cb)
    // - mmio_write / mmio_read (doorbell 路径)

    // 🆕 v0.2: cycle accounting (仅记录 + getter)
    uint64_t last_descriptor_complete_cycle() const noexcept {
        return last_descriptor_complete_cycle_;
    }

private:
    // 🆕 v0.2: cycle 跟踪 (per TInv-3 强制由 board 推进,SDMA 仅记录)
    uint64_t last_descriptor_complete_cycle_ = 0;
};
```

### 4.2 translate_cb_ 内部 timing-mode 增强

```cpp
// SdmaEngineTLM 内部既有的 translate_cb_(iova, size, &phys) 调用
// v0.2 修改: 回调内部转调 gmmu_->translate_timing() 累加 lat
//
// 内部调用流程 (per SdmaEngineTLM::process_descriptor()):
void SdmaEngineTLM::process_descriptor(DmaDescriptor desc) {
    uint64_t phys = 0;
    // 调用既有的 translate_cb_ 回调
    int rc = translate_cb_(desc.iova, desc.size, phys);
    if (rc != 0) {
        submit_fence(desc, /*status=*/kFenceError);
        return;
    }

    // 🆕 v0.2 (per 六轮 P0-12): 累加 GMMU TLB latency
    // 关键设计: translate_cb_ 在 timing-mode 下包装为调 translate_timing 并通过成员 last_tlb_latency_
    // 回传 latency (避免第二次显式 translate_timing 调用导致 TLB hit lat=0 错误)
    // 由 DGpuBoard::init_timing_mode 注入包装层:
    //   sdma->set_translate_cb([&](uint64_t iova, uint32_t size, uint64_t& phys) {
    //       return gmmu->translate_timing(iova, size, &phys, &last_tlb_latency_);
    //   });
    uint64_t lat_cycles = last_tlb_latency_;  // 来自 translate_cb_ 包装层 (单次调用)
    last_tlb_latency_ = 0;  // 重置

    // 🆕 v0.2: 累加 cycle (TInv-3 强制由 board 推进,SDMA 仅记录最近 cycle)
    // v0.2 (per 六轮 P0-13): SDMA 无 current_cycle_ 成员;last_descriptor_complete_cycle_ 由
    // DGpuBoard::sdma_fence_complete 回调注入 (= board.current_cycle_)
    // TInv-3: 任何模块不可自行 ++cycle
    last_descriptor_complete_cycle_ = 0;  // 实际由 board 回调覆盖

    // 沿用 functional-mode 路径: memcpy 搬运 (0 cycle)
    memcpy(vram_backdoor_ + desc.vram_offset, host_backdoor_ + phys, desc.size);

    // 沿用 fence 路径
    submit_fence(desc, /*status=*/kFenceSuccess);
}
```

> **设计要点**:
> - **无新端口**: SDMA 5 端口 (`desc_in/mem_in/mem_out/host_out/done_out`) **不变**
> - **无 outstanding 拆分**: Descriptor 沿用 functional-mode memcpy 路径
> - **cycle accounting 在哪里**: `translate_cb_` 回调注册 `GmmuTLM::translate_timing`,SDMA `last_descriptor_complete_cycle_` 记录最近完成 cycle
> - **stats 暴露**: GMMU stats (`tlb_hits`/`tlb_misses`/`total_walk_cycles`) 由 GMMU 维护

### 4.3 公共 API 兼容性

```cpp
// 既有测试代码 (per minimal_v1) 100% 兼容, 无需修改
auto* sdma = dynamic_cast<SdmaEngineTLM*>(board.get_internal_instance("sdma"));
REQUIRE(sdma != nullptr);
sdma->set_vram_backdoor(host_buf.data(), host_buf.size());
sdma->set_translate_cb([&](uint64_t iova, uint32_t size, uint64_t& phys) {
    return gmmu->translate(iova, size, phys);  // 兼容 minimal_v1 既有签名
});

// timing-mode 启用时 (DGpuBoard::init_timing_mode):
// 1. SDMA 内部检测 simulation_mode_ == Timing
// 2. translate_cb_ 内**额外**调用 translate_timing 累加 lat (callback 包装层)
// 3. last_descriptor_complete_cycle_ 在 fence 时记录
```

### 4.4 设计要点

- **v0.2 删除 SDMA outstanding/AXI master port** (per H2/H3): SDMA 沿用既有 5-port + backdoor path,**无** outstanding 表、**无** rid 关联、**无** 乱序完成逻辑。Phase 6 AXI4Mapper 设计模式作**参考**,**不**复用类 (per M8)、**不**调用 `AXI4Mapper::track_outstanding()` (此方法不存在)
- **乱序完成支持**: resp 按 rid 关联,**不**要求按 issue 顺序;测试场景可乱序验证
- **v0.2 无 sub-req 概念** (七轮 P0-4 终稿): SDMA 无 outstanding 表,无 sub-req 拆分;fence 完成仅依赖 functional-mode 既有路径
- **stale resp 容忍**: rid 不在 outstanding_ 中(已被 erase),静默忽略

---

## 5. VramControllerTLM 新建

### 5.1 类设计

```cpp
// include/tlm/vram_controller_tlm.hh (🆕 新建)
// 前置: MemoryTLM T4.0 protected 化 (per M10 Oracle 反馈)
//   - MemoryTLM 的 req_in_ / resp_out_ / stats_ / backing_view_ / backing_view_size_
//     / size_cap_ / pending_resps_ / current_cycle_ 改为 protected
//   - 添加 friend class VramControllerTLM
//   - 公共 API 零修改 ([memory_tlm] 既有单测零回归)
#ifndef TLM_VRAM_CONTROLLER_TLM_HH
#define TLM_VRAM_CONTROLLER_TLM_HH

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "tlm/memory_tlm.hh"  // 继承行为 (per M10)
#include <cstdint>

// 🆕 D3 触发 (per ADR-DGPU-07): 继承 MemoryTLM + 行缓冲 + bandwidth 上限
class VramControllerTLM : public MemoryTLM {
public:
    explicit VramControllerTLM(const std::string& name, EventQueue* eq)
        : MemoryTLM(name, eq),
          stats_row_hits_(stats_.addScalar("vram_row_hits", "VRAM row buffer hits", "count")),
          stats_row_misses_(stats_.addScalar("vram_row_misses", "VRAM row buffer misses", "count")),
          stats_bandwidth_limit_waits_(stats_.addScalar("bandwidth_limit_waits", "Cycles waiting for bandwidth", "count")) {
    }

    ~VramControllerTLM() override = default;

    std::string get_module_type() const override { return "VramControllerTLM"; }

    // 🆕 D3 行缓冲 + bandwidth 参数 setter
    void set_vram_params(uint64_t row_hit_cycles, uint64_t row_miss_cycles,
                          uint64_t bandwidth_gbps) noexcept {
        row_hit_cycles_ = row_hit_cycles;
        row_miss_cycles_ = row_miss_cycles;
        bandwidth_gbps_ = bandwidth_gbps;
    }

    // 🆕 行缓冲 (LRU, 16-entry,简化)
    void invalidate_row_buffer() noexcept { row_buffer_valid_ = false; }
    bool row_hit(uint64_t addr) const noexcept {
        if (!row_buffer_valid_) return false;
        constexpr uint64_t kRowSize = 4096;
        return (addr & ~(kRowSize - 1)) == row_buffer_base_;
    }

    // 覆盖 MemoryTLM::row_hit,使用 VRAM 行缓冲
    void on_config_loaded() override {
        // 优先读 vram_size_bytes (per ADR-DGPU-05 单一真源)
        const auto& cfg = get_config();
        if (cfg.contains("vram_size_bytes") && cfg["vram_size_bytes"].is_number()) {
            set_size_bytes(cfg["vram_size_bytes"].get<uint64_t>());
        }
    }

private:
    // 行缓冲 (16-entry LRU,简化)
    static constexpr uint64_t kRowBufferEntries = 16;
    uint64_t row_buffer_base_ = 0;
    bool row_buffer_valid_ = false;

    uint64_t row_hit_cycles_ = 100;
    uint64_t row_miss_cycles_ = 200;
    uint64_t bandwidth_gbps_ = 32;  // 32 GB/s 默认上限

    // stats (继承自 MemoryTLM::stats_,额外添加)
    tlm_stats::Scalar& stats_row_hits_;
    tlm_stats::Scalar& stats_row_misses_;
    tlm_stats::Scalar& stats_bandwidth_limit_waits_;
};

#endif // TLM_VRAM_CONTROLLER_TLM_HH
```

### 5.2 row buffer + bandwidth 行为

```cpp
// src/tlm/vram_controller_tlm.cc (🆕 新建)
#include "tlm/vram_controller_tlm.hh"

void VramControllerTLM::tick() override {
    // 复用 MemoryTLM::tick 行为 + override row_hit 逻辑
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        const uint64_t addr = req.address.read();
        const bool is_write = req.is_write.read();

        // 1. 行缓冲 hit/miss 判断 (覆盖 MemoryTLM::row_hit)
        if (row_hit(addr)) {
            ++stats_row_hits_;
            // 命中行缓冲: latency = row_hit_cycles_
        } else {
            ++stats_row_misses_;
            // 行 miss: 更新 row buffer + latency = row_miss_cycles_
            row_buffer_base_ = addr & ~0xFFFULL;
            row_buffer_valid_ = true;
        }

        // 2. bandwidth 限制 (bytes/cycle 上限)
        const uint64_t bytes_this_req = req.size.read();
        const uint64_t cycles_needed = (bytes_this_req + bandwidth_gbps_ - 1) / bandwidth_gbps_;
        if (cycles_needed > 1) {
            // 累加 bandwidth wait cycles 到 pending_resps_
            ++stats_bandwidth_limit_waits_;
            // (本提案 v0.1 简化: 累加到 latency,后续 D4 加专门的 bandwidth 调度)
        }
    }

    // 调父类 tick (复用 MemoryTLM::tick 的 pending_resps_ 处理)
    MemoryTLM::tick();
}
```

### 5.3 模块注册 (per H1 Oracle 反馈)

```cpp
// ❌ 错误 (v0.1 提案): REGISTER_CHSTREAM 是无参宏 (chstream_register.hh:46),传参会预处理失败
// const bool _reg_vramctrltlm = (REGISTER_CHSTREAM(VramControllerTLM), true);

// ✅ 正确做法 (per H1): 在 chstream_register.hh 宏体内追加 registerObject 行
// include/chstream_register.hh:46 既有 REGISTER_CHSTREAM 宏体内追加:
// ModuleFactory::registerObject<VramControllerTLM>("VramControllerTLM");
// (与既有 MemoryTLM 同一模式)
//
// REGISTER_MODULE 静态断言 is_base_of<SimModule, T> 会失败 (per M10):
// MemoryTLM 继承自 ChStreamModuleBase → SimObject,**不**是 SimModule
```

> **v0.2 修正说明 (per H1)**:
> - 原 v0.1 写 `REGISTER_CHSTREAM(VramControllerTLM)` 是**错误**的——`REGISTER_CHSTREAM` 是无参硬编码宏 (`chstream_register.hh:46`),**不**接受参数
> - 正确做法: 在 `chstream_register.hh:46` 宏体内追加 `registerObject<VramControllerTLM>`(与既有 `registerObject<MemoryTLM>` 同一模式)
> - **不**使用 `REGISTER_MODULE(VramControllerTLM)`(`is_base_of<SimModule>` 静态断言失败 — VramControllerTLM 是 SimObject 系)

### 5.4 设计要点

- **继承而非新写**: 复用 MemoryTLM 的 `pending_resps_` + `current_cycle_` + stats_,仅 override row hit/miss
- **row buffer 16-entry LRU 简化**: v0.1 不实现真 LRU 替换,仅保留 "last row";D4 加完整 LRU
- **bandwidth 简化**: `cycles_needed = ceil(bytes / bandwidth_gbps_)` 累加到 latency;D4 加专门调度器

---

## 6. CrossbarTLM + DGpuBoard 集成

### 6.1 CrossbarTLM 复用

`include/tlm/crossbar_tlm.hh` 现有 4 端口 (`NUM_PORTS=4`, `crossbar_tlm.hh:33`),与本提案 4 routes 一致。**但需新增** C++ 改动(per M4 Oracle 反馈):
- `set_arb_latency(uint64_t cycles)` setter
- `stats_arb_cycles_` (Scalar) + `stats_port_utilization_[N]` (Scalar × 4)
- `on_arb_complete()` 累加 stats
- 总工作量 0.5d,纳入 T5 范围

### 6.2 DGpuBoard init_timing_mode() 分支

```cpp
// include/tlm/gpu/dgpu_board_shell.hh 扩展
class DGpuBoard {
public:
    // ... 既有 API + load_soc_config + init + mmio_* 不变 ...

    // 🆕 v0.1: 仿真模式字段 (per simulation_mode JSON 字段)
    enum class SimulationMode { Functional, Timing };
    SimulationMode simulation_mode() const noexcept { return simulation_mode_; }

    // 🆕 v0.1: cycle 跟踪 + advance
    uint64_t current_cycle() const noexcept { return current_cycle_; }

private:
    SimulationMode simulation_mode_ = SimulationMode::Functional;
    uint64_t current_cycle_ = 0;

    // 🆕 v0.1: timing-mode init 分支
    void init_timing_mode();
};
```

```cpp
// src/tlm/gpu/dgpu_board_shell.cc 扩展
bool DGpuBoard::init() {
    // ... 既有 init 逻辑 (resize vram_storage_ + bind_memory_backings) ...

    // 🆕 v0.1: 读 simulation_mode 字段
    if (cfg_.contains("simulation_mode") && cfg_["simulation_mode"] == "timing") {
        simulation_mode_ = SimulationMode::Timing;
        init_timing_mode();
    } else {
        simulation_mode_ = SimulationMode::Functional;
        // 既有 functional-mode 路径(零 diff)
    }
    return true;
}

void DGpuBoard::init_timing_mode() {
    // 1. 找 internal modules
    auto* mem    = dynamic_cast<MemoryTLM*>(soc_->getInternalInstance("memory"));
    auto* gmmu   = dynamic_cast<GmmuTLM*>(soc_->getInternalInstance("gmmu"));
    auto* sdma   = dynamic_cast<SdmaEngineTLM*>(soc_->getInternalInstance("sdma"));
    auto* xbar   = dynamic_cast<CrossbarTLM*>(soc_->getInternalInstance("crossbar"));
    auto* vram   = dynamic_cast<VramControllerTLM*>(soc_->getInternalInstance("vram_ctrl"));

    // v0.2 (per 七轮 P0-4): VramControllerTLM 继承 MemoryTLM,任一存在即可;互斥实例化下 vram 可能为 null, mem 也可能为 null
    if (!(mem || vram) || !gmmu || !sdma) {
        throw std::runtime_error("timing-mode requires (MemoryTLM 或 VramControllerTLM 二选一) + GmmuTLM + SdmaEngineTLM");
    }

    // 2. 注入 cycle accounting 参数 (从 JSON 读)
    if (cfg_["modules"][0]["modules"][memory_idx]["params"].contains("read_latency_hit_cycles")) {
        const auto& mem_params = cfg_["modules"][0]["modules"][memory_idx]["params"];
        mem->set_timing_params(
            mem_params["read_latency_hit_cycles"].get<uint64_t>(),
            mem_params["read_latency_miss_cycles"].get<uint64_t>(),
            mem_params["write_latency_cycles"].get<uint64_t>(),
            mem_params.value("use_zero_delay_for_test", false));
    }

    // 3. GMMU TLB 参数
    if (cfg_["modules"][0]["modules"][gmmu_idx]["params"].contains("tlb_miss_latency_cycles")) {
        const auto& gmmu_params = cfg_["modules"][0]["modules"][gmmu_idx]["params"];
        gmmu->set_timing_params(gmmu_params["tlb_miss_latency_cycles"].get<uint64_t>());
        gmmu->set_tlb_size(32);  // default
    }

    // 4. SDMA cycle accounting (v0.2 简化: 不再处理 max_outstanding,SDMA 沿用既有 5-port)
    if (auto* sdma_params = ...) {
        // v0.2: SDMA enable_cycle_accounting 标志 (沿用既有 5-port,仅记录 last_descriptor_complete_cycle_)
        if (cfg_["modules"][0]["modules"][sdma_idx]["params"].value("enable_cycle_accounting", false)) {
            sdma->set_cycle_accounting_enabled(true);
        }
    }

    // 5. VramControllerTLM (可选)
    if (vram && cfg_["modules"][0]["modules"][vram_idx]["params"].contains("bandwidth_gbps")) {
        const auto& vram_params = cfg_["modules"][0]["modules"][vram_idx]["params"];
        vram->set_vram_params(
            vram_params["row_hit_cycles"].get<uint64_t>(),
            vram_params["row_miss_cycles"].get<uint64_t>(),
            vram_params["bandwidth_gbps"].get<uint64_t>());
    }

    // 6. CrossbarTLM arb 参数 (复用既有 crossbar_tlm.hh API)
    if (xbar && cfg_["modules"][0]["modules"][xbar_idx]["params"].contains("arb_latency_cycles")) {
        const auto& xbar_params = cfg_["modules"][0]["modules"][xbar_idx]["params"];
        xbar->set_arb_latency(xbar_params["arb_latency_cycles"].get<uint64_t>());
    }

    // 7. inject cycle advance hook (per tick 推所有 timing-mode 模块)
    cycle_advance_modules_.push_back(mem);
    // GmmuTLM 是 SimModule 同步 translate,**不**需要 cycle advance (per M14)
    // SdmaEngineTLM **不**注册 (无 advance_cycle 方法, per R10 统一决策)
    if (vram) cycle_advance_modules_.push_back(vram);   // VramControllerTLM 是 ChStreamModuleBase

    // ⚠️ P2-R10 (per H4 + Oracle 三轮反馈): cycle_advance_modules_ 类型 `std::vector<ChStreamModuleBase*>`
    // 调用 `mod->advance_cycle()` 必须有 ChStreamModuleBase 虚函数。T0 实施时须:
    //   1. 在 include/core/chstream_module.hh 的 ChStreamModuleBase 添加
    //      `virtual void advance_cycle() noexcept {}` 默认空实现 (per H4)
    //   2. SdmaEngineTLM 现有 5-port + backdoor,**不**需 advance_cycle 修改
    //   3. VramControllerTLM 继承 MemoryTLM,MemoryTLM 已有 advance_cycle
    //   4. MemoryTLM 已有 advance_cycle (per design §2.1)
    //   若不添加基类方法,可改为 `std::vector<ChStreamModuleBase*> advance_modules_;
    //   std::vector<MemoryTLM*> mem_modules_; std::vector<VramControllerTLM*> vram_modules_;` 多态列表
}

void DGpuBoard::tick() {
    ++current_cycle_;
    for (auto* mod : cycle_advance_modules_) {
        mod->advance_cycle();
    }
    // ... 既有 tick 逻辑 ...
}
```

### 6.3 设计要点

- **`simulation_mode` 字段 default = functional**: 既有 JSON 配置零修改(向后兼容)
- **cycle advance 集中**: DGpuBoard::tick 统一推,**不**依赖模块自行 advance(per TInv-3)
- **init_timing_mode 仅在 simulation_mode=timing 调用**: functional-mode 零 diff

---

## 7. JSON 配置 dgpu_soc_timing_v1.json

完整示例见 [`docs/designs/dgpu-soc/timing-mode.md` §3.6](../../docs/designs/dgpu-soc/timing-mode.md#36-json-dgpu_soc_timing_v1json)。

关键字段:
- 顶层 `simulation_mode: "timing"` (必填)
- 8 modules (6 functional + CrossbarTLM + VramControllerTLM)
- 0 new connections (timing-mode 沿用 minimal_v1 functional-mode 拓扑,per H2 Oracle 反馈)
- timing 参数 (read/write latency cycles, tlb_miss_latency, enable_cycle_accounting, arb_latency, bandwidth_gbps) (max_outstanding 删,v0.2 简化)

---

## 8. 测试套件

### 8.1 单元测试 (≥ 30 cases, `[dgpu_soc_timing]` 标签)

| 测试文件 | cases | 覆盖 |
|----------|-------|------|
| `test_memory_tlm_timing.cc` | 6 | read/write latency / pending_resps_ order / out-of-range / use_zero_delay_for_test |
| `test_gmmu_tlm_timing.cc` | 6 | TLB hit / TLB miss + page walk / TLB fill / 32-entry overflow / cross-page |
| `test_sdma_engine_timing.cc` | 5 | TLB hit/miss cycle + fence complete cycle + translate_cb latency + 公共 API 零回归 + last_descriptor_complete_cycle_ 记录 (v0.2 简化) |
| `test_vram_controller_tlm.cc` | 5 | row hit/miss / bandwidth upper limit / invalidate / stats |
| `test_crossbar_timing.cc` | 3 | 4 routes / busy port wait / stats_arb_cycles |

### 8.2 E2E 性能回归测试 (≥ 5 cases)

| 测试文件 | cases | 覆盖 |
|----------|-------|------|
| `test_dgpu_soc_timing_h2d.cc` | 1 | 4MB H2D throughput + fence cycle accuracy |
| `test_dgpu_soc_timing_d2h.cc` | 1 | 4MB D2H throughput + MSI-X cycle |
| `test_dgpu_soc_timing_tlb.cc` | 1 | TLB hit rate (sequential access) |
| `test_dgpu_soc_timing_concurrent.cc` | 1 | SDMA + GMMU + Host egress 并发 cycle 验证 |
| `test_dgpu_soc_timing_inv.cc` | 1 | 6 条新 Invariants 全验证 |

### 8.3 minimal_v1 零回归验证

- `[minimal_dgpu_soc]` 41 assertions 全绿
- `[abi][minimal_dgpu_soc]` 28 assertions 全绿
- `[pcie-memory]` 24 cases 全绿

---

## 9. 兼容性矩阵

| 项 | 影响 | 验证方法 |
|----|------|----------|
| 23 ABI 签名 | 0 修改 | `git diff HEAD -- include/abi/cpptlm_emulator.h` 仅包含既有内容 |
| DGpuBoard v2.0.2 API | 0 修改(仅新增 `simulation_mode_` 字段) | 既有 `[dgpu_board]` 测试全绿 |
| minimal_v1 functional-mode 行为 | 0 修改 | `[minimal_dgpu_soc]` 41 assertions 全绿 |
| SdmaEngineTLM 5 端口 API | 0 修改 (v0.2 **不**新增 AXI master port, per H3) | 既有 `[sdma]` 测试全绿 |
| MemoryTLM 公共 API | 0 修改(仅新增 `set_timing_params`) | 既有 `[memory_tlm]` 测试全绿 |
| GmmuTLM 公共 API | 0 修改(仅新增 `translate_timing` + `set_timing_params`) | 既有 `[gmmu]` 测试全绿 |
| JSON schema | 扩展(顶层 `simulation_mode` + 新模块 `VramControllerTLM`) | 既有 JSON 文件 `validate_topology` 全绿 |
| 既有 functional 配置 | 兼容(默认 `simulation_mode="functional"`) | `dgpu_soc_minimal_v1.json` 0 修改 |

---

## 10. 风险与回退

| 风险 | 回退策略 |
|------|----------|
| R1 cycle 参数失真 | 通过文档明示"仿真建模值";提供 analytical 公式对照(per arch doc §12.2) |
| R2 last_descriptor_complete_cycle_ 累加不准确 (TLB hit/miss path 与 analytical model ±5%) | 调 `set_cycle_accounting_enabled(false)` 立即退回 functional-mode 行为 (无 cycle accounting) |
| R3 TLB 简化失真 | v0.1 文档明示"32-entry 直接映射,无 LRU";D4 加完整 LRU |
| R4 minimal_v1 回归 | `[dgpu_soc_timing]` 标签独立测试套件;`simulation_mode` default=functional |
| R5 23 ABI 破坏 | 严格约束:`simulation_mode` 仅 DGpuBoard 内部字段,不暴露 ABI |
| R6 cycle advance 漂移 | Inv-3 强制 board 统一 advance |
| R7 VramControllerTLM bandwidth 拖慢 | `use_zero_delay_for_test=true` 旁路 |
| R8 实施工作量超出预期 | tasks.md T1-T5 分阶段,可单独推迟 D3/VramControllerTLM |
| R9 CacheReqBundle 无 rid/user 字段 (v0.2 删超) | SDMA 沿用既有 5-port,**不**新增端口 |

---

## 11. 实施步骤总结

| 任务 | 工作量 | 依赖 | 关键产出 |
|------|--------|------|----------|
| T0 OpenSpec change 立项 + ADR-DGPU-11 | 1d | - | proposal.md + ADR |
| T1 MemoryTLM cycle-approximate | 1d | T0 | `defer_response()` + 5 时序参数 |
| T2 GmmuTLM TLB + page walk | 1d | T0 | `translate_timing()` + 32-entry TLB |
| T3 SdmaEngineTLM cycle accounting (v0.2 简化) | 0.5d | T1+T2 | translate_cb_ 内调 translate_timing 累加 lat + last_descriptor_complete_cycle_ 记录 (无新端口/无 outstanding/无 rid) |
| T4 VramControllerTLM 新建 | 1d | T1 | 行缓冲 + bandwidth |
| T5 CrossbarTLM C++ 扩展 + DGpuBoard::init_timing_mode | **2d** | T1-T4 | 4 routes JSON (GMMU 不在 Crossbar) + CrossbarTLM C++ 改动 (`set_arb_latency` + `stats_*`) + `simulation_mode` 切换 |
| T6 dgpu_soc_timing_v1.json + minimal test | 1d | T5 | JSON + 30 unit cases |
| T7 E2E 性能回归测试 | 2d | T6 | 5 E2E cases + Inv-6 |
| T8 文档同步 (arch doc §14 + AGENTS.md 状态) | 0.5d | T7 | 文档全套同步 |
| **总计** | **10.5d ≈ 2 周** | - | - |

详细 task 拆分见 [tasks.md](./tasks.md)。