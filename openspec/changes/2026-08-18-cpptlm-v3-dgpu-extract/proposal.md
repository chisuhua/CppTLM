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

### 4. 4 测试文件处置（per UsrLinuxEmu `37a91b6` §D6.2）

| 测试 | 处置 |
|---|---|
| `tests/test_memory_bridge.cc` | 🗑️ **删除**（测试对象 `MemoryBridge` 物理删除） |
| `tests/test_memory_bridge_poll.cc` | 🔄 **重写**为 `tests/test_completion_ring.cc`（`poll_kernel` 语义 → CompletionRing push/host_notify） |
| `tests/test_kernel_launch_tlm_ext.cc` | ✅ **保留 + 改符号**（`MockPtxEmuDriver` → SQ/CQ doorbell mock；`KernelLaunchRequest`/`setMemoryBridge` 复用） |
| `tests/test_gpu_soc_perf.cc` | ✂️ **拆分**（scoreboard perf 保留；MemoryBridge poll perf → CompletionRing） |

### 5. 9 周双轨时间线（per #19 v3.0 RFC）

```
W1      (P0 冻结):    G-D4 迁移 + HSK-6 ack + Mode A [[deprecated]]
W1-3    (P1 双轨):    PtxEmuSubmodule + DGpuBar/Doorbell/SQ/CQ + CompletionRing
W4-6    (P2 收敛):    ISmExecutor + PtxEmuSubmodule 汇合 + E2E
W6-8    (P3 重构):    KernelLaunchTLM 重构 + CompletionRing push + dual-rail E2E
W8-9    (P4 物理删除): 11 项删除 + v2.1.0 → v3.0.0 bump + 808 测试验证 + tag
```

## Acceptance Gate

| Gate | Owner | 当前状态 |
|---|---|:---:|
| #1 UsrLinuxEmu ADR-090 v2 ✅ Accepted | UsrLinuxEmu | ✅ commit `37a91b6` |
| #2 CppTLM maintainer ack | CppTLM | ✅ commit `369cf71`（HSK-6 ack） |
| #3 PTX-EMU Architecture Team HSK-6 发出 | PTX-EMU | ✅ commit `25e36f60` |
| #4 PTX-EMU HSK-6 + UsrLinuxEmu 双 ack 收齐 | PTX-EMU + UsrLinuxEmu | 🚫 Ack 截止 2026-09-01 |
| #5 G-D4 17 条 static_assert 迁移 | CppTLM | ⏳ W1 启动 |
| #6 Mode A/B dual-rail E2E (5 类 microbenchmark, cycle ±15%) | CppTLM + UsrLinuxEmu | ⏳ W6-8 |
| #7 808 测试 baseline 验证 | CppTLM | ⏳ W8-9 |
| #8 v3.0.0 tag + 删除 11 项完成 | CppTLM | ⏳ W9 |

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