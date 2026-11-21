## MODIFIED Requirements

### Requirement: cep-capability-map-contract

CEP SHALL 定义 **协议实例能力声明 schema**(`include/tlm/cep/protocol_instance.schema.json`)与 **3 条不变量**(`INV-CEP-12-IC1` ~ `IC3`)。

#### Scenario: schema 必含 7 个字段组 / 13 个顶层属性(IC1)

- **WHEN** 契约测试校验 schema 定义
- **THEN** schema 必含 **7** 个字段组(共 **13** 个**必含**顶层属性):`id`/`family`/`adapter`/`status`/`protocol_version` / `flow_support` / `perm`+`atomic` / `consistency_labels` / `ordering_assumption`+`vc_binding` / `sharer_tracker` / `combination_capabilities`
  > ⚠️ **口径修订(2026-10-21,per 代码复审)**:原写「8 个字段组」但枚举只有 **7** 组;
  > `ADR-CEP-12` IC1 原写「6 个」且枚举 7 组、`ADR-CEP-12` 决策汇总写「8 个」、
  > `ADR-CEP-17 §3.4` 写「6 个」—— **四处互不一致且无人断言**。
  > 现统一为 **7 个字段组 / 13 个必含顶层属性**(= schema `required` 属性数),并由测试机器断言。
  > ⚠️ **口径澄清(2026-11-21,per 登记册队列 2 的 I-6)**:schema 的属性**总数**为 **14** —— 第 14 项
  > `combination_contract` 是**可选**声明项(⛔ 不进 `required`)⇒ 本 Scenario 的「13 个顶层属性」指
  > **必含**(`required`)属性数,⛔ **不是** schema 属性总数(见下一 Scenario)。
- **AND** `flow_support` **恰好 10 项**,与 canonical 10 流**逐名对齐**
- **AND** `perm` 值域**恰好 4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)
- **AND** `combination_capabilities` **恰好 3 维**(`concurrent` / `hierarchical` / `heterogeneous`),且**正交于** `family`
- **AND** `consistency_labels` 必含 `read` / `write` 两级 **+** `request_level` 映射(承载"同一 wire 请求在不同标签下语义不同")

#### Scenario: combination_contract 是可选声明项(不进 required)

- **WHEN** 契约测试校验 `combination_contract` 的形态
- **THEN** schema 的 `properties` **恰 14** 项(第 14 项 = `combination_contract`)且 `required` **仍 13** 项 ⇒ `INV-CEP-12-IC1` 的「13 个必含顶层属性」**逐字为真**、其机器载体**零改动**
- **AND** `combination_contract.bridging.early_ack` 的取值域**恰 3** 值(`allow-early-write-ack` / `deny-early-completion` / `unknown`),⛔ **不含** `not-applicable`
- **AND** `combination_contract.bridging.required` **恰** `["early_ack"]`;`combination_contract.compound_consistency.required` **恰** `["adopted"]`
- **AND** `combination_contract.compound_consistency.edge_classes` 的取值域**恰 3** 值(`rfe` / `fr` / `ws`),⛔ 本仓**只登记取值域**、⛔ **不定义**其语义(语义锚点 = `ADR-CEP-13` 决策 2)
- **AND** `combination_contract.compound_consistency.adopted == true` ⇒ `edge_classes` **非空** ∧ `verification_scope` **非空**;`adopted == false` ⇒ 允许两者为空
- **AND** 本仓权威域(`repo_owned_instance_ids` ∪ `authority_migrations[].instance_id`)的**每个**实例 SHALL 声明 `combination_contract.bridging.early_ack`(取值可为 `unknown`)
- **AND** ⛔ `combination_contract` **不得**被用作 litmus 语料的派生来源(工装的图是**通道依赖图**、边类是**操作级**关系 ⇒ 范畴错误 ⇒ 假绿)

#### Scenario: 逐网络校验 ordering_assumption / vc_binding(IC2,2026-10-22 修订)

- **WHEN** 契约测试加载一份 `ordering_assumption` / `vc_binding` 为 **per-network 嵌套形态**的协议实例声明
- **THEN** 对每个 network `n` ∈ `ordering_assumption` ∪ `vc_binding`:
  - `ordering_assumption[n]` 的每个 `vc_id` ∈ `vc_binding[n].values()`
  - `vc_binding[n].values()` 中每个 `vc_id` **<** `fabric_config.networks[n].vc_count`(槽位上界)
  - `vc_binding[n]` 的每个消息类 ∈ **6 类规范词表** `{request,response,snoop,snoop_resp,atomic,query}`
- **AND** network 须在 fabric 网络命名空间中登记(per `22-nsa §0.5.4.1` NS-R1);未登记 ⇒ **配置加载 fail**
- **AND** `vc_id` 取值 ∈ 已声明域(per `noc_bundles_tlm.hh:38` 的 0-3)
- **AND** `ordering_assumption` 与 `vc_binding` 的 network 键集**必须相等**(逐网络对称,per ArchForge `32 号 §6.2`)

> ⚠️ **取代说明(2026-10-22,per ADR-CEP-12 §8 Status Update)**:本 scenario 原为
> 「named_network 必须解析到唯一 `vc_id`」——**「唯一性」这一谓词在 per-network 嵌套形态下整体消失**
> (network 即 key,其下 `vc_id` 是**显式**给出的,不存在多义)。
> **机器校验**:`scripts/test/cep_decl_alignment_check.sh` 第 ②/③/④/⑤ 项。

#### Scenario: status 是计数口径的唯一依据(IC3)

- **WHEN** 契约测试统计 adapter 实例数
- **THEN** 仅 `status == implemented` 的实例计入
- **AND** `planned` / `excluded` **不**计入
- **AND** 统计结果与 `07 号 §5.1` 的口径表述**一致**
