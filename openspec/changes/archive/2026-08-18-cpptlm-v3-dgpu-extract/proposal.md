# cpptlm-v3-dgpu-extract: dGPU Board Submodule Implementation Proposal

## Why

[UsrLinuxEmu ADR-090 v2 commit `37a91b6`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md)（2026-08-18 升 ✅ Accepted，Gate #1/#2/#5/#6 ✅）决策：

- **CppTLM 角色反转**：UsrLinuxEmu + TaskRunner = CUDA app 入口；**CppTLM = 被驱动的 dGPU 板卡**（PCIe 设备语义，对齐 gem5 full-system GPU 工业惯例）
- **替代 UsrLinuxEmu HAL backend 集成**：原 ADR-076 v1 模式（HAL dlopen `libptxemu_device.so` 同步执行）违反 [ADR-036 three-way separation](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-036-three-way-separation.md)
- **CPPTLMBRIDGE_VERSION 冻结于 2**：HSK-6 协议承诺，任何解冻触发 HSK-7

[PTX-EMU HSK-6 公告 commit `25e36f60`](https://github.com/chisuhua/PTX-EMU/blob/main/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md)（CppTLM 已 ack `369cf71`）：消费关系废止两阶段删除流程。

[CppTLM issue #19 v3.0 RFC](https://github.com/chisuhua/CppTLM/issues/19)（Gate #2 ack 2026-08-18）：完整实施路径 = `PtxEmuSubmodule façade + DGpuBar/Doorbell/SQ-CQ 三件套 + CompletionRing 重设计 + 9 周 P0-P4 时间线`。

Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy`（4 轮评估）验证事实基础：8 函数 ABI（PTX-EMU `cpptlm_module.h:12-52`）、17 条 G-D4 static_assert（cpptlm_bridge.h:243-306 + ptx_emu_driver.hh:27）、HSK-1 真相源归属、ANTLR4 不在 CppTLM scope。

## What Changes

### 1. 新建文件（Per Phase）

| Phase | 文件 | 用途 |
|---|---|---|
| **P0** | `include/cudart/abi_guards.h` | 集中 17 条 G-D4 static_assert（从 cpptlm_bridge.h:243-306 + ptx_emu_driver.hh:27 迁移） |
| **P1** | `include/tlm/gpu/ptx_emu_submodule.{h,cc}` | 8 函数 ABI façade（封装 `libptxemu_device.so` dlsym） |
| **P1** | `include/tlm/gpu/dgpu_bar.hh` | PCIe BAR0 MMIO 模拟（CFG + BAR0 regs + BAR1 VRAM） |
| **P1** | `include/tlm/gpu/doorbell.hh` | SQ tail register（host→device 异步信号） |
| **P1** | `include/tlm/gpu/submission_queue.hh` | SQ consumer（NVMe 模型） |
| **P1** | `include/tlm/gpu/completion_ring.hh` | CompletionRing push + host_notify 钩子（替代 `AsyncCompletionAdapter::setCompletionCallback`） |
| **P2** | `include/tlm/gpu/is_m_executor.{h,cc}` | SM executor 接口（installImage / dispatch / setCompletionCallback） |
| **P3** | `include/tlm/gpu/kernel_launch_tlm.hh`（重构） | 适配 `ISmExecutor` 接口 |
| **P4** | `include/cpptlm_version.h` | CPPTLM_VERSION_MAJOR/MINOR/PATCH（3.0.0） |

### 2. 修改文件

| 文件 | 修改 |
|---|---|
| `include/cudart/cpptlm_bridge.h` | 删除 16 条 static_assert（:243-306），改 `#include "cudart/abi_guards.h"`；保留 ABI 真值源声明（CPPLMBRIDGE_VERSION + 函数声明） |
| `include/tlm/gpu/ptx_emu_driver.hh` | 删除 1 条 static_assert（:27），改 `#include "cudart/abi_guards.h"`；`IPtxEmuDriver`/`DriverWrapper` 加 `[[deprecated]]` |
| `include/tlm/gpu/memory_bridge.hh` | `MemoryBridge` 类加 `[[deprecated]]` |
| `CMakeLists.txt` | v2.1.0 → v3.0.0 BREAKING bump（per ADR-088 §D6.2） |
| `CMakeLists.txt` | 注册 `cpptlm_ptx_emu_submodule` target（link `${CMAKE_DL_LIBS}`） |

### 3. 删除清单（11 项，per UsrLinuxEmu `37a91b6` §D6.1）

**Phase 4 物理删除**：

| # | 项 | 位置 |
|---|---|---|
| 1 | `MemoryBridge` 类 | `include/tlm/gpu/memory_bridge.hh` |
| 2 | `IPtxEmuDriver` 接口 | `include/tlm/gpu/ptx_emu_driver.hh:19` |
| 3 | `DriverWrapper` 类 | `include/tlm/gpu/ptx_emu_driver.hh:51` |
| 4 | `g_ptx_emu_driver` 全局 | CppTLM 仓（PTX-EMU ↔ CppTLM 入口点） |
| 5 | `cpptlm_set_driver` ABI 入口 | CppTLM 仓（PTX-EMU 方向） |
| 6 | `ptx_emu_driver_shim.cc` | `src/tlm/gpu/ptx_emu_driver_shim.cc` |
| 7 | vendored `cpptlm_bridge.h` | `include/cudart/cpptlm_bridge.h`（14837 字节） |
| 8 | vendored `pipeline_interface.h` | `include/cudart/pipeline_interface.h`（1659 字节） |
| 9 | vendored `scoreboard_interface.h` | `include/cudart/scoreboard_interface.h`（1278 字节） |
| 10 | vendored `tensor_core_interface.h` | `include/cudart/tensor_core_interface.h`（1709 字节） |
| 11 | `PtxEmuDriverApi` 布局锁 | `include/tlm/gpu/ptx_emu_driver.hh:27`（已迁至 `abi_guards.h`） |

### 4. 4 测试文件处置（per UsrLinuxEmu `37a91b6` §D6.2）[Oracle C-NEW-1 路径修正]

| 测试 | 处置 |
|---|---|
| `test/test_memory_bridge.cc` | 🗑️ **删除**（测试对象 `MemoryBridge` 物理删除） |
| `test/test_memory_bridge_poll.cc` | 🔄 **重写**为 `test/test_completion_ring.cc`（`poll_kernel` 语义 → CompletionRing push/host_notify） |
| `test/test_kernel_launch_tlm_ext.cc` | ✅ **保留 + 改符号**（`MockPtxEmuDriver` → SQ/CQ doorbell mock；`KernelLaunchRequest`/`setMemoryBridge` 复用） |
| `test/test_gpu_soc_perf.cc` | ✂️ **拆分**（scoreboard perf 保留；MemoryBridge poll perf → CompletionRing） |

### 5. 9 周双轨时间线（per #19 v3.0 RFC）

```
W1      (P0 冻结):    G-D4 迁移 + HSK-6 ack + Mode A [[deprecated]]
W1-3    (P1 双轨):    PtxEmuSubmodule + DGpuBar/Doorbell/SQ/CQ + CompletionRing
W4-6    (P2 收敛):    ISmExecutor + PtxEmuSubmodule 汇合 + E2E
W6-8    (P3 重构):    KernelLaunchTLM 重构 + CompletionRing push + dual-rail E2E
W8-9    (P4 物理删除): 11 项删除 + v2.1.0 → v3.0.0 bump + 846 baseline 验证 + tag
```

## Acceptance Gate

| Gate | Owner | 当前状态 |
|---|---|:---:|
| #1 UsrLinuxEmu ADR-090 v2 ✅ Accepted | UsrLinuxEmu | ✅ commit `37a91b6` |
| #2 CppTLM maintainer ack | CppTLM | ✅ commit `369cf71`（HSK-6 ack） |
| #3 PTX-EMU Architecture Team HSK-6 发出 | PTX-EMU | ✅ commit `25e36f60` |
| #4 PTX-EMU HSK-6 + UsrLinuxEmu 双 ack 收齐 | PTX-EMU + UsrLinuxEmu | 🚫 Ack 截止 2026-09-01 |
| #5 G-D4 17 条 static_assert 迁移 | CppTLM | ✅ commit `fa2b3ec`（P0-1 已完成） |
| #6 Mode A/B dual-rail E2E [Oracle A.4 改写] | CppTLM + UsrLinuxEmu | ⏳ W6-8 |
| #6.a **功能等价** blocking | CppTLM | ⏳ W6-8 |
| #6.b **cycle 偏差 informational**（median/p95 报告,超 ±15% 触发人工 review）| CppTLM | ⏳ W6-8 |
| #7 baseline 测试验证 [Oracle 二次改写 — T-P3-5 双轨] | CppTLM | ⏳ W8-9 |
| #7.a **兼容 baseline**: 全部 Catch2 test cases ≥ **846**(含 legacy SM 单元测试,保证不破坏现有覆盖) | CppTLM | ⏳ W8-9 |
| #7.b **dGPU 路径 coverage**: DGpuBoardTLM / Doorbell / SQ / CQ / /PtxEmuSubmodule 直接相关测试 ≥ **60**(per tasks.md § 测试目标 +5/+30/+25/+50/+50=160,只统计与 dGPU 直接相关者) | CppTLM | ⏳ W8-9 |
| #8 v3.0.0 tag + 删除 11 项完成 | CppTLM | ⏳ W9 |
| **#9** JSON-config E2E [Oracle 新增 / 用户主需求] | CppTLM | ⏳ W6-8 |
| **#9.a** `configs/dgpu_board_v1.json` 通过 `validate_topology` | CppTLM | ⏳ W6-8 |
| **#9.b** `test/test_dgpu_board_from_config.cc` 6 条 SECTION 全部 PASS(instantiateAll + StreamAdapter 注入 + installImage + doorbell→SQ→dispatch→CompletionRing + host_notify + 负面路径)| CppTLM | ⏳ W6-8 |
| **#10** v0.4 增量验收 [design.md §3-§7 落地] | CppTLM | ⏳ W4-8 |
| **#10.a** Doorbell::ring strong-ordered write path 测试 PASS(PCIe Gen5 x16 250-700ns 区间断言) | CppTLM | ⏳ W6-8 |
| **#10.b** Task Dependency Table 256 slot + LIFO eviction 测试 PASS | CppTLM | ⏳ W6-8 |
| **#10.c** TMU Glue `TmuDispatchProcessor` submit/on_complete/try_chain_dependent 测试 PASS | CppTLM | ⏳ W6-8 |
| **#10.d** TMD-aware 8 用例(T-TMD-01~08) PASS(6 区字段 + Grid vs Queue + Scheduler Cache + dep chain + LIFO + 环检测)| CppTLM | ⏳ W6-8 |
| **#10.e** Task Dispatch Pipeline 端到端(500ns-2us 预算 + 7 条错误传播路径)| CppTLM | ⏳ W6-8 |

## Cross-Repo Coordination

| 仓 | 跟踪载体 | 当前状态 |
|---|---|---|
| UsrLinuxEmu | ADR-090 v2 (`e03b5a1` + `37a91b6`) + annex §E | ✅ Accepted |
| CppTLM | **本 openspec change** + HSK-6 ack (`369cf71`) | 🔄 实施中 |
| PTX-EMU | HSK-6 (`25e36f60`) + HSK-PROTOCOL-NOTES (`bf1a652d`) | ✅ 已发出 |
| TaskRunner | tadr-308 + openspec change (独立并行) | 🟡 进行中 |

**跨仓 commit 顺序**（per ADR-035 §R5.1）：
```
UsrLinuxEmu ADR-090 v2 ✅ → PTX-EMU HSK-6 ✅ → CppTLM ack ✅ → CppTLM P0-P4 实施 → UsrLinuxEmu submodule bump + Mode B E2E
```

### Oracle 评审修订记录（2026-08-18）

本提案经过 Oracle read-only 咨询,确认以下必改项已纳入 tasks.md / specs:

| Oracle Finding | 严重度 | 修订落地 |
|---|---|---|
| **C-NEW-1** 路径与扩展名约定违反（`tests/` → `test/`, `.cpp` → `.cc`）| HIGH | tasks.md T-P0-4；proposal.md §4 已修正 |
| **C-NEW-2** 非 SimObject 组件无仿真时间集成,JSON 注册路径缺失 | **CRITICAL** | tasks.md T-P2-4（DGpuBoardTLM 包装前置） |
| **C-NEW-3** CompletionRing spec 自死锁矛盾 | HIGH | specs/completion-ring.md §3.2/§5.2 裁决 |
| **C-NEW-4** SQ::enqueue 签名 void vs bool 矛盾 | MEDIUM | specs/dgpu-board.md §4.1 改 `bool enqueue` |
| **C-NEW-5** mock .so 路径/依赖未定义 | MEDIUM | tasks.md T-P1-A4 |
| **C-NEW-6** P4 删 cpptlm_bridge.h 与 17 条断言的生存关系 | MEDIUM | 风险登记 R10（per Oracle D.3）|
| **C-NEW-7** `g_ptx_emu_driver` 位置含糊 | LOW | 风险登记 R11（per Oracle D.3）|
| **Oracle A.4** ±15% cycle tolerance 科学性不足 | MEDIUM | Gate #6 改写为功能等价 blocking + cycle informational |
| **Oracle A.5** Doorbell 不在 BAR0 MMIO 空间 | MEDIUM | specs/dgpu-board.md §2.5 新增 BAR0 MMIO 地址映射表 |
| **Oracle B.3** mock .so 双层策略 | LOW | specs/sm-executor.md §7.2 改双层 + 删 mock_smock_pcie_device.so |
| **Oracle Section C** JSON-config E2E 测试规格 | 用户主需求 | tasks.md T-P3-4 + Gate #9 新增 |

## v3.0.0 弃用与遗留路径声明 [Oracle 二次审查 T-P3-5]

> **触发**: Oracle 二次审查 F-NEW-CONCERN-1 [HIGH] — 12 个 SM 模块 + 18 个 GPU 测试在 v3.0.0 继续 PASS 但无 dGPU 路径调用方,构成"静默弃用" + "假绿"风险。
>
> **结论**: **Option A (Pure PTX-EMU delegation) 路径本身正确**,但必须显式声明模块命运与 boundary。

### 12 SM 模块 v3.0.0 命运表

| # | 模块 | 位置 | v3.0.0 命运 | 理由 |
|:---:|---|---|---|---|
| 1 | `GpuComputeUnitTLM` | `include/tlm/gpu/gpu_compute_unit_tlm.hh/.cc` | ⚠️ **Legacy** | apu_soc / Phase 8.A 单元测试覆盖;dGPU 路径不调用 |
| 2 | `MinimalWarpSchedulerTLM` | `include/tlm/gpu/minimal_warp_scheduler_tlm.hh/.cc` | ⚠️ **Legacy** | 同上 |
| 3 | `WavefrontTLM` | `include/tlm/gpu/wavefront_tlm.hh/.cc` | ⚠️ **Legacy** | 同上 |
| 4 | `ScoreboardTLM` | `include/tlm/gpu/scoreboard_tlm.hh/.cc` | ⚠️ **Legacy + Internal-deprecate** | 原为 D1-Full Adapter 设计;D1-Full 路径废止 |
| 5 | `PipelineTLM` | `include/tlm/gpu/pipeline_tlm.hh/.cc` | ⚠️ **Legacy + Internal-deprecate** | 同上 |
| 6 | `TensorCoreTLM` | `include/tlm/gpu/tensor_core_tlm.hh/.cc` | ⚠️ **Legacy + Internal-deprecate** | 同上 |
| 7 | `SharedMemoryTLM` | `include/tlm/gpu/shared_memory_tlm.hh/.cc` | ⚠️ **Legacy** | apu_soc 单元测试覆盖 |
| 8 | `VectorRegfileTLM` | `include/tlm/gpu/vector_regfile_tlm.hh/.cc` | ⚠️ **Legacy** | 同上 |
| 9 | `GpuMeshNoC` | `include/tlm/gpu/gpu_mesh_noc_tlm.hh/.cc` | ✅ **保留重构** | DGpuBoardTLM 内部 VRAM routing 可复用 |
| 10 | `MemoryClusterTLM` | `include/tlm/gpu/memory_cluster_tlm.hh/.cc` | ✅ **保留重构** | DGpuBoardTLM 内部 VRAM backing 可复用 |
| 11 | `GpuSocTLM` | `include/tlm/gpu/gpu_soc_tlm.hh/.cc` | ✅ **保留**(apu_soc 顶层需要) | 与 dGPU board path 正交 |
| 12 | `AsyncCompletionAdapter` | `include/tlm/gpu/async_completion_adapter.hh` | 🗑️ **删除 `setCompletionCallback` 入口** | T-P3-2 替换为 CompletionRing |

### 显式声明

- ❌ **v3.0.0 不删除**上述 Legacy SM 模块(避免 apu_soc 兼容破坏 + Phase 8.A/B 单元测试失效)
- ❌ **v3.0.0 不实施** SMContext Adapter 注入(ADR-NV-02 D1-Full 路径废止)
- ❌ **v3.0.0 不要求** "完备 Nvidia SM 仿真" — PTX-EMU `libptxemu_device.so::image_execute` 已是自包含 SM 执行器(per ADR-NV-02 §2.4 "PTX-EMU 无独立 Scoreboard 组件"),CppTLM 端只需 PCIe 设备语义 + VRAM backing + 8 ABI 透传
- ⚠️ **Gate #7 baseline** = "全部 ≥ 846 PASS (含 legacy SM)" 而非 "dGPU 路径 N PASS"

### 影响范围

- 18 个 GPU 单元测试(`test_gpu_compute_unit_*.cc` / `test_minimal_warp_scheduler_tlm.cc` / `test_pipeline_tlm.cc` / `test_scoreboard_tlm.cc` / `test_tensor_core_tlm.cc` / `test_shared_memory_tlm.cc` / `test_vector_regfile_tlm.cc` / `test_wavefront_tlm.cc` / `test_memory_cluster_tlm.cc` / `test_gpu_mesh_noc_tlm.cc` / `test_simmodule_gpu_hierarchy.cc` / `test_gpu_cluster_shared.cc` / `test_gpu_standalone.cc` / `test_gpu_soc_phase8a.cc` / `test_gpu_soc_tlm.cc` 等): ✅ 保留作为 legacy 单元测试,新增 `[legacy]` Catch2 标签区分

## Migration (Phase 化详细)

完整 P0-P4 操作步骤 + commit 模板 + 验证命令，详见：

- [`docs/05-advanced/cpptlm-v3-dgpu-extract-action-plan.md`](../../05-advanced/cpptlm-v3-dgpu-extract-action-plan.md)（如已 commit）或 `/tmp/cpptlm-action-plan.md`（起草版本，22 操作步骤，~740 行）
- [`tasks.md`](tasks.md)（P0-P4 任务清单）

## References

### 上游 ADR
- UsrLinuxEmu [ADR-090 v2 commit `e03b5a1`](https://github.com/chisuhua/UsrLinuxEmu/commit/e03b5a1)（✅ Accepted）
- UsrLinuxEmu [ADR-090 v2 commit `37a91b6`](https://github.com/chisuhua/UsrLinuxEmu/commit/37a91b6)（Oracle F-NEW-2 §D5/§D6 修订）
- UsrLinuxEmu [annex §E 跟踪表](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/05-advanced/adr-090-cross-repo-coordination.md)
- UsrLinuxEmu [ADR-036 three-way separation](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-036-three-way-separation.md)
- UsrLinuxEmu [ADR-023 HAL append-only](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-023-hal-interface.md)
- UsrLinuxEmu [ADR-088 §D6.2 BREAKING 流程](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md)
- UsrLinuxEmu [ADR-035 §R5.1 cross-repo commit 顺序](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-035-governance-policy.md)

### 跨仓 ADR/TADR/HSK
- PTX-EMU [ADR-0029 §D8](https://github.com/chisuhua/PTX-EMU/blob/main/docs/adr/ADR-0029-ptxemu-image-executor.md)（HSK-1 真相源，已 ship）
- PTX-EMU [HSK-6 公告 commit `25e36f60`](https://github.com/chisuhua/PTX-EMU/commit/25e36f60)
- PTX-EMU [HSK-PROTOCOL-NOTES commit `bf1a652d`](https://github.com/chisuhua/PTX-EMU/commit/bf1a652d)
- CppTLM [HSK-6 ack commit `369cf71`](https://github.com/chisuhua/CppTLM/commit/369cf71)
- CppTLM [issue #19 v3.0 RFC](https://github.com/chisuhua/CppTLM/issues/19)（Gate #2 ack）
- TaskRunner [tadr-308 openspec change commit `6b1d39d`](https://github.com/chisuhua/TaskRunner/commit/6b1d39d)（独立并行）

### Oracle Session
- `ses_ff2106f84ffeM2oItBEa9iu4hL`（v1 启动，识别 ADR-076 v1 违规）
- `ses_fef78854dffeLfDJh7p8ELuMLy`（v2 决策 + 4 轮评估 + 5-step self-check）

### 仓 HEAD 锚点（v2 §C0.4 新规）
```
PTX-EMU    @ccd34155 (真相源持有方)
CppTLM     @585e4ff (实施起点)
UsrLinuxEmu@e03b5a1 + 37a91b6 (ADR-090 v2)
TaskRunner @cdb3633 (submodule)
```

### 关键文件位置（CppTLM 仓 @585e4ff）
- `include/cudart/cpptlm_bridge.h:14-16`（HSK-1 真相源自述）
- `include/cudart/cpptlm_bridge.h:55`（CPPTLMBRIDGE_VERSION = 2 冻结点）
- `include/cudart/cpptlm_bridge.h:243-306`（16 条 G-D4 static_assert，迁移源）
- `include/tlm/gpu/ptx_emu_driver.hh:19`（IPtxEmuDriver 接口，待删）
- `include/tlm/gpu/ptx_emu_driver.hh:27`（1 条 G-D4 static_assert，迁移源）
- `include/tlm/gpu/ptx_emu_driver.hh:51`（DriverWrapper 类，待删）
- `src/tlm/gpu/ptx_emu_driver_shim.cc`（待删）
- `include/tlm/gpu/memory_bridge.hh`（MemoryBridge 类，待删）
- `include/tlm/gpu/kernel_launch_tlm.hh:30`（KernelLaunchRequest，复用）

---

**起草**: Sisyphus (2026-08-18, 基于 HSK-6 ack `369cf71` + UsrLinuxEmu ADR-090 v2 Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` 4 轮评估)
**Owner**: CppTLM Team
**配合**: [`tasks.md`](tasks.md)