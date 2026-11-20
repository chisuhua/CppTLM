# cep-litmus-falsification Specification

## Purpose
为「复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`」提供**可运行的证伪尝试**面：把已声明的逐网络通道依赖边与 litmus 语料里的跨域边合成**复合图**，在图上搜索环，从而把「该组合是否可被证伪」从**隐式**变成**可机检**。

本 capability 只做**证伪尝试**，⛔ **不判定公理成立**（per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」）：无环 ⇒ 「本次语料范围内**未证伪**」；边类 `rfe`/`fr`/`ws` 当前**两仓均无定义** ⇒ 语料为空时判定为「**不可证伪**」这一真结果，⛔ 不是「公理已成立」。

来源：登记册队列 2 的 **I-6a**（`INV-CEP-13-IC1` 的证据来源）· 归档 change `2026-11-21-cpptlm-cep-litmus-falsification-harness` · 台账 Entry #513。

## Requirements

### Requirement: cep-litmus-falsification

CEP SHALL 提供**复合一致性公理**的 litmus **证伪工装**，并 SHALL 以**可机检的判定形态**报告结果；⛔ SHALL NOT 以任何形态表述为「已证明」。

#### Scenario: 复合图 SHALL 由已声明边与语料跨域边合成

- **WHEN** 工装在协议实例声明（`protocol_instances.json` 的逐网络通道依赖边）与 litmus 语料的跨域边之上建图
- **THEN** 顶点 SHALL 为 `(instance, network, vc)`，与既有装配期闸 `channel_dependency_graph.hh` 同形
- **AND** 语料边端点未绑定时 SHALL **不参与**建图（⛔ 不得因此造出假环）
- **AND** 边类（`class`）SHALL **只被记录**、SHALL NOT 被解释（`rfe`/`fr`/`ws` 当前**无定义**）

#### Scenario: 判定 SHALL 只有「证伪 / 未证伪 / 不可证伪」三形态

- **WHEN** 语料为空
- **THEN** 判定 SHALL 为 `reachable = false` 且 SHALL 给出**具体原因**（边类无声明面 ⇒ 复合公理当前不可证伪）
- **AND** SHALL NOT 表述为「公理成立」
- **WHEN** 语料非空且图中无环
- **THEN** 判定 SHALL 为 `falsified = false`，语义 SHALL 为「在本次语料范围内**未证伪**」
- **AND** SHALL NOT 表述为「已证明」（per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」）
- **WHEN** 语料非空且图中存在环
- **THEN** 判定 SHALL 为 `falsified = true` 且 SHALL 列出**环的顶点序列**

#### Scenario: 判定 SHALL 携带覆盖边界

- **WHEN** 工装产出判定
- **THEN** 判定 SHALL 含**非空** `coverage_note`，写明「边类当前无定义 ⇒ 只对已声明跨域边尝试证伪」
- **AND** 判定产物 SHALL NOT 出现「已证明 / proved」字样

#### Scenario: 证伪机械 SHALL 有鉴别力

- **WHEN** 喂入含**跨域环**（两个网络各一顶点互指）的语料
- **THEN** 判定 SHALL 翻转为 `falsified = true`
- **AND** 环文本 SHALL 同时包含**两个网络名**
- **AND** 该形态 SHALL 由独立闸以**反向 fixture** 强制校验（⛔ 工装不得退化为恒绿占位）
