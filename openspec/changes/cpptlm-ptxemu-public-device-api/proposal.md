# cpptlm-ptxemu-public-device-api: PTX-EMU 端公共设备 API + 双层 facade 迁移

> **状态**: 📋 Proposed — 2026-08-21 · **日期**: 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联变更**: [`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (S1, 已完成, W1-2 PTX-EMU 深度集成基础设施)
> **Handshake**: HSK-8（CppTLM 发起 + PTX-EMU ack, 锁定 `ptxemu/device_api.h` 为唯一公共契约）
> **目标**: CppTLM 仅 include PTX-EMU 的公共头 (`ptxemu/device_api.h`), PTX-EMU 实现细节封装在 PTX-EMU 端静态库 `.a` 内
> **Oracle 评审**: ses_fdb70164bffe2vBN71uiaV90aY (4 轮咨询完成, 2026-08-21)

---

## Why

S1 (`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`) 通过 `cmake/PTXEmuCore.cmake` shim 方案 + `ptx_emu_submodule_mvp.cc` / `cuda_core_adapter_mvp.cc` 编译防火墙实现了 PTX-EMU 深度集成，OFF 路径 850/850 + ON 路径 891/891 测试全 PASS。

但 S1 当前的 "CppTLM 端编译防火墙" **不满足** 用户要求的封装级别：

- `ptx_emu_submodule_mvp.cc` 直接 include 12 个 PTX-EMU 实现头（`ptxsim/sm_context.h`/`ptxsim/warp_context.h`/`ptxsim/thread_context.h`/`ptxsim/warp_state.h`/`ptxsim/gpu_context.h`/`ptxsim/instruction_factory.h`/`ptxsim/execution_types.h`/`ptx_ir/operand_context.h`/`ptx_ir/statement_context.h`/`ptx_ir/ptxir_reader.h`/`register/register_bank_manager.h`/`memory/simple_memory.h`），并**完整持有** PTX-EMU 内部类（`GPUContext`/`SMContext`/`WarpContext`/`ThreadContext`/`WarpState`）的完整类型
- `cuda_core_adapter_mvp.cc` 调 `sm->exe_once()`/`sm->set_scoreboard()`/`thread->get_state()`/`warp->get_thread()` 等 PTX-EMU 内部 API
- 编译防火墙只是"约定级封装"——头文件漂移照样炸 CppTLM 编译，爆炸点集中在 2 个文件而非消除爆炸点
- PTX-EMU 远端 `origin/main` (8 commits ahead of `c2038a93`) 已经主动清理 `cleanup-cudart-cpptlm-bridge-coupling` Phase 1-4（1018 行删除），HSK-6 已接受桥梁废弃，CppTLM 端继续走"直接 include 实现头"路径与 PTX-EMU 端清理方向不一致

**触发事件**:
- 2026-08-21 PTX-EMU 远端 main 主动删除 cpptlm 桥接代码（`include/cudart/cpptlm_bridge.h` + `cudart/cpptlm_bridge/PtxEmuDriverShim.h/cpp` + `cudart/stub_bridge.h` + `memory.cpp` 中所有 GLOBAL LD/ST bridge 调用）
- 2026-08-21 S1 工作树完成（13 新文件 + 6 修改文件未提交，850/891 测试全 PASS）
- 2026-08-21 Oracle ses_fdb70164bffe2vBN71uiaV90aY 完成 4 轮咨询，输出选项 X (PTX-EMU 端公共设备 API `IPtxEmuDevice`)
- 2026-08-21 用户给出新方向："CppTLM 只需要 header 的源代码，cpp 代码可以不用暴露给 CppTLM，只要最后通过库链接后 CppTLM 可以调用 PTX-EMU 端构建的函数"

## What Changes

### 1. PTX-EMU 端新增（独立 PR, PTX-EMU 仓库）

**BREAKING**: PTX-EMU 仓库新增公共设备 API 头（PTX-EMU 端 1-2d Short~Medium 工作量）

| 新增 | 用途 |
|------|------|
| `include/ptxemu/device_api.h` (~200 行) | PTX-EMU **唯一** 公共头（CppTLM 唯一入口） |
| `include/ptxemu/ir/statement.h` | `StatementContext` 晋升为公共 IR 值类型（纯数据） |
| `src/ptxemu/device_api_impl.cc` (~400 行) | 薄适配层：封装 `GPUContext`/`SMContext`/`WarpContext`/`ThreadContext`/`WarpState`/`HardwareMemoryManager`/`RegisterBankManager`，做 DTO 映射 |
| `tests/build_cpptlm_consume/consumer_smoke.cc` | 最小 consumer exe（仅 include `ptxemu/device_api.h` 即可链接通过） |
| `tests/build_cpptlm_consume/drift_check.cmake` | GLOB drift 门禁（PTX-EMU `ptxemu_core` target 源文件 vs `git ls-files`） |
| `CMakeLists.txt` 新增 `add_library(ptxemu_core STATIC ...)` | PTX-EMU 端库目标（PUBLIC `include/ptxemu` + PRIVATE 内部头目录） |
| `option(PTXEMU_BUILD_TESTING OFF)` 默认值 | 隔离 PTX-EMU 自身 tests/tools 到 `if(PROJECT_IS_TOP_LEVEL)` |
| `static_assert` 锁 | PTX-EMU 内部 `EXE_STATE` ↔ 公共 `ThreadState` 同构（abi_guards 模式内化） |

### 2. CppTLM 端改造（bum submodule + 单 PR 三件事）

**BREAKING**: `cmake/PTXEmuCore.cmake` 全 246 行删除（4 门禁 + 74 文件清单 + 5 层 include 排序 + stub.cc append）

| 改动 | 用途 |
|------|------|
| 根 `CMakeLists.txt` `add_subdirectory(external/PTX-EMU)` | 替换 `include(cmake/PTXEmuCore.cmake)` |
| `target_link_libraries(cpptlm_core PUBLIC ptxemu_core)` | 替换显式源列表（PUBLIC 链接保留） |
| `ptx_emu_submodule_mvp.cc` 12 include → 1 include | `ptxemu/device_api.h` |
| `cuda_core_adapter_mvp.cc` 5 include → 1 include | `ptxemu/device_api.h` |
| `sm->exe_once()` → `device->step_once()` | facade.cc/adapter.cc 调用点 |
| `sm->set_scoreboard/pipeline/tensor_core()` 3 个 → `device->attach_timing(sb, pl, tc)` | adapter.cc 3→1 聚合 |
| `new GPUContext/delete gpu_` → `create_device/destroy_device` | 生命周期归 PTX-EMU 工厂 |
| `WarpContext*`/`ThreadContext*` 指针 → `uint32_t warp_id`/`int lane` 句柄 | 句柄替换完整类型 |
| `RegOperand/acquire_register` → `read/write_register_u32/u64(warp, lane, reg)` | 寻址内化 |
| 删除 vendored HSK-4 三接口头 | `scoreboard_interface.h`/`pipeline_interface.h`/`tensor_core_interface.h` 改由 PTX-EMU PUBLIC include 传递 |
| 删除 stub.cc | `src/tlm/gpu/ptx_emu_bridge_stub.cc` 升级后 PTX-EMU 不再消费 `g_cpptlm_bridge` |
| 删除 vendored `cpptlm_bridge.h` | `include/cudart/cpptlm_bridge.h` PTX-EMU 端已删除 (commit `09786635`) |
| 删除 `MemoryBridge`/`PtxEmuDriverShim` | `include/tlm/gpu/memory_bridge.hh` + `src/tlm/gpu/memory_bridge.cc` + `src/tlm/gpu/ptx_emu_driver_shim.cc` (HSK-6 P4 提前) |
| 删除 `test/ptxemu_link_smoke.cc` | 链接 smoke 职责迁至 PTX-EMU `consumer_smoke.cc` |
| `abi_guards.h` 拆分迁移 | 16 条 enum 断言迁移到 `include/cudart/hsk4_abi_guards.h`；`sizeof(PtxEmuDriverApi)==64` 删除（ABI 契约已废止） |
| 新增 `include 防火墙 grep 门禁` | `cmake/PTXEmuCore.cmake` 删除后新建（除 facade/adapter 外任何 .cc include `ptxsim/|ptx_ir/|memory/|register/` 即 configure 失败） |

### 3. 测试改造（CppTLM 端）

| 文件 | 改动类型 | 工作量 |
|------|---------|--------|
| `test/test_ptx_emu_facade_decode.cc` | roundtrip 测试迁至 PTX-EMU 端；CppTLM 保留 golden-bytes decode 测试 | Medium |
| `test/test_ptx_emu_facade_arith.cc` (19 内部引用) | fixture 改用 PTX 源码字符串 + 新增 `load_ptx_source()` API | Medium |
| `test/test_ptx_emu_facade_branch.cc` (10) | fixture 改用 PTX 源码 | Medium |
| `test/test_ptx_emu_facade_barrier.cc` (15) | fixture 改用 PTX 源码 | Medium |
| `test/test_ptx_emu_facade_memory.cc` (8) | PTX `ld/st.global` 源码 + 公共方法断言 | Short |
| `test/test_ptx_emu_facade_state.cc` (6) | `warp_status()` DTO 断言 | Short |
| `test/test_cuda_core_adapter_mvp_*.cc` ×6 | 4 Quick + 2 Short（基本幸存） | Short |
| `test/ptxemu_link_smoke.cc` | 删除（迁至 PTX-EMU consumer_smoke） | Quick |

### 4. 跨仓协调

| 步骤 | 责任方 | 内容 |
|------|--------|------|
| 步骤 1 | CppTLM | 起草 HSK-8 spec（`docs/superpowers/specs/2026-XX-XX-hsk-8-ptxemu-public-api.md`）|
| 步骤 2 | PTX-EMU | 基于 `origin/main` (post-deletion) 新增库目标 + `device_api.h` + impl + 契约测试，PTX-EMU CI 全绿后合入 |
| 步骤 3 | CppTLM | submodule bump 到步骤 2 commit，单 PR 三件事：bump + `add_subdirectory` + 删除桥接残留簇 |
| 步骤 4 | CppTLM | 跑 6 条验证标准（详见 design.md Risks）|

---

## Capabilities

### New Capabilities

- `ptxemu-public-device-api`: PTX-EMU 端维护 `ptxemu/device_api.h` 公共契约，包含 `IPtxEmuDevice` 抽象接口 + `create_device/destroy_device` 工厂 + `PTXEMU_API_VERSION` 版本守卫。锁定 PTX-EMU 内部重构不影响 CppTLM 编译。
- `ptxemu-internal-impl-hiding`: PTX-EMU 内部头（`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`/`cudart/*.h`）通过 CMake `target_include_directories(ptxemu_core PRIVATE ...)` 封装，CppTLM 编译路径中不可见。
- `cpptlm-facade-public-api-only`: CppTLM 端 facade.cc / adapter.cc 仅 include `ptxemu/device_api.h`，**禁止** include 任何 PTX-EMU 内部头（CMake grep 门禁强制）。

### Modified Capabilities

（无。本 change 不修改任何现有 spec；只新增 capabilities + 物理删除 S1 已 deprecated 的 `MemoryBridge`/`PtxEmuDriverShim` 实现。这些实现的删除已在 `include/tlm/gpu/memory_bridge.hh` 类定义上标注 `[[deprecated("MemoryBridge frozen by HSK-6; removed in cpptlm-v3-dgpu-extract P4 after Mode B E2E")]]`，是已规划路径的提前执行。）

---

## Impact

### Affected code

**CppTLM 端**:
- 删除: `cmake/PTXEmuCore.cmake` (246 行)、`src/tlm/gpu/ptx_emu_bridge_stub.cc`、`include/cudart/cpptlm_bridge.h`、`include/tlm/gpu/memory_bridge.hh`、`src/tlm/gpu/memory_bridge.cc`、`src/tlm/gpu/ptx_emu_driver_shim.cc`、`test/ptxemu_link_smoke.cc`、`include/cudart/abi_guards.h` (拆分)
- 改动: `src/tlm/gpu/ptx_emu_submodule_mvp.cc` (12 include → 1)、`src/tlm/gpu/ptx_emu_submodule_mvp.hh`、`src/tlm/gpu/cuda_core_adapter_mvp.cc` (5 include → 1)、`src/tlm/gpu/cuda_core_adapter_mvp.hh`、`CMakeLists.txt`、`src/CMakeLists.txt`、`test/CMakeLists.txt`、`include/cudart/AGENTS.md`、`docs/superpowers/specs/2026-08-18-hsk-6-response.md` (状态段追加)、`AGENTS.md` (HSK 链路状态段追加)
- 12 测试文件改写 (4 Medium + 2 Short + 4 Quick + 2 Short)

**PTX-EMU 端 (独立 PR)**:
- 新增: `include/ptxemu/device_api.h` (~200 行)、`include/ptxemu/ir/statement.h`、`src/ptxemu/device_api_impl.cc` (~400 行)、`tests/build_cpptlm_consume/`、`CMakeLists.txt` 改 (`add_library(ptxemu_core ...)` + PUBLIC/PRIVATE include 拆分 + `PROJECT_IS_TOP_LEVEL` 隔离)
- PTX-EMU 仓库不接受此 PR 内的 cpptlm_bridge.h 回归（HSK-6 已废止）

### Affected APIs

**新增公共 API** (`ptxemu/device_api.h`):
- `ptxemu::IPtxEmuDevice` 抽象接口
- `ptxemu::create_device/destroy_device` 工厂
- `ptxemu::DeviceConfig`/`WarpStatus`/`LaneStatus`/`ThreadState` DTO
- `ptxemu::decode_ptxir` 字节流解码
- `PTXEMU_API_VERSION` 版本守卫宏

**新增公共 API** (`ptxemu/ir/statement.h`):
- `ptxemu::Statement` (晋升自 PTX-EMU 内部 `StatementContext`)

**BREAKING 改动**:
- CppTLM 端 `PtxEmuSubmoduleMVP` facade 的内部实现完全重写（公开 API 不变）
- CppTLM 端 `CudaCoreAdapter` 内部实现完全重写（公开 API 不变）
- S1 stub.cc 提供的 `g_cpptlm_bridge = nullptr` 符号被 PTX-EMU 端 `create_device/destroy_device` 工厂取代
- HSK-6 已 deprecated 的 `MemoryBridge`/`PtxEmuDriverShim` 在本 change 中物理删除（提前于原 v3.0 P4 计划）

### Affected dependencies

- PTX-EMU submodule pin: `c2038a93` → 步骤 2 的 PTX-EMU PR 合入 commit
- HSK 协议: 新增 HSK-8（CppTLM 发起 + PTX-EMU ack，锁定 `device_api.h` 契约）
- HSK-3 (ExternalProject_Add) 已被 HSK-6 废止的方向解除约束；新方向由 HSK-8 锁定

### Affected systems

- CppTLM 仿真器 OFF 路径（不依赖 PTX-EMU）: 不受影响
- CppTLM 仿真器 ON 路径（CPPTLM_WITH_PTX_EMU=ON）: 完全迁移到 `device_api.h`
- PTX-EMU 仓库: 接受步骤 2 PR（新增库目标 + 公共头 + 契约测试，不涉及 cpptlm_bridge.h 回归）
- UsrLinuxEmu (PTX-EMU 下游用户): 不受影响（PTX-EMU 自身的 ABI 不变，新增 `IPtxEmuDevice` 是扩展）

---

## 验证标准 (Bump PR 合入前必跑)

- [ ] OFF 路径 850/850 全 PASS + ON 路径 891/891 全 PASS
- [ ] facade.cc/adapter.cc 对新 PTX-EMU 内部头编译**零告警**（实际已无 PTX-EMU 内部头 include）
- [ ] `grep -r "g_cpptlm_bridge\|cpptlm_set_driver\|cpptlm_bridge.h" src/ include/` **归零**
- [ ] `grep -r "ptxsim/\|ptx_ir/\|memory/simple_memory\|register/register_bank" src/ include/` **归零**（除 `device_api.h` 外零 PTX-EMU 头）
- [ ] `PTXEMU_API_VERSION` 版本守卫断言生效（submodule pin 过低时 FATAL_ERROR）
- [ ] include 防火墙 grep 门禁生效（任何 .cc include `ptxsim/|ptx_ir/|memory/|register/` 即 configure 失败）
- [ ] abi_guards 反向故意失败测试触发编译错误（沿用 `fa2b3ec` 双重验证法）
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS（AGENTS.md / ONBOARDING.md / roadmap.md / scripts/README.md 路径全有效）