# Tasks: cpptlm-d1-p1-pipeline-scoreboard — D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 从 `cpptlm-f12b-ld-impl` P1/P2/P3 剥离）
> **Parent**: `proposal.md` + `design.md` (cpptlm-d1-p1-pipeline-scoreboard)
> **前置**: P0 `cpptlm-f12b-ld-impl` 归档 + PTX-EMU 交付 P1 接口
> **总工时**: ~6.5d（CppTLM 端 #C4+#C3+#C5, 与 internal-plan.md Day-by-Day 估算一致）

## ⚠️ 启动条件

- [ ] **P0 已归档**: `cpptlm-f12b-ld-impl` → `openspec/changes/archive/`
- [ ] **PTX-EMU P1 接口已提交**: `include/ptxsim/scoreboard_interface.h` + `pipeline_interface.h` + `tensor_core_interface.h` + `SMContext` 修改（3 setter + exe_once 注入点）
- [ ] **PTX-EMU P1 测试已通过**: PTX-EMU `test_scoreboard_injection` + `test_pipeline_injection` + `test_nullptr_fallback` 全 PASS

---

## Phase 1: 3 核心模块（#C4, ~1d）

### 1.1 ScoreboardTLM（~0.3d）

- [ ] **1.1.1** `include/tlm/gpu/scoreboard_tlm.hh`
  - `class ScoreboardTLM : public IScoreboardInternal`
  - ≥12 entries hazard table
  - `has_free_entry()` / `allocate(reg_id, warp_id)` / `release(reg_id, warp_id)` / `tick()`
- [ ] **1.1.2** `src/tlm/gpu/scoreboard_tlm.cc`
  - 分配策略：FIFO 线性扫描 `active_count_`；释放时设置 `active=false` + 递减计数
  - `tick()` 推进内部计数器
- [ ] **1.1.3** `test/test_scoreboard_tlm.cc`（3 单测）
  - [ ] `allocate_13th_entry_returns_false`（12 entries 满）
  - [ ] `release_frees_entry_for_reuse`
  - [ ] `has_free_entry_after_release_returns_true`

### 1.2 PipelineTLM（~0.3d）

- [ ] **1.2.1** `include/tlm/gpu/pipeline_tlm.hh`
  - `class PipelineTLM : public IPipelineLatencyInternal`
  - `get_fractional_cycles(instruction, pipe_id)` / `get_fractional_cycles_by_type(stmt_type, pipe_id)`
- [ ] **1.2.2** `src/tlm/gpu/pipeline_tlm.cc`
  - 默认延迟表（可配置）：FFMA=4.0, LDG=32.0, HMMA=64.0 等
- [ ] **1.2.3** `test/test_pipeline_tlm.cc`（2 单测）
  - [ ] `ffma_returns_4_cycles_on_P0_INT_FP32`
  - [ ] `unknown_instruction_returns_0`

### 1.3 TensorCoreTLM（~0.3d）

- [ ] **1.3.1** `include/tlm/gpu/tensor_core_tlm.hh`
  - `class TensorCoreTLM : public ITensorCoreTimingInternal`
  - 6 精度（FP16/FP32/FP64/BF16/INT8/INT4）
  - `get_latency(prec)` / `get_throughput_cycles(prec)` / `get_latency_mnk(prec, M, N, K)`
- [ ] **1.3.2** `src/tlm/gpu/tensor_core_tlm.cc`
  - 默认延迟：FP16=4, FP32=8, BF16=4, INT8=2, INT4=1
- [ ] **1.3.3** `test/test_tensor_core_tlm.cc`（2 单测）
  - [ ] `fp16_returns_4_cycles`
  - [ ] `tf32_returns_8_cycles`

### 1.4 12 端点 `static_assert`（~0.1d）

- [ ] **1.4.1** `test/test_12_endpoint_static_assert.cc`
  - `static_assert(static_cast<int>(PipelineId::P0_INT_FP32) == 0);` … × 6
  - `static_assert(static_cast<int>(TcPrecision::FP4) == 0);` … × 6
  - 编译通过 = 验证通过

**Commit**:
```bash
git add include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh \
        include/tlm/gpu/{scoreboard,pipeline,tensorcore}_internal.hh \
        src/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.cc \
        test/test_{scoreboard,pipeline,tensorcore}_tlm.cc \
        test/test_12_endpoint_static_assert.cc
git commit -m "feat(tlm/gpu): ScoreboardTLM + PipelineTLM + TensorCoreTLM (D1-Full P1 #C4)

3 core modules + 12-endpoint PipelineId+TcPrecision static_assert compile-time guards.

Refs:
- 综合计划 §3 Task #C4
- Oracle Gap G1 (12-endpoint sync)
- PTX-EMU P1 interface: scoreboard_interface.h / pipeline_interface.h / tensor_core_interface.h"
```

---

## Phase 2: 4 Adapter（#C3, ~0.5d）

- [ ] **2.1** `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}`
  - WarpContext* ↔ uint32_t warp_id 转换
- [ ] **2.2** `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}`
- [ ] **2.3** `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}`
- [ ] **2.4** `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}`
- [ ] **2.5** `test/test_d1_adapters.cc`（4 Adapter 单测）

**Commit**:
```bash
git add include/tlm/gpu/adapter/cpptlm_{warp_scheduler,scoreboard,pipeline,tensor_core}_adapter.{hh,cc} \
        test/test_d1_adapters.cc
git commit -m "feat(tlm/gpu): 4 Adapters (D1-Full P1 #C3)

WarpScheduler+Scoreboard+Pipeline+TC adapters. WarpContext*<->uint32_t
conversion. nullptr → PTX-EMU InstructionLatencyTable fallback.

Refs: 综合计划 §3 Task #C3, ADR-NV-02 §6.2 G-D1~G-D7"
```

---

## Phase 3: IAsyncCompletion 占位（#C5, ~1h）

- [ ] **3.1** `include/tlm/gpu/async_completion_adapter.hh` + `.cc`
  - `class AsyncCompletionAdapter : public IAsyncCompletion`
  - `register_completion_callback()` 存 map
  - `fire_completion()` 触发（Phase 9+ 才调用，当前占位）
- [ ] **3.2** 编译通过（独立模式 `async_completion_ = nullptr` 无影响）

**Commit**:
```bash
git add include/tlm/gpu/async_completion_adapter.hh src/tlm/gpu/async_completion_adapter.cc
git commit -m "feat(tlm/gpu): IAsyncCompletion placeholder (D1-Full P2 #C5)

Phase 9+ TMA async seam. Phase 8.B = nullptr (no-op)."
```

---

## Phase 4: 集成验证 + KernelLaunchTLM 激活（~0.5d）

- [ ] **4.1** 修改 `include/tlm/gpu/kernel_launch_tlm.hh`：激活 P0 预留的 4 Adapter setter
- [ ] **4.2** 修改 `src/tlm/gpu/kernel_launch_tlm.cc`：`inject_into_sm_context()` 真实注入
- [ ] **4.3** `CMakeLists.txt` 注册全部新目标
- [ ] **4.4** `test/CMakeLists.txt` 注册 5 个新测试
- [ ] **4.5** `test/python/test_gpgpu_sim_comparison.py`（G-D5 5 类 microbenchmark）

---

## P1 验收门

- [ ] **G-D1** 3 纯虚接口编译通过，无 CppTLM 头文件污染 PTX-EMU
- [ ] **G-D2** `set_blocked_cycles_for_active()` 对 warp 内活跃线程正确设置延迟
- [ ] **G-D3** `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle
- [ ] **G-D4** 4 Adapter `static_assert` 12 端点 0-5 双向一致
- [ ] **G-D5** 5 类 microbenchmark vs gpgpu-sim ±15%
- [ ] **G-D6** 4 setter 全 nullptr 时 PTX-EMU 零退化
- [ ] **G-D7** scoreboard/pipeline/TC 任意 nullptr 时回退到 InstructionLatencyTable
- [ ] **G-D8** exe_once() scoreboard stall → re-schedule → release → re-issue 完整循环

## P1 依赖关系图

```
P0 归档 + PTX-EMU P1 接口交付
              ↓
┌───────────────────────────────────┐
│ 🔴 Phase 1: 3 核心模块 (#C4, ~1d)  │
│  ScoreboardTLM + PipelineTLM       │
│  + TensorCoreTLM + 12 static_assert│
│  G-D1, G-D7 验证                  │
└───────────────┬───────────────────┘
                ↓
┌───────────────────────────────────┐
│ 🟣 Phase 2: 4 Adapter (#C3, ~0.5d)│
│  WarpScheduler + Scoreboard        │
│  + Pipeline + TC                   │
│  G-D4, G-D6 验证                  │
└───────────────┬───────────────────┘
                ↓
┌───────────────────────────────────┐
│ ⚪ Phase 3: Async Seam (1h)        │ ← #C5 占位
└───────────────┬───────────────────┘
                ↓
┌───────────────────────────────────┐
│ 🟢 Phase 4: 集成验证 (~0.5d)       │
│  G-D2, G-D3, G-D5, G-D8 验证      │
└───────────────────────────────────┘
                ↓
        P1 归档
```

## 关键依赖

- **P0 归档**: `MemoryBridge` + `KernelLaunchTLM` 扩展已完成（4 Adapter setter 接口已预留）
- **PTX-EMU P1 接口**: `include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h` + `SMContext` 3 setter + exe_once 注入点
- **HSK-1/2/3**: 已在 P0 验证通过
- **后续**: P1 归档后 → 双端全量 D1-Full 验收（P0+P1 联合）