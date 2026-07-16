# Tasks: cpptlm-f12b-ld-impl — CppTLM 端 P0 D1 实施 (MemoryBridge)

> **Status**: ✅ Ready to Archive（2026-07-16 验证: P0 全过, 776/776 / 15562 断言, 已合并 main @ `73e5422`）
> **Parent**: `proposal.md` + `design.md` (cpptlm-f12b-ld-impl)
> **Reference**: `2026-06-24-gpu-soc-phase8b-core/tasks.md`（架构定义 P0/P1/P2/P3 阶段）
> **Worktree**: `CppTLM/.worktrees/feature-d1-full-impl` (branch `feature/d1-full-impl`, 已与 main 同步)
> **总工时**: P0 ~5d (已交付); P1/P2/P3 已拆分至 `cpptlm-d1-p1-pipeline-scoreboard` (2026-07-15 Oracle + Metis 双审后拆分)

> **拆分说明**: 原始 tasks.md 包含 Phase 0/1/2/3/4 全部 D1 工作。2026-07-15 拆分后:
> - 本 change (cpptlm-f12b-ld-impl) 仅覆盖 **Phase 0 + Phase 1** (P0 MemoryBridge + KernelLaunchTLM)
> - **Phase 2 (P1 D1-Full Compute) + Phase 3 (P2 Async Seam) + Phase 4 (P3 集成验证)** 已迁移至 `cpptlm-d1-p1-pipeline-scoreboard` change
> - 下方 Phase 2/3/4 任务保留作为历史归档参考,标记为 `[ ]` 但不再 active

## Phase 0: D1 启动前（强制最先完成，~0.5d）

> ✅ **完成** (2026-07-16 验证)

- [x] 0.1 vendor `include/cudart/cpptlm_bridge.h` from PTX-EMU commit 8dc000ec
  - 来源: `/workspace/project/PTX-EMU/include/cudart/cpptlm_bridge.h` @ commit `8dc000ec`
  - 提取命令: `git show 8dc000ec:include/cudart/cpptlm_bridge.h > include/cudart/cpptlm_bridge.h`
  - 验证: `sha256sum` 与 PTX-EMU commit 8dc000ec 字节级一致（`c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe`）
  - 后续 re-vendor @ commit `603bd8bc` (2026-07-16, 新增 `cpptlm_attach_bridge` + 可见性宏)
- [x] 0.2 写 `include/cudart/AGENTS.md`（vendor provenance 记录）
- [x] 0.3 验证 baseline 仍 764/764 pass (实际升级至 **776/776 / 15562 断言**, 见 `docs/superpowers/specs/2026-07-15-phase05-baseline-report.md`)
- [x] 0.4 与 PTX-EMU 团队协调 D1 同步点（HSK-1/2/3 全就绪, 见 `docs/superpowers/specs/2026-07-15-cpptlm-hsk-response.md`）

## Phase 1: P0 F12b-LD MemoryBridge（~3d）

> ✅ **完成** (commit `73e5422` + 9 个相关 commit, 776/776 测试通过)

### 1.1 #C1 MemoryBridge 实施（Day 1-2, ~1d）

- [x] **1.1.1** 创建 `include/tlm/gpu/memory_bridge.hh`
  - `class MemoryBridge : public CppTLMBridge`
  - 5 虚方法 override（version/submit_kernel/poll_kernel/synchronize_stream/global_access）
  - 构造函数注入 2 个依赖（KernelLaunchTLM* + CrossbarTLM*）
  - 私有 `deep_copy_args_()` helper
- [x] **1.1.2** 创建 `src/tlm/gpu/memory_bridge.cc`
  - `version()` → 返回 `CPPTLMBRIDGE_VERSION`（=1）
  - `submit_kernel()` → deep-copy + FIFO push
  - `poll_kernel()` → 查 map + PTX-EMU 内部状态
  - `synchronize_stream()` → 遍历 stream 等待
  - `global_access()` → `gpu_xbar_->query_latency(device_addr)`
  - 错误码转发 `cudaError_t`（`0`/`cudaErrorInvalidValue`/`UINT64_MAX`）
- [x] **1.1.3** `src/CMakeLists.txt` 注册 MemoryBridge (隐式在 `cpptlm_core` 静态库)
- [x] **1.1.4** 创建 `test/test_memory_bridge.cc`（12 个 `[f12b]` 单测, 实测 12/12 PASS + 15 assertions）
  - [x] ABI 版本一致性
  - [x] submit_kernel 入队 + args deep-copy
  - [x] poll_kernel 返回语义 (P0 立即完成)
  - [x] synchronize_stream 迭代器安全
  - [x] global_access 延迟查询
  - [x] deep_copy_args_ 独立性
  - [x] 参数校验 (nullptr 等)

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

- [x] **1.2.1** 创建 `include/tlm/gpu/kernel_launch_tlm.hh` (扩展 4 Adapter setter 预留)
  - `class KernelLaunchTLM : public ChStreamModuleBase`
  - `tick()` + `submit()` + `set_ptx_emu_context()` + 4 Adapter setter 预留 (P1 用)
  - 私有 `call_ptx_emu_exe_once_()` + `poll_ptx_emu_completion_()` helper
  - `MAX_PTX_STEPS_PER_TICK=10000` 上限
- [x] **1.2.2** 创建 `src/tlm/gpu/kernel_launch_tlm.cc` (扩展 P0 钩子)
  - 构造函数创建 MemoryBridge 实例
  - `tick()` 集成 MemoryBridge 调用
  - `submit()` FIFO push
  - `set_*()`：4 Adapter setter 预留 (D1-Full P1 用)
  - `set_ptx_emu_context()`：接收 PTX-EMU 端 handle
- [x] **1.2.3** `src/CMakeLists.txt` KernelLaunchTLM 已在 cpptlm_core 中
- [x] **1.2.4** 创建 `test/test_kernel_launch_tlm_ext.cc`（P0 钩子单测, 嵌入 test_phase* 系列）

### 1.3 G-F0 vector_add 烟雾测试（Day 5, ~0.3d）

- [x] **1.3.1** 创建 `configs/vector_add_n1024.json`（n=1024²）
- [x] **1.3.2** 创建 `test/python/test_f12b_smoke.py`
  - 启动 `cpptlm_sim` with F12b-LD enabled (通过 `--f12b-ld` flag)
  - 运行 vector_add kernel
  - 输出逐元素 diff
  - 延迟 ≤ 2× standalone baseline
- [ ] **1.3.3** PTX-EMU 端 Phase 1 实施完成后，**双端联合验证**（PTX-EMU 端 HSK-1/2/3 已就绪, 双端验证待 D5 EOD 锁定 CPPTLM_COMMIT_HASH 后进行）

### P0 验收门

> ✅ **验证状态 (2026-07-16)**:

- [x] **G-F0** `vector_add` 输出逐元素与 standalone PTX-EMU 一致 + 延迟 ≤ 2× baseline (烟雾测试已通过, `test_f12b_smoke.py`)
- [x] **G-F1** `g_cpptlm_bridge == nullptr` 时 PTX-EMU 零退化（独立模式字节级回退, `test_kernel_launch_tlm_ext.cc` 验证)
- [x] **G-F2** 有 bridge 时 `cudaLaunchKernel` 立即返回（MemoryBridge.submit_kernel 异步实现, 测试通过）
- [x] **G-F3** `global_access()` 延迟与 CppTLM NoC 路由延迟一致（CrossbarTLM::query_latency 误差 ≤ 5%, 测试通过）
- [x] **G-F4** `cudaDeviceSynchronize` 正确等待所有 kernel 完成（MemoryBridge.synchronize_stream, 测试通过）
- [x] **G-F5** F12b-LD 集成测试: `cpptlm_tests [gpu][f12b]` 全 PASS (**12/12 用例 / 15 断言**)

## Phase 2: P1 D1-Full Compute 注入（~2.5d） — 已拆分至 `cpptlm-d1-p1-pipeline-scoreboard`

### 2.1 #C4 3 核心模块（Day 6, ~1d）

> ⚠️ **已拆分** → 详见 `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` §Phase 1

- [ ] **2.1.1** `include/tlm/gpu/scoreboard_tlm.hh` + `scoreboard_tlm.cc` → 见 d1-p1 tasks.md §1.1
- [ ] **2.1.2** `include/tlm/gpu/pipeline_tlm.hh` + `pipeline_tlm.cc` → 见 d1-p1 tasks.md §1.2
- [ ] **2.1.3** `include/tlm/gpu/tensor_core_tlm.hh` + `tensor_core_tlm.cc` → 见 d1-p1 tasks.md §1.3
- [ ] **2.1.4** 12 端点 `static_assert`（PipelineId 6 + TcPrecision 6）— 与 PTX-EMU 端双向一致 → 见 d1-p1 §1.4
- [ ] **2.1.5** 3 个单测 + 12 端点 enum 验证测试 → 见 d1-p1 §1.1.3 / §1.2.3 / §1.3.3

### 2.2 #C3 4 Adapter（Day 7, ~0.5d）

> ⚠️ **已拆分** → 详见 `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` §Phase 2

- [ ] **2.2.1** `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}` → 见 d1-p1 §2.1
- [ ] **2.2.2** `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}` → 见 d1-p1 §2.2
- [ ] **2.2.3** `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}` → 见 d1-p1 §2.3
- [ ] **2.2.4** `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}` → 见 d1-p1 §2.4
- [ ] **2.2.5** 4 个 Adapter 单测 → 见 d1-p1 §2.5
- [ ] **2.2.6** WarpContext* ↔ uint32_t 转换测试 → 见 d1-p1 §2.1

### P1 验收门

> ⚠️ **已拆分** → 详见 d1-p1 tasks.md §G-D1~G-D8

**Commit** (P1 拆分后):
> 详见 `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` §Phase 1+2 commit 模板

## Phase 3: P2 Phase 9+ Async Seam（~1h）

> ⚠️ **已拆分** → 详见 `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` §Phase 3

- [ ] **3.1** `include/tlm/gpu/async_completion_adapter.hh` + `.cc` → 见 d1-p1 §3.1
- [ ] **3.2** 编译通过验证（独立模式 `async_completion_ = nullptr` 无影响）→ 见 d1-p1 §3.2

**Commit** (P2 拆分后):
> 详见 `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` §Phase 3 commit 模板

## Phase 4: P3 集成验证（~1 周）

> ⚠️ **已拆分** → 详见 `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` §Phase 4

- [ ] **4.1** `tests/python/test_gpgpu_sim_comparison.py`（5 类 microbenchmark）
- [ ] **4.2** `tests/integration/cpptlm/test_full_pipeline.cc`
- [ ] **4.3** `docs/microarchitecture/` 6 个微架构 doc
- [ ] **4.4** 1 GB203 × 1M < 60s 性能验收
- [ ] **4.5** `docs_sync_check.sh --strict` 0 missing
- [ ] **4.6** `ptxemu_tests` + `cpptlm_tests [gpu]` 双 100% PASS

### P3 验收门

> ⚠️ **已拆分** → 详见 d1-p1 tasks.md §G-D5, G-D8

## 综合验收 Gates

- [x] **G-F0** `vector_add` 烟雾测试通过 (✅ 2026-07-16)
- [ ] **G-D1~G-D8** D1-Full Compute 验证门 → 详见 d1-p1 §P1 验收门
- [ ] **G-D5** 5 类 microbenchmark vs gpgpu-sim ±15% → 详见 d1-p1 §P3 验收门
- [ ] **G6** apu_soc 兼容性全绿（不破坏 Phase 8.A）✅ (776/776 测试通过, Phase 8.A 模块无回归)

## 实施后节点

- [x] **Oracle 审查** → ✅ cpptlm 端验证通过 (776/776 / 15562 断言, Phase 0.5 baseline 双端对齐)
- [x] **Phase 0.5 baseline worktree 对比验证** → ✅ 详见 `docs/superpowers/specs/2026-07-15-phase05-baseline-report.md`
- [ ] **OpenSpec 归档** → `openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/` (P0 部分)
- [x] **PR 合并** → `main` ✅ (commit `73e5422` 包含 P0 全部 10 commits)
- [ ] **后续** → P1/P2/P3 实施归入 `cpptlm-d1-p1-pipeline-scoreboard` change

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
