# legacy-modules-removal Specification

## Purpose

记录 CppTLM v2.2 中 `include/modules/legacy/` 目录、`BUILD_LEGACY_MODULES` CMake 开关、`CPUSim` 类型的完全移除。`CpuCluster` 移至 `include/tlm/cluster/`，`CPUSim` 由 `CPUTLM` 替代，CMake 选项删除，所有相关 `AGENTS.md` / `CHANGELOG.md` / `docs/migration-v2.2.md` 同步更新。

## Requirements

### Requirement: BUILD_LEGACY_MODULES 编译开关完全删除

`BUILD_LEGACY_MODULES` CMake 选项 MUST 从 `CMakeLists.txt`、`src/CMakeLists.txt`、`include/modules.hh` 三处完全删除。删除后，`CpuCluster` MUST **无条件**注册到 `ModuleFactory`（不再受 ON/OFF 守卫）。

#### Scenario: CMakeLists.txt 不含 BUILD_LEGACY_MODULES

- **WHEN** 执行 `grep -rn "BUILD_LEGACY_MODULES" CMakeLists.txt src/CMakeLists.txt`
- **THEN** **0 命中**

#### Scenario: include/modules.hh 无 #ifdef BUILD_LEGACY_MODULES

- **WHEN** 读 `include/modules.hh` 全部内容
- **THEN** **不包含** `#ifdef BUILD_LEGACY_MODULES`、`#ifndef BUILD_LEGACY_MODULES`、`#if defined(BUILD_LEGACY_MODULES)` 中任一字符串

#### Scenario: 默认构建含 CpuCluster 注册

- **WHEN** 默认配置 `cmake -S . -B build`（`BUILD_LEGACY_MODULES=OFF` 历史默认）
- **THEN** 编译产物 `build/lib/cpptlm_core.a` 中 `nm` 输出**包含**符号 `ModuleFactory::registerModule<CpuCluster>`（或等价 RTTI/typeinfo 符号）；`./build/bin/cpptlm_tests` 启动后 `ModuleFactory::getRegisteredModuleTypes()` 包含 `"CpuCluster"`

### Requirement: include/modules/legacy/ 目录完全删除

`include/modules/legacy/` 整个目录（包括 `cpu_cluster.hh` 已移走、`cpu_sim.hh` 删除、`README.md` 删除）MUST 从 git 树中 `rm` + `rmdir` 清空。删除后，`#include "modules/legacy/..."` 的任何代码 MUST **不**能编译。

#### Scenario: 目录物理删除

- **WHEN** 执行 `ls include/modules/legacy/`
- **THEN** 输出 `No such file or directory`

#### Scenario: include 路径不可达

- **WHEN** 任意 `.cc` 文件包含 `#include "modules/legacy/cpu_sim.hh"` 或 `#include "modules/legacy/cpu_cluster.hh"`
- **THEN** CMake configure 阶段或 `cmake --build` 阶段**失败**，错误信息包含 `"modules/legacy/cpu_sim.hh: No such file or directory"`

#### Scenario: include/AGENTS.md 移除 legacy 引用

- **WHEN** 读 `include/AGENTS.md`（目录级 AGENTS）
- **THEN** **不包含** "modules/legacy"、"legacy" 路径引用（除归档历史说明外）

### Requirement: CPUSim 退场并由 CPUTLM 替代

`CPUSim` 类型 MUST 从 `include/modules/legacy/cpu_sim.hh` 删除，**所有引用点**替换为 `CPUTLM`（`include/tlm/cpu_tlm.hh`）。`REGISTER_OBJECT` 宏不再注册 `CPUSim`。

#### Scenario: CPUSim 类型符号消失

- **WHEN** 编译默认配置
- **THEN** `nm build/lib/cpptlm_core.a | grep CPUSim` **0 命中**（或仅剩 `typeinfo for CPUSim` 但无 `CPUSim::tick` 等方法符号）

#### Scenario: REGISTER_OBJECT 不注册 CPUSim

- **WHEN** 读 `include/modules.hh` 中的 `REGISTER_OBJECT` 宏定义
- **THEN** 宏体**不包含** `registerObject<CPUSim>` 或 `CPUSim` 字符串

#### Scenario: CPUSim 引用点全部迁移

- **WHEN** 执行 `grep -rn "CPUSim" --include="*.cc" --include="*.hh" --include="*.json" --include="*.py" --include="*.md" -- src/ test/ examples/ configs/ cpptlm/ include/ docs/`
- **THEN** 命中**仅限** `CHANGELOG.md`（历史变更说明）与 `docs/migration-v2.2.md`（迁移指南）；其他目录**0 命中**

#### Scenario: CPUTLM 等价功能

- **WHEN** 旧 JSON 配置 `"type": "CPUSim"` 被替换为 `"type": "CPUTLM"`
- **THEN** C++ ModuleFactory 加载通过；`CPUTLM` 实例被构造；与 CacheTLM/MemoryTLM 端到端跑通（参考 v2.1 已有的 `configs/cpu_tlm_test.json`）

### Requirement: AGENTS.md 与 CHANGELOG 同步

`include/AGENTS.md`、`include/tlm/AGENTS.md`、`configs/AGENTS.md`、`CHANGELOG.md` MUST **同步**更新，反映 `BUILD_LEGACY_MODULES` 移除与 `CpuCluster` 新位置。

#### Scenario: include/AGENTS.md 注册宏段更新

- **WHEN** 读 `include/AGENTS.md` "注册宏体系" 段
- **THEN** `REGISTER_OBJECT` 描述标注 `/* no-op: CPUSim removed in v2.2; use REGISTER_CHSTREAM for CPUTLM */`；`REGISTER_MODULE` 描述标注"无条件注册 CpuCluster (位于 include/tlm/cluster/)"；**不包含** `BUILD_LEGACY_MODULES` 字符串

#### Scenario: include/tlm/AGENTS.md 加 CpuCluster 行

- **WHEN** 读 `include/tlm/AGENTS.md` 模块列表
- **THEN** 表格中**包含**一行 `| `cluster/cpu_cluster.hh` | `CpuCluster` | N (按 `modules` 字段) | 集群容器，持有 N 个 CPUTLM + CacheTLM + MemoryTLM |`

#### Scenario: CHANGELOG.md v2.2 条目

- **WHEN** 读 `CHANGELOG.md` 顶部
- **THEN** v2.2 条目**包含**以下子项：
  - "**Removed** `BUILD_LEGACY_MODULES` CMake option (`AGENTS.md` migration path updated)"
  - "**Removed** `include/modules/legacy/` 目录 (`cpu_sim.hh` removed; `cpu_cluster.hh` moved to `include/tlm/cluster/`)"
  - "**Added** `CpuCluster` 增强：override `tick`/`set_config`/`get_module_type`，支持 N 层 JSON 嵌套 + `MAX_DEPTH=8` 限深"
  - "**Added** `configs/example_simmodule_nested_{2level,3level_static}.json` 示例配置"
  - "**Added** `test/test_simmodule_nested.cc` 7 用例（含跨 cluster outputs 暴露端口 E2E）+ `test/python/test_simmodule_emitter.py` 6 用例"
  - "**BREAKING** v2.1.x 引用 `BUILD_LEGACY_MODULES=ON` 项目需迁移到 CPUTLM（见 `docs/migration-v2.2.md`）"

### Requirement: 文档同步路径校验通过

`scripts/test/docs_sync_check.sh --strict` MUST 通过（所有反引号路径在仓库内真实存在）。

#### Scenario: docs_sync_check.sh 0 error

- **WHEN** 执行 `./scripts/test/docs_sync_check.sh --strict`
- **THEN** 退出码 0；输出 `365/365 paths valid`（或更新后的有效路径数）
