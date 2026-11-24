# cep-compound-graph-falsification Specification

## Purpose
为「复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`」提供**可运行的证伪尝试**面：把**操作级轨迹**（线程 → 操作（读 / 写 + 地址 + 值））与**逐域模型**（域绑定的协议实例之 `consistency_labels`）合成**操作级复合图**，在图上搜索**环**，从而把「该组合是否可被证伪」从**隐式**变成**可机检**。

本 capability 只做**证伪尝试**，⛔ **不判定公理成立**（per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」）：存在**可行候选** ⇒ 「本次语料范围内**未证伪**」；⛔ **不检查域内一致性**（值面 / SWMR —— 属各域自身的验证义务，per `ADR-CEP-22` §4.2 缺口①）；⛔ **不验证**「迹中的域标识忠实代表 Ordering Point 作用域」（该假设由**迹作者**承担 —— 域身份**只**存在于迹数据面，per `ADR-CEP-22` §4.2 缺口②）。

来源：登记册队列 2 的 **I-6a**（`INV-CEP-13-IC1` 的证据来源）· 归档 change `2026-11-21-cpptlm-cep-litmus-falsification-harness` · 台账 Entry #513 · **架构裁定** `ADR-CEP-22`（✅ 已签发 v1.0 · 每域模型来源 + 域间边构造规则 · 台账 Entry #541 · 用户 ratify）· 实施 change `cep-memory-model-operational-trace`（台账 Entry #542）。

## Requirements

### Requirement: cep-compound-graph-falsification

CEP SHALL 提供**复合一致性公理**的**操作级**证伪工装，并 SHALL 以**可机检的判定形态**报告结果；⛔ SHALL NOT 以任何形态表述为「已证明」。

#### Scenario: 复合图 SHALL 由**操作级轨迹**与**逐域模型**合成

- **WHEN** 工装在**操作级轨迹**（线程 → 操作 → 读自映射 → 同地址写序候选）与**逐域模型**（域绑定的协议实例之 `consistency_labels`）之上建图
- **THEN** 顶点 SHALL 为**操作**（读 / 写 + 地址 + 值），⛔ **不得**为装配期**通道依赖图**的顶点形态（通道图 ≠ 操作图 ⇒ 混图 = **假绿**）
- **AND** 域内程序序边 SHALL 依该域的**读侧 / 写侧授权表**过滤（`SC` = 全部 · `RC-acquire` = {源是读} · `RC-release` = {目标是写} · `RC-data` = ∅ · `PLO` = {目标是写}）
- **AND** 域标签在授权表**未登记**时（`TSO` / 未知）SHALL 判为**不可判**，⛔ SHALL NOT 静默按 `SC` 处理
- **AND** 语料边端点未绑定时 SHALL **不参与**建图（⛔ 不得因此造出假环）

#### Scenario: 判定 SHALL 只有「证伪 / 未证伪 / 不可判 / 拒收」四形态

- **WHEN** 候选枚举完成且**存在可行候选**
- **THEN** 判定 SHALL 为 `not_falsified`，语义 SHALL 为「在本次语料范围内**未证伪**」
- **AND** SHALL NOT 表述为「公理成立」或「已证明」
- **WHEN** **全部**候选成环
- **THEN** 判定 SHALL 为 `falsified` 且 SHALL 给出**见证**（环上的操作 id 序列 + 成环候选标识）
- **WHEN** 候选枚举触上限且尚未找到可行候选
- **THEN** 判定 SHALL 为 `undecided` + 原因码 `search-truncated`，⛔ SHALL NOT 判为「未证伪」
- **WHEN** 输入非法（派生边作为输入 / 通道边混入 / 域未绑定 / 域实例不可解析 / 畸形迹）
- **THEN** 判定 SHALL 为 `invalid`（**拒收**）且 SHALL 给出**唯一**原因码，⛔ SHALL NOT 与「不可判」混同

#### Scenario: 判定 SHALL 携带**覆盖边界**

- **WHEN** 工装产出判定
- **THEN** 判定 SHALL 含**非空** `coverage_note`，写明「只尝试证伪」「⛔ 不检查域内一致性」「⛔ 不验证域标识的 Ordering Point 作用域忠实性」
- **AND** 判定产物 SHALL NOT 出现「已证明 / proved」字样

#### Scenario: **聚合语义** SHALL 为「∃ 可行 ⇒ 未证伪；∀ 成环 ⇒ 证伪」

- **WHEN** 同一址写的**候选写序**不唯一
- **THEN** 工装 SHALL **逐候选**判定并在判定产物中报告每个候选的判定
- **AND** **任一**候选成环 SHALL NOT 直接构成「证伪」（模型**允许**另一候选 ⇒ 该迹**不**反驳公理）
- **AND** **全部**候选成环 SHALL 判为 `falsified`

#### Scenario: **双图折衷** SHALL 显式化

- **WHEN** 工装产出判定
- **THEN** **公理面**（证伪图的输入）边类 SHALL 为 `ppo_com` · `rfe` · `fr` · `ws`，⛔ SHALL NOT 含域内 `rf`
- **AND** **观测面**（判定产物报告）SHALL **报告**域内 `rf`（跨域读自报告为 `rfe`）
- **AND** 跨域关系上的边类 SHALL ⊆ `combination_contract.compound_consistency.edge_classes` 的取值域
