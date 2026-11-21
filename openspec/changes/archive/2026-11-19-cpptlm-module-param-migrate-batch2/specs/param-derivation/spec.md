# param-derivation Specification (delta)

## REMOVED Requirements

### Requirement: ParamRule structure

> **REMOVED reason**: C++ 侧的 `ParamRules` 声明机制随「读取即声明」迁移完成而**整体退役** ——
> `RouterTLM` 迁移后，`registerParamRules` **零生产调用方**、`getParamRulesRegistry()` 只剩一张空表、
> 其校验器 `validate_module_params` 是 `static` **零调用方死代码**（`ADR-X.10` A 表：决策 4.5 / 5 / 6 / 7 Stage 2 **全部未实施**）⇒
> 留着会让后续读者误以为「规则表仍是活的声明面」。
> ⇒ `include/core/param_rules.hh`（`ParamRule` / `ParamRules` / `ParamType`）与工厂侧注册表一并删除。
>
> ⚠️ **本 Requirement 描述的「结构体字段」本就与实现不符**（`ADR-X.10` A 表决策 1 行：正文的
> `default_val` / `min_val` / `max_val` / `derive_expr` / `description` 与实现的 typed optional 字段不一致，
> 且实现**无 `derive_expr` 字段**）⇒ 删除它同时消除一处**已登记的字段级漂移**。

## MODIFIED Requirements

### Requirement: Module param rules declaration

模块参数 SHALL 以模块内的**一次读取调用即一次声明**的方式加载（`load_param(key, target[, lo, hi][, def])`）—— 该调用同时完成类型校验、范围校验、默认值填充与赋值，并登记该键为「已消费」。

⚠️ C++ 侧**不再存在**「与读取分离的独立声明面」（`get_param_rules()` / `registerParamRules` / 注册表 / `configs/param_rules/*.json` 的 C++ 侧消费均已退役）⇒ 同一参数 SHALL NOT 在两处各写一遍。

#### Scenario: RouterTLM declares coordinate params as required

- **WHEN** 某已准入类型（如 `RouterTLM`）的实例被加载
- **THEN** 其 `node_x` / `node_y` / `mesh_x` / `mesh_y` SHALL 由 `load_param` 以「无默认值 = 必填」的形式声明
- **AND** 该类型 SHALL **NOT** 出现在任何规则注册表中（注册表机制已退役）

#### Scenario: RouterTLM declares flit_width with derivation

- **WHEN** 配置为 `RouterTLM` 提供 `flit_width`
- **THEN** 该键 SHALL 被判为**未知键**并使装配失败
- **AND** 原因：`RouterTLM` 的 `flit_width` 声明已随旧声明面退役删除，且实现**从未消费**它 ⇒ 它既无配置命中、也无实现效果
- **AND** ⚠️ 本 Scenario 原断言「未显式提供时按 `derive_expr` 派生」在实现下从未成立（派生求值未实现）⇒ 该结论保持反转

#### Scenario: 未准入类型维持既有声明面

- **WHEN** 某类型尚未迁入准入表
- **THEN** 其参数声明与行为 SHALL 与改动前逐项一致
- **AND** 未知键检查 SHALL NOT 作用于该类型
- **AND** 本 change 之后该集合 SHALL 仅含测试夹具
