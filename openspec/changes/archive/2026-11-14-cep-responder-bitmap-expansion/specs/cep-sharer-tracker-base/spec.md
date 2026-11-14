## ADDED Requirements

### Requirement: responder 位宽扩容的机制与触发判据

系统 SHALL 以成文规则定义 sharer 位宽的扩容路径,使「何时扩容 / 扩到多少 / 随扩容同步哪些点」不依赖口头约定。

#### Scenario: 扩容的最小改动面

- **WHEN** 决定扩容到宽度 `W`
- **THEN** 改动 SHALL 限于 `kResponderMappedBits` 与 `kResponderBitToNode` 表项
- **AND** 当 `W > 8` 时 SHALL 先扩该数组(现为 `std::array<node_id_t, 8>`) —— 因 `responder_node_of` 的上界判据用 `kResponderMappedBits`,超过数组尺寸会直接越界

#### Scenario: 两个上限不可混

- **WHEN** 讨论位宽上限
- **THEN** `responder_set_t` 的位宽给出**掩码上限 = 64**(且 `kNoBit = 64u` 同值)
- **AND** `kResponderBitToNode` 的**表上限 = 数组尺寸**(当前 8)
- **AND** 系统 SHALL 以 `static_assert(kResponderMappedBits <= 64)` 守住掩码上限

#### Scenario: 不得预占无权威来源的位

- **WHEN** 扩容需要新增 node 到映射表
- **THEN** 该 node SHALL 来自权威声明(协议族声明 / 实例的 13 字段)
- **AND** SHALL NOT 填入自拟的 node
- **AND** 若某族需 > 64 sharer,SHALL 另立决策文档,不在本机制内处理

#### Scenario: 宽度未触发扩容时保持现行为

- **WHEN** 无任何协议族声明给出 > 4 个参与 sharer 跟踪的节点
- **THEN** `kResponderMappedBits` SHALL 保持 4,且既有 4 个 node 的 bit 映射逐位不变
- **AND** 编译期断言 `kResponderMappedBits <= 64` SHALL 成立

#### Scenario: 三处同步义务缺失时被识别

- **WHEN** 某次扩容修改了宽度与映射表
- **THEN** 同一变更 SHALL 同时处理:① 相关 main spec 的「未映射位」措辞 ② 新扩族的装配期硬断言 ③ 运行时掩码判据
- **AND** 上述任一遗漏时该变更 SHALL 被视为不完整

#### Scenario: 映射表与宽度不自洽时被检出

- **WHEN** 宽度覆盖的某个 bit 在映射表中指向无效节点
- **THEN** 满射判据 SHALL 失败(该 bit 必须映射到有效节点,且其位号必须回到原值)
