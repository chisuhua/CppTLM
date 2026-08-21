# Spec: ptxemu-internal-impl-hiding

> **Capability**: `ptxemu-internal-impl-hiding`
> **Version**: 1
> **Status**: 📋 Proposed — 2026-08-21

## ADDED Requirements

### Requirement: PTX-EMU 内部头通过 CMake PRIVATE include 封装

PTX-EMU 仓库 MUST 在 `ptxemu_core` 静态库 target 上设置 `target_include_directories(ptxemu_core PRIVATE ${PTXEMU_SRC})`（PRIVATE 而非 PUBLIC）。该设置确保 PTX-EMU 内部头（`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`/`cudart/*.h`/`utils/*.h`）不会传递到 CppTLM 的编译路径。

#### Scenario: CppTLM 编译路径不包含 PTX-EMU 内部头
- **WHEN** CppTLM 根 `CMakeLists.txt` 执行 `add_subdirectory(external/PTX-EMU)` 并链接 `ptxemu_core`
- **THEN** CppTLM 编译单元的 include 搜索路径 MUST 仅含 `include/ptxemu/`（PUBLIC 传递路径）+ CppTLM 自己的 include 路径；MUST NOT 含 `${PTXEMU_SRC}`（PRIVATE 内部路径）

#### Scenario: CppTLM include PTX-EMU 内部头失败
- **WHEN** CppTLM 端某 .cc 文件 `#include "ptxsim/sm_context.h"` 或 `#include "ptx_ir/statement_context.h"` 或 `#include "memory/simple_memory.h"` 或 `#include "register/register_bank_manager.h"`
- **THEN** 编译 MUST 失败（头文件 not found）；CMake include 防火墙 grep 门禁 MUST 在 configure 阶段 FATAL_ERROR 拦截该 include

### Requirement: PTX-EMU 内部 `EXE_STATE`/`WarpState` 等不可暴露

PTX-EMU 内部 `ptxsim::EXE_STATE`/`ptxsim::WarpState`/`ptxsim::WarpContext`/`ptxsim::SMContext`/`ptxsim::ThreadContext`/`ptxsim::GPUContext`/`memory::SimpleMemory`/`register::RegisterBankManager` 等类型 MUST NOT 出现在 `ptxemu/device_api.h` 公共头中。公共头 MUST 仅暴露 `ptxemu::` 命名空间下的 DTO 与抽象接口。

#### Scenario: 公共头不暴露内部类型
- **WHEN** 检查 `ptxemu/device_api.h`
- **THEN** MUST NOT 出现 `EXE_STATE`/`WarpState`/`WarpContext`/`SMContext`/`ThreadContext`/`GPUContext`/`SimpleMemory`/`RegisterBankManager` 等内部类型名；DTO 字段类型 MUST 是 `uint32_t`/`uint64_t`/`size_t`/`bool`/`std::string`/`std::vector<Statement>` 等 C++17 标准类型或 `ptxemu::` 自有类型

#### Scenario: 跨仓编译失败兜底
- **WHEN** PTX-EMU 内部重构意外修改内部类签名
- **THEN** `device_api_impl.cc` MUST NOT 编译失败（impl 持有完整内部类型，可正常修改）；但 CppTLM 端 MUST NOT 编译失败（公共头签名不变）

### Requirement: PTX-EMU 内部头修改不触发 CppTLM 重编译

PTX-EMU 内部头（`ptxsim/*.h`/`ptx_ir/*.h`/...）的 API 变更 MUST NOT 影响 CppTLM 端任何 .o 文件的重编译（除 `device_api_impl.cc` 内部 .o）。这通过 CMake PRIVATE include 实现。

#### Scenario: PTX-EMU 内部头修改
- **WHEN** PTX-EMU 开发者修改 `src/ptxsim/instructions/memory.cpp` 的实现
- **THEN** CppTLM 端所有 .o MUST NOT 重编译；只有 PTX-EMU 内部 `memory.cpp.o` 和 `device_api_impl.cc.o` 重编译