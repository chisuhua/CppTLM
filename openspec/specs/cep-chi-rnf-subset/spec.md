# cep-chi-rnf-subset Specification

## Purpose
定义 **CHI RN-F 子集**(`chi_rnf` 协议实例,per `ADR-CEP-12` 决策 1~10 + `ADR-CEP-11 §3` 锁表 `chi-r2`)的
**实例面**可执行契约:6 通道 adapter + 6→4 `perm` 投影 + 6→3 消息类映射 + 实例声明 + 静默升级声明 +
**真实载体首验**。

CHI RN-F 是 CEP 平台的**第 2 个**协议实例(第 1 个 = `moesi_gpu`)与**第 1 个**「非 ArchForge 贡献」的
工业协议实例(per `07 号 §5.1.1`:CHI 属框架级工业族口径,ArchForge 只贡献 `moesi_gpu` / `mesif`)。
⇒ 其**声明落点**由另一 capability `cep-instance-authority-partition` 承担。

交付**实例语义**,**不**交付完整 CHI(per `ADR-CEP-16 §4`:Stage 1.4 只做 RN-F subset,完整 CHI → Stage 4+ 维护态)。

## Requirements

### Requirement: cep-chi-rnf-subset

CEP SHALL 为 **CHI RN-F 子集**提供:6 通道 adapter + 4 值 `perm` 投影 + 6→3 消息类映射 +
符合冻结 schema 的实例声明 + 静默升级声明 + **真实载体**上的协议语义首验。

> **边界**:不改 `protocol_instance.schema.json`(冻结面)· 不改 `adapter_schema_contracts.hh`
> (判据本体零改动)· 不改 `CohIfBundle`(队列 #7 的 CC-01 / CC-02 零触碰)· 不扩 `CATEGORY_*`(恒 3 类)·
> 不扩 `perm`(恒 4 值)· 新增 invariant 数 = 0。载体落点见 `design.md` D-η。

#### Scenario: CR-01 —— `chi_rnf` 实例声明 SHALL 符合**冻结** schema 的全 13 属性

- **WHEN** 核对 `protocol_instances.json` 的 `chi_rnf` 实例声明
- **THEN** 子条件 ① SHALL 成立:该实例 SHALL 含 `protocol_instance.schema.json` 的**全部 13 个
  `required` 顶层属性**(缺一即红)
- **AND** 子条件 ② SHALL 成立:`perm` SHALL 为**全部 4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)
  —— 4 值锁不破(per `ADR-CEP-02` CEN-Ib11)
- **AND** 子条件 ③ SHALL 成立:`protocol_version` SHALL = **`chi-r2`**(per `ADR-CEP-11 §3` 锁定表;
  表外取值 = 装配期 fail)
- **AND** 子条件 ④ SHALL 成立:`family` SHALL = **`industrial`**;`adapter` SHALL = **`ChiRnfAdapter`**
  (逐字对齐 `07 号 §5.1.1` 的 adapter 名)
- **AND** 子条件 ⑤ SHALL 成立:`status` 在实现落地前 SHALL 为 `planned`,落地后翻 `implemented`
  (per `ADR-CEP-12` 决策 9:`status` 是**本仓权威字段**)
- **AND** **反向 fixture**:删 `chi_rnf` 的任一 `required` 键 ⇒ 断言必红
- **AND** **反向 fixture**:把 `protocol_version` 写成锁定表外的值 ⇒ 断言必红
- **AND** **正控**:`moesi_gpu` / `mesif` 两条既有声明 SHALL 仍 PASS

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-01 五子条件断言 + 2 条反向 fixture(删 required 键 /
> 表外版本)+ 正控断言

#### Scenario: CR-02 —— `chi_rnf` 的 `ordering_assumption` / `vc_binding` SHALL 自洽

- **WHEN** 核对 `chi_rnf` 的排序假设与 VC 绑定
- **THEN** 子条件 ① SHALL 成立:`ordering_assumption[net]` 的每个 `vc_id` SHALL ∈
  `vc_binding[net]` 的**值域**(per `ADR-CEP-12` 决策 5)
- **AND** 子条件 ② SHALL 成立:`vc_binding[net]` 的每个消息类 SHALL ∈ **6 类规范词表**
  `{request, response, snoop, snoop_resp, atomic, query}`(per ArchForge 32 号 §6.0.1)
- **AND** 子条件 ③ SHALL 成立:两个字段的**网络键 SHALL 对称**(per 32 号 §6.2 纪律)
- **AND** 子条件 ④ SHALL 成立:每个 `vc_id` SHALL `< fabric.json` 该网络的 `vc_count`
- **AND** 子条件 ⑤ SHALL 成立:**响应向**消息类(`response` / `snoop_resp`)绑定的 `vc_id` SHALL 在
  `ordering_assumption` 中声明为 **`p2p_ordered`**(per `ordering_point_checker.hh` J1 判据)
- **AND** **反向 fixture**:把 `ordering_assumption` 的某个 `vc_id` 改成不在 `vc_binding` 值域内 ⇒ 必红
- **AND** **反向 fixture**:把 `vc_binding` 的消息类写成 6 类词表外的值 ⇒ 必红
- **AND** **反向 fixture**:删掉 `ordering_assumption` 的某个网络键(键不对称)⇒ 必红
- **AND** **正控**:`moesi_gpu` / `mesif` 两条既有实例 SHALL 仍 PASS

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-02 五子条件断言 + 3 条反向 fixture + 正控断言

#### Scenario: CR-03 —— CHI 的 6→3 消息类映射 SHALL 完备、值域受限,且恒等 SHALL 被登记

- **WHEN** 核对 `ChiRnfAdapter` 的消息类 → `CATEGORY_*` 映射表
- **THEN** 子条件 ① SHALL 成立:该实例**实际使用**的每个消息类 SHALL 有**恰好一行**映射(无多、无少)
  —— 「实际使用」的界定规则 =「subset 声明的 opcode 集能在该类上**发出或吸收**一个 wire 事务」
- **AND** 子条件 ② SHALL 成立:每个映射目标 SHALL ∈ 链路层 `CATEGORY_*` 的 **3 类**
  (`CATEGORY_*` **不**为承载 6 类词表而扩类,per `ADR-CEP-12 §8.3` ②)
- **AND** 子条件 ③ SHALL 成立:映射表**相对平台方向默认表非恒等** —— 至少一行的目标 ≠ 同名 `CATEGORY_*`
- **AND** 子条件 ④ SHALL 成立:若映射**确实恒等**,则 SHALL 携带 `identity_reason`(恒等是**合法状态**,
  但必须**被登记**)
- **AND** **反向 fixture**:删 `snoop` 映射行 ⇒ 必红
- **AND** **反向 fixture**:目标写成第 4 类 ⇒ 必红
- **AND** **反向 fixture**:全表恒等且**无** `identity_reason` ⇒ 必红
- **AND** **正控**:`moesi_gpu` 的映射 SHALL 仍 PASS

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-03 四子条件断言 + 3 条反向 fixture + 正控断言
>
> **⚠️ 已知语义缺口(如实登记,不静默放过)**:AC-04 的恒等判定**遍历全 6 类**默认表
> (`adapter_schema_contracts.hh:388-398`)⇒ 只用 4 类的子集 adapter **机械判「非恒等」** ——
> 结构上真、语义上误导。CHI 的 4 行全部与方向默认表一致 ⇒ `kMappingIsIdentity` = `true` +
> 携带 `kIdentityReason`。该缺口的**修复归独立 change**(机制改动 = MODIFIES
> `cep-adapter-schema-contracts` main spec ⇒ 挤占 KPI slot 且越界)⇒ 本 change **只登记**。

#### Scenario: CR-04 —— CHI 的 6→4 `perm` 投影 SHALL 完备,且瞬态 SHALL 无投影

- **WHEN** 核对 `ChiRnfAdapter` 的状态 → `perm` 投影与状态转换
- **THEN** 子条件 ① SHALL 成立:**每个稳定态**(`I` / `SC` / `UC` / `UD` / `SD`)SHALL 有 4 值投影(缺一即红)
- **AND** 子条件 ② SHALL 成立:**瞬态态 SHALL 无投影**(跨域接口不得出现中间态,per `INV-CEP-05-IC1`:
  Stub 只暴露稳定状态)
- **AND** 子条件 ③ SHALL 成立:`UC + Store → UD` 的转换 SHALL 存在(目标态 = `UD`)
- **AND** 子条件 ④ SHALL 成立:该转换后的 `perm` SHALL 从 `UNIQUE` 升为 `WRITEBACK`(秩严格升高)
- **AND** 子条件 ⑤ SHALL 成立:`SD → WRITEBACK` 的**语义损失 SHALL 被显式登记**(与 `moesi_gpu` 的
  `O → WRITEBACK` 合并损失同型);且登记 SHALL 注明「该损失在 RN 侧**不可补偿**」——
  `chi_rnf` 的 `sharer_tracker` 为「不参与」⇒ sharer 信息只存在于 HN-F
- **AND** **反向 fixture**:让瞬态态拥有投影 ⇒ 必红
- **AND** **反向 fixture**:改掉 `UC + Store → UD` 的转换 ⇒ 必红
- **AND** **正控**:`moesi_gpu` 的投影 SHALL 仍 PASS

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-04 五子条件断言 + 2 条反向 fixture + 正控断言

#### Scenario: CR-05 —— `chi_rnf` 的静默升级 SHALL 被声明且被契约判据接受

- **WHEN** 核对 `perm_upgrades.json` 的 `chi_rnf` 行
- **THEN** 子条件 ① SHALL 成立:`perm_upgrades.json` SHALL 含一行
  `{instance_id: "chi_rnf", from: "UNIQUE", to: "WRITEBACK"}`
- **AND** 子条件 ② SHALL 成立:该行的 `instance_id` SHALL 存在于 `protocol_instances.json` 的
  `instances[]`(引用完整性,per AC-02 子条件 ①)
- **AND** 子条件 ③ SHALL 成立:该实例的 `perm` 数组 SHALL **同时包含** `from` 与 `to` 两个值
- **AND** 子条件 ④ SHALL 成立:`to` 的权限秩 SHALL ≥ `from` 的权限秩(静默升级**不得**降权限)
- **AND** **反向 fixture**:把 `instance_id` 改成不存在的 id ⇒ 必红
- **AND** **反向 fixture**:改成 `UNIQUE → READ`(降权限)⇒ 必红
- **AND** **正控**:`moesi_gpu` 的既有行 SHALL 仍 PASS;`mesif` SHALL **不**出现在声明源里
  (AC-03 的负控见证)

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-05 四子条件断言 + 2 条反向 fixture + 正控断言
>
> **⚠️ 台账勘误义务**:台账 Entry #71 的 F-1 称「CHI RN-F 是**显式**请求升级 ⇒ **不需要**该演进」——
> 其引证的 `07 号 §5.1.1` CHI 行**无**「显式/静默升级」字样(引证失锚),且 F-1 混淆了**两条升级轴**:
> **轴 1**(获取独占 S→U)CHI 显式;**轴 2**(独占干净→脏 UC→UD)CHI **静默**。F-1 的**排期结论不受影响**
> (perm 形态演进跟唯一真实消费者 `moesi_gpu` 走,机制已由队列 #6 交付)⇒ 本 change **追加勘误 entry**
> (台账 append-only,原文不改)。

#### Scenario: CR-06 —— `chi_rnf` 的 `sharer_tracker` SHALL 为「不参与」+ 退化 caps 行

- **WHEN** 核对 `chi_rnf` 的 `sharer_tracker`
- **THEN** 子条件 ① SHALL 成立:`impl` SHALL = **`不参与`**(schema 枚举允许该字面值)——
  依据:RN-F **不维护** sharer set(由 HN-F 维护,超出 RN-F adapter 作用域)+ CHI 在 RN 侧是
  **副本携带权限**协议(`UC`/`UD`/`SC`/`SD` 本地编码权限)⇒ 正是该枚举值的原义
- **AND** 子条件 ② SHALL 成立:`caps` SHALL 等于**退化行** `scope=NONE` / `completeness=EXACT` /
  `authority=NONE` / `aggregation_at=NONE`(对齐 `ADR-CEP-04` 的 `BroadcastNull` 退化行)
- **AND** 子条件 ③ SHALL 成立:本仓 SHALL **不**新增 SharerTracker 实现(「不参与」不是第 6 种实现;
  per `ADR-CEP-04` 决策 7 的实现清单不变)
- **AND** **反向 fixture**:`impl = "不参与"` 但 `caps.scope = "SYSTEM"` ⇒ 必红
- **AND** **正控**:`moesi_gpu` 的 `SparseSnoopFilter` + 非退化 caps 行 SHALL 仍 PASS

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-06 三子条件断言 + 1 条反向 fixture + 正控断言

#### Scenario: CR-10 —— CHI 事务 SHALL 由**真插件**驱动、穿过 `ProtocolBridge` 抵达对端

- **WHEN** 用 `ChiRnfProtocolEngine`(真插件)装配一个 CHI 协议实例并走 JSON 端口装配
- **THEN** 子条件 ① SHALL 成立:一次 CHI 事务 SHALL **穿过** adapter 抵达对端模块(端到端断言,非仅绑定成功)
- **AND** 子条件 ② SHALL 成立:该事务 SHALL 由 `ChiRnfProtocolEngine` 驱动 —— 判据 SHALL 钉在
  `CohIfNullProtocolEngine` **结构上不可能**具备的行为上(Null 的全部 `on_*` 把参数 `(void)` 丢弃、
  **零发射**)
- **AND** 子条件 ③ SHALL 成立:装配 SHALL 通过 `module_factory` 的桥消费路径
  (`cep.bridges` + `protocol_instances` 声明源),且桥的 `protocol_id_` SHALL 与实例声明的 `family`
  **编号对齐**(per 队列 #7 的 CC-05 ④)
- **AND** **反向 fixture**:把引擎换成 `CohIfNullProtocolEngine` ⇒ 断言**必红**(Null 零发射)
- **AND** **正控**:换成 `ChiRnfProtocolEngine` ⇒ 断言绿

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-10 端到端断言 + Null 对照反向 fixture + 正控

#### Scenario: CR-11 —— CHI 的 6 通道 SHALL **各有**观测点事务

- **WHEN** 驱动 CHI RN-F 子集的 6 个通道
- **THEN** 子条件 ① SHALL 成立:`TXREQ` / `RXRSP` / `RXDAT` / `RXSNP` / `TXRSP` / `TXDAT` **每个通道**
  SHALL 至少产生 1 次可观测事务
- **AND** 子条件 ② SHALL 成立:`TXREQ` 的**双流承载** SHALL 被分别观测(`Request` 与 `Writeback`
  两条 CohIf 流)—— 按流写覆盖会漏掉这一点
- **AND** **反向 fixture**:屏蔽任一通道 ⇒ 该通道的观测点断言**必红**
- **AND** **正控**:6 通道全通 ⇒ 全绿

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-11 逐通道观测点断言 + 屏蔽通道反向 fixture

#### Scenario: CR-12 —— CHI 的 Credit 流控 SHALL 可观测(耗尽停发 / 补充恢复)

- **WHEN** 在 Credit 耗尽与补充两种状态下驱动 CHI 事务
- **THEN** 子条件 ① SHALL 成立:Credit 耗尽时 SHALL **停发**(可观测:发出计数不再增长)
- **AND** 子条件 ② SHALL 成立:补充 Credit 后 SHALL **恢复**发送(**正控**)
- **AND** **反向 fixture**:旁路 Credit 记账(记账不参与门控)⇒ 耗尽时仍继续发送 ⇒ 断言**必红**
- **AND** **正控**:补充 Credit 后恢复 ⇒ 绿

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-12 两子条件断言 + 旁路记账反向 fixture + 正控

#### Scenario: CR-13 —— CHI 的 TxnID 管理 SHALL 保证在途唯一且完成后可复用

- **WHEN** 驱动多个并发 CHI 事务
- **THEN** 子条件 ① SHALL 成立:**在途**事务的 `transaction_id` SHALL **唯一**(不得重复)
- **AND** 子条件 ② SHALL 成立:事务**完成**后其 `transaction_id` SHALL **可被复用**(释放登记)
- **AND** **反向 fixture**:强制注入一个在途重复的 `transaction_id` ⇒ 断言**必红**
- **AND** **正控**:完成后复用 ⇒ 绿

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-13 两子条件断言 + 重复 TxnID 反向 fixture + 正控

#### Scenario: CR-14 —— CHI 的 CompAck SHALL 恰好发一个,不需者 SHALL 零发送

- **WHEN** 驱动需 CompAck 与不需 CompAck 两类 CHI 事务
- **THEN** 子条件 ① SHALL 成立:需 CompAck 的读类事务在收到数据后 SHALL **恰好发 1 个** CompAck
- **AND** 子条件 ② SHALL 成立:不需 CompAck 的事务 SHALL **零发送**(**正控**)
- **AND** **反向 fixture**:抑制 CompAck ⇒ 断言**必红**
- **AND** **正控**:不需 CompAck 者零发送 ⇒ 绿

> **锚点**:`test_cep_chi_rnf_subset.cc` 的 CR-14 两子条件断言 + 抑制 CompAck 反向 fixture + 正控
