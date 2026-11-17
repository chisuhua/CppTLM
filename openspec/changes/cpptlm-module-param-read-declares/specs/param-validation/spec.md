# param-validation Specification (delta)

## MODIFIED Requirements

### Requirement: Parameter range validation

对**已准入**类型，数值参数的取值校验 SHALL 在**模块内**（`load_param` 的声明点）于装配期完成；未准入类型维持既有路径。

校验失败 SHALL 抛 `cpptlm::ParamValidationError`（携带模块名 + 参数名 + 期望范围），由装配调用点捕获后使 `instantiateAll` 返回 false。

#### Scenario: Valid range passes validation

- **WHEN** 某参数的声明范围为 `[1,64]`，配置给出 `8`
- **THEN** 装配 SHALL 成功，且成员取值 == 8

#### Scenario: Below range value fails validation

- **WHEN** 某参数的声明范围为 `[1,64]`，配置给出 `0`
- **THEN** 装配 SHALL 失败（`instantiateAll` 返回 false）
- **AND** 错误信息 SHALL 含该参数名与期望范围 `[1,64]`
- **AND** SHALL NOT 静默截断到边界或回退默认值

#### Scenario: Above range value produces warning

- **WHEN** 某参数的声明范围为 `[1,64]`，配置给出 `65`
- **THEN** 装配 SHALL **失败**（本 change 把「仅告警」收紧为失败 —— 静默越界会让配置写了不生效）
- **AND** 错误信息 SHALL 含该参数名与期望范围 `[1,64]`

#### Scenario: vc_count range enforcement

- **WHEN** 配置为 `RouterTLM` 提供 `vc_count`
- **THEN** 该键 SHALL 被判为**未知键**并使装配失败
- **AND** 原因：`vc_count` **不在** `RouterTLM` 的参数声明内 —— 其 `NUM_VCS` 仍是编译期常量，声明它只会退化成「声明了却无效」（`ADR-X.17` D-6）
- **AND** ⚠️ 本 Scenario 原断言「`vc_count` 越界 ⇒ `[PARAM ERROR] vc_count must be in [1, 8]`」在当前实现下从未成立（范围校验器是零调用方死代码，且该参数无效果）⇒ 本 change 反转其结论

### Requirement: Required parameter enforcement

参数「必填」由**声明中缺省默认值**表达（`load_param(key, target)` 三参数形态 / `def = nullopt`）。缺失必填参数 SHALL 使装配失败。

未准入类型继续沿用其既有强制面（如 `get_param_rules()` 的 `required` 标志、CFG-08 的 per-type 硬编码分支）。

#### Scenario: Missing required parameter fails

- **WHEN** 已准入类型的某参数无默认值（必填），配置未提供该键
- **THEN** 装配 SHALL 失败
- **AND** 错误信息 SHALL 含该参数名

#### Scenario: Derivable required parameter passes

- **WHEN** 某必填参数带 `derive_expr` 派生路径
- **THEN** 缺失但可派生的参数 SHALL NOT 导致装配失败（⚠️ 派生机制当前**未实现**，见 `param-derivation` 的同名 Requirement）

#### Scenario: 未准入类型的必填保护零变化

- **WHEN** 某未准入类型（如 `NICTLM`）缺其必填参数
- **THEN** 其失败行为与错误面 SHALL 与改动前逐项一致
- **AND** 该类型的 CFG-08 分支 SHALL 保持存在（`NICTLM` 的 `node_id` 必填唯一强制点）
