## ADDED Requirements

### Requirement: SimModule 嵌套深度限制

`SimModule::simulate_instantiate(cfg)` MUST 包含深度计数器，**禁止**嵌套深度超过 `MAX_DEPTH=8`（默认值，可通过编译期宏覆盖）。计数器 MUST 使用 `static thread_local int` 避免多线程仿真间相互干扰。

#### Scenario: 单层嵌套 depth=1 不抛错

- **WHEN** 顶层 `CpuCluster` 调用 `simulate_instantiate(cfg)`，其内部包含 1 层 `SimObject` 子模块
- **THEN** `depth_` 计数：进入 +1（=1），递归激活子 `SimModule` 再 +1（=2），子 `simulate_instantiate` 完成 -1（=1），顶层完成 -1（=0）；不抛 `std::runtime_error`

#### Scenario: 8 层嵌套 depth=8 不抛错（边界）

- **WHEN** JSON 配置包含 8 层 `CpuCluster` 嵌套，最内层持有 1 个 `CPUTLM`
- **THEN** `depth_` 计数依次为 1, 2, 3, 4, 5, 6, 7, 8；每一层 `simulate_instantiate` 不抛错；最内层 `CPUTLM` 被成功构造

#### Scenario: 9 层嵌套 depth=9 抛异常

- **WHEN** JSON 配置包含 9 层 `CpuCluster` 嵌套
- **THEN** 第 9 层进入 `simulate_instantiate` 时 `++depth_ = 9 > MAX_DEPTH=8`，**抛 `std::runtime_error`**，错误信息**包含**子串 `"depth"`、`"9"`、`"8"`，以及 JSON 节点路径（如 `modules[0].modules[0]...`）。抛错后**所有 9 层的 RAII `DepthGuard` 析构**（guard 在 `throw` 之前入栈），`depth_` **恢复为 0**

#### Scenario: 计数器在抛错后通过 RAII 自动恢复

- **WHEN** 第 9 层 `simulate_instantiate` 抛 `std::runtime_error`（无外层 try/catch）
- **THEN** 9 个 `DepthGuard` 实例在栈展开时按构造逆序析构，每个 guard 触发 `--depth_`，**最终 `depth_ == 0`**（不是 8 或 9）；后续 `simulate_instantiate` 调用应能正常进入新顶层（depth=1）而不被旧计数污染

#### Scenario: thread_local 在多线程隔离

- **WHEN** 两个线程同时调用不同 SimModule 实例的 `simulate_instantiate`
- **THEN** 两个线程的 `depth_` 计数器**相互独立**；A 线程 depth=3 不影响 B 线程 depth=1

### Requirement: MAX_DEPTH 可配置

`MAX_DEPTH` MUST 定义为 `SimModule` 类的 `static constexpr int` 成员，**允许**通过编译期宏 `CPPTLM_SIMMODULE_MAX_DEPTH` 覆盖默认值。

#### Scenario: 默认 MAX_DEPTH = 8

- **WHEN** 编译时未定义 `CPPTLM_SIMMODULE_MAX_DEPTH` 宏
- **THEN** `SimModule::MAX_DEPTH == 8`

#### Scenario: 编译宏覆盖 MAX_DEPTH

- **WHEN** 编译时定义 `CPPTLM_SIMMODULE_MAX_DEPTH=16`（如 `target_compile_definitions(cpptlm_core PUBLIC CPPTLM_SIMMODULE_MAX_DEPTH=16)`）
- **THEN** `SimModule::MAX_DEPTH == 16`；16 层嵌套不抛错，17 层抛错

### Requirement: 限深错误信息可观测

深度超限抛出的 `std::runtime_error` 错误信息 MUST **包含**当前 depth 值、MAX_DEPTH 值、以及触发的 JSON 节点路径（`modules[0].modules[1].modules[2]...`），便于调试。

#### Scenario: 错误信息包含 depth 与 path

- **WHEN** 第 9 层 `CpuCluster`（路径 `modules[0].modules[0].modules[0].modules[0].modules[0].modules[0].modules[0].modules[0]`）触发深度超限
- **THEN** 抛出的 `std::runtime_error::what()` 返回字符串**包含**子串 `"depth"`、`"9"`、`"8"`、`"modules[0].modules[0]..."`
