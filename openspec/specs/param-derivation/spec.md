# param-derivation Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.

## Requirements

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

### Requirement: Dynamic derivation evaluation

参数派生表达式（`derive_expr`）**当前未实现**（`ADR-X.10` 决策 7 Stage 2 未闭合）。本 Requirement 描述的是**目标行为**而非现状；任何闸/测试 SHALL NOT 以其为「已实现」基线。

#### Scenario: Derivation based on mesh size

- **WHEN** config provides mesh_x=8, mesh_y=8 but not data_width, and derive_expr="(mesh_x * mesh_y > 16) ? 128 : 64"
- **THEN** data_width SHALL be derived as 128

#### Scenario: Derivation with small mesh

- **WHEN** config provides mesh_x=2, mesh_y=2 but not data_width, and derive_expr="(mesh_x * mesh_y > 16) ? 128 : 64"
- **THEN** data_width SHALL be derived as 64

#### Scenario: Explicit value overrides derivation

- **WHEN** config provides both mesh_x=8 and data_width=256 explicitly
- **THEN** data_width SHALL use the explicit value 256, not the derived value

### Requirement: Default value assignment

Parameters with no explicit value and no derivation expression SHALL use their declared default value.

默认值 SHALL **只落到模块成员**，**SHALL NOT** 写回配置 JSON。

#### Scenario: Default value applied when not provided

- **WHEN** config does not include `buffer_size` and its declaration has default 8
- **THEN** the module member SHALL be set to 8
- **AND** the config object's `params` SHALL NOT gain a `buffer_size` key
