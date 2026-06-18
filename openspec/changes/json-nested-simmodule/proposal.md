# JSON-Nested SimModule

## Why

CppTLM v2.1 的 `SimModule` 基类已提供 `instantiate(json)` 解析、`outputs/inputs` 暴露端口 API、`getInternalInstance/getInternalOutputPort` 内部端口访问能力，但**当前没有任何 active JSON 配置或 C++ 示例在调用**这套 API（`grep '"outputs"\s*:\s*\[' configs/*.json` = 0 命中）。同时 `CpuCluster` 唯一派生的 `SimModule` 子类是空心类（`tick() {}`，无 override），`cpu_cluster.hh` 仍留在 `modules/legacy/` 下、被 `BUILD_LEGACY_MODULES=OFF`（默认）`no-op` 守卫禁注册。**整套 SimModule 嵌套基础设施处于"设计就绪、零用户"状态。**

本变更同时解决 3 个耦合问题：(1) 把 SimModule 嵌套从"死代码"变成"活的可演示能力"；(2) 通过 JSON 配置驱动的多层嵌套示例展示 `SimModule`/`SimObject` 边界与限深保护；(3) 移除 `BUILD_LEGACY_MODULES` 编译守卫与整个 `modules/legacy/` 目录，让 `CpuCluster` 始终可用、用 CPUTLM 替代 CPUSim、消除"v2.1 已 deprecated 但还在编译"的历史负债。

## What Changes

### 新增能力

- **SimModule JSON 多层嵌套**：顶层 `ModuleFactory::instantiateAll` 构造 `SimModule` 后，由顶层或构造路径**显式调用** `SimModule::simulate_instantiate(cfg)`，后者递归触发内部子 `SimModule` 的 `simulate_instantiate`，支持任意深度的 `CpuCluster→CpuCluster→...` 嵌套结构（运行时展开深度 = 配置深度，限深 8 防御栈溢出）。`ModuleFactory::instantiateAll` 签名**不变**，无新增参数。
- **限深保护**：`SimModule::simulate_instantiate` 加 `static thread_local int depth_` + `MAX_DEPTH=8` + RAII `DepthGuard` 异常安全保证，超出抛 `std::runtime_error`，防止 JSON 错误触发真递归栈溢出；抛错时所有已入栈 guard 析构恢复 `depth_=0`。
- **CpuCluster 增强**：从 `include/modules/legacy/cpu_cluster.hh` 移出至 `include/tlm/cluster/cpu_cluster.hh`，override `tick()`、`set_config(params)`、`get_module_type()`；提供 `set_num_cpus(int)` 等参数透传；与 CPUTLM/CacheTLM/MemoryTLM 完整 E2E 跑通。
- **C++ Catch2 测试** `test/test_simmodule_nested.cc`（标签 `[simmodule]`）：7 个用例覆盖 1 层/2 层/3 层嵌套、限深护栏、outputs/inputs 暴露端到端、CpuCluster E2E 数据流、跨 cluster outputs 暴露端口 E2E 数据流贯通。
- **C++ 示例** `examples/example_simmodule_nested.cc`：main 加载 JSON 后打印拓扑结构、限深验证、E2E req/resp 跑一次。
- **Python 示例** `examples/generate_nested_soc.py`：用 `cpptlm/topo/` + `cpptlm/library/` 生成多层嵌套 JSON，跑一次端到端。
- **JSON 配置** `configs/example_simmodule_nested_2level.json` + `configs/example_simmodule_nested_3level_static.json`：活的配置示例。
- **Python 测试** `test/python/test_simmodule_emitter.py`：6 个用例验证 emitter 生成的 JSON 正确性 + 可被 C++ ModuleFactory 加载。

### 移除能力

- **BREAKING — 删除 `BUILD_LEGACY_MODULES` CMake 选项**：`CMakeLists.txt:44` `option(BUILD_LEGACY_MODULES ...)` 删除；`src/CMakeLists.txt:40-44` `if(BUILD_LEGACY_MODULES) ... target_compile_definitions(...)` 块删除。`include/modules.hh` 的 `#ifdef BUILD_LEGACY_MODULES` 守卫删除，`REGISTER_OBJECT`/`REGISTER_MODULE` 始终展开为有效注册代码。
- **BREAKING — 删除 `include/modules/legacy/` 整个目录**：
  - `cpu_cluster.hh` → 移至 `include/tlm/cluster/cpu_cluster.hh`（**保留并增强**，见上）
  - `cpu_sim.hh` → 删除（CPUSim 已被 `include/tlm/cpu_tlm.hh` 的 `CPUTLM` 替代）
  - `README.md` → 删除
  - 整个 `legacy/` 目录 `rmdir`。
- **BREAKING — `CPUSim` 类型退场**：所有 `CPUSim` 引用点替换为 `CPUTLM`。`REGISTER_OBJECT` 宏从 `ModuleFactory::registerObject<CPUSim>("CPUSim")` 改为 **no-op** 或**直接删除**该宏（v2.1 推荐路径统一走 `REGISTER_CHSTREAM`）。

### 修改能力

- **`include/modules.hh`**：移除 `BUILD_LEGACY_MODULES` 守卫；`REGISTER_MODULE` 简化为 `ModuleFactory::registerModule<CpuCluster>("CpuCluster");`（无 `#ifdef`）。
- **`include/core/sim_module.hh`**：加 `static thread_local int depth_` + `constexpr int MAX_DEPTH = 8` + 限深检查（在 `instantiate()` 入口/出口递增/递减并 `if (depth_ > MAX_DEPTH) throw`）。
- **`include/core/sim_module.hh`**：加 `static thread_local int depth_` + `constexpr int MAX_DEPTH = 8` + `MAX_DEPTH` 编译宏覆盖点；新增公共方法 `void SimModule::simulate_instantiate(const json& cfg)` 实现 depth_ RAII guard 包夹的限深检查 + 递归触发子 SimModule 的 `simulate_instantiate`（含幂等守卫 + dynamic_cast 派生类识别）。
- **`include/tlm/AGENTS.md`**：加 CpuCluster 行（与现有 CacheTLM/CrossbarTLM/MemoryTLM/CPUTLM 并列）。
- **`configs/AGENTS.md`**：移除"已归档"段中关于 `BUILD_LEGACY_MODULES` 的描述。
- **`include/AGENTS.md`**：更新"注册宏体系"段（移除 `BUILD_LEGACY_MODULES` 守卫说明）。
- **`CHANGELOG.md`**：v2.2 条目（SimModule 嵌套示例 + 移除 legacy）。

### 不修改

- `SimModule` 基类的 `outputs/inputs` JSON 字段解析（`parsePortConfigs`，`include/core/sim_module.hh:93-121`）—— 现有逻辑已正确，本 change 仅通过测试**验证**它工作。
- 15 个 active `openspec/specs/` 中的任何一个 —— 本 change 不修改现有 C++ schema 或行为约束。
- 任何 `configs/*.json` 的 active 文件（除新增 2 个 nested 示例）。

## Capabilities

### New Capabilities

- `simmodule-nested`: SimModule 通过 JSON 配置实现多层（CpuCluster→CpuCluster→...）嵌套实例化能力，含 `outputs/inputs` 暴露端口 API 端到端验证。
- `simmodule-depth-guard`: SimModule 嵌套深度限制保护（MAX_DEPTH=8 + thread_local depth_ 计数器 + 越界 throw），防止 JSON 错误触发无限递归。
- `cpu-cluster`: CpuCluster 模块从 `modules/legacy/` 移出、增强并定位于 `include/tlm/cluster/`，override `tick`/`set_config`/`get_module_type`，与 CPUTLM/CacheTLM/MemoryTLM 端到端跑通。
- `legacy-modules-removal`: 移除 `BUILD_LEGACY_MODULES` 编译开关、删除整个 `include/modules/legacy/` 目录、CPUSim 退场（由 CPUTLM 替代）、`REGISTER_OBJECT` 宏不再注册 CPUSim。

### Modified Capabilities

（无 — 现有 15 个 active spec 都不涉及 SimModule 嵌套、legacy 目录或 BUILD_LEGACY_MODULES 守卫的 REQUIREMENT 变更。配置 LINT 规则可能扩展但属后续 change。）

## Impact

### 新增文件

- `include/tlm/cluster/cpu_cluster.hh`（~60 行）
- `include/tlm/cluster/cpu_cluster.cc`（~80 行）
- `test/test_simmodule_nested.cc`（~280 行，7 用例）
- `examples/example_simmodule_nested.cc`（~80 行）
- `examples/generate_nested_soc.py`（~100 行）
- `configs/example_simmodule_nested_2level.json`（~30 行）
- `configs/example_simmodule_nested_3level_static.json`（~50 行）
- `test/python/test_simmodule_emitter.py`（~200 行，6 用例）
- `openspec/changes/json-nested-simmodule/proposal.md`（本文档）
- `openspec/changes/json-nested-simmodule/design.md`
- `openspec/changes/json-nested-simmodule/tasks.md`
- `openspec/changes/json-nested-simmodule/specs/simmodule-nested/spec.md`
- `openspec/changes/json-nested-simmodule/specs/simmodule-depth-guard/spec.md`
- `openspec/changes/json-nested-simmodule/specs/cpu-cluster/spec.md`
- `openspec/changes/json-nested-simmodule/specs/legacy-modules-removal/spec.md`

### 修改文件

- `include/core/sim_module.hh`（+20 行：depth_ + MAX_DEPTH + RAII guard + simulate_instantiate 公共方法 + 幂等守卫）
- `include/modules.hh`（重构：移除 `#ifdef BUILD_LEGACY_MODULES`）
- `CMakeLists.txt`（-2 行：删除 `option(BUILD_LEGACY_MODULES ...)`）
- `src/CMakeLists.txt`（-6 行：删除 `if(BUILD_LEGACY_MODULES) ...` 块）
- `include/tlm/AGENTS.md`（+1 行 CpuCluster 条目）
- `configs/AGENTS.md`（更新"已归档"段）
- `include/AGENTS.md`（更新"注册宏体系"段）
- `CHANGELOG.md`（v2.2 条目）
- 所有 `REGISTER_MODULE` / `REGISTER_OBJECT` 引用点（test/*.cc，~5 处）：移除 no-op 注释，改为直接调用

### 删除文件

- `include/modules/legacy/cpu_cluster.hh`（**内容移至** `include/tlm/cluster/cpu_cluster.hh`）
- `include/modules/legacy/cpu_sim.hh`（CPUSim 退场）
- `include/modules/legacy/README.md`（目录 README）

### 依赖

- 无新 C++ 依赖
- 无新 Python 依赖
- 仅复用现有 nlohmann/json、cpptlm/topo、cpptlm/library

### 风险

- **BREAKING**：v2.1.x 引用 `BUILD_LEGACY_MODULES=ON` 开启 legacy 模块的项目需要迁移到 CPUTLM/CPUTLM，本 change 在 CHANGELOG 明确说明。
- **BREAKING**：`#include "modules/legacy/cpu_sim.hh"` 路径不再存在，所有引用点需改为 `#include "tlm/cpu_tlm.hh"`。
- **性能**：`SimModule::instantiate` 加 `static thread_local` 计数器对性能无影响（O(1) 递增）。
- **测试隔离**：新增 6 个 C++ 用例 + 6 个 Python 用例，需在 `ctest` 与 `pytest` 中分别注册（CMakeLists + pyproject.toml）。
