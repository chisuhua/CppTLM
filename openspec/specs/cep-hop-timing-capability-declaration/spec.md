# cep-hop-timing-capability-declaration Specification

## Purpose

把「某个协议实例的**协议语义**有没有 hop（消息穿越网络的跳数）计数 / 消息级时序（何时到达、耗时多久）模型」从**引擎头文件里的散文**变成**编译期 + 闸双重保证的声明面**：`kHopTimingCapabilities`（8 实例 · 逐行 `basis`）+ **引擎层反向存在性**判据（引擎头一旦出现 hop / 时序 API ⇒ 必红 ⇒ 强制走决策 + 更新声明）+ **编译期否定断言**（5 个实例引擎类型上零该类 API ⇒ 引擎新增即编译失败）。

本能力**只登记现状、只让缺口可检出**，⛔ **不补齐** hop / 时序模型（那属协议 FSM 语义设计，per 宪章归 ArchForge）。

⚠️ **层的区分（本能力最容易被误读的地方）**：
- **协议语义层**（本能力登记的对象）—— 8 个实例的引擎 ⇒ 现状**零** hop / 时序 API；
- **步进钩子** —— 引擎有 `tick()`（驱动入站分派 + egress 发送）⇒ ⛔ 那是**全局步进**，⛔ **不是**消息级时序模型；
- **载体 / 器件层** —— **已有** hop 与时延概念（flit 载体的 hop 计数字段 · responder 与桩缓存核的固定延迟参数）⇒ ⛔ 「无 hop / 时序」**仅对协议语义层成立**，⛔ **不得**表述为「全平台无 hop / 时序」。

来源：gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G10**（依赖 **G1** · G1 已于 2026-11-23 闭合）· 台账 Entry **#543**（立项授权）· 实施 change `cep-hop-timing-capability-declaration` · 形态照抄 I1（G1）的现成范式。

## Requirements

### Requirement: 协议语义层的 hop / 时序建模声明面

CEP SHALL 提供**协议语义层**的 hop / 时序建模**声明面**，并 SHALL 以**可机检的形态**报告每个已声明协议实例的现状。

#### Scenario: 声明面 SHALL 覆盖全部已声明实例

- **WHEN** 核对 `kHopTimingCapabilities` 的实例集与 `protocol_instances.json` 的实例集
- **THEN** 两者 SHALL **相等**（双向零 orphan：既无漏声明，也无多余声明）
- **AND** 每行 SHALL 有**非空** `basis`（判定依据）且 `id` SHALL 唯一

#### Scenario: 通用层 SHALL 不含协议名

- **WHEN** 扫描声明头 `include/tlm/cep/hop_timing_capabilities.hh`
- **THEN** SHALL **不**出现任何协议**类名**，SHALL **不** `#include` 协议引擎 / adapter 头
- **AND** 载体 / 器件层的既有事实 SHALL 按**角色**描述（⛔ 不按协议名描述）

### Requirement: 协议语义层的 hop / 时序 **现状锁**

声明面 SHALL 把「协议语义层**现状恒无** hop / 时序模型」锁成**可机检的现状**，而非散文。

#### Scenario: 声明为非 None ⇒ 必红

- **WHEN** 声明表中任一行的 `protocol_hop` 或 `protocol_timing` **≠** `None`
- **THEN** 契约闸 SHALL 判红（红因 `hop-timing-protocol-model-present`）
- **AND** 该改动 SHALL 被视为「**协议语义层已有** hop / 时序模型」⇒ SHALL 走**决策**并更新声明面，⛔ 不得静默长出

### Requirement: 引擎层 hop / 时序 API 的反向存在性

契约闸 SHALL 能检出「协议引擎头出现 hop / 时序 API」这一**现状变化**，使缺口从**不可检出**变为**可检出**。

#### Scenario: 引擎头出现 hop / 时序 API ⇒ 必红

- **WHEN** 任一 `include/tlm/cep/*_protocol_engine.hh` 在**去注释后**出现 hop / 时序 API token
- **THEN** 契约闸 SHALL 判红（红因 `hop-timing-engine-api-detected`）并**点名**该文件与 token
- **AND** `tick` SHALL **不**计入该模式词（它是**全局步进钩子**，⛔ 不是消息级时序模型）

#### Scenario: 编译期否定断言（5 个实例引擎）

- **WHEN** 编译期对 5 个实例引擎类型求值 `has_hop_api_v` / `has_timing_api_v`
- **THEN** 二者 SHALL 均为 `false`（失败签名 SHALL 含**实例 id 字面量**）
- **AND** 探针本身 SHALL 有**正控**（对**确实**暴露该类 API 的合成类型 SHALL 为 `true`）⇒ ⛔ 探针无牙 ⇒ 判据假绿

### Requirement: 载体 / 器件层既有事实的存在性

契约闸 SHALL 守住「载体 / 器件层**已有** hop 与时延概念」这一事实，使声明面**不因载体层演进而失真**。

#### Scenario: 载体 / 器件层 hop / 时延事实被删 ⇒ 必红

- **WHEN** flit 载体的 hop 计数字段 / responder 的固定完成延迟 / 桩缓存核的固定 snoop 延迟 中任一被移除
- **THEN** 契约闸 SHALL 判红（红因 `hop-timing-carrier-fact-missing`）
- **AND** 该改动 SHALL 要求**同步更新**声明面（⛔ 不得默默失真）

### Requirement: 诚实边界

声明面 SHALL 以**机器可读**形态登记其边界，使「无 hop / 时序」的表述**不越层**。

#### Scenario: 边界常量 SHALL 齐备

- **WHEN** 扫描声明头
- **THEN** SHALL 同时存在三个常量：`kTickIsSteppingHookNotTimingModel` · `kHopTimingAbsenceIsProtocolSemanticsOnly` · `kProtocolSemanticsHasNoHopOrTimingModel`
- **AND** SHALL 含「**仅对协议语义层**」与「**步进**」两处措辞

#### Scenario: 本能力 SHALL 不补齐缺口

- **WHEN** 核对本能力的边界声明
- **THEN** SHALL 明示「**只登记现状**、⛔ **不补** hop / 时序模型」（归 ArchForge）
- **AND** ⛔ SHALL NOT 表述为「全平台无 hop / 时序」

### Requirement: 与 G1 的正交性

本能力 SHALL 与 `cep-protocol-engine-capability-declaration`（G1）**正交**：同一事实**只**有一个真源。

#### Scenario: 声明表区段内 SHALL 无 G1 字段

- **WHEN** 扫描声明表的 struct / init 区段
- **THEN** SHALL **不**出现 `egress` / `in_flight` 字样（红因 `hop-timing-duplicate-source`）
- **AND** 本能力 SHALL **不**改动 `include/tlm/cep/engine_capabilities.hh`（⛔ 零真源重叠）
