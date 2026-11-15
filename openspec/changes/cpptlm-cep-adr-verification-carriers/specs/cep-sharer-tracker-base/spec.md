## MODIFIED Requirements

### Requirement: responder 位宽扩容的机制与触发判据

系统 SHALL 以成文规则定义 sharer 位宽的扩容路径,使「何时扩容 / 扩到多少 / 随扩容同步哪些点」不依赖口头约定。

> **本次修订(2026-11-15,per 台账 Entry #431)**:本 Requirement **新增 2 条 Scenario** —— 把 `ADR-CEP-21` **V1-6 自认的真空**(「当前**无任何机器闸/测试钉死 `kResponderMappedBits == 4`**」)与 **V2-6 的同形判据**(运行时 mask 数值锚)落成**机器可判**。
> ⚠️ **边界**:本 Requirement 规定的是「**当前状态的变更检测义务**」,**不**论证「4 是正确的宽度」—— 宽度的正确性来自 `ADR-CEP-21` 的阶段 2 触发判据(2a 机判 / 2b 人裁),本 change **不**动该判据、**不**执行阶段 2、**不**满足/替代 `V2-6`。
> ⚠️ **⛔ 不写成恒等式**:mask 的判据 SHALL 用**数值锚**(`0xF`),SHALL NOT 用 `kMappedResponderMask == (1 << kResponderMappedBits) - 1` —— `sharer_tracker.hh:141-142` 的定义**就是**该式 ⇒ 恒真 ⇒ 零判别力。

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

#### Scenario: 宽度常量与掩码数值锚 SHALL 被机器钉死

- **WHEN** 契约测试检查 responder 位宽的**当前状态**
- **THEN** `kResponderMappedBits` SHALL 等于 **4**(阶段 1「未扩容」状态被钉死)⇒ 任何把它改成 `W` 的变更 SHALL 判红,除非该变更**同时**完成 `ADR-CEP-21` 阶段 2(触发判据 + architect 裁决 + 台账 Entry)
- **AND** `kMappedResponderMask` SHALL 等于 **0xF**(数值锚)⇒ 掩码语义漂移 SHALL 判红
- **AND** 上述两条判据 SHALL 用**具体数值**表达,SHALL NOT 用 `kMappedResponderMask == (1 << kResponderMappedBits) - 1` 这类**定义复述式**(恒真 ⇒ 零判别力)
- **AND** 失败消息 SHALL 含**实测值**

> **依据**:`ADR-CEP-21` V1-6(自认真空)· `ADR-CEP-21` V2-6(运行时 mask 判据的同形形态)· `ADR-CEP-21:47`(文本锚盲区的实证)。
> **锚点**:`include/tlm/cep/sharer_tracker.hh:92`(`kResponderMappedBits`)· `:141-142`(`kMappedResponderMask`)· `test/test_cep_sharer_tracker_base.cc`。

#### Scenario: 持久反向 fixture SHALL 在脚本侧独立存活且只写临时目录

- **WHEN** 仓内执行本 Requirement 的**持久**反向 fixture
- **THEN** 该 fixture SHALL 位于**脚本侧**(⛔ 不在 C++ 测试文件内)⇒ 删除 C++ 用例后 fixture 仍能令判据变红
- **AND** fixture SHALL 仅**写自己的 `mktemp -d` 临时目录**(⛔ 不改写仓内任何文件)⇒ 可安全进 ctest 并行运行
- **AND** **宽度毒化**(影子树复制头文件 ⇒ 改 `kResponderMappedBits` ⇒ 编译最小探针)SHALL 令判据**以非零退出码**判红
- **AND** ⚠️ **编译失败 SHALL 归类为环境错误并以 `exit 2` 退出**,SHALL NOT 被计作「判据失败」(⛔ 编译失败不得冒充判据失败)
- **AND** 毒化态 SHALL 判红、未毒化态 SHALL 判绿

> **锚点**:`scripts/test/cep_adr_verification_carriers_reverse_verify.sh` · 形态先例 `scripts/test/cep_flow_support_reverse_verify.sh`(只写临时目录)· `scripts/test/cep_tilelink_adapter_reverse_verify.sh`(影子树编译期锁毒化副本)。
