# Tasks: cpptlm-d1-p1-pipeline-scoreboard - D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 从 `cpptlm-f12b-ld-impl` P1/P2/P3 剥离）
> **Design Revision 1**: 2026-07-17（HSK-4/5 后简化 -- vendor 头文件 + 去掉 Internal/空壳 Adapter）
> **Design Revision 2**: 2026-07-18（Metis + Oracle 双审后修复 4 架构 P0 + 4 文档 P0）
> **Parent**: `proposal.md` + `design.md` (cpptlm-d1-p1-pipeline-scoreboard)
> **前置**: P0 `cpptlm-f12b-ld-impl` 归档 + PTX-EMU 交付 P1 接口
> **总工时**: ~3.5d（Phase 1）+ ~1.5d（Phase 4，原 ~1d，Oracle 审查后上调）-- CppTLM 端 #C4+#C5（#C3 空壳 Adapter 已去掉）

## 启动条件

- [x] **P0 已归档**: `cpptlm-f12b-ld-impl` -> `openspec/changes/archive/`（commit `b94eccc`）
- [x] **PTX-EMU P1 接口已提交**: HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) + HSK-5 (`367fd6a5`) 已交付
- [ ] **PTX-EMU P1 测试已通过**: 21/21 ABI + 27/27 helpers + 13/13 barrier PASS；⏳ PTX-7a (7 Mock) + PTX-7b (4 集成) 待

> **注意**: 启动条件 2 已实质解锁（HSK-4/5 响应文档 `docs/superpowers/specs/2026-07-17-hsk-4-5-responses.md`）。Phase 1+2 可独立推进，Phase 4 集成验证需等 PTX-7a/7b。
>
> **2026-07-18 Metis + Oracle 双审后**: Phase 1 可立即启动（仅依赖 vendor 头文件，不依赖 PTX-7a/7b）。G-D6/G-D7 验收已重划到 PTX-EMU 端。

---

## Phase 1: Vendor 头文件 + 3 核心模块 + 12 端点 static_assert（#C4, ~1.5d）

### 1.0 Vendor 3 接口头文件（~0.2d）

- [ ] **1.0.1** `include/cudart/scoreboard_interface.h`（vendor from PTX-EMU `8acfd2d1`，16 行，`git show 8acfd2d1:include/ptxsim/scoreboard_interface.h`）
- [ ] **1.0.2** `include/cudart/pipeline_interface.h`（vendor from PTX-EMU `9e7361b9`，29 行，含 `PipelineId` enum，`git show 9e7361b9:include/ptxsim/pipeline_interface.h`）
- [ ] **1.0.3** `include/cudart/tensor_core_interface.h`（vendor from PTX-EMU `463038e0`，31 行，含 `TcPrecision` enum，`git show 463038e0:include/ptxsim/tensor_core_interface.h`）
- [ ] **1.0.4** 更新 `include/cudart/AGENTS.md`：记录 3 vendor 头文件 provenance（SHA-256 + commit hash + 同步策略）
  - **同步策略**: PTX-EMU 接口 commit 后 24h 内手动同步（`git show <commit>:<path>`）+ SHA-256 校验 + 12 端点 static_assert 回归
  - **提取方式**: 必须用 `git show <commit>:<path>`，**不复制 working tree**（避免未提交修改污染）

### 1.1 ScoreboardTLM（~0.3d）

- [ ] **1.1.1** `include/tlm/gpu/scoreboard_tlm.hh`
  - `class ScoreboardTLM : public IScoreboard`（直接实现 PTX-EMU 接口，无 Internal 层）
  - `#include "cudart/scoreboard_interface.h"`
  - `MAX_ENTRIES = 512`（**global scoreboard 语义**，覆盖 64 warps × 8 in-flight，见 design.md §2.1）+ `std::array<Entry, MAX_ENTRIES>` + `active_count_`
  - 4 override: `has_free_entry()` / `allocate(reg_id, warp_id)` / `release(reg_id, warp_id)` / `tick()`
- [ ] **1.1.2** `src/tlm/gpu/scoreboard_tlm.cc`
  - `allocate`: 
    1. **duplicate 检查**: 若已存在 active `(reg_id, warp_id)` entry，返回 false（**rejects 决议**，2026-07-18，依据 PTX-EMU rollback 逻辑 `sm_context.cpp:37-43`）
    2. 线性扫描找 `!active` slot，标记 active + `++active_count_`，返回 true
    3. 满则返回 false
  - `release`: 找匹配 `(reg_id, warp_id, active)` slot，标记 inactive + `--active_count_`，返回 true；未找到返回 false
  - `tick()`: P1 no-op（**死锁缓解**: 见 design.md §2.1 -- Step A rollback 完整 + 容量充足 + `decrement_blocked_cycles` 每 tick 执行，正常 workload 无死锁；Phase 4 可加超时释放）
- [ ] **1.1.3** `test/test_scoreboard_tlm.cc`（6 单测，tag `[gpu][d1p1]`）
  - [ ] `has_free_entry_initial_returns_true`
  - [ ] `allocate_fills_entries_and_returns_false_when_full`（**512 entries 满**，非 64）
  - [ ] `release_frees_entry_for_reuse`
  - [ ] `release_unknown_reg_returns_false`
  - [ ] `duplicate_allocate_returns_false`（**rejects 决议**，重复 `(reg_id, warp_id)` 返回 false）
  - [ ] `multi_warp_allocate_independent`（不同 warp_id 同 reg_id 可同时 active，验证 global scoreboard 语义）

### 1.2 PipelineTLM（~0.2d）

- [ ] **1.2.1** `include/tlm/gpu/pipeline_tlm.hh`
  - `class PipelineTLM : public IPipelineLatencyProvider`（直接实现，无 Internal 层）
  - `#include "cudart/pipeline_interface.h"`
  - 2 override: `get_fractional_cycles(instruction, pipe_id)` / `get_fractional_cycles_by_type(statement_type, pipe_id)`（注意：`statement_type` 是 `int`，非 `StatementType`，HSK-4 字节级确认）
  - 1 监控方法: `is_placeholder() const`（P1 返回 true，Phase 4 改 false）
- [ ] **1.2.2** `src/tlm/gpu/pipeline_tlm.cc`
  - P1 占位: 所有调用返回 `1.0`（**⚠️ PHASE 1 PLACEHOLDER - DO NOT USE AS CANONICAL VALUE**，Phase 4 对齐 gpgpu-sim 精确值 G-D5）
- [ ] **1.2.3** `test/test_pipeline_tlm.cc`（4 单测，tag `[gpu][d1p1]`）
  - [ ] `get_fractional_cycles_returns_1_placeholder`
  - [ ] `get_fractional_cycles_by_type_returns_1_placeholder`
  - [ ] `all_6_pipeline_ids_return_1`（遍历 PipelineId 0-5）
  - [ ] `is_placeholder_returns_true_in_phase1`

### 1.3 TensorCoreTLM（~0.2d）

- [ ] **1.3.1** `include/tlm/gpu/tensor_core_tlm.hh`
  - `class TensorCoreTLM : public ITensorCoreTiming`（直接实现，无 Internal 层）
  - `#include "cudart/tensor_core_interface.h"`
  - 2 override: `get_latency(prec)` / `get_throughput_cycles(prec)`
  - 不 override `get_latency_mnk`（用 PTX-EMU 头文件 default impl 退化到 `get_latency`）
  - 1 监控方法: `is_placeholder() const`（P1 返回 true，Phase 4 改 false）
- [ ] **1.3.2** `src/tlm/gpu/tensor_core_tlm.cc`
  - P1 占位: `get_latency` 返回 `1`，`get_throughput_cycles` 返回 `1`（**⚠️ PHASE 1 PLACEHOLDER**，Phase 4 对齐 G-D5）
- [ ] **1.3.3** `test/test_tensor_core_tlm.cc`（4 单测，tag `[gpu][d1p1]`）
  - [ ] `get_latency_returns_1_placeholder`
  - [ ] `get_throughput_cycles_returns_1_placeholder`
  - [ ] `all_6_tc_precisions_return_1`（遍历 TcPrecision 0-5）
  - [ ] `is_placeholder_returns_true_in_phase1`

### 1.4 12 端点 static_assert + 签名级验证（~0.1d）

- [ ] **1.4.1** `test/test_12_endpoint_static_assert.cc`
  - `#include "cudart/pipeline_interface.h"` + `#include "cudart/tensor_core_interface.h"` + `#include "cudart/scoreboard_interface.h"` + `#include <type_traits>`
  - 6 `static_assert` PipelineId (P0_INT_FP32=0 ... P4_TC=5)
  - 6 `static_assert` TcPrecision (FP4=0 ... TF32=5)
  - **签名级 `static_assert`**（2026-07-18 Oracle 建议，见 design.md §4）: 用 `decltype` + `std::is_same_v` 验证 3 接口的所有纯虚方法签名
  - 编译通过 = 验证通过（无 runtime 代码）

### 1.5 CMake 集成（~0.1d）

- [ ] **1.5.1** `src/CMakeLists.txt`：新增 3 行
  - `tlm/gpu/scoreboard_tlm.cc   # D1-Full P1: IScoreboard 实现`
  - `tlm/gpu/pipeline_tlm.cc     # D1-Full P1: IPipelineLatencyProvider 实现`
  - `tlm/gpu/tensor_core_tlm.cc  # D1-Full P1: ITensorCoreTiming 实现`
- [ ] **1.5.2** `test/CMakeLists.txt`：GLOB 自动发现 4 个新 `test_*.cc`（无需手动注册）

**Commit**:
```bash
git add include/cudart/{scoreboard,pipeline,tensor_core}_interface.h \
        include/cudart/AGENTS.md \
        include/tlm/gpu/{scoreboard,pipeline,tensor_core}_tlm.hh \
        src/tlm/gpu/{scoreboard,pipeline,tensor_core}_tlm.cc \
        src/CMakeLists.txt \
        test/test_{scoreboard,pipeline,tensor_core}_tlm.cc \
        test/test_12_endpoint_static_assert.cc
git commit -m "feat(tlm/gpu): 3 核心模块 + 12 端点 static_assert (D1-Full P1 #C4)

ScoreboardTLM + PipelineTLM + TensorCoreTLM 直接实现 PTX-EMU 纯虚接口
(vendor 头文件 from HSK-4)。12 端点 PipelineId+TcPrecision static_assert
+ 签名级 decltype 验证 编译期拦截。Phase 1 占位 latency = 1.0
(G-D5 精确对齐留给 Phase 4)。MAX_ENTRIES=512 (global scoreboard,
覆盖 64 warps × 8 in-flight)。duplicate allocate rejects 决议。

Refs:
- 综合计划 §3 Task #C4
- HSK-4: PTX-EMU 8acfd2d1/9e7361b9/463038e0
- HSK-5: PTX-EMU 367fd6a5
- Design Revision 1 (2026-07-17): vendor + 去掉 Internal/空壳 Adapter
- Design Revision 2 (2026-07-18): Metis + Oracle 双审修复 4 架构 P0"
```

---

## Phase 2: WarpScheduler Adapter（optional，~0.3d，待 Phase 4 评估）

> **Design Revision 2026-07-17**: 原 Phase 2 含 4 Adapter（WarpScheduler + Scoreboard + Pipeline + TC），简化后 3 空壳 Adapter 已去掉（3 核心模块直接实现 PTX-EMU 接口）。仅保留 WarpScheduler Adapter 做 `WarpContext*` <-> `uint32_t warp_id` 转换。
>
> **2026-07-18 Oracle 审查**: `WarpContext::get_physical_warp_id()`（`warp_context.h:284`）已提供 `uint32_t` 返回，PTX-EMU `step_a_scoreboard_check`（`sm_context.cpp:35`）直接用此值。WarpScheduler Adapter 可能不需要。

- [ ] **2.1** 评估 WarpScheduler Adapter 必要性（Phase 4 启动时决策）
  - 若 PTX-EMU `set_blocked_cycles_for_active(uint32_t warp_id)` 需 CppTLM `WarpContext*` 转换 -> 实施
  - 若 CppTLM 端可直接用 `warp_id` 索引 -> 不需要 Adapter，跳过

---

## Phase 3: IAsyncCompletion 占位（#C5, ~1h）

> ✅ **状态（2026-07-17 验证）**:
> - `include/tlm/gpu/async_completion_adapter.hh` 已落地（97 行, header-only 设计, 因体量小无需独立 `.cc`）
> - 测试: `test/test_async_completion_adapter.cc`（5 TEST_CASE 全部通过）
> - 4 件套验证: `[gpu]` 92 cases / 224 assertions ALL PASS
> - commit: `e69cd1d feat(tlm/gpu): AsyncCompletionAdapter placeholder (D1-Full P2 #C5)`
>
> **2026-07-18 Oracle 审查**: `IAsyncCompletion` 是 CppTLM 自定义接口（非 PTX-EMU vendor）。PTX-EMU 端 `sm_context.h` 无 `set_async_completion` setter。当前为投机性脚手架，Phase 9+ 确认 PTX-EMU 端是否需要。

- [x] **3.1** `include/tlm/gpu/async_completion_adapter.hh`（header-only，因体量小无 `.cc`）
  - `class AsyncCompletionAdapter : public IAsyncCompletion`
  - `register_completion_callback()` 存 map（`std::unordered_map<uint64_t, std::function<void()>>`）
  - `fire_completion()` 占位（仅递增 `fire_completion_count_`，**不调用 callback，Phase 8.B 语义**）
  - 监控接口: `fire_completion_count() / pending_callback_size()`
- [x] **3.2** 编译通过（独立模式 `async_completion_ = nullptr` 无影响）+ 单测 5 case 全 pass

---

## Phase 4: 集成验证 + KernelLaunchTLM 扩展（~1.5d，等 PTX-7a/7b，原 ~1d 上调）

> **2026-07-18 Oracle 审查**: `KernelLaunchTLM` P0 仅有 2 setter（`setMemoryBridge`/`set_ptx_emu_context`），**无 P1 的 3 setter**（`set_scoreboard`/`set_pipeline`/`set_tensor_core`）。Phase 4 不是"激活预留 setter"，而是"新增 3 setter + 新增 `inject_into_sm_context()` + 改 `call_ptx_emu_exe_once_()` 从 stub 变真实注入"。

- [ ] **4.1** 修改 `include/tlm/gpu/kernel_launch_tlm.hh`：**新增** 3 setter（`set_scoreboard` / `set_pipeline` / `set_tensor_core`）+ 3 成员变量（`IScoreboard*` / `IPipelineLatencyProvider*` / `ITensorCoreTiming*`，非所有权）
- [ ] **4.2** 修改 `include/tlm/gpu/kernel_launch_tlm.hh`：**新增** `inject_into_sm_context()` 方法（一次性注入 3 核心模块到 PTX-EMU `SMContext`，用 `injected_` flag 防重）
- [ ] **4.3** 修改 `src/tlm/gpu/kernel_launch_tlm.cc`：`call_ptx_emu_exe_once_()` 从 P0 stub 改为真实注入（`static_cast<GPUContext*>(ptx_emu_context_)->exe_once(warp_id)`）
- [ ] **4.4** 修改 `src/tlm/gpu/kernel_launch_tlm.cc`：`tick()` 中 `while (!pending_.empty())` 循环逻辑（真实 exe_once 会消耗 pending_ 中的 kernel）
- [ ] **4.5** `test/python/test_gpgpu_sim_comparison.py`（G-D5 5 类 microbenchmark vs gpgpu-sim ±15%）
- [ ] **4.6** Latency 精确对齐：PipelineTLM + TensorCoreTLM 占位值 -> gpgpu-sim 精确值 + `is_placeholder_` 改为 `false`

**3 核心模块实例传递方式**: 程序化 setter（非 JSON 配置）。在 `main.cpp` 或测试代码中显式调用 `kernel_launch->set_scoreboard(scoreboard.get())` 等。理由：3 核心模块是 CppTLM 端实现，不参与 `ModuleFactory::instantiateAll()` JSON 拓扑解析。

---

## P1 验收门

> **2026-07-18 Metis + Oracle 双审后明确归属**:
> - **CppTLM 端可独立验证**: G-D1, G-D4
> - **PTX-EMU 端验收**: G-D6, G-D7（nullptr 回退在 PTX-EMU `step_a/b/c` helper 实现，CppTLM 端无法独立测试）
> - **双端联合验收**: G-D2, G-D3, G-D5, G-D8（需 PTX-7a/7b 完成）

- [ ] **G-D1** [CppTLM 端] 3 纯虚接口编译通过，无 CppTLM 头文件污染 PTX-EMU（vendor 头文件零依赖）
- [ ] **G-D2** [双端] `set_blocked_cycles_for_active()` 对 warp 内活跃线程正确设置延迟
- [ ] **G-D3** [双端] `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle
- [ ] **G-D4** [CppTLM 端] 12 端点 `static_assert` PipelineId 0-5 + TcPrecision 0-5 编译期一致 + **签名级 `decltype` 验证**
- [ ] **G-D5** [双端] 5 类 microbenchmark vs gpgpu-sim ±15%（Phase 4）
- [ ] **G-D6** [PTX-EMU 端] **3 setter** 全 nullptr 时 PTX-EMU 零退化（验证在 PTX-EMU `test_nullptr_fallback`）
- [ ] **G-D7** [PTX-EMU 端] scoreboard/pipeline/TC 任意 nullptr 时回退到 **PTX-EMU 内置 InstructionLatencyTable**（此回退逻辑在 PTX-EMU 端实现，CppTLM 端无需实现 InstructionLatencyTable）
- [ ] **G-D8** [双端] exe_once() scoreboard stall -> re-schedule -> release -> re-issue 完整循环

## P1 依赖关系图（简化后）

```
P0 归档 + HSK-4/5 交付
               |
               v
┌───────────────────────────────────────────┐
│ Phase 1: Vendor + 3 核心模块 + 12 assert  │  ← 可立即启动
│  (#C4, ~1.5d)                              │
│  ScoreboardTLM + PipelineTLM + TensorCoreTLM│
│  + 12-endpoint static_assert + 签名级验证   │
│  G-D1, G-D4 验证                           │
└───────────────────┬───────────────────────┘
                    |
                    v
┌───────────────────────────────────────────┐
│ Phase 2: WarpScheduler Adapter (optional) │  ← 待 Phase 4 评估
│  (~0.3d, may skip)                         │
└───────────────────┬───────────────────────┘
                    |
                    v
┌───────────────────────────────────────────┐
│ Phase 3: Async Seam (1h)                   │  ← ✅ 已完成
│  AsyncCompletionAdapter (e69cd1d)          │
└───────────────────┬───────────────────────┘
                    |
                    v
┌───────────────────────────────────────────┐
│ Phase 4: 集成验证 (~1.5d)                  │  ← 等 PTX-7a/7b
│  KernelLaunchTLM 新增 3 setter             │
│  + inject_into_sm_context()                │
│  + 改 call_ptx_emu_exe_once_() 真实注入     │
│  + G-D5 latency 精确对齐                   │
│  G-D2, G-D3, G-D5, G-D8 验证              │
└───────────────────────────────────────────┘
                    |
                    v
                P1 归档
```

## 关键依赖

- **P0 归档**: `MemoryBridge` + `KernelLaunchTLM` 扩展已完成（**仅 2 setter**：`setMemoryBridge`/`set_ptx_emu_context`，无 P1 的 3 setter）✅
- **PTX-EMU P1 接口**: HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) + HSK-5 (`367fd6a5`) 已交付 ✅
- **HSK-1/2/3**: 已在 P0 验证通过 ✅
- **HSK-4/5**: CppTLM 端已响应 (`docs/superpowers/specs/2026-07-17-hsk-4-5-responses.md`) ✅（2026-07-18 修订 4 处过时表述）
- **后续**: P1 归档后 -> 双端全量 D1-Full 验收（P0+P1 联合）
