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

## 2. 6 个核心模块详细设计

### 2.1 SubCoreTLM

```cpp
class SubCoreTLM : public ChStreamModuleBase {
public:
    SubCoreTLM(const std::string& name, EventQueue* eq, uint32_t num_warps);
    std::string get_module_type() const override { return "SubCoreTLM"; }
    void tick() override;
    uint64_t get_current_cycle() const { return cycle_; }
    // 测试辅助
    uint32_t num_active_warps() const;
private:
    std::array<std::unique_ptr<WarpSchedulerTLM>, 4> schedulers_;
    std::unique_ptr<ScoreboardTLM> scoreboard_;
    std::unique_ptr<PipelineTLM> pipeline_;
    std::unique_ptr<TensorCoreTLM> tensor_core_;
    uint64_t cycle_ = 0;
};
```

**内部 tick()**：
1. 4 个 WarpScheduler 各自选下一个 warp（round-robin）
2. 选中的 warp 发射到 Pipeline 或 TensorCore
3. 检查 Scoreboard hazard
4. 推进 cycle_

**对外 black-box**：外界只看 `tick()` + `get_current_cycle()`

### 2.2 WarpSchedulerTLM

```cpp
class WarpSchedulerTLM : public ChStreamModuleBase {
public:
    WarpSchedulerTLM(const std::string& name, EventQueue* eq, uint32_t max_warps);
    std::string get_module_type() const override { return "WarpSchedulerTLM"; }
    // CGGTY 阈值简化模型（按 SM_120 paper Fig. 10）
    uint32_t scheduling_latency_cycles(uint32_t active_warps, uint32_t dep_chain_cyc) const;
private:
    uint32_t max_warps_;
    static constexpr uint32_t CGGTY_THRESHOLD = 5;
    static constexpr double CGGTY_SPEEDUP = 6.0;
};
```

**CGGTY 简化模型**：
```cpp
uint32_t WarpSchedulerTLM::scheduling_latency_cycles(uint32_t active_warps, uint32_t dep_chain_cyc) const {
    if (active_warps < CGGTY_THRESHOLD) {
        return dep_chain_cyc;  // 无 latency hiding
    } else {
        return static_cast<uint32_t>(dep_chain_cyc / CGGTY_SPEEDUP);  // 6× 加速
    }
}
```

### 2.3 ScoreboardTLM

```cpp
class ScoreboardTLM : public ChStreamModuleBase {
public:
    ScoreboardTLM(const std::string& name, EventQueue* eq, uint32_t entries);
    std::string get_module_type() const override { return "ScoreboardTLM"; }
    bool has_free_entry() const;
    bool allocate(uint32_t sb_id);
    bool release(uint32_t sb_id);
private:
    std::set<uint32_t> allocated_;
    uint32_t entries_;  // 默认 12
};
```

### 2.4 TensorCoreTLM

```cpp
enum class TcPrecision { FP4, FP6, FP8, FP16, BF16, TF32 };

class TensorCoreTLM : public ChStreamModuleBase {
public:
    TensorCoreTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "TensorCoreTLM"; }
    uint32_t latency(TcPrecision prec) const { return 29; }  // 统一管线
    uint32_t throughput_cyc(TcPrecision prec) const { return 23; }
private:
    static constexpr uint32_t UNIFIED_LATENCY = 29;
    static constexpr uint32_t UNIFIED_THROUGHPUT = 23;
};
```

**SM_120 paper 关键发现**（C2）：12 种非 FP64 精度共享 29/23 cyc。简化模型按此实现。

### 2.5 PipelineTLM

```cpp
enum class PipelineId { P0_INT_FP32, V_SIMD, P1_FP64, P2_SFU, P3_LSU, P4_TC };

class PipelineTLM : public ChStreamModuleBase {
public:
    PipelineTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "PipelineTLM"; }
    double execute(const std::string& instruction, PipelineId pipe);
private:
    // instruction → cycle 查表（按 SM_120 paper Table 3 + 简化）
    std::unordered_map<std::string, double> latency_table_;
};
```

**关键查表项**（节选）：
| Instruction | Pipeline | Cycles |
|------------|----------|--------|
| IADD3 | P0 | 2.22 |
| FFMA | P0 | 4.22 |
| VIADD.U8x4 | V | 4.0 |
| DFMA | P1 | 64.13 |
| MUFU.RCP | P2 | 44.28 |
| LDG | P3 | ~30 |
| HMMA | P4 | 29 |

### 2.6 L2PartitionTLM

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
    bool partitioned_;  // Hopper 双片 vs GB203 单片
};
```

---

## 3. 5 类 microbenchmark JSON 配置

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

## 5. 与 Phase 8.A 集成

- 替换 8.A 的 `GpuComputeUnitTLM`（black-box 1 cyc/tick）→ SubCoreTLM（microarch）
- ComputeCluster 现在内含 1 SubCoreTLM + 1 WarpSchedulerTLM + 1 ScoreboardTLM + 1 PipelineTLM + 1 TensorCoreTLM
- 共享 GpuCluster 的 L2PartitionTLM

---

## 6. 性能 M2 验收

- 1 GB203 (110 SM, 9 GPC × 6 TPC × 2 SM) × 1M cycles < 60s（单核）
- 5 类 microbenchmark 端到端 < 300s/场景

---

## 7. 关联 spec 章节

- `gpu-soc-architecture.md §3.2` 14 个新模块清单（Phase 8.B 部分）
- `gpu-soc-architecture.md §5.2` Phase 8.B 验收点 M2
- `gpu-soc-architecture.md §6` 验证策略
