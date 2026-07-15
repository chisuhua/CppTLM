# gpu_soc Phase 8.B Implementation Plan (D1-Full)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> **Revision**: 2026-07-14 — D1-Full 策略升级（ADR-NV-02 Status Update）

**Goal:** 实施 `openspec/changes/2026-06-24-gpu-soc-phase8b-core` change —— 6 个核心模块（Scoreboard / WarpScheduler / Pipeline / TensorCore / L2Partition / SubCore）+ 4 个 Adapter + 5 类 microbenchmark + gpgpu-sim 区间对照；M2 验收（带宽 ±15%, 1 GB203 × 1M < 60s）。

**Architecture:** Phase-accurate sub-core 仿真。SM 内部从 black-box（8.A）升级到 4 级层次（SubCore → 4 WarpScheduler + Scoreboard + Pipeline + TensorCore）。L2PartitionTLM 挂在 GpuCluster per GPC。CGGTY 5-warp 阈值（按 SM_120 paper Fig. 10）+ 6 精度 TC 统一管线（29/23 cyc）。

**D1-Full 策略**：6 个核心模块同时支持**两种模式**：
- **独立模式**（Phase A）：CppTLM 内部 `uint32_t` 接口，合成 workload 验证逻辑
- **D1-Full 注入模式**（Phase B）：通过 `tlm::I*Internal` 接口 + 4 个 Adapter 桥接到 PTX-EMU 的 `IScoreboard` / `IPipelineLatencyProvider` / `ITensorCoreTiming` / `WarpScheduler` 纯虚接口

**关联 ADR**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md`（2026-07-14 Status Update: D1-Lite → D1-Full）
**PTX-EMU 改造综合任务书**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`

**Tech Stack:**
- C++17（CppTLM 核心）
- Catch2 v3.7.0（测试）
- Python pytest（gpgpu-sim 对照）
- pre-commit + clang-format

---

## File Structure (新增/修改)

### 新增头文件（6 个 + 2 个 bundle + 4 个 internal 接口 + 4 个 Adapter = 16 个）
```
include/tlm/gpu/
├── scoreboard_tlm.hh           (Task 9)
├── warp_scheduler_tlm.hh       (Task 10)
├── pipeline_tlm.hh             (Task 11)
├── tensor_core_tlm.hh          (Task 12)
├── l2_partition_tlm.hh         (Task 13)
├── subcore_tlm.hh              (Task 14)
├── iscoreboard_internal.hh     (Task 9 — D1-Full 内部接口)
├── ipipeline_internal.hh       (Task 11 — D1-Full 内部接口)
├── itensorcore_internal.hh     (Task 12 — D1-Full 内部接口)
├── scoreboard_adapter.hh       (Task 15a — D1-Full Adapter)
├── pipeline_adapter.hh         (Task 15a — D1-Full Adapter)
├── tensorcore_adapter.hh       (Task 15a — D1-Full Adapter)
└── warp_scheduler_adapter.hh   (Task 15a — D1-Full Adapter)
include/bundles/
├── warp_state_bundle.hh        (Task 9)
└── tensor_core_bundle.hh       (Task 12)
```

### 新增 C++ 实现
```
src/tlm/gpu/{scoreboard,warp_scheduler,pipeline,tensor_core,l2_partition,subcore}_tlm.cc
```

### 新增测试（7 个 .cc + 1 个 Python）
```
test/test_{scoreboard,warp_scheduler,pipeline,tensor_core,l2_partition,subcore}_tlm.cc   (Task 9-14)
test/test_gpu_soc_phase8b.cc                                                                    (Task 15)
test/python/test_gpgpu_sim_comparison.py                                                        (Task 15)
```

### 新增配置
```
configs/templates/gpu_soc/gpu_soc_phase8b.json  (Task 15)
```

### 新增微架构 doc（6 个）
```
docs/soc_arch/modules/gpu-{subcore,warp-scheduler,scoreboard,tensor-core,pipeline,l2-partition}.md  (Task 16)
```

### 修改文件
```
include/chstream_register.hh                              (+6 行注册)
include/tlm/cluster/compute_cluster.hh                    (集成 SubCoreTLM)
include/tlm/cluster/gpu_cluster.hh                        (集成 L2PartitionTLM)
```

---

## 实施任务（9 个 + 2 个验收节点 = 11 个任务）

> **TDD 模式说明**：完整 TDD 代码见主 plan `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.B 节。本 plan 列出**文件、关键 API、commit 消息**。

### Task 9: ScoreboardTLM ≥12 entries 单元测试

**关键 API**（独立模式 + D1-Full 内部接口）：
```cpp
// 独立模式接口 (CppTLM 内部 uint32_t)
class ScoreboardTLM : public ChStreamModuleBase {
    ScoreboardTLM(const std::string& name, EventQueue* eq, uint32_t entries = 12);
    bool has_free_entry() const;
    bool allocate(uint32_t sb_id);
    bool release(uint32_t sb_id);
    void tick();
};

// D1-Full 内部接口 (桥接到 PTX-EMU IScoreboard)
class tlm::IScoreboardInternal {
public:
    virtual ~IScoreboardInternal() = default;
    virtual bool has_free_entry() const = 0;
    virtual bool allocate(uint32_t reg_id, uint32_t warp_id) = 0;  // 双参数 match PTX-EMU
    virtual bool release(uint32_t reg_id, uint32_t warp_id) = 0;
    virtual void tick() = 0;
};
// ScoreboardTLM : public IScoreboardInternal
// 12 个 allocate 全成功，第 13 个 allocate 失败
```

**验收**：`./build/bin/cpptlm_tests "[gpu][sb]"` PASS

**Commit**：`feat(tlm/gpu): ScoreboardTLM ≥12 entries (Phase 8.B Task 9)`

---

### Task 10: WarpSchedulerTLM CGGTY 5-warp 阈值

**关键 API**：
```cpp
class WarpSchedulerTLM : public ChStreamModuleBase {
    WarpSchedulerTLM(const std::string& name, EventQueue* eq, uint32_t max_warps = 12);
    uint32_t scheduling_latency_cycles(uint32_t active_warps, uint32_t dep_chain_cyc) const;
};
// active_warps < 5 → dep_chain_cyc; >= 5 → dep_chain_cyc / 6
```
> **D1-Full**: `WarpSchedulerTLM` 本身保留 `uint32_t` 接口；Task 15a 的 `CppTLMWarpSchedulerAdapter : public ptxsim::WarpScheduler` 负责 `WarpContext* ↔ uint32_t` 映射。
```

**验收**：`./build/bin/cpptlm_tests "[gpu][sched]"` PASS (4 warps/268cyc=268, 5 warps/268cyc=44)

**Commit**：`feat(tlm/gpu): WarpSchedulerTLM CGGTY 5-warp threshold (Phase 8.B Task 10)`

---

### Task 11: PipelineTLM 5+V 抽象 + 分数 cycle

**关键 API**（独立模式 + D1-Full 内部接口）：
```cpp
enum class PipelineId { P0_INT_FP32, V_SIMD, P1_FP64, P2_SFU, P3_LSU, P4_TC };

// 独立模式接口
class PipelineTLM : public ChStreamModuleBase {
    PipelineTLM(const std::string& name, EventQueue* eq);
    double execute(const std::string& instruction, PipelineId pipe);
};

// D1-Full 内部接口 (桥接到 PTX-EMU IPipelineLatencyProvider)
class tlm::IPipelineLatencyInternal {
public:
    virtual ~IPipelineLatencyInternal() = default;
    virtual double get_fractional_cycles(
        const std::string& instruction, tlm::PipelineId pipe_id) const = 0;
    virtual double get_fractional_cycles_by_type(
        int statement_type, tlm::PipelineId pipe_id) const = 0;
};
// PipelineTLM : public IPipelineLatencyInternal
// 查表: IADD3=2.22, FFMA=4.22, VIADD.U8x4=4.0, DFMA=64.13, MUFU.RCP=44.28, LDG=30, HMMA=29
```

**验收**：`./build/bin/cpptlm_tests "[gpu][pipe]"` PASS

**Commit**：`feat(tlm/gpu): PipelineTLM 5+V fractional cycle (Phase 8.B Task 11)`

---

### Task 12: TensorCoreTLM 6 精度统一管线

**关键 API**（独立模式 + D1-Full 内部接口）：
```cpp
enum class TcPrecision { FP4, FP6, FP8, FP16, BF16, TF32 };

// 独立模式接口
class TensorCoreTLM : public ChStreamModuleBase {
    TensorCoreTLM(const std::string& name, EventQueue* eq);
    uint32_t latency(TcPrecision) const { return 29; }  // 12 精度统一
    uint32_t throughput_cyc(TcPrecision) const { return 23; }
};

// D1-Full 内部接口 (桥接到 PTX-EMU ITensorCoreTiming)
class tlm::ITensorCoreTimingInternal {
public:
    virtual ~ITensorCoreTimingInternal() = default;
    virtual uint32_t get_latency(tlm::TcPrecision prec) const = 0;
    virtual uint32_t get_throughput_cycles(tlm::TcPrecision prec) const = 0;
};
// TensorCoreTLM : public ITensorCoreTimingInternal
```

**验收**：`./build/bin/cpptlm_tests "[gpu][tc]"` PASS（6 精度都返回 29/23）

**Commit**：`feat(tlm/gpu): TensorCoreTLM 6-precision unified (Phase 8.B Task 12)`

---

### Task 13: L2PartitionTLM multi-slice 近/远分区

**关键 API**：
```cpp
class L2PartitionTLM : public ChStreamModuleBase {
    L2PartitionTLM(const std::string& name, EventQueue* eq,
                   uint32_t slices, uint32_t capacity_mb, bool partitioned);
    uint32_t access_latency(uint32_t gpc_id, uint32_t slice_id) const;
};
// 同 GPC slice = 79 cyc, 跨 GPC = 180 cyc
```

**验收**：`./build/bin/cpptlm_tests "[gpu][l2]"` PASS

**Commit**：`feat(tlm/gpu): L2PartitionTLM multi-slice (Phase 8.B Task 13)`

---

### Task 14: SubCoreTLM black-box pipe 封装（双模式）

**关键 API**：
```cpp
class SubCoreTLM : public ChStreamModuleBase {
    SubCoreTLM(const std::string& name, EventQueue* eq, uint32_t num_warps = 32);
    void tick() override;
    uint64_t get_current_cycle() const;

    // D1-Full 注入模式 (Phase B: F12b-LD 后)
    #ifdef HAS_PTXEMU
    void set_sm_context(ptxsim::SMContext* sm_ctx);
    void set_scoreboard(IScoreboard* sb);
    void set_pipeline_latency_provider(IPipelineLatencyProvider* p);
    void set_tensor_core_timing(ITensorCoreTiming* tc);
    #endif
};
// 独立模式: 内部 4×WarpScheduler + 1×Scoreboard + 1×Pipeline + 1×TC, 合成 workload
// D1-Full 模式 (sm_ctx_ != nullptr): 通过 4 个 setter 注入 PTX-EMU, tick() → sm_ctx_->exe_once()
```

**验收**：`./build/bin/cpptlm_tests "[gpu][subcore]"` PASS

**Commit**：`feat(tlm/gpu): SubCoreTLM black-box pipe wrapper (Phase 8.B Task 14)`

---

### Task 15a: Adapter 层（4 个 PTX-EMU 桥接适配器）

> **依赖**: Task 9-14 全部完成 + PTX-EMU 侧 #1~#4（接口头文件）交付
> **D1-Full**: 本任务实现 CppTLM ↔ PTX-EMU 的桥接层

**Files**:
- Create: `include/tlm/gpu/warp_scheduler_adapter.hh` — `CppTLMWarpSchedulerAdapter : public ptxsim::WarpScheduler`
- Create: `include/tlm/gpu/scoreboard_adapter.hh` — `ScoreboardAdapter : public IScoreboard`（桥接 `ScoreboardTLM` → `IScoreboard`）
- Create: `include/tlm/gpu/pipeline_adapter.hh` — `PipelineAdapter : public IPipelineLatencyProvider`（桥接 `PipelineTLM` → `IPipelineLatencyProvider`）
- Create: `include/tlm/gpu/tensorcore_adapter.hh` — `TensorCoreAdapter : public ITensorCoreTiming`（桥接 `TensorCoreTLM` → `ITensorCoreTiming`）

**关键结构**：
```cpp
// ScoreboardAdapter: 转发 uint32_t → (reg_id, warp_id)
class ScoreboardAdapter : public IScoreboard {
    tlm::IScoreboardInternal* tlm_sb_;
public:
    bool has_free_entry() const override { return tlm_sb_->has_free_entry(); }
    bool allocate(uint32_t reg_id, uint32_t warp_id) override {
        // tlm_sb_ 内部维护 reg_id ↔ (warp_id, dest_reg) 映射
        return tlm_sb_->allocate(reg_id, warp_id);
    }
    bool release(uint32_t reg_id, uint32_t warp_id) override {
        return tlm_sb_->release(reg_id, warp_id);
    }
    void tick() override { tlm_sb_->tick(); }
};

// PipelineAdapter: 转发 PTX-EMU int statement_type → CppTLM PipelineId
class PipelineAdapter : public IPipelineLatencyProvider {
    tlm::IPipelineLatencyInternal* tlm_pipe_;
public:
    double get_fractional_cycles(const std::string& instr, PipelineId pipe) const override {
        return tlm_pipe_->get_fractional_cycles(instr, static_cast<tlm::PipelineId>(pipe));
    }
    double get_fractional_cycles_by_type(int stmt_type, PipelineId pipe) const override {
        return tlm_pipe_->get_fractional_cycles_by_type(stmt_type, static_cast<tlm::PipelineId>(pipe));
    }
};
// static_assert: PipelineId 0-5 枚举值一致性
```

**验证**（独立模式，不依赖 PTX-EMU 编译）：
- `ScoreboardAdapter` 用 Mock `IScoreboardInternal` 验证转发
- `PipelineAdapter` 用 Mock `IPipelineLatencyInternal` 验证转发
- 4 个 `static_assert` 验证枚举值一致性

**Commit**：`feat(tlm/gpu): 4 PTX-EMU adapters for D1-Full injection (Phase 8.B Task 15a)`

---

### Task 15: 5 类 microbenchmark + gpgpu-sim 区间对照

**Files**:
- Create: `configs/templates/gpu_soc/gpu_soc_phase8b.json`
- Create: `test/test_gpu_soc_phase8b.cc`（集成测试）
- Create: `test/python/test_gpgpu_sim_comparison.py`（gpgpu-sim 对照）

**5 类场景**（按 SM_120 paper + Jarmusch 2507.10789 + Luo 2501.12084 baseline）：
| 场景 | 配置 | gpgpu-sim baseline |
|------|------|:---:|
| GEMM | FP16, M=N=K=4096 | 700 GB/s |
| FlashAttn | b=8, h=16, seq=512 | 470 GB/s |
| vector_add | n=1024² | 1176 GB/s |
| stencil | 3D 7-point, N=512³ | 940 GB/s |
| sparse SpMV | 10k×10k, 0.01 | 230 GB/s |

**Python 对照**（`test_gpgpu_sim_comparison.py`）：
```python
@pytest.mark.parametrize("pattern,factory,baseline", [
    ("GEMM", lambda: gw.GEMM(m=4096, n=4096, k=4096, dtype="FP16"), 700),
    ("FlashAttn", lambda: gw.FlashAttention(batch=8, head=16, seq_len=512), 470),
    ("vector_add", lambda: gw.VectorAdd(n=1024*1024), 1176),
    ("stencil", lambda: gw.Stencil3D(n=512, points=7), 940),
    ("sparse_spmv", lambda: gw.SparseSpMV(rows=10000, cols=10000, density=0.01), 230),
])
def test_bandwidth_within_15pct(pattern, factory, baseline):
    sim = gs.simulate(topo=nv.gb203_consumer(), workload=factory(),
                      duration_cycles=1_000_000, metrics=["bandwidth"])
    measured = sim.report()["bandwidth"]
    error_pct = abs(measured - baseline) / baseline * 100
    assert error_pct <= 15, f"{pattern}: {measured} vs {baseline} ({error_pct:.1f}%)"
```

**注意**：8.B 实施时 `cpptlm.gpu_workload` 和 `cpptlm.gpu_soc` Python 子包尚未存在（8.C 才实现），需要**临时**用 `import cpptlm.gpu_workload as gw; gw = ...` stub 或 `from cpptlm.gpu_workload.stub import ...` 占位。8.C 完成后切换。

**验收**：
- `test_gpu_soc_phase8b.cc` 5 类跑通
- `test_gpgpu_sim_comparison.py` 5 类 ±15% 带宽

**Commit**：`feat(gpu_soc): Phase 8.B 5 microbenchmarks + gpgpu-sim ±15% bandwidth (Task 15)`

---

### Task 16: 6 个微架构 doc + 性能 M2 验收 + docs_sync

**Files**: 6 个微架构 doc（每 ~150-300 行）

**验收清单（全部勾选才算 M2 通过）**：
- [ ] `[gpu][subcore][sched][sb][tc][pipe][l2]` 全 pass
- [ ] `test_gpu_soc_phase8b.cc` 5 类 microbenchmark 跑通
- [ ] `test_gpgpu_sim_comparison.py` 5 类 ±15% 带宽
- [ ] 性能：1 GB203 (110 SM) × 1M cycles < 60 秒
- [ ] `docs_sync_check.sh --strict` 0 missing
- [ ] `format.sh --check` clean
- [ ] 现有 `[gpu]` (75 cases) + `[phase7]` (1 case) + `[apu_soc]` 全绿（不破坏 apu_soc，Phase 8.A 也不破坏）

**Commit**：`docs(gpu_soc): Phase 8.B microarchitecture docs + M2 verification (Task 16)`

---

## 验收节点 1：Oracle 审查

### Task 17: Oracle 审查（**新增**）

> **目标**：在归档 OpenSpec change 前，用 Oracle subagent 审查 Phase 8.B 全部 commit + 验收结果。

**Files:**
- Create: `docs/validation/phase8b_oracle_review.md`

- [ ] **Step 1: 收集证据**

```bash
# 收集所有 Phase 8.B 相关 commit
git log --oneline | grep -i "phase 8\|8\.B\|gpu_soc" | head -20

# 跑全量测试
./build/bin/cpptlm_tests --reporter compact > /tmp/phase8b_test_output.txt 2>&1
tail -5 /tmp/phase8b_test_output.txt
# 期望: "All tests passed (N assertions in M test cases)" 其中 M ≥ 703 + 8.B 新增

# 性能数据
time ./build/bin/cpptlm_tests "[gpu][soc][phase8b]" --reporter compact 2>&1 | tail -3
# 期望: < 60 秒

# gpgpu-sim 对照
python -m pytest test/python/test_gpgpu_sim_comparison.py -v 2>&1 | tail -15
# 期望: 5 passed

# 文档同步
./scripts/test/docs_sync_check.sh --strict 2>&1 | tail -3
# 期望: 0 missing

# 格式
./scripts/build/format.sh --check 2>&1 | tail -3
# 期望: clean
```

- [ ] **Step 2: 调用 Oracle subagent**

```
调用 subagent_type="oracle" 提供以下信息:

任务: 审查 Phase 8.B 实施质量 (对应 openspec change 2026-06-24-gpu-soc-phase8b-core)

参考文档:
- Spec: docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md §5.2
- OpenSpec change: openspec/changes/2026-06-24-gpu-soc-phase8b-core/
- 本 plan: docs/superpowers/plans/2026-06-24-gpu-soc-phase8b.md
- 依赖: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/ (M1 已完成)
- ADR: docs/adr/ADR-NV-01-gpu-soc-architecture-target.md

待审查 commit: (从 Step 1 收集的 8 个 commit)
测试基线: 703+7 → 期望 710/710
gpgpu-sim 对照: 5 类 ±15%
性能: 1 GB203 × 1M < 60s

请审查 6 维度:
1. **代码质量**: 6 个新核心模块是否遵循 CppTLM 风格? SubCore 内部 4 scheduler + scoreboard + pipeline + TC 集成是否清晰?
2. **架构一致性**: SubCoreTLM black-box 边界是否清晰? L2PartitionTLM 集成到 GpuCluster 是否破坏 apu_soc?
3. **测试覆盖**: 6 个模块单元测试 + 5 类 microbenchmark + gpgpu-sim 对照 + apu_soc 兼容性?
4. **gpgpu-sim 精度**: 5 类场景带宽是否都在 ±15% 范围内? 哪些类可能超差?
5. **性能**: 1 GB203 × 1M cycles < 60s 是否达标? 是否有瓶颈?
6. **文档同步**: 6 个微架构 doc + AGENTS.md + roadmap 是否同步?

请输出:
- ✅/❌ 6 维度评估
- 🚨 阻塞问题
- 💡 改进建议
- 📊 整体评价: APPROVED / NEEDS_FIX
```

- [ ] **Step 3: 记录 Oracle 审查结果**

写入 `docs/validation/phase8b_oracle_review.md`:
```markdown
# Phase 8.B Oracle 审查报告

**日期**: YYYY-MM-DD
**审查者**: Oracle subagent
**OpenSpec change**: 2026-06-24-gpu-soc-phase8b-core

## 6 维度评估
| 维度 | 评估 | 备注 |
|------|------|------|
| 1. 代码质量 | ✅/❌ | |
| 2. 架构一致性 | ✅/❌ | |
| 3. 测试覆盖 | ✅/❌ | |
| 4. gpgpu-sim 精度 | ✅/❌ | |
| 5. 性能 | ✅/❌ | |
| 6. 文档同步 | ✅/❌ | |

## 阻塞问题
(若无则 "无")

## 改进建议

## 整体评价
**APPROVED** / **NEEDS_FIX**
```

- [ ] **Step 4: 验证 Oracle 评价**

- **APPROVED** → 继续 Task 18
- **NEEDS_FIX** → 回到 Task 9-16 修复后重新 Step 1-3

**Commit**: `docs(validation): Phase 8.B Oracle review report (Task 17)`

---

## 验收节点 2：归档 OpenSpec Change

### Task 18: 归档 OpenSpec Change

> **目标**：Oracle 批准后，把 OpenSpec change 从 `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` 移到 `archive/`，并更新相关文档。

**Files:**
- Move: `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` → `openspec/changes/archive/2026-06-24-gpu-soc-phase8b-core/`
- Modify: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`

- [ ] **Step 1: 验证 Task 17 Oracle 评价为 APPROVED**

```bash
grep "整体评价" docs/validation/phase8b_oracle_review.md
# 必须: **APPROVED**
```

若未 APPROVED，**不要继续**——回到 Task 17 修复后重新审查。

- [ ] **Step 2: 归档 OpenSpec change**

```bash
git mv openspec/changes/2026-06-24-gpu-soc-phase8b-core openspec/changes/archive/2026-06-24-gpu-soc-phase8b-core
ls -la openspec/changes/archive/ | grep gpu-soc-phase8b
# 期望: 看到 2026-06-24-gpu-soc-phase8b-core
```

- [ ] **Step 3: 更新 roadmap 状态**

在 §0 状态摘要表追加 Phase 8.B row:
```markdown
| **Phase 8.B 核心仿真** | ⏳ 进行中 → ✅ 已归档 | ✅ Done | 目标: 708 → 715/715 |
```

在 §2 Phase E 表格中更新 8.B 状态: `8.A (✅ Done) → 8.B (✅ Archived) → 8.C (🔄 Ready)`

- [ ] **Step 4: 提交 + 推送**

```bash
git add openspec/changes/ docs/superpowers/plans/2026-06-20-future-work-roadmap.md docs/validation/phase8b_oracle_review.md
git commit -m "chore(openspec): archive gpu-soc-phase8b-core + update roadmap (D1-Full)

OpenSpec change 2026-06-24-gpu-soc-phase8b-core 已完成 (D1-Full):
- 6 个核心模块 (Scoreboard / WarpScheduler / Pipeline / TensorCore / L2Partition / SubCore)
- 4 个 Adapter (WarpScheduler / Scoreboard / Pipeline / TensorCore)
- 5 类 microbenchmark + gpgpu-sim 区间对照 (±15%)
- 6 个微架构 doc
- M2 验收: 1 GB203 × 1M < 60s, apu_soc 兼容, 测试通过
- Oracle 审查 APPROVED
- ADR-NV-02: D1-Lite → D1-Full (2026-07-14 Status Update)

后续依赖: 2026-06-24-gpu-soc-phase8c-advanced (8.C 高级特性, 3 周)"
git push origin main
```

- [ ] **Step 5: 验证归档 + 推送成功**

```bash
ls openspec/changes/ | grep gpu-soc-phase8b-core
# 期望: 无输出

ls openspec/changes/archive/ | grep gpu-soc-phase8b-core
# 期望: 2026-06-24-gpu-soc-phase8b-core

git log --oneline -5
# 期望: "chore(openspec): archive gpu-soc-phase8b-core"

git status
# 期望: Your branch is up to date with 'origin/main'
```

---

## 整体验收 Gates

- [ ] **G1 单元测试**: `[gpu][subcore][sched][sb][tc][pipe][l2]` 全 pass
- [ ] **G1b Adapter 测试**: `[gpu][adapter]` 4 个 Adapter 转发 test + enum static_assert
- [ ] **G2 集成测试**: `test_gpu_soc_phase8b.cc` 5 类 microbenchmark 跑通
- [ ] **G3 gpgpu-sim 对照**: 5 类带宽 ±15%
- [ ] **G4 性能 M2**: 1 GB203 × 1M < 60s
- [ ] **G5 文档**: 6 个微架构 doc + PTX-EMU 改造任务书 + docs_sync 0 missing
- [ ] **G6 格式**: format.sh --check clean
- [ ] **G7 兼容**: Phase 8.A + apu_soc 全绿（不破坏）
- [ ] **G8 Oracle 审查**: docs/validation/phase8b_oracle_review.md 显示 APPROVED
- [ ] **G9 OpenSpec 归档**: 2026-06-24-gpu-soc-phase8b-core 在 archive/ 目录

## 执行时间线

| Task | 周 | 累计 |
|------|:---:|:---:|
| Task 9 (Scoreboard) | 0.5 | 0.5 |
| Task 10 (WarpScheduler) | 1 | 1.5 |
| Task 11 (Pipeline) | 1.5 | 3 |
| Task 12 (TensorCore) | 0.5 | 3.5 |
| Task 13 (L2Partition) | 0.5 | 4 |
| Task 14 (SubCore) | 1 | 5 |
| Task 15a (Adapter 层) | 0.5 | 5.5 |
| Task 15 (5 microbenchmarks + gpgpu-sim) | 0.5 | 6 |
| Task 16 (6 microarch doc) | 0.5 | 6.5 |
| **Task 17 (Oracle 审查)** | 0.1 | 6.6 |
| **Task 18 (归档)** | 0.1 | **6.8 周** |

并行加速：Task 9-14 可 6 人并行（独立模块）→ 关键路径 ~1.5 周 + 0.5（Adapter）+ 0.5（microbenchmark）= 2.5 周

## 关联文档

- **OpenSpec change**: `openspec/changes/2026-06-24-gpu-soc-phase8b-core/`
- **依赖**: `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/`（M1 必须先完成）
- **Spec**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` §5.2
- **主 plan**: `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.B
- **ADR**: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` + `docs/adr/ADR-NV-02-phase8b-d1-strategy.md`（D1-Full, 2026-07-14 Status Update）
- **PTX-EMU 改造综合任务书**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`
- **D1-Full 协同计划**: `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`
- **roadmap 父文档**: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`
- **下一个 change**: `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/`（依赖本 M2）
