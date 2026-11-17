# module-param-effect-landing Specification (delta)

## ADDED Requirements

### Requirement: module-param-effect-landing

出现在模块配置中的每个参数键 SHALL 具有**确定效果** —— 它要么被**真实消费**（值进入运行时行为），要么被**显式拒绝**（非法值使装配失败），要么其**产出方已停止产出**（配置中出现该键 ⇒ 装配失败并报出该键名）。

⛔ SHALL NOT 存在「配置里有键、实现零效果」的键 —— 既不得静默丢弃，也不得长期以「已消费但不经 `load_param`」的方式豁免。

#### Scenario: 配置里的每个参数键 SHALL 有确定效果

- **WHEN** 某模块实例的配置含一个参数键
- **THEN** 该键 SHALL 属于三态之一：真实消费 / 显式拒绝 / 产出方已停止产出
- **AND** 该键 SHALL 在参数基线台账中有对应条目（含状态与实现位置）
- **AND** ⛔ 不得静默丢弃（配置写了不生效）

#### Scenario: 真实消费的键 SHALL 产生可观测差异

- **WHEN** 某键按「真实消费」处置
- **THEN** 两个不同合法值 SHALL 产生至少一个**可观测指标**的差异
- **AND** SHALL 存在对应的行为判别测试
- **AND** ⛔ 不得为通过判别而新增「只为测试存在」的观测面

#### Scenario: 产出方已停止产出的键 SHALL 使配置显式失败

- **WHEN** 配置含一个「产出方已停止产出」的键
- **THEN** 装配 SHALL **失败**（`instantiateAll` 返回 false）
- **AND** 错误信息 SHALL 含该键名
- **AND** SHALL NOT 静默忽略该键

#### Scenario: 伪参数 SHALL 只接受唯一合法值

- **WHEN** 某键的合法值集合只有一个元素（伪参数）
- **THEN** 写入非该值的配置 SHALL 使装配失败
- **AND** 错误信息 SHALL 含该键名与合法值集合
- **AND** ⛔ 不得以「已消费但不经 `load_param`」的方式长期豁免该键

#### Scenario: 零效果键 SHALL NOT 以豁免方式长期留存

- **WHEN** 某键既未被真实消费、也未被显式拒绝、其产出方也未停止产出
- **THEN** 该键 SHALL NOT 被登记为「已消费但不经 `load_param`」
- **AND** 该情形 SHALL 视为未闭合的缺陷并逐条登记
