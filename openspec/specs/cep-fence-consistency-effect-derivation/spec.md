# cep-fence-consistency-effect-derivation Specification

## Purpose

把「某个协议实例的**协议语义**上，fence（barrier）带来什么一致性效果」从**散文**变成
**可机检的现状面**：`kFenceEffectCapabilities`（8 实例 · 由 `derive_fence_effect()` **生成**）
+ **独立 oracle**（判据测试内**硬编码**期望表 ⇄ 编译出的派生值）+ **输入副本漂移检测**
（头文件副本 ⇄ `protocol_instances.json` 逐实例逐字段）。

本能力**只登记现状、只让缺口可检出**，⛔ **不补齐** fence 的**协议语义**（那属协议 FSM 语义设计，
per 宪章归 ArchForge）。

⚠️ **本单元的决策面（随本 spec 的五步授权链 ratify）**：推导规则表 **R1–R7**（见下）。它把
调研文档 §4.3.1 的**论文口径**提升为机器可检规则 ⇒ **不是**「零决策变更」。
⚠️ **诚实边界**：派生值 = **声明效果**（由声明推导）⛔ **不是**引擎的行为语义 ⛔ **不是**
**跨一致性域**的 barrier 保证（per ADR-CEP-13 决策 1：barrier_domain ≠ 一致性域边界 ⇒
本视图粒度 = **实例**）⚠️ R5 中「read=RC-data ⇒ fence 不提供 acquire」的解读为 **provisional**
（待 architect 追认）。

来源：gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G4** + §2.3.1 素材盘点结论 ·
台账 Entry **#544**（立项授权 + 规则表 + metis 处置）· 形态照抄 G10 的现成范式。

## Requirements

### Requirement: fence 一致性效果的派生声明面

CEP SHALL 提供**协议语义层**的 fence 一致性效果**派生声明面**，并 SHALL 以**可机检的形态**
报告每个已声明协议实例的推导结果。

#### Scenario: 声明面 SHALL 覆盖全部已声明实例

- **WHEN** 核对 `kFenceEffectCapabilities` 的实例集与 `protocol_instances.json` 的实例集
- **THEN** 两者 SHALL **相等**（双向零 orphan：既无漏声明，也无多余声明）
- **AND** 每行 SHALL 有**非空** `basis`（判定依据）且 `id` SHALL 唯一

#### Scenario: 派生值 SHALL 由推导函数生成

- **WHEN** 编译期逐行求值 `effect == derive_fence_effect(read_label, write_label, has_barrier)`
- **THEN** 8 行 SHALL **全部**成立（失败签名 SHALL 含**实例 id 字面量**）
- **AND** `effect` SHALL **不由手写**（表行由 `make_fence_effect_capabilities()` 生成）

### Requirement: 推导规则（决策面 · 随五步链 ratify）

派生 SHALL 遵循优先级规则表 **R1-R7**；⛔ **规则表与实现不得分离**（闸在契约面与实现面双向核对）。

#### Scenario: 规则表 SHALL 按下列优先级生效

- **WHEN** 推导某实例的 fence 效果
- **THEN** SHALL 依次应用：**R1** 无 barrier 通道 ⇒ **None**（**最高**优先级 · 遮蔽模型规则）→
  **R2** read / write 标签**族不同** ⇒ **Invalid**（域外）→ **R3** 族 = SC ⇒ **None**
  （SC 已全序 ⇒ 与 barrier 存在与否**无关**）→ **R4** 族 = TSO ⇒ **FullBarrier** →
  **R5** 族 = RC ⇒ **按方向逐项**（write=RC-release ⇒ release；read=RC-acquire ⇒ acquire；
  论文 RC 行是**并集上界** ⇒ 本能力取**子集**）→ **R6** 族 = PLO ⇒ **Partial**（只补 W→W 与 R→W）→
  **R7** 标签族 ∉ {SC, TSO, RC-*, PLO} ⇒ **Invalid**（域外）

#### Scenario: 域外组合 ⇒ 必红

- **WHEN** 任一实例的 read / write 标签**跨族**或**不在枚举内**
- **THEN** 契约闸 SHALL 判红（红因 `fence-derivation-out-of-domain`）
- **AND** 该行 SHALL **不得**被当作合法派生值

#### Scenario: 规则表 SHALL 落在契约面

- **WHEN** 扫描 main spec
- **THEN** 规则行 **R1 ` R7** SHALL 全部在位
- **AND** 闸 SHALL 在**契约面与实现面**双向核对（⛔ 规则只写在代码里 ⇒ 必红）

### Requirement: 派生正确性的独立 oracle

派生正确性 SHALL 由**独立于推导实现**的 oracle 判定，⛔ **不得**自证。

#### Scenario: 硬编码期望表 SHALL 与派生值逐实例相等

- **WHEN** 比对判据测试内的**硬编码期望表**与编译出的派生值
- **THEN** 8 个实例 SHALL **全部**相等
- **AND** 期望表 SHALL **不由** `derive_fence_effect()` 生成（⛔ 否则恒真 ⇒ 假绿）

#### Scenario: oracle 缺失 ⇒ 必红

- **WHEN** 判据测试内的硬编码期望表缺失或行数不足
- **THEN** 契约闸 SHALL 判红（红因 `fence-oracle-missing`）⇒ 防止判据退化为空

#### Scenario: 死分支 SHALL 有正控

- **WHEN** 某推导分支在当前实例集下**无实例**（TSO / PLO / RC 单方向）
- **THEN** 判据测试 SHALL 以**合成行**覆盖该分支（⛔ 合成行**不进**实例表 ⇒ 不破坏双向零 orphan）

### Requirement: 输入副本漂移与 JSON 缺键

声明面内含标签 / barrier 的**镜像副本** ⇒ SHALL 有判据保证其与真源**逐字段一致**。

#### Scenario: 副本与 JSON 不一致 ⇒ 必红

- **WHEN** 头文件副本的 (read_label, write_label, has_barrier) 与 `protocol_instances.json` 不等
- **THEN** 契约闸 SHALL 判红（红因 `fence-input-copy-drift`）
- **AND** 该漂移 SHALL 被视为「声明面失真」⇒ **不得**静默

#### Scenario: JSON 缺键 ⇒ 必红

- **WHEN** 某实例缺 `consistency_labels` 或 `flow_support.barrier`
- **THEN** 契约闸 SHALL 判红（红因 `fence-json-key-missing`）
- **AND** ⛔ SHALL **NOT** 静默跳过该实例（跳过 ⇒ 漏检）

### Requirement: 诚实边界

派生声明面 SHALL 以**机器可读**形态登记其边界，使「fence 效果」的表述**不越层**。

#### Scenario: 边界常量 SHALL 齐备

- **WHEN** 扫描声明头
- **THEN** SHALL 同时存在六个常量：`kFenceEffectIsDeclarationOnly` ·
`kFenceEffectIsNotCrossDomainGuarantee` · `kBarrierAbsenceMeansNoFenceEffect` ·
`kScModelMeansNoExtraFenceEffect` · `kRcEffectIsPaperUpperBoundSubset` ·
`kFenceEffectGranularityIsPerInstance`
- **AND** SHALL 含「**声明效果**」措辞（⛔ 非引擎行为语义）

#### Scenario: 本能力 SHALL 不补齐缺口

- **WHEN** 核对本能力的边界声明
- **THEN** SHALL 明示「**只登记现状**、⛔ **不补** fence 的**协议语义**」（归 ArchForge）
- **AND** SHALL 明示「⛔ **不是**跨一致性域的 barrier 保证」

### Requirement: 通用层协议中立与正交性

本能力 SHALL 与 G1（`cep-protocol-engine-capability-declaration`）与 G10
（`cep-hop-timing-capability-declaration`）**正交**：同一事实**只**有一个真源。

#### Scenario: 通用层 SHALL 不含协议类名

- **WHEN** 扫描声明头 `include/tlm/cep/fence_effects.hh`
- **THEN** SHALL **不**出现任何协议**类名**，SHALL **不** `#include` 协议引擎 / adapter 头

#### Scenario: 声明区段 SHALL 无 G1 / G10 字段

- **WHEN** 扫描声明表的 struct + init 区段
- **THEN** SHALL **不**出现 `egress` / `in_flight` / `hop_count` / `last_hops` / `latency_cycles` 字样
