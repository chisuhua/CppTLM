# Design: gpu_soc Phase 8.B — 核心仿真

> **Status**: 🟢 Ready to Start (Phase 8.A ✅ Archived, `e8280fe`)
> **Parent**: `proposal.md` (2026-06-24-gpu-soc-phase8b-core)

---

## 1. SM 内部 4 级层次（从 Phase 8.A 的 black-box 升级到 microarch 仿真）

```
Phase 8.A (现状)
  ComputeCluster
  └── GpuComputeUnitTLM [black-box 1 cycle/tick]

Phase 8.B (目标)
  ComputeCluster
  └── SubCoreTLM [black-box pipe, 4× WarpSchedulerTLM]
        ├── WarpSchedulerTLM ×4 (warpid % 4 静态绑定)
        │     └── 5-warp CGGTY 阈值 (4→5 加速 6×)
        ├── ScoreboardTLM (≥12 entries)
        ├── PipelineTLM (5+V 抽象, 分数 cycle)
        │     ├── P0 INT/FP32
        │     ├── V-pipe SIMD
        │     ├── P1 FP64
        │     ├── P2 SFU/MUFU
        │     ├── P3 LSU
        │     └── P4 TensorCore
        └── TensorCoreTLM (6 精度, 29/23 cyc 统一)
```

**L2PartitionTLM** 挂在 GpuCluster 级别（per GPC L2 slice），不在 SM 内部：
```
GpuCluster
├── GpcCluster × N
│   └── L2PartitionTLM (该 GPC 的 L2 slice)
└── TpcCluster × M × N
    └── ...
```

---

## 2. 6 个核心模块详细设计（含 PTX-EMU 接口对齐）

> **2026-07-03 修订**：基于审查 PTX-EMU 仓库 8 个关键头文件 + ADR-NV-02 D1-Full 决策，6 模块接口增加 PTX-EMU 注入接口继承 + Adapter 层设计。
> **核心原则**：Phase 8.B 模块**零 PTX-EMU 依赖**——CppTLM 内部接口 (`tlm::I*Internal`) 与 PTX-EMU 注入接口 (`IScoreboard` 等) 通过 Adapter 桥接。

### 2.1 SubCoreTLM（SMContext 包裹 + 双模式 tick）

```cpp
class SubCoreTLM : public ChStreamModuleBase {
public:
    SubCoreTLM(const std::string& name, EventQueue* eq, uint32_t num_warps);
    std::string get_module_type() const override { return "SubCoreTLM"; }

    // D1-Full: 包裹真实 SMContext
    void set_sm_context(ptxsim::SMContext* sm_ctx);  // 仅 F12b-LD 集成时非空
    ptxsim::SMContext* get_sm_context() const;

    void tick() override;
    uint64_t get_current_cycle() const { return cycle_; }
    uint32_t num_active_warps() const;
private:
    // 独立模式 (sm_ctx_=nullptr): 使用内部 CppTLM 组件
    std::array<std::unique_ptr<WarpSchedulerTLM>, 4> schedulers_;
    std::unique_ptr<ScoreboardTLM> scoreboard_;
    std::unique_ptr<PipelineTLM> pipeline_;
    std::unique_ptr<TensorCoreTLM> tensor_core_;

    // D1 模式 (sm_ctx_≠nullptr): 注入 PTX-EMU 组件
    ptxsim::SMContext* sm_ctx_ = nullptr;
    uint64_t cycle_ = 0;
};
```

**tick() 双模式**：
- `sm_ctx_ == nullptr`（独立模式）：4 scheduler → pipeline/tc → scoreboard → 推进 cycle
- `sm_ctx_ != nullptr`（D1 模式）：调用 `sm_ctx_->exe_once()`，SMContext 内部使用注入的 WarpScheduler+Scoreboard+Pipeline+TC

### 2.2 WarpSchedulerTLM（重命名 MinimalWarpSchedulerTLM + CGGTY）

```cpp
class WarpSchedulerTLM : public ChStreamModuleBase {
public:
    WarpSchedulerTLM(const std::string& name, EventQueue* eq, uint32_t max_warps);
    std::string get_module_type() const override { return "WarpSchedulerTLM"; }
    // PTX-EMU 兼容接口 (保留 uint32_t warp_id, F12a 已对齐)
    void add_warp(uint32_t warp_id);
    void remove_warp(uint32_t warp_id);
    std::optional<uint32_t> schedule_next();
    bool all_warps_finished() const;
    void update_state(uint32_t warp_id, bool blocked, uint32_t blocked_cycles);
    // CGGTY 阈值简化模型（按 SM_120 paper Fig. 10）
    uint32_t scheduling_latency_cycles(uint32_t active_warps, uint32_t dep_chain_cyc) const;
    void tick() override;
private:
    uint32_t max_warps_;
    static constexpr uint32_t CGGTY_THRESHOLD = 5;
    static constexpr double CGGTY_SPEEDUP = 6.0;
    struct WarpState { bool blocked = false; uint32_t blocked_cycles_remaining = 0; };
    std::unordered_map<uint32_t, WarpState> warps_;
    std::vector<uint32_t> order_;
    size_t next_idx_ = 0;
};
```

**迁移路径**：重命名 `MinimalWarpSchedulerTLM` → `WarpSchedulerTLM`，保留旧注册项 + 标注 `[[deprecated]]`，新增 CGGTY 阈值逻辑 + priority 队列。

### 2.3 ScoreboardTLM（实现 `tlm::IScoreboardInternal`）

```cpp
// 新增内部接口: include/tlm/gpu/scoreboard_interface.hh
class IScoreboardInternal {
public:
    virtual ~IScoreboardInternal() = default;
    virtual bool has_free_entry() const = 0;
    virtual bool allocate(uint32_t sb_id) = 0;
    virtual bool release(uint32_t sb_id) = 0;
    virtual void tick() = 0;
};

class ScoreboardTLM : public ChStreamModuleBase, public IScoreboardInternal {
public:
    ScoreboardTLM(const std::string& name, EventQueue* eq, uint32_t entries);
    std::string get_module_type() const override { return "ScoreboardTLM"; }
    bool has_free_entry() const override;
    bool allocate(uint32_t sb_id) override;
    bool release(uint32_t sb_id) override;
    void tick() override;
private:
    uint32_t bitmask_ = 0;   // O(1) 位操作 (was std::set)
    uint32_t entries_;        // 默认 12
};
```

**PTX-EMU 对接**：`CppTLMScoreboardAdapter : public IScoreboard` 桥接 `IScoreboard::allocate(reg_id, warp_id)` ↔ `IScoreboardInternal::allocate(sb_id)`。

### 2.4 TensorCoreTLM（实现 `tlm::ITensorCoreTimingInternal`）

```cpp
// 新增内部接口: include/tlm/gpu/tensor_core_interface.hh
enum class TcPrecision : uint32_t { FP4=0, FP6=1, FP8=2, FP16=3, BF16=4, TF32=5 };
// 枚举值 0-5 与 PTX-EMU TcPrecision 一致，Adapter static_cast 转换

class ITensorCoreTimingInternal {
public:
    virtual ~ITensorCoreTimingInternal() = default;
    virtual uint32_t get_latency(TcPrecision prec) const = 0;
    virtual uint32_t get_throughput_cycles(TcPrecision prec) const = 0;
};

class TensorCoreTLM : public ChStreamModuleBase, public ITensorCoreTimingInternal {
public:
    TensorCoreTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "TensorCoreTLM"; }
    uint32_t get_latency(TcPrecision prec) const override { return 29; }  // 统一管线
    uint32_t get_throughput_cycles(TcPrecision prec) const override { return 23; }
private:
    static constexpr uint32_t UNIFIED_LATENCY = 29;
    static constexpr uint32_t UNIFIED_THROUGHPUT = 23;
};
```

**SM_120 paper 关键发现**（C2）：12 种非 FP64 精度共享 29/23 cyc。Phase 8.B 简化模型按此实现。

**PTX-EMU 对接**：`CppTLMTensorCoreAdapter : public ITensorCoreTiming` 直接转发（枚举值一致）。

### 2.5 PipelineTLM（实现 `tlm::IPipelineLatencyInternal`）

```cpp
// 新增内部接口: include/tlm/gpu/pipeline_interface.hh
enum class PipelineId : uint32_t { P0_INT_FP32=0, V_SIMD=1, P1_FP64=2, P2_SFU=3, P3_LSU=4, P4_TC=5 };
// 枚举值 0-5 与 PTX-EMU PipelineId 一致，Adapter static_cast 转换

class IPipelineLatencyInternal {
public:
    virtual ~IPipelineLatencyInternal() = default;
    virtual double get_fractional_cycles(const std::string& instruction, PipelineId pipe_id) const = 0;
};

class PipelineTLM : public ChStreamModuleBase, public IPipelineLatencyInternal {
public:
    PipelineTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "PipelineTLM"; }
    double get_fractional_cycles(const std::string& instruction, PipelineId pipe_id) const override;
private:
    std::unordered_map<std::string, double> latency_table_;
};
```

**关键查表项**（节选）：
| Instruction | Pipeline | Cycles |
|------------|----------|--------|
| IADD3 | P0_INT_FP32 | 2.22 |
| FFMA | P0_INT_FP32 | 4.22 |
| VIADD.U8x4 | V_SIMD | 4.0 |
| DFMA | P1_FP64 | 64.13 |
| MUFU.RCP | P2_SFU | 44.28 |
| LDG | P3_LSU | ~30 |
| HMMA | P4_TC | 29 |

**PTX-EMU 对接**：`CppTLMPipelineAdapter : public IPipelineLatencyProvider` 转发 `get_fractional_cycles_by_type(int stmt_type, PipelineId pipe)` → `get_fractional_cycles(instr, tlm::PipelineId)`。

### 2.6 L2PartitionTLM（独立，无 PTX-EMU 耦合）

```cpp
class L2PartitionTLM : public ChStreamModuleBase {
public:
    L2PartitionTLM(const std::string& name, EventQueue* eq,
                   uint32_t slices, uint32_t capacity_mb, bool partitioned);
    std::string get_module_type() const override { return "L2PartitionTLM"; }
    // 同 GPC slice = 79 cyc, 跨 GPC slice = 180 cyc (按 SM_120 paper)
    uint32_t access_latency(uint32_t gpc_id, uint32_t slice_id) const;
private:
    uint32_t slices_;
    uint32_t capacity_mb_;
    bool partitioned_;   // Hopper 双片 vs GB203 单片
    uint32_t slices_per_gpc_;  // 派生: slices / num_gpc
};
```

**L2PartitionTLM 仅通过 MemoryBridge 路径使用**（F12b-LD），无 PTX-EMU 直接耦合。
```

---

## 3. Adapter 层设计（Task 15, F12b-LD 集成时编译）

Adapter 桥接 CppTLM 模块 ↔ PTX-EMU 注入接口。每个 Adapter ~30-50 LOC，位于 `include/tlm/gpu/adapter/`。

```
include/tlm/gpu/adapter/          (新目录, 仅 libcpptlm_cudart.so 编译)
├── cpptlm_warp_scheduler_adapter.hh/.cc   ─ 桥接 WarpScheduler ↔ WarpSchedulerTLM
├── cpptlm_scoreboard_adapter.hh/.cc       ─ 桥接 IScoreboard ↔ ScoreboardTLM
├── cpptlm_pipeline_adapter.hh/.cc         ─ 桥接 IPipelineLatencyProvider ↔ PipelineTLM
└── cpptlm_tensor_core_adapter.hh/.cc      ─ 桥接 ITensorCoreTiming ↔ TensorCoreTLM
```

**CppTLMWarpSchedulerAdapter**（桥接 `WarpContext*` ↔ `uint32_t`）：

```cpp
class CppTLMWarpSchedulerAdapter : public WarpScheduler {
    WarpSchedulerTLM* tlm_scheduler_;
    std::unordered_map<uint32_t, WarpContext*> warp_map_;
public:
    void add_warp(WarpContext* warp) override {
        warp_map_[warp->get_physical_warp_id()] = warp;
        tlm_scheduler_->add_warp(warp->get_physical_warp_id());
    }
    WarpContext* schedule_next() override {
        auto id = tlm_scheduler_->schedule_next();
        return id.has_value() ? warp_map_[id.value()] : nullptr;
    }
    // ... 其余接口类似转发
};
```

**CppTLMScoreboardAdapter**（关键：`warp_id` 存 Adapter 内部 map，CppTLM 内部不依赖）：

```cpp
class CppTLMScoreboardAdapter : public IScoreboard {
    tlm::ScoreboardTLM* impl_;
    std::unordered_map<uint32_t, uint32_t> reg_to_warp_;  // reg_id → warp_id
public:
    bool allocate(uint32_t reg_id, uint32_t warp_id) override {
        reg_to_warp_[reg_id] = warp_id;
        return impl_->allocate(reg_id);
    }
    // ...
};
```

---

## 4. 5 类 microbenchmark JSON 配置

5 个 JSON config 模板 + 1 个集成 config `gpu_soc_phase8b.json`：

```json
{
  "name": "gpu_soc Phase 8.B microbenchmark",
  "modules": [
    { "name": "gpu_soc", "type": "GpuSocTLM" },
    { "name": "kernel_launch", "type": "KernelLaunchTLM",
      "params": {"kernel_pattern": "GEMM", "M": 4096, "N": 4096, "K": 4096} },
    { "name": "compute_unit_0", "type": "GpuComputeUnitTLM" },
    { "name": "subcore_0", "type": "SubCoreTLM", "params": {"num_warps": 32} },
    { "name": "warp_sched_0", "type": "WarpSchedulerTLM", "params": {"max_warps": 12} },
    { "name": "scoreboard_0", "type": "ScoreboardTLM", "params": {"entries": 12} },
    { "name": "tensor_core_0", "type": "TensorCoreTLM" },
    { "name": "pipeline_0", "type": "PipelineTLM" },
    { "name": "l2_partition", "type": "L2PartitionTLM",
      "params": {"slices": 9, "capacity_mb": 96, "partitioned": false} }
  ]
}
```

---

## 4. Python gpgpu-sim 对照测试

`test/python/test_gpgpu_sim_comparison.py`（5 类场景 pytest）：

```python
@pytest.mark.parametrize("pattern,workload_factory,baseline_gbs", [
    ("GEMM", lambda: gw.GEMM(m=4096, n=4096, k=4096, dtype="FP16"), 700),
    ("FlashAttn", lambda: gw.FlashAttention(batch=8, head=16, seq_len=512), 470),
    ("vector_add", lambda: gw.VectorAdd(n=1024*1024), 1176),
    ("stencil", lambda: gw.Stencil3D(n=512, points=7), 940),
    ("sparse_spmv", lambda: gw.SparseSpMV(rows=10000, cols=10000, density=0.01), 230),
])
def test_bandwidth_within_15pct(pattern, workload_factory, baseline_gbs):
    sim = gs.simulate(topo=nv.gb203_consumer(), workload=workload_factory(),
                      duration_cycles=1_000_000, metrics=["bandwidth"])
    measured = sim.report()["bandwidth"]
    error_pct = abs(measured - baseline_gbs) / baseline_gbs * 100
    assert error_pct <= 15, f"{pattern}: {measured} vs {baseline_gbs} ({error_pct:.1f}%)"
```

注意：`cpptlm.gpu_workload` 和 `cpptlm.gpu_soc` Python 子包是 8.C 任务。但 8.B 的 microbenchmark 提前需要这些 stub。**执行顺序**：8.B 实施时**临时**手动 import 或 stub，8.C 完整实现后切换。

---

## 5. 与 PTX-EMU 集成（D1-Full, 依赖 ADR-NV-02）

> **2026-07-03 修订**：基于 ADR-NV-02，Phase 8.B 目标升级为 **D1-Full**——WarpScheduler + Scoreboard + Pipeline + TensorCore 全部注入 PTX-EMU。

### 5.1 SMContext 注入流程（F12b-LD 完成后）

```cpp
// libcpptlm_cudart.so 初始化
auto* sm = gpu_context->get_sm(0);

// 创建 CppTLM 模块
auto scheduler     = std::make_unique<WarpSchedulerTLM>("ws", eq, 64);
auto scoreboard    = std::make_unique<ScoreboardTLM>("sb", eq, 12);
auto pipeline      = std::make_unique<PipelineTLM>("pipe", eq);
auto tensor_core   = std::make_unique<TensorCoreTLM>("tc", eq);

// 创建 Adapter
auto scheduler_adapter = std::make_unique<CppTLMWarpSchedulerAdapter>(scheduler.get());
auto scoreboard_adapter = std::make_unique<CppTLMScoreboardAdapter>(scoreboard.get());
auto pipeline_adapter   = std::make_unique<CppTLMPipelineAdapter>(pipeline.get());
auto tc_adapter         = std::make_unique<CppTLMTensorCoreAdapter>(tensor_core.get());

// D1-Full 注入
sm->set_warp_scheduler(std::move(scheduler_adapter));
sm->set_scoreboard(scoreboard_adapter.get());
sm->set_pipeline_latency_provider(pipeline_adapter.get());
sm->set_tensor_core_timing(tc_adapter.get());
```

### 5.2 验证双轨制

| 层次 | 阶段 A (无 PTX-EMU) | 阶段 B (F12b-LD 后) |
|------|--------------------|---------------------|
| **Level 1** | 合成输入验证 6 模块逻辑 | PTX-EMU 集成 subcase |
| **Level 2** | 合成 workload → CppTLM NoC → bandwidth | 真实 CUDA kernel `.cu` → PTX-EMU → MemoryBridge → bandwidth |
| **Level 3** | vs gpgpu-sim（同 workload 参数） | vs gpgpu-sim（同 `.cu` kernel）+ vs standalone PTX-EMU (±10%) |

### 5.3 接口契约

| CppTLM 内部接口 | PTX-EMU 注入接口 | Adapter |
|----------------|-----------------|---------|
| `tlm::IScoreboardInternal` | `IScoreboard` (PTX-EMU `include/ptxsim/scoreboard_interface.h`) | `CppTLMScoreboardAdapter` |
| `tlm::IPipelineLatencyInternal` | `IPipelineLatencyProvider` (PTX-EMU `include/ptxsim/pipeline_interface.h`) | `CppTLMPipelineAdapter` |
| `tlm::ITensorCoreTimingInternal` | `ITensorCoreTiming` (PTX-EMU `include/ptxsim/tensor_core_interface.h`) | `CppTLMTensorCoreAdapter` |
| `WarpSchedulerTLM` (CppTLM) | `WarpScheduler` (PTX-EMU `include/ptxsim/warp_scheduler.h`) | `CppTLMWarpSchedulerAdapter` |

**枚举一致性**：`tlm::PipelineId`(0-5) = `::PipelineId`(0-5) · `tlm::TcPrecision`(0-5) = `::TcPrecision`(0-5) —— Adapter `static_cast` 零开销转换。

---

## 6. 性能 M2 验收

- 1 GB203 (110 SM, 9 GPC × 6 TPC × 2 SM) × 1M cycles < 60s（单核）
- 5 类 microbenchmark 端到端 < 300s/场景

---

## 7. 关联 spec 章节

- `gpu-soc-architecture.md §3.2` 14 个新模块清单（Phase 8.B 部分）
- `gpu-soc-architecture.md §5.2` Phase 8.B 验收点 M2
- `gpu-soc-architecture.md §6` 验证策略
