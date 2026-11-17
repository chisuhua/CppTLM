# module-param-load-declares Specification (delta)

## MODIFIED Requirements

### Requirement: module-param-unknown-key-detection

已**准入**的模块类型，其 `params` 中的**未知键** SHALL 在装配期被检出并使装配失败;检出由基类统一执行（不依赖各模块自行调用）⇒ 模块少调一次也绕不过。

「准入」的含义 = 该类型的全部参数均由 `load_param` 声明（或已按 D-2 登记为「已消费但不经 `load_param`」），且已从 CFG-08 的 per-type 硬编码键表退役。

⚠️ **未准入类型维持宽松**（零回归）—— 但本 change 之后，**除测试夹具外不再有未准入类型**（全部参数读取面类型均已迁入）⇒ 未知键检查的覆盖面 = 全部有配置读取面的类型。

#### Scenario: 已准入类型上的拼错参数名 SHALL 装配失败

- **WHEN** 某已准入类型的实例配置含一个未被声明、也未被登记为「已消费」的键
- **THEN** 装配 SHALL **失败**（`instantiateAll` 返回 false）
- **AND** 错误信息 SHALL 含该键名
- **AND** 失败源 SHALL 是基类的未知键检出（⛔ 不得依赖模块内手写 throw）

#### Scenario: 未准入类型 SHALL 维持宽松（零回归）

- **WHEN** 某类型尚未准入（仍有旧声明面，或其 CFG-08 分支仍在）
- **THEN** 其未知键 SHALL **不被**检出，装配行为 SHALL 与改动前逐项一致
- **AND** 本 change 之后该集合 SHALL 仅含测试夹具

#### Scenario: legacy 扁平路径 SHALL 豁免未知键检查

- **WHEN** 某实例的参数写在 module entry 顶层（无 `params` 子对象）
- **THEN** 未知键检查 SHALL 被豁免（条目字段如 `name` / `type` / `layout` 不是参数）
- **AND** 必填检查 SHALL 照常生效（豁免不得吞必填）

#### Scenario: 已消费但不经 load_param 的键 SHALL 显式登记

- **WHEN** 某键由板级直读路径消费、由调用方生成但本模块尚未实现其语义，或为非标量键（对象数组 / 嵌套块）
- **THEN** 该键 SHALL 以 `ignore_param(key)` **显式**登记
- **AND** 该登记 SHALL 在参数基线台账的对应表中有条目
- **AND** ⛔ 不得以放宽未知键检查（整表跳过）的方式消化此类键

#### Scenario: 未知键检查 SHALL 覆盖全部参数读取面类型

- **WHEN** 全仓所有**有配置读取面**的类型均已迁入准入表
- **THEN** 准入类型清单 SHALL 覆盖这些类型（判据 = 从 `cpptlm::admitted_param_types()` 派生的断言）
- **AND** ⛔ 测试内 SHALL NOT 硬编码第二份类型清单
