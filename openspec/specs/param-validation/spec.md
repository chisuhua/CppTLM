# param-validation Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.

## Requirements

### Requirement: Parameter range validation

参数取值校验 SHALL 在**模块内**（`load_param` 的声明点）于装配期完成 —— 对**全部已准入类型**生效。

校验失败 SHALL 抛 `cpptlm::ParamValidationError`（携带模块名 + 参数名 + 期望范围），由装配调用点捕获后使 `instantiateAll` 返回 false。

⚠️ 工厂侧的范围校验器（`validate_module_params`）随旧声明面退役而删除 ⇒ **范围校验的执行者唯一**（模块内）。

#### Scenario: Valid range passes validation

- **WHEN** 某参数的声明范围为 `[1,64]`，配置给出 `8`
- **THEN** 装配 SHALL 成功，且成员取值 == 8

#### Scenario: Below range value fails validation

- **WHEN** 某参数的声明范围为 `[1,64]`，配置给出 `0`
- **THEN** 装配 SHALL **失败**（`instantiateAll` 返回 false）
- **AND** 错误信息 SHALL 含该参数名与期望范围 `[1,64]`
- **AND** SHALL NOT 静默截断到边界或回退默认值

#### Scenario: Above range value produces warning

- **WHEN** 某参数的声明范围为 `[1,64]`，配置给出 `65`
- **THEN** 装配 SHALL **失败**（静默越界会让配置写了不生效 ⇒ 收紧为失败）
- **AND** 错误信息 SHALL 含该参数名与期望范围 `[1,64]`

#### Scenario: vc_count range enforcement

- **WHEN** 配置为 `RouterTLM` 提供 `vc_count`
- **THEN** 该键 SHALL 被判为**未知键**并使装配失败
- **AND** 原因：`vc_count` **不在** `RouterTLM` 的参数声明内 —— 其 `NUM_VCS` 仍是编译期常量，声明它只会退化成「声明了却无效」（`ADR-X.17` D-6）
- **AND** ⚠️ 本 Scenario 原断言「`vc_count` 越界 ⇒ `[PARAM ERROR] vc_count must be in [1, 8]`」在实现下从未成立（范围校验器是零调用方死代码，且该参数无效果）⇒ 该结论保持反转

### Requirement: Parameter constraint declarations

Modules SHALL be able to declare inter-parameter constraints that MUST be satisfied.

#### Scenario: Topology size constraint
- **WHEN** RouterTLM declares constraint "node_x < mesh_x && node_y < mesh_y" with message "node coordinates must be within mesh bounds"
- **THEN** config with node_x=5, mesh_x=4 SHALL fail validation with the constraint message

#### Scenario: Constraint satisfied passes
- **WHEN** RouterTLM constraint is "node_x < mesh_x && node_y < mesh_y"
- **THEN** config with node_x=3, mesh_x=4 SHALL pass validation

### Requirement: Required parameter enforcement

参数「必填」由**声明中缺省默认值**表达（`load_param(key, target)` 形态 / `def = nullopt`）⇒ **必填真值唯一**。

缺失必填参数 SHALL 使装配失败;嵌套实例的装配失败 SHALL **向上传播**（外层 `instantiateAll` 返回 false）⇒ 必填保护不得因「嵌套装配失败被吞」而失效。

#### Scenario: Missing required parameter fails

- **WHEN** 已准入类型的某参数无默认值（必填），配置未提供该键
- **THEN** 装配 SHALL **失败**
- **AND** 错误信息 SHALL 含该参数名

#### Scenario: Derivable required parameter passes

- **WHEN** 某必填参数带 `derive_expr` 派生路径
- **THEN** 缺失但可派生的参数 SHALL NOT 导致装配失败（⚠️ 派生机制当前**未实现**，见 `param-derivation` 的同名 Requirement）

#### Scenario: 未准入类型的必填保护零变化

- **WHEN** 某未准入类型缺其必填参数
- **THEN** 其失败行为与错误面 SHALL 与改动前逐项一致
- **AND** 本 change 之后该集合 SHALL 仅含测试夹具

### Requirement: Param/config field separation is a validation precondition

The ModuleFactory param-validation pipeline SHALL treat the `params` / `config` field separation as a precondition: parameter validation logic operates on the `params` field, and the `config` field is reserved exclusively for external configuration file references.

#### Scenario: Validation reads params field only

- **WHEN** ModuleFactory::validateConfig() processes a module with both `params` and `config` fields
- **THEN** param-validation logic SHALL only read the `params` field for type-specific rules (range, constraints, required)
- **AND** SHALL NOT consult the `config` field for parameter values (it is a file path, not a parameter dict)

#### Scenario: SimObject with config dict produces no params

- **WHEN** a SimObject (e.g. `TrafficGenTLM`) module has `"config": { "pattern": "SEQUENTIAL" }` instead of `"params"`
- **THEN** ModuleFactory SHALL inject empty/default parameters into the module instance
- **AND** the user's intended configuration SHALL be silently ignored (CURRENT BUG)
- **AND** the `config-lint` LINT005 rule SHALL flag this configuration as an error

#### Scenario: SimModule with config as file path loads correctly

- **WHEN** a SimModule (e.g. `CpuCluster`) module has `"config": "/path/to/cfg.json"` and the file exists
- **THEN** ModuleFactory SHALL load the file and call `sim_mod->instantiate(internal_cfg)`
- **AND** the file's contents SHALL be merged into the module's configuration

#### Scenario: Param validation does not read config as fallback

- **WHEN** param-validation needs a value that is not in `params`
- **THEN** validation SHALL NOT fall back to reading the `config` field as a dict
- **AND** SHALL fail validation with the standard "missing required parameter" error
