# ADR-X.15: cpptlm-v3-dgpu-extract (角色反转 + v3.0.0 BREAKING bump + 11 项删除清单)

> **状态**: ✅ Accepted — 2026-08-18
> **日期**: 2026-08-18
> **影响**: CppTLM 角色语义、CMakeLists 版本号、11 个接口/头文件/全局符号的物理生命周期
> **类别**: 跨仓协议同步 + BREAKING 架构变更
> **来源**: PTX-EMU HSK-6 (`25e36f60`) + CppTLM HSK-6 ack (`369cf71`) + UsrLinuxEmu ADR-090 v2 (`e03b5a1` + `37a91b6`) + CppTLM #19 v3.0 RFC

---

## 1. Context (背景)

### 1.1 原架构假设 (v2.1)

v2.1 时期，CppTLM 在 PTX-EMU 协同仿真中扮演"桥接层 (bridge)"角色：

- `MemoryBridge` (CppTLM) 暴露 `CppTLMBridge` 虚接口给 PTX-EMU
- PTX-EMU 通过 `g_cpptlm_bridge` 全局指针 + `cpptlm_attach_bridge()` ABI 调用 CppTLM 时序模拟
- `IPtxEmuDriver` + `DriverWrapper` + `g_ptx_emu_driver` + `cpptlm_set_driver()` 形成**反向** ABI（PTX-EMU → CppTLM）
- CppTLM vendored 副本 PTX-EMU 4 个 cudart 头（`cpptlm_bridge.h` 14837 字节 + 3 接口 vendored）

此架构基于 UsrLinuxEmu ADR-076 v1 模式（HAL dlopen `libptxemu_device.so` 同步执行）。

### 1.2 触发的双重 Oracle session 评估

**2026-08-17 Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL`** 识别 ADR-076 v1 模式违反 UsrLinuxEmu ADR-036 three-way separation（HAL 桥承担硬件行为提供者职责）。

**2026-08-18 Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy`**（HSK-6 草案 + ADR-090 v2 §D5/§D6）4 轮评估 + 5-step self-check 通过。

### 1.3 跨仓协议栈（已发出 + 已 ack）

| 协议 | 发出方 | commit | 接收方 | ack commit | 状态 |
|---|---|---|---|---|:---:|
| HSK-1 cppTLMBridge ABI | PTX-EMU | `8dc000ec` | CppTLM | `73e5422` | ✅ Closed |
| HSK-2 ANTLR4 4.13.2 | PTX-EMU | (N/A) | CppTLM | — | ✅ Closed (无 CppTLM 端变更) |
| HSK-3 ExternalProject_Add | PTX-EMU | `c16ff97` | CppTLM | — | ✅ Closed |
| HSK-4 3 纯虚接口头 | PTX-EMU | `8acfd2d1` / `9e7361b9` / `463038e0` | CppTLM | (rebase `367fd6a5`) | 🟡 待 CppTLM rebase |
| HSK-5 `advance()` deferred | PTX-EMU | — | CppTLM | — | 🟡 Deferred → **CANCELLED by HSK-6** |
| **HSK-6 CppTLM 桥接消费关系废止** | **PTX-EMU** | **`25e36f60`** | **CppTLM** | **`369cf71`** | **✅ Accepted** |

### 1.4 上游决策

- **UsrLinuxEmu ADR-088 v3**: 跨仓 cpptlm-emu-bridge 协调（cpptlm + usrlinuxemu + ptx-emu + taskrunner 四方协调，H2D DMA VRAM 加载路径）
- **UsrLinuxEmu ADR-090 v2** (`e03b5a1` + `37a91b6`): §D3.3 DGpuBar/Doorbell/SQ/CQ 最小完备集；§D3.4 CompletionRing host_notify 重设计；§D5/§D6 HSK-6 联发协议 + 两阶段删除
- **CppTLM #19 v3.0 RFC** (Gate #2 ack 2026-08-18): §D3 Mode B 终态；§E 9 周 P0-P4 时间线

---

## 2. Decision (决策)

### D1. 角色反转 (Role Reversal)

✅ **CppTLM 从"桥接层 (bridge)"角色转变为"被驱动的 dGPU 板卡 (PCIe device semantics)"角色**

- CppTLM 不再是 PTX-EMU 的同步执行后端（`submit_kernel` 异步路径废弃）
- CppTLM 模拟**真实 GPU 板卡 PCIe 设备**语义（CFG + BAR0 MMIO + Doorbell SQ tail + CQ）
- UsrLinuxEmu + TaskRunner = CUDA app 入口；CppTLM = 被驱动设备

**理由**：
- 对齐 gem5 full-system GPU 工业惯例
- 解除 HAL 桥承担硬件行为提供者的 ADR-036 违反
- 物理上 PCIe 设备是 async device，host 必须 doorbell 通知，与 CppTLM 异步仿真语义对齐

**实施路径**: P1 双轨 (W1-3) 实施 `PtxEmuSubmodule` + `DGpuBar` + `Doorbell` + `SubmissionQueue` + `CompletionRing` 骨架。

### D2. v3.0.0 BREAKING bump

✅ **CMakeLists.txt 版本号从 `2.1.0` → `3.0.0`**

- 删除 11 项（见 §3 清单）— 任何 vendored `cpptlm_bridge.h` 用户立即崩溃
- 新建 `include/cpptlm_version.h` 提供版本宏 `CPPTLM_VERSION_MAJOR/MINOR/PATCH`
- 808 测试 baseline（从 v2.1.0 ~764 baseline 扩展）

**理由**:
- 11 项删除构成 API 表面破坏性变更
- 与 PTX-EMU `CPPTLMBRIDGE_VERSION=2` 永久冻结承诺一致（HSK-6 §1 协议）
- 8 个 vendored 接口全部废弃，无兼容路径

### D3. CPPTLMBRIDGE_VERSION 永久冻结于 2

✅ **`CPPTLMBRIDGE_VERSION = 2` 永久冻结**

- 真相源保留不删：`PTX-EMU@ccd34155:include/cudart/cpptlm_bridge.h` 294 行（PTX-EMU 侧继续维护）
- 任何解冻企图（包括但不限于升 VERSION 3、CppTLM 兼容新 VERSION）必须发出 **HSK-7** 公告
- 禁止静默 bump（per `PTX-EMU@ccd34155:include/cudart/AGENTS.md` "CPPTLMBRIDGE_VERSION bump 治理"）

**理由**:
- 11 项删除后，CppTLM 不再是消费方，无法触发 ABI 漂移
- 14 天 ack 窗口已建立 HSK-1~5 历史惯例，HSK-7 流程可执行

### D4. 11 项物理删除清单 (P4 阶段)

✅ **HSK-6 §2.1 表 1-11 全部物理删除（前置条件：P0-1 门禁 + Mode B E2E + HSK-6 ACCEPTED）**

| # | 项 | 位置 (CppTLM@`585e4ff`) | 状态 |
|---|---|---|:---:|
| 1 | `MemoryBridge` 类 | `include/tlm/gpu/memory_bridge.hh` | 🟡 待 P4 删 |
| 2 | `IPtxEmuDriver` 接口 | `include/tlm/gpu/ptx_emu_driver.hh:19` | 🟡 待 P4 删 |
| 3 | `DriverWrapper` 类 | `include/tlm/gpu/ptx_emu_driver.hh:51` | 🟡 待 P4 删 |
| 4 | `g_ptx_emu_driver` 全局符号 | 全仓引用 | 🟡 待 P4 删 |
| 5 | `cpptlm_set_driver` ABI 入口 | 全仓引用 | � 待 P4 删 |
| 6 | `ptx_emu_driver_shim.cc` | `src/tlm/gpu/ptx_emu_driver_shim.cc` | 🟡 待 P4 删 |
| 7 | vendored `cpptlm_bridge.h` | `include/cudart/cpptlm_bridge.h` (14837 字节) | 🟡 待 P4 删 |
| 8 | vendored `pipeline_interface.h` | `include/cudart/pipeline_interface.h` (1659 字节) | 🟡 待 P4 删 |
| 9 | vendored `scoreboard_interface.h` | `include/cudart/scoreboard_interface.h` (1278 字节) | 🟡 待 P4 删 |
| 10 | vendored `tensor_core_interface.h` | `include/cudart/tensor_core_interface.h` (1709 字节) | 🟡 待 P4 删 |
| 11 | `PtxEmuDriverApi` 布局锁 (已迁) | `include/tlm/gpu/ptx_emu_driver.hh:27` | ✅ 已迁至 `abi_guards.h` (`fa2b3ec`) |

**理由**:
- 实施 dGPU board (DGpuBar + Doorbell + SQ/CQ) 后，`MemoryBridge` 同步执行路径完全废弃
- `IPtxEmuDriver` + `DriverWrapper` + `g_ptx_emu_driver` + `cpptlm_set_driver` 全部被 `PtxEmuSubmodule` (dlopen `libptxemu_device.so` + 8 function pointers) 替代
- 4 个 vendored cudart 头被 PTX-EMU git submodule 替代（per UsrLinuxEmu ADR-090 v2 §D3.2 Option B）

### D5. HSK-5 `advance()` deferred → CANCELLED

✅ **HSK-5 中保留为 deferred 的 `PtxEmuDriverApi::advance()` 函数随 `IPtxEmuDriver` 接口整体删除而永久废止**

- HSK-5 状态由 deferred → **CANCELLED by HSK-6**
- 协议序列不再保留悬空项

---

## 3. P0-1 硬门禁（已完成）

✅ **G-D4 静态断言集中托管（HSK-6 §2.2 P0-1）— commit `fa2b3ec`**

**变更**:
- 新建 `include/cudart/abi_guards.h` (112 行) — 集中托管 17 条 `static_assert`
  - 16 条 from `cpptlm_bridge.h:243-306`（PipelineId 6 + TcPrecision 6 + is_same_v 4）
  - 1 条 from `ptx_emu_driver.hh:27`（`sizeof(PtxEmuDriverApi) == 64` 布局锁）
- 修改 `include/cudart/cpptlm_bridge.h` (-75/+6) — 删除 G-D4 namespace 块，末尾 `#include "cudart/abi_guards.h"`（保留 225 行 `cudaStream_t` 断言，不在 HSK-6 §2.2 范围）
- 修改 `include/tlm/gpu/ptx_emu_driver.hh` (-4/+2) — 删除 sizeof 断言，`#include "cudart/abi_guards.h"`

**双重验证**:
| 项 | 命令 | 期望 | 实际 |
|---|---|---|:---:|
| abi_guards.h | `grep -c '^static_assert' include/cudart/abi_guards.h` | 17 | ✅ 17 |
| cpptlm_bridge.h | `grep -c '^static_assert' include/cudart/cpptlm_bridge.h` | 1 | ✅ 1（仅 225 行 cudaStream_t）|
| ptx_emu_driver.hh | `grep -c '^static_assert' include/tlm/gpu/ptx_emu_driver.hh` | 0 | ✅ 0 |
| 反向验证 | 故意让 `PipelineId::P0_INT_FP32 == 1` | 编译失败 | ✅ `abi_guards.h:40` 触发 `G-D4 ABI drift: PipelineId::P0_INT_FP32 != 0 (expected 0)` |
| 正向验证 | `cmake --build build -j$(nproc)` | 100% 成功 | ✅ 无编译错误 |
| 测试 | `./build/bin/cpptlm_tests` | 全部 PASS | ✅ 846 test cases / 18910 assertions |

---

## 4. 替代路径（实施路线，HSK-6 §1）

### 4.1 P0 (W1, 已完成)

- ✅ T-P0-1 G-D4 静态断言迁移
- � T-P0-2 Mode A 冻结（给 `MemoryBridge` / `IPtxEmuDriver` / `DriverWrapper` 加 `[[deprecated]]` 标注）

### 4.2 P1 双轨 (W1-3)

- **轨 A — PtxEmuSubmodule façade** (`include/tlm/gpu/ptx_emu_submodule.hh` + `.cc`, `namespace tlm`)
  - 封装 PTX-EMU 8 函数 ABI (CPPTLM_MODULE_VERSION 2)
  - DSO: `libptxemu_device.so`
  - 8 个 dlsym function pointers (per `PTX-EMU@ccd34155:include/cudart/cpptlm_module.h:18-52`)

- **轨 B — dGPU 最小完整集** (per UsrLinuxEmu ADR-090 v2 §D3.3)
  - `DGpuBar` (`include/tlm/gpu/dgpu_bar.hh`) — PCIe BAR0 MMIO（CFG + BAR0 regs + BAR1 VRAM backing）
  - `Doorbell` (`include/tlm/gpu/doorbell.hh`) — SQ tail register (host→device 异步信号)
  - `SubmissionQueue` (`include/tlm/gpu/submission_queue.hh`) — SQ consumer (NVMe 模型)
  - `CompletionRing` (`include/tlm/gpu/completion_ring.hh`) — 重设计替代 `AsyncCompletionAdapter::setCompletionCallback`（per §D3.4，避免多 stream 并发 std::function 回调表）

- **复用**: `MemoryCluster` + `GpuNoC`（已有，不新写）

### 4.3 P2 (W4-6) 收敛

- `ISmExecutor` 3 ABI 汇合（`installImage` / `dispatch` / `setCompletionCallback`）
- `PtxEmuSubmodule` 集成到 `ISmExecutor`
- E2E 路径实施

### 4.4 P3 (W6-8) 重构

- `KernelLaunchTLM` 重构适配 `ISmExecutor`
- `CompletionRing` push/host_notify 实施
- Mode A/B dual-rail E2E（5 类 microbenchmark：GEMM / vector_add / FlashAttention / stencil / SpMV，cycle 数 ±15% tolerance）

### 4.5 P4 (W8-9) 物理删除

- 11 项物理删除清单（§2.D4）
- CMakeLists.txt v2.1.0 → v3.0.0
- 新建 `include/cpptlm_version.h`
- 808 测试 baseline 验证
- Tag `v3.0.0`

---

## 5. 跨仓 commit 顺序（per ADR-035 §R5.1）

```
[1] PTX-EMU commit `25e36f60`: HSK-6 公告发出 ✅
   ↓ (CppTLM ack + UsrLinuxEmu ack)
[2] CppTLM commit `369cf71`: HSK-6 ack 响应 ✅
   ↓
[3] CppTLM commit `fa2b3ec`: P0-1 G-D4 静态断言迁移 ✅ (本 ADR 实施起点)
   ↓
[4] CppTLM commit (待): Mode A 冻结 + HSK-6 ack 跟踪
   ↓
[5] UsrLinuxEmu commit (待): submodule bump external/PTX-EMU + E2E 集成测试
   ↓
[6] CppTLM commit (待): Phase 2 物理删除 + v2.1.0 → v3.0.0 bump
   ↓ (并行)
[5'] TaskRunner (并行): tadr-308 创建 + `IGpuDriver::load_kernel_module` 1 method 新增
```

---

## 6. Acceptance Gates (验收门)

| Gate | 内容 | 验证方法 | 状态 |
|---|---|---|:---:|
| G-X.15-1 | G-D4 17 条静态断言迁至 abi_guards.h | `grep -c` + 反向验证 | ✅ `fa2b3ec` |
| G-X.15-2 | CPPTLMBRIDGE_VERSION = 2 永久冻结 | HSK-7 公告触发条件 | 🟡 W8-9 锁定 |
| G-X.15-3 | Mode A 冻结（11 项加 `[[deprecated]]`）| 编译告警 + 不影响现有调用 | � 待实施 |
| G-X.15-4 | PtxEmuSubmodule + DGpu/Doorbell/SQ/CQ 骨架 | 编译通过 + mock library 测试 | 🟡 待实施 |
| G-X.15-5 | ISmExecutor 汇合 + E2E 路径 | 端到端 dispatch 链路 | 🟡 待实施 |
| G-X.15-6 | Mode A/B dual-rail E2E（5 类 microbenchmark）| cycle 数 ±15% tolerance | 🟡 待实施 |
| G-X.15-7 | 11 项物理删除 | `grep -r` 验证无残留 + cmake --build | 🟡 待实施 |
| G-X.15-8 | 808 测试 baseline | ctest PASS | 🟡 待实施 |
| G-X.15-9 | Tag v3.0.0 | `git tag -a v3.0.0` | 🟡 待实施 |

---

## 7. Consequences (后果)

### 7.1 正面

- **架构清晰化**：CppTLM 不再跨"被调用方 + 桥接层 + dGPU 模拟"三重角色，回归纯设备仿真
- **物理删除可逆门禁解除**：11 项删除后，`MemoryBridge` 同步执行路径不再被任何代码引用
- **跨仓协议栈完整闭环**：HSK-1~6 全部 Closed/Accepted，序列不留悬空项（HSK-5 advance() 已 CANCELLED）
- **测试基线扩展**：从 ~764 测试扩展到 808 测试（per UsrLinuxEmu ADR-090 v2 §D6.2 同步扩展）

### 7.2 负面

- **BREAKING bump 影响**：任何外部 `MemoryBridge::submit_kernel` / `IPtxEmuDriver::advance` 直接调用方必须迁移至 PtxEmuSubmodule façade
- **Mode A 冻结期间 deprecation warning**：现有调用方会看到编译器告警（不阻断构建）
- **ANTLR4 章节移除**：原 v3.0 RFC 含 ANTLR4 spike，HSK-6 ack 后确认不在 CppTLM scope（per HSK-6 §3.3 + Oracle session），移除降低 v3.0 RFC 复杂度

### 7.3 风险（per UsrLinuxEmu 行动计划 §风险登记）

| # | 风险 | 概率 | 影响 | 缓解 |
|---|---|:---:|:---:|---|
| R1 | P0-1 静态断言迁移遗漏 | 中 | 高 | `grep -c` + 反向验证（已实施）；PTX-EMU 真相源 `ccd34155` 行数对比 |
| R2 | ANTLR4 误解 | 低 | 中 | HSK-6 §3.3 + Oracle session 决议明确不在 CppTLM scope |
| R3 | PtxEmuSubmodule dlsym 失败（libptxemu_device.so 不可用）| 中 | 中 | 启动时 `dlerror()` 报错；Mode A 兜底；测试用 mock library |
| R4 | DGpuBar / Doorbell / SQ/CQ 实现复杂度高估 | 中 | 中 | W1-3 三周实施；骨架先实现，P3 重构完善 |
| R5 | 808 测试 baseline 难达到 | 中 | 中 | 现有 846 baseline + P0-P3 累计 ~600 新测试 |
| R6 | HSK-6 ack 超时（2026-09-01 后仍未收齐）| 低 | 低 | 14 天 + 超时无异议兜底条款；HSK-6 ack 已 `369cf71` 收到 |

---

## 8. References

### 8.1 跨仓锚点

- **PTX-EMU HSK-6** (commit `25e36f60`): https://github.com/chisuhua/PTX-EMU/blob/main/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md
- **PTX-EMU 真相源** (commit `ccd34155`): `include/cudart/cpptlm_bridge.h:14-16` 自述 "PTX-EMU 是 ABI 提供方，CppTLM 是消费方"
- **PTX-EMU 8 ABI** (commit `ccd34155`): `include/cudart/cpptlm_module.h:18-52`
- **UsrLinuxEmu ADR-088 v3**: 跨仓 cpptlm-emu-bridge 协调（commit `e03b5a1`）
- **UsrLinuxEmu ADR-090 v2** (commit `e03b5a1` + `37a91b6`): https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md

### 8.2 CppTLM 仓内锚点

- **CppTLM #19 v3.0 RFC** (Gate #2 ack 2026-08-18): https://github.com/chisuhua/CppTLM/issues/19
- **CppTLM HSK-6 ack 响应**: `docs/superpowers/specs/2026-08-18-hsk-6-response.md` (commit `369cf71`)
- **CppTLM openspec change**: `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/` (commit `5d9473a` + `9e9a081` 修正 + `fa2b3ec` 本 ADR 实施起点)
- **P0-1 G-D4 静态断言**: `include/cudart/abi_guards.h` (commit `fa2b3ec`)
- **UsrLinuxEmu 行动计划**（配套实施指南）: `/tmp/cpptlm-action-plan.md`

### 8.3 Oracle Sessions

- **HSK-6 草案评估**: `ses_fef78854dffeLfDJh7p8ELuMLy` (4 轮评估 + 5-step self-check)
- **ADR-076 v1 违规识别**: `ses_ff2106f84ffeM2oItBEa9iu4hL` (2026-08-17)

### 8.4 相关 ADR (CppTLM 仓内)

- ADR-X.1~X.14 (事务/错误/复位/插件/构建/集成等)
- ADR-NV-01 (gpu_soc 独立 SoC 仿真目标)
- ADR-NV-02 (Phase 8.B D1-Full 全栈注入策略)
- ADR-INC-01 (ApuSoC incorporate_parent late-binding)

### 8.5 历史 HSK 响应

- HSK-1~3 响应: `2026-07-17-hsk-1-2-3-responses.md`
- HSK-4~5 响应: `2026-07-17-hsk-4-5-responses.md`
- **HSK-6 响应**: `2026-08-18-hsk-6-response.md` (本 ADR 决策源头)
