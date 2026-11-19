# module-param-load-declares Specification

## Purpose
模块内可配置参数（如 `RouterTLM` 的 `buffer_size`）的**声明与读取同源**：一次 `load_param(key, target[, lo, hi][, def])` 调用同时完成类型校验、范围校验、默认值填充、赋值与「已消费键」登记；加载结束后由 `SimObject::set_config` 统一检出**未知键**并使装配失败。

本规范约束「已准入类型」（`cpptlm::is_param_admitted()`，唯一 C++ 真源）── 未准入类型维持既有声明面与行为，逐类型迁移。不覆盖：非标量参数（数组 / 嵌套对象 / 字符串枚举）、`configs/param_rules/*.json` 的存废、Python 链与代码生成。

决策源：`docs/adr/ADR-X.17-module-param-read-declares.md`；基线台账：`docs/implementation/module-param-baseline.md`。

## Requirements

### Requirement: module-param-read-declares

模块的可配置参数 SHALL 以「一次读取调用即一次声明」的方式加载 —— 该调用同时完成**类型校验**、**范围校验**、**默认值填充**与**赋值**，并登记该键为「已消费」。⇒ 不存在与读取分离的独立声明面（⛔ 同一参数 SHALL NOT 在两处各写一遍）。

#### Scenario: 参数越界 SHALL 在装配期失败

- **WHEN** 某模块实例的配置含越界参数值（如 `buffer_size: 0` 或 `65`，声明范围 1..64）
- **THEN** 装配 SHALL **失败**（`instantiateAll` 返回 false）
- **AND** 错误信息 SHALL 含**模块名 + 键名 + 期望范围**
- **AND** SHALL **NOT** 静默截断到边界
- **AND** SHALL **NOT** 回退默认值而让装配通过

#### Scenario: 参数类型错误 SHALL 在装配期失败

- **WHEN** 某模块实例的配置含类型不符的参数值（如 `buffer_size: "abc"`）
- **THEN** 装配 SHALL **失败**
- **AND** SHALL **NOT** 依赖模块内部 `get<T>()` 的顺带异常作为唯一防线

#### Scenario: 数值边界 SHALL NOT 被静默放宽或收窄

- **WHEN** 某参数的声明范围与成员类型可表示范围不一致（如声明 `1..64` 但成员为 `uint8_t`）
- **THEN** 装配 SHALL 以**声明范围**为准判定（⛔ 不得因成员类型更宽而静默放宽）
- **AND** 整型参数 SHALL 拒绝非整数 JSON 值（`8.0` / `1e20` / `true` 一律失败，⛔ 不得依赖 `get<int64_t>()` 的隐式转换）
- **AND** 无符号参数 SHALL 拒绝负值

#### Scenario: 无 params 键的实例 SHALL 仍获得默认值

- **WHEN** 某模块实例的配置**不含** `params` 键（或 `params` 中不含该参数）
- **THEN** 声明的默认值 SHALL 落到对应成员
- **AND** 该实例 SHALL 被加载一次（⛔ 不得因 `params` 缺失而跳过加载）

#### Scenario: 加载 SHALL 幂等

- **WHEN** 同一实例被二次加载
- **THEN** 成员值 SHALL 不变
- **AND** 加载 SHALL NOT 清除或重置**非本机制写入**的状态
- **AND** 该「不清除」性质 SHALL 由**既有副作用套件零变化**承载判定（本 change 不迁移带此类副作用的类型 ⇒ 其行为差异只可能来自被改动的公共路径）

#### Scenario: 默认值 SHALL NOT 写回配置 JSON

- **WHEN** 某参数声明了默认值，而实例配置中**不提供**该键
- **THEN** 默认值 SHALL 落到成员
- **AND** 装配后的配置对象中 SHALL **NOT** 出现该键（⛔ 不得为「让默认值生效」而把默认值注入回 `params`）

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

### Requirement: module-param-behavioral-evidence

每个被声明的参数 SHALL 附带一条**行为判别**测试：显式设置两个不同合法值（默认值 / 非默认值）⇒ 至少一个可观测指标（统计量 / 只读访问器 / 输出计数）不同。⛔ 无可观测行为影响的参数 SHALL NOT 进入声明。

#### Scenario: 声明了却无效果的参数 SHALL 被拒绝

- **WHEN** 某参数被声明，但其值不影响任何可观测指标（如声明了 `vc_count` 而维度仍是编译期常量）
- **THEN** 该参数 SHALL NOT 进入声明
- **AND** 该缺陷 SHALL 由行为判别测试的缺失暴露（⛔ 机制本身不保证「写入即被读取」）
