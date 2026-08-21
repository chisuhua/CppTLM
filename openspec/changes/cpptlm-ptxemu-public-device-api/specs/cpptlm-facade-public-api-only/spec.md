# Spec: cpptlm-facade-public-api-only

> **Capability**: `cpptlm-facade-public-api-only`
> **Version**: 1
> **Status**: 📋 Proposed — 2026-08-21

## ADDED Requirements

### Requirement: CppTLM facade.cc / adapter.cc 仅 include `ptxemu/device_api.h`

CppTLM 仓库 MUST 在 `src/tlm/gpu/ptx_emu_submodule_mvp.cc` 和 `src/tlm/gpu/cuda_core_adapter_mvp.cc`（以及它们对应的 .hh 头文件）中**仅** include `ptxemu/device_api.h`（+ PTX-EMU 公开的 HSK-4 接口头 + CppTLM 自身头）。MUST NOT include 任何 PTX-EMU 内部头（`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`）。

#### Scenario: facade.cc 仅 include 公共头
- **WHEN** 检查 `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- **THEN** 所有 `#include` 行 MUST 仅引用 `ptxemu/device_api.h` + CppTLM 自身头；任何 `ptxsim/`/`ptx_ir/`/`memory/`/`register/` include MUST 由 CMake 防火墙拦截

#### Scenario: adapter.cc 仅 include 公共头
- **WHEN** 检查 `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- **THEN** 所有 `#include` 行 MUST 仅引用 `ptxemu/device_api.h` + CppTLM 自身头；同上 MUST 由 CMake 防火墙拦截

### Requirement: CMake 防火墙 grep 门禁

CppTLM 仓库 MUST 在根 `CMakeLists.txt`（或独立 cmake include 文件）新增 grep 门禁：当 `CPPTLM_WITH_PTX_EMU=ON` 时，configure 阶段 MUST 扫描 `src/` 与 `test/` 下所有 `.cc/.hh` 文件，若除 `src/tlm/gpu/ptx_emu_submodule_mvp.cc` 和 `src/tlm/gpu/cuda_core_adapter_mvp.cc` 外任何文件 include `ptxsim/`/`ptx_ir/`/`memory/`/`register/`，MUST FATAL_ERROR 并报告违规文件路径。

#### Scenario: 防火墙拦截违规 include
- **WHEN** 有人在 `src/tlm/gpu/some_module.cc` 添加 `#include "ptxsim/sm_context.h"`
- **THEN** cmake configure MUST FATAL_ERROR：`CMake Error: PTX-EMU internal header ptxsim/sm_context.h included in src/tlm/gpu/some_module.cc — must only be included by facade.cc or adapter.cc`

#### Scenario: 白名单文件通过
- **WHEN** `src/tlm/gpu/ptx_emu_submodule_mvp.cc` 或 `src/tlm/gpu/cuda_core_adapter_mvp.cc` include PTX-EMU 头
- **THEN** 防火墙 MUST NOT 拦截（白名单豁免）

### Requirement: 模板方法改为显式重载

CppTLM 仓库的 `PtxEmuSubmoduleMVP` facade MUST NOT 使用模板方法（如 `template<typename T> read_register<T>`），因为跨抽象接口（虚函数）不能传递模板参数。MUST 改为非模板显式重载（在 CppTLM 侧 facade 内做 inline 转发，调用 `IPtxEmuDevice::read_register_u32/u64/f32/f64`）。

#### Scenario: 模板方法移除
- **WHEN** 检查 `include/tlm/gpu/ptx_emu_submodule_mvp.hh`
- **THEN** MUST NOT 出现 `template<...>` 方法签名（除标准库兼容包装如 `std::pair`/`std::vector`）；所有功能方法 MUST 是非模板方法

#### Scenario: 内联转发
- **WHEN** `PtxEmuSubmoduleMVP::read_register<T>` 被调用
- **THEN** MUST inline 转发到 `device_->read_register_u32/read_register_u64`（按 `T` 类型分派）；编译时 `T` 必须为 `uint32_t`/`uint64_t`/`float`/`double` 之一

### Requirement: 内部类型指针 → `uint32_t` 句柄

CppTLM 仓库的 `PtxEmuSubmoduleMVP` 和 `CudaCoreAdapter` MUST NOT 持有 PTX-EMU 内部类型的完整类型指针（如 `GPUContext*`/`SMContext*`/`WarpContext*`/`ThreadContext*`）。MUST 改为不透明 `uint32_t warp_id` + `int lane` 句柄（句柄 ID 由 `IPtxEmuDevice` 内部维护）。

#### Scenario: 无完整类型指针
- **WHEN** 检查 `include/tlm/gpu/ptx_emu_submodule_mvp.hh` 和 `include/tlm/gpu/cuda_core_adapter_mvp.hh`
- **THEN** MUST NOT 出现 `GPUContext*`/`SMContext*`/`WarpContext*`/`ThreadContext*` 等 PTX-EMU 内部类指针；MUST 出现 `uint32_t warp_id_` 或 `uint32_t sm_idx_` 等句柄字段

#### Scenario: 句柄生命周期归 PTX-EMU
- **WHEN** CppTLM 端 `PtxEmuSubmoduleMVP` 析构
- **THEN** MUST 调用 `ptxemu::destroy_device(device_)` 释放 PTX-EMU 端资源；句柄 ID 在 destroy 后失效（任何再调用 MUST 返回错误）

### Requirement: 桥接残留簇物理删除

CppTLM 仓库 MUST 在本 change 内物理删除以下文件（不在 v3.0 P4 阶段才删，本 change 提前）：
- `src/tlm/gpu/ptx_emu_bridge_stub.cc`（PTX-EMU 不再消费 `g_cpptlm_bridge`）
- `include/cudart/cpptlm_bridge.h`（PTX-EMU 端已删除 `include/cudart/cpptlm_bridge.h` commit `09786635`）
- `include/tlm/gpu/memory_bridge.hh` + `src/tlm/gpu/memory_bridge.cc`（HSK-6 已 `[[deprecated]]`，提前于 v3.0 P4）
- `src/tlm/gpu/ptx_emu_driver_shim.cc`（PTX-EMU 不再调用 `cpptlm_set_driver`）
- `test/ptxemu_link_smoke.cc`（链接 smoke 职责迁至 PTX-EMU 端 `consumer_smoke.cc`）
- `include/cudart/abi_guards.h` 中的 `sizeof(PtxEmuDriverApi)==64`（ABI 契约已废止）

#### Scenario: 文件物理删除
- **WHEN** 本 change 应用
- **THEN** `git ls-files | grep -E "(ptx_emu_bridge_stub|cpptlm_bridge\.h|memory_bridge|ptx_emu_driver_shim|ptxemu_link_smoke)"` MUST 返回空

#### Scenario: 引用归零
- **WHEN** 检查 `src/` 和 `include/` 中所有 .cc/.hh 文件
- **THEN** MUST NOT 出现 `g_cpptlm_bridge`/`cpptlm_set_driver`/`cpptlm_attach_bridge`/`cpptlm_detach_bridge`/`PtxEmuDriverApi`/`MemoryBridge`/`PtxEmuDriverShim` 等桥接符号

### Requirement: `abi_guards.h` 拆分迁移

CppTLM 仓库 MUST 把 `include/cudart/abi_guards.h` 中的 17 条 static_assert 拆分：
- 16 条 enum 断言（PipelineId 6 + TcPrecision 6 + is_same_v 4）→ 迁移至 `include/cudart/hsk4_abi_guards.h`（HSK-4 vendored 接口的独立 guards）
- `sizeof(PtxEmuDriverApi)==64` 这条 → 删除（ABI 契约已废止，PTX-EMU 端不再维护 PtxEmuDriverApi）

#### Scenario: 16 条 enum 断言迁移
- **WHEN** 检查 `include/cudart/hsk4_abi_guards.h`
- **THEN** MUST 包含 16 条 static_assert（6 PipelineId 端点 + 6 TcPrecision 端点 + 4 is_same_v 签名级），保留 HSK-6 P0-1 门禁语义

#### Scenario: 反向故意失败验证
- **WHEN** 故意将 `PipelineId::P0_INT_FP32 = 1`（违反 expected 0）
- **THEN** 编译 MUST 失败：`static assertion failed: G-D4 ABI drift: PipelineId::P0_INT_FP32 != 0 (expected 0)`