# config-type-registration-align Specification (delta)

## ADDED Requirements

### Requirement: config-type-registration-align

配置**顶层 `modules[]`** 引用的每一个模块 `type` SHALL 在**运行时注册表**（`ModuleFactory::getRegisteredTypes()`）中存在;若不存在，装配 SHALL **失败**并报出**类型名 + 实例名 + 来源文件**。


#### Scenario: 未注册类型 SHALL 使装配失败且报出类型名

> **为何必须是失败而不是告警**：当前实现（`src/core/module_factory.cc` 约 385 行）对未注册类型只打一行 `DPRINTF` 后**继续** ⇒ **装配成功但模块缺失** ⇒ 拓扑不完整而仿真照跑 ⇒ **结果静默失真**。且 `DPRINTF` 仅在 Debug 构建生效（`include/core/sim_core.hh:8-16`）⇒ Release / CI 默认构建下**连日志都没有**。
>
> ⛔ **不得**用「严格模式开关」弱化：开关救不了嵌套路径（`SimModule::simulate_instantiate` 丢弃返回值），且会给后人留下「非严格模式即可」的叙事出口 —— 与 `ADR-X.17`「⛔ 禁止用静默豁免长期掩盖」冲突。
>
> ⛔ **不得**引入第二份类型白名单：判据集合 SHALL 从**运行时注册表**派生（零源码解析、零硬编码类型名）。
>
> **范围限定（⚠️ 与实现对齐，2026-11-18 增补）**：本条**只覆盖顶层 `modules[]`**。
> **为何必须限定**：当前实现（`src/core/module_factory.cc` 的 Step 0.3 预扫描）**只扫顶层** —— 内联嵌套 `modules`（SimModule 内的子模块）的失败传播由 `cpptlm-module-param-migrate-batch2` 的 **T0.2** 承接（`SimModule::simulate_instantiate` 改造）。⚠️ 若本条写成「**每一个**」，则默认 `openspec archive`（⛔ 不用 `--skip-specs`）会把**未实现的嵌套能力**合进 main spec ⇒ 与 `AGENTS.md`「✅ 已完成归档 = 能力已在契约内」的语义**直接冲突**（归档动作本身在撒谎）。
> **后续**：C2 落地时应对本条出 **MODIFIED** 升级以摘除「顶层」限定。

- **WHEN** 拓扑配置中某个模块的 `type` 不在运行时注册表中
- **THEN** 装配 SHALL **失败**（返回 false）
- **AND** 错误信息 SHALL 同时包含该**类型名**、该**实例名**与**来源配置文件路径**
- **AND** 错误信息 SHALL 输出到 **stderr**（⛔ **NOT** 仅 `DPRINTF` —— Release 构建下后者无输出）
- **AND** 失败时 SHALL **不残留任何已创建实例**（零半构造状态）

#### Scenario: 已注册类型 SHALL NOT 被误伤

- **WHEN** 拓扑配置中每个模块的 `type` 均在运行时注册表中
- **THEN** 装配 SHALL **成功**（返回 true）
- **AND** 每个模块实例 SHALL **存在**且可被取回
- **AND** SHALL **NOT** 产生任何错误输出

#### Scenario: 装配失败后 SHALL 仍可重入

- **WHEN** 同一进程内先经历一次因未注册类型导致的装配失败，随后用合法配置再次装配
- **THEN** 第二次装配 SHALL **成功**
- **AND** 第二次装配的实例集合 SHALL **完整**（⛔ 失败路径不得留下悬挂的静态注册项）

#### Scenario: 配置类型集合 SHALL 与运行时注册表一致

- **WHEN** 对 `configs/**`（含 `configs/templates/**`，排除 `configs/param_rules/**`）在 `extends` / `include` / `${var}` **展开之后**求其模块 `type` 集合
- **THEN** 该集合与运行时注册表的**差集 SHALL 为空**
- **AND** 该检查 SHALL **可重复执行**（同一输入 ⇒ 同一结论），且 SHALL **NOT** 依赖任何硬编码的类型名清单
