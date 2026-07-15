# Tasks: cpptlm-f12b-ld-impl — CppTLM 端 D1 实施

> **Status**: Proposed
> **Parent**: `proposal.md` + `design.md` (cpptlm-f12b-ld-impl)
> **Reference**: `2026-06-24-gpu-soc-phase8b-core/tasks.md`（架构定义 P0/P1/P2/P3 阶段）
> **Worktree**: `CppTLM/.worktrees/feature-d1-full-impl` (branch `feature/d1-full-impl`)
> **总工时**: ~5.2d（CppTLM 端 #C1~#C5）

## Phase 0: D1 启动前（强制最先完成，~0.5d）

> ⚠️ **MUST**: 不完成本 Phase 不允许进入 Phase 1。

- [ ] 0.1 vendor `include/cudart/cpptlm_bridge.h` from PTX-EMU commit 8dc000ec
  - 来源: `/workspace/project/PTX-EMU/include/cudart/cpptlm_bridge.h` @ commit `8dc000ec`
  - 提取命令: `git show 8dc000ec:include/cudart/cpptlm_bridge.h > include/cudart/cpptlm_bridge.h`
  - 验证: `sha256sum` 与 PTX-EMU commit 8dc000ec 字节级一致（`c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe`）
- [ ] 0.2 写 `include/cudart/AGENTS.md`（vendor provenance 记录）
- [ ] 0.3 验证 baseline 仍 764/764 pass
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
  ./build/bin/cpptlm_tests  # 必须 764/764 PASS
  ```
- [ ] 0.4 与 PTX-EMU 团队协调 D1 同步点（commit hash 双向 sync）

## Phase 1: P0 F12b-LD MemoryBridge（~3d）

### 1.1 #C1 MemoryBridge 实施（Day 1-2, ~1d）

- [ ] **1.1.1** 创建 `include/tlm/gpu/memory_bridge.hh`
  - `class MemoryBridge : public CppTLMBridge`
  - 5 虚方法 override（version/submit_kernel/poll_kernel/synchronize_stream/global_access）
  - 构造函数注入 3 个依赖（KernelLaunchTLM* + CrossbarTLM* + MemoryController*）
  - 私有 `deep_copy_args_()` helper
- [ ] **1.1.2** 创建 `src/tlm/gpu/memory_bridge.cc`
  - `version()` → 返回 `CPPTLMBRIDGE_VERSION`（=1）
  - `submit_kernel()` → deep-copy + FIFO push
  - `poll_kernel()` → 查 map + PTX-EMU 内部状态
  - `synchronize_stream()` → 遍历 stream 等待
  - `global_access()` → `gpu_xbar_->query_latency(device_addr)`
  - 错误码转发 `cudaError_t`（`0`/`cudaErrorInvalidValue`/`UINT64_MAX`）
- [ ] **1.1.3** CMakeLists.txt 注册 MemoryBridge 目标
  ```cmake
  add_library(memory_bridge STATIC
      src/tlm/gpu/memory_bridge.cc
  )
  target_link_libraries(memory_bridge PUBLIC cpptlm_core)
  target_include_directories(memory_bridge PUBLIC include)
  ```
- [ ] **1.1.4** 创建 `tests/unit/cpptlm/test_memory_bridge.cc`（7 个单测）
  - [ ] `version_returns_cpptlm_bridge_version`（==1）
  - [ ] `submit_kernel_deep_copies_args`（5 类：int/float/ptr/struct/array）
  - [ ] `submit_kernel_returns_0_on_success` + 错误码转发
  - [ ] `poll_kernel_returns_UINT64_MAX_on_unknown_id`
  - [ ] `poll_kernel_returns_0_on_completed` + erase
  - [ ] `synchronize_stream_waits_for_all_kernels`
  - [ ] `global_access_returns_UINT64_MAX_on_unmapped_addr`
  - [ ] `global_access_queries_noc_latency_correctly`（mock CrossbarTLM）

**Commit**:
```bash
git add include/tlm/gpu/memory_bridge.hh src/tlm/gpu/memory_bridge.cc \
        tests/unit/cpptlm/test_memory_bridge.cc CMakeLists.txt
git commit -m "feat(tlm/gpu): MemoryBridge implements CppTLMBridge (D1-Full P0 #C1)

5 virtual methods (version/submit_kernel/poll_kernel/synchronize_stream/
global_access) + kernel_args deep-copy + NoC route latency query via
CrossbarTLM::query_latency.

Refs:
- ADR-NV-02 §5.2 R9 (静默数据损坏缓解)
- 综合计划 §2.1 Task #C1
- PTX-EMU HSK-1 commit 8dc000ec
- vendor: include/cudart/cpptlm_bridge.h (SHA-256 c19e66a3...)"
```

### 1.2 #C2 KernelLaunchTLM 实施（Day 3-4, ~1d）

- [ ] **1.2.1** 创建 `include/tlm/gpu/kernel_launch_tlm.hh`
  - `class KernelLaunchTLM : public ChStreamModuleBase`
  - `tick()` + `submit()` + `set_ptx_emu_context()` + 4 Adapter setter 预留
  - 私有 `call_ptx_emu_exe_once_()` + `poll_ptx_emu_completion_()` helper
  - `MAX_PTX_STEPS_PER_TICK=10000` 上限
- [ ] **1.2.2** 创建 `src/tlm/gpu/kernel_launch_tlm.cc`
  - 构造函数创建 MemoryBridge 实例
  - `tick()`：
    1. `bridge_->synchronize_stream(0)`（默认 stream）
    2. 循环 `call_ptx_emu_exe_once_()` 最多 10000 次
    3. 每次后检查完成 + erase
  - `submit()`：FIFO push + 触发 exe_once
  - `set_*()`：4 Adapter setter（D1-Full P1 用）
  - `set_ptx_emu_context()`：接收 PTX-EMU 端 handle
- [ ] **1.2.3** CMakeLists.txt 注册 KernelLaunchTLM 目标
- [ ] **1.2.4** 创建 `tests/unit/cpptlm/test_kernel_launch.cc`
  - [ ] `tick_polls_bridge_synchronize_stream`
  - [ ] `tick_calls_ptx_emu_exe_once_max_10000_times`
  - [ ] `submit_pushes_to_fifo_in_order`
  - [ ] `g_cpptlm_bridge_nullptr_byte_identical_to_baseline`

**Commit**:
```bash
git add include/tlm/gpu/kernel_launch_tlm.hh src/tlm/gpu/kernel_launch_tlm.cc \
        tests/unit/cpptlm/test_kernel_launch.cc CMakeLists.txt
git commit -m "feat(tlm/gpu): KernelLaunchTLM EventQueue + PTX-EMU driver (D1-Full P0 #C2)

FIFO scheduling + MAX_PTX_STEPS_PER_TICK=10000 deadlock guard.
Byte-identical fallback when g_cpptlm_bridge == nullptr.

Refs: 综合计划 §2.1 Task #C2, ADR-NV-02 §5 R6"
```

### 1.3 G-F0 vector_add 烟雾测试（Day 5, ~0.3d）

- [ ] **1.3.1** 创建 `configs/vector_add_n1024.json`（n=1024²）
- [ ] **1.3.2** 创建 `tests/python/test_f12b_smoke.py`
  - 启动 `cpptlm_sim` with F12b-LD enabled
  - 运行 vector_add kernel
  - 输出逐元素 diff
  - 延迟 ≤ 2× standalone baseline
- [ ] **1.3.3** PTX-EMU 端 Phase 1 实施完成后，**双端联合验证**

**Commit**:
```bash
git add configs/vector_add_n1024.json tests/python/test_f12b_smoke.py
git commit -m "test(f12b): G-F0 vector_add smoke (D1-Full P0 质量门)

Output byte-equal with standalone PTX-EMU + latency <= 2x baseline.
This is the G-F0 quality gate before P1 D1-Full injection."
```

### P0 验收门

- [ ] **G-F0** `vector_add` 输出逐元素与 standalone PTX-EMU 一致 + 延迟 ≤ 2× baseline
- [ ] **G-F1** `g_cpptlm_bridge == nullptr` 时 PTX-EMU 零退化（独立模式字节级回退）
- [ ] **G-F2** 有 bridge 时 `cudaLaunchKernel` 立即返回（异步）
- [ ] **G-F3** `global_access()` 延迟与 CppTLM NoC 路由延迟一致（误差 ≤ 5%）
- [ ] **G-F4** `cudaDeviceSynchronize` 正确等待所有 kernel 完成
- [ ] **G-F5** F12b-LD 集成测试: `cpptlm_tests [gpu][f12b]` 全 PASS

## Phase 2: P1 D1-Full Compute 注入（~2.5d）

### 2.1 #C4 3 核心模块（Day 6, ~1d）

- [ ] **2.1.1** `include/tlm/gpu/scoreboard_tlm.hh` + `scoreboard_tlm.cc`
  - `class ScoreboardTLM : public IScoreboardInternal`
  - ≥12 entries hazard table
  - `has_free_entry()` / `allocate(reg_id, warp_id)` / `release(reg_id, warp_id)`
- [ ] **2.1.2** `include/tlm/gpu/pipeline_tlm.hh` + `pipeline_tlm.cc`
  - `class PipelineTLM : public IPipelineLatencyInternal`
  - 5+V 抽象（`get_fractional_cycles_by_type`）
- [ ] **2.1.3** `include/tlm/gpu/tensorcore_tlm.hh` + `tensorcore_tlm.cc`
  - `class TensorCoreTLM : public ITensorCoreTimingInternal`
  - 6 精度（`get_latency(precision)`）
- [ ] **2.1.4** 12 端点 `static_assert`（PipelineId 6 + TcPrecision 6）— 与 PTX-EMU 端双向一致
- [ ] **2.1.5** 3 个单测 + 12 端点 enum 验证测试

### 2.2 #C3 4 Adapter（Day 7, ~0.5d）

- [ ] **2.2.1** `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}`（Task 10b）
- [ ] **2.2.2** `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}`（Task 15）
- [ ] **2.2.3** `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}`（Task 15）
- [ ] **2.2.4** `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}`（Task 15）
- [ ] **2.2.5** 4 个 Adapter 单测
- [ ] **2.2.6** WarpContext* ↔ uint32_t 转换测试

### P1 验收门

- [ ] **G-D1** 3 纯虚接口编译通过，无 CppTLM 头文件污染 PTX-EMU
- [ ] **G-D2** `set_blocked_cycles_for_active()` 对 warp 内活跃线程正确设置延迟
- [ ] **G-D3** `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle（5 类 microbenchmark）
- [ ] **G-D4** 4 Adapter `static_assert` 12 端点 0-5 双向一致
- [ ] **G-D6** 4 setter 全 nullptr 时 PTX-EMU 零退化
- [ ] **G-D7** scoreboard/pipeline/TC 任意 nullptr 时回退到 InstructionLatencyTable

**Commit**:
```bash
git add include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh \
        include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.cc \
        include/tlm/gpu/adapter/ \
        tests/unit/cpptlm/
git commit -m "feat(tlm/gpu): 3 core modules + 4 Adapters (D1-Full P1 #C3+#C4)

D1-Full Compute injection: Scoreboard/Pipeline/TC + WarpContext*<->uint32_t
adapters. 12-endpoint PipelineId+TCprecision static_assert compile-time guards.

Refs: 综合计划 §3, ADR-NV-02 §6.2 G-D1~G-D7"
```

## Phase 3: P2 Phase 9+ Async Seam（~1h）

- [ ] **3.1** `include/tlm/gpu/async_completion_adapter.hh` + `.cc`
  - `class AsyncCompletionAdapter : public IAsyncCompletion`
  - `register_completion_callback()` 存回调
  - `fire_completion()` 触发（Phase 9+ 才调用，Phase 8.B 占位）
- [ ] **3.2** 编译通过验证（独立模式 `async_completion_ = nullptr` 无影响）

**Commit**:
```bash
git add include/tlm/gpu/async_completion_adapter.hh
git commit -m "feat(tlm/gpu): AsyncCompletionAdapter placeholder (D1-Full P2 #C5)

Phase 9+ TMA async seam reservation. Phase 8.B independent mode = nullptr."
```

## Phase 4: P3 集成验证（~1 周）

- [ ] **4.1** `tests/python/test_gpgpu_sim_comparison.py`（5 类 microbenchmark）
  - GEMM (FP16, M=N=K=4096) — gpgpu-sim 700 GB/s ±15%
  - FlashAttn (b=8, h=16, seq=512) — 470 GB/s ±15%
  - vector_add (n=1024²) — 1176 GB/s ±15%
  - stencil (3D 7-point, N=512³) — 940 GB/s ±15%
  - sparse SpMV (10k×10k, 0.01) — 230 GB/s ±15%
- [ ] **4.2** `tests/integration/cpptlm/test_full_pipeline.cc`（Level 1 合成 + Level 2 真实 CUDA）
- [ ] **4.3** `docs/microarchitecture/` 6 个微架构 doc
- [ ] **4.4** 1 GB203 × 1M < 60s 性能验收
- [ ] **4.5** `docs_sync_check.sh --strict` 0 missing
- [ ] **4.6** `ptxemu_tests` + `cpptlm_tests [gpu]` 双 100% PASS

### P3 验收门

- [ ] **G-D5** 5 类 microbenchmark vs gpgpu-sim ±15%
- [ ] **G-D8** exe_once() scoreboard stall → re-schedule → release → re-issue 完整循环无状态不一致

## 综合验收 Gates

- [ ] **G0** P0/P1/P2/P3 所有阶段完成
- [ ] **G1** `[gpu][subcore][sched][sb][tc][pipe][l2]` 全 PASS
- [ ] **G2** `test_gpu_soc_phase8b.cc` Level 1 合成 workload 5 类 microbenchmark 跑通
- [ ] **G3** `test_gpgpu_sim_comparison.py` 带宽 ±15%
- [ ] **G4** 1 GB203 × 1M < 60s
- [ ] **G5** 6 个微架构 doc + docs_sync 0 missing
- [ ] **G6** apu_soc 兼容性全绿（不破坏 Phase 8.A）
- [ ] **G7** Adapter 编译通过（与 PTX-EMU 头文件联编）
- [ ] **G-F0** `vector_add` 烟雾测试通过

## 实施后节点

- [ ] **Oracle 审查**（调 oracle subagent）
- [ ] **Phase 0.5 baseline worktree 对比验证**：`cd ../baseline-d1-full && cmake --build build && ctest` 对比零退化
- [ ] **OpenSpec 归档** → `openspec/changes/archive/2026-07-15-cpptlm-f12b-ld-impl/`
- [ ] **PR 合并** → `main`（经 PTX-EMU 端同步验证后）

## 依赖关系图

```
2026-07-15 HSK-1/2/3 全部就绪
         ↓
vendor cpptlm_bridge.h (Phase 0)
         ↓
┌─────────────────────┐
│ 🔴 P0: MemoryBridge │ ← #C1 + #C2 + G-F0
│  G-F0~G-F5 验证     │
└──────────┬──────────┘
           ↓
┌────────────────────────────────────┐
│ 🟣 P1: D1-Full Compute 注入        │
│  P1.1 3 核心模块 (#C4)              │
│  P1.2 4 Adapter (#C3)                │
│  G-D1~G-D4, G-D6, G-D7 验证         │
└──────────┬─────────────────────────┘
           ↓
┌──────────────────────┐
│ ⚪ P2: Async Seam (1h)│ ← #C5 占位
└──────────┬───────────┘
           ↓
┌─────────────────────────────────────┐
│ 🟢 P3: 集成验证（~1 周）             │
│  G-D5, G-D8, G4, G5 验证             │
│  5 类 microbenchmark vs gpgpu-sim ±15%│
└─────────────────────────────────────┘
           ↓
  D1 全部交付，OpenSpec 归档
```

## 关键依赖

- **必须先完成**: Phase 0（vendor ABI 头文件 + baseline 验证）
- **PTX-EMU 端同步**: PTX-EMU `feature/cpptlm-d1-full` 分支并行实施 #1-#10
- **HSK-1 依赖**: 头文件已 vendor 到 CppTLM（commit 8dc000ec 字节级一致）
- **HSK-3 依赖**: 选项 1 ExternalProject_Add 待 D5 EOD CPPTLM_COMMIT_HASH 锁定
- **P0 → P1 依赖**: #C1 + #C2 必须先完成（接口就绪）
- **P1 → P2 依赖**: P1 #C3 Adapter 层完成（`IAsyncCompletion` setter）
- **P2 → P3 依赖**: P0 + P1 + P2 全部完成
- **后续 change**: D1 交付后 P1 D1-Full Compute 实施（已包含在本 change 内）
