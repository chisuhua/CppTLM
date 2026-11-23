# cep-protocol-engine-capability-declaration Specification

## Purpose

把「某个协议实例的协议语义由谁承载、承载到哪一层」从引擎头文件里的散文变成**机器可查的声明面**：`kEngineCapabilities` 逐实例登记 egress 形态，`EngineTypeTraits<T>` 逐引擎登记在飞事务建模与否并与 `HasInFlightApi<T>`（真实公开 API）**编译期交叉核对**，瞬态态以策略上限锁住，实例集与 `protocol_instances.json` 双向零 orphan。

本能力**只登记现状、只让缺口可检出**，⛔ 不补齐瞬态态 / 在飞 snoop 集合 / egress 机检（那属协议 FSM 语义设计，归 ArchForge 与架构裁决）。`egress` 字段当前**无机器判据**，由 `kEgressHasMachineCheck = false` 显式登记。

## Requirements

### Requirement: 协议引擎承载能力声明面

CEP 平台 SHALL 提供一个**协议中立**的承载能力声明面 `include/tlm/cep/engine_capabilities.hh`，其中：
(a) `kEngineCapabilities` 表 SHALL 为每个已在 `include/tlm/cep/protocol_instances.json` 声明的协议实例**恰有一行**；
(b) 每行 SHALL 含 `id`（逐字等于 JSON 的 `id` 键值）、`egress`（取值域 `ProtocolMessages` / `None`）、`basis`（非空）；
(c) 该头 SHALL NOT 出现任何具体协议引擎类名或 adapter 类名，且 SHALL NOT `#include` 任何 `*_protocol_engine.hh` / `*_adapter.hh`（协议差异只允许出现在协议头，per `include/tlm/cep/AGENTS.md` 铁律 2）。

#### Scenario: 声明面覆盖全部已声明实例

- **WHEN** 对 `kEngineCapabilities` 的 `id` 集与 `protocol_instances.json` 的 `id` 集求差
- **THEN** 两个方向的差集均为空（双向零 orphan）

#### Scenario: 通用层不含协议名

- **WHEN** 在 `engine_capabilities.hh` 中检索任一具体引擎类名 / adapter 类名
- **THEN** 零命中；且检索 `#include "*_protocol_engine.hh"` / `#include "*_adapter.hh"` 时零命中

### Requirement: 在飞事务建模的编译期交叉核对

对每个提供 `EngineTypeTraits<T>` 特化的引擎类型，平台 SHALL 以**编译期断言**保证：声明的 `in_flight`（`Modeled` / `NotModeled`）与该类型是否真实暴露公开 API `txn_in_flight` **一致**（通过 SFINAE 反射探测，⛔ 不依赖文本匹配）。

#### Scenario: 声明为 Modeled 但无 API ⇒ 编译失败

- **WHEN** 某引擎特化声明 `in_flight == Modeled` 而其类型未暴露 `txn_in_flight`
- **THEN** 编译失败，且编译输出命中该实例 id 的字面量签名

#### Scenario: 声明为 NotModeled 但暴露了 API ⇒ 编译失败

- **WHEN** 某引擎特化声明 `in_flight == NotModeled` 而其类型暴露了 `txn_in_flight`
- **THEN** 编译失败，且编译输出命中该实例 id 的字面量签名

### Requirement: 瞬态态策略上限

对**持有 `kTransientStateCount` 字段**的 adapter（`chi_rnf` / `mesif` / `moesi_gpu` / `tilelink_c` / `cxl_cache`），平台 SHALL 以编译期断言保证 `kTransientStateCount <= kMaxTransientStatesPerInstance`（= 1）。
该阈值的语义 SHALL 为「**现状锁**」——提升阈值属 architect 裁决事项，⛔ 不属于实现自由度。
`ualink_adapter` / `nvlink_adapter` 无该字段 ⇒ SHALL NOT 被本条约束覆盖。

#### Scenario: 任一 adapter 新增瞬态态 ⇒ 编译失败

- **WHEN** 某持字段 adapter 的 `kTransientStates` 被增加到超过阈值
- **THEN** 编译失败，且编译输出命中该 adapter 实例 id 的字面量签名

### Requirement: 引擎头文件集反向存在性

平台 SHALL 以一个闸断言 `include/tlm/cep/*_protocol_engine.hh` 的文件集合**恰为**已登记的 6 个引擎头（5 个实例引擎 + 1 个 `cohif_protocol_engine.hh`，后者是 `NullProtocolEngine<2>` 的可注册外壳、不承载协议语义、无实例特化）；出现此外的第 7 个或缺失其一 ⇒ 判定 FAIL。
该判据 SHALL 在报告中标明其已知局限（无法区分共用命名前缀的两个 `cxl` 实例），⛔ SHALL NOT 表述为「逐实例绑定」。

#### Scenario: 新增引擎头未更新声明 ⇒ 必红

- **WHEN** 在 `include/tlm/cep/` 下新增任一名为 `*_protocol_engine.hh` 的文件而 `kEngineCapabilities` 未同步
- **THEN** 闸判 FAIL，红因码 `engine-cap-new-engine-detected`

### Requirement: egress 无机器判据的诚实登记

平台 SHALL 以文件级常量 `kEgressHasMachineCheck` 显式登记「`egress` 字段当前**无**机器判据」这一事实；该常量 SHALL 为 `false`。
若将来引入 egress 的机器判据，SHALL 同时把该常量翻为 `true` 并补齐对应判据；⛔ SHALL NOT 在无判据的情况下声明 `true`。

#### Scenario: 无判据却声明有判据 ⇒ 必红

- **WHEN** `kEgressHasMachineCheck == true` 而仓内不存在任何 egress 机检载体
- **THEN** 判据 FAIL，红因码 `engine-cap-egress-claim-without-check`

### Requirement: 声明自洽谓词

平台 SHALL 提供纯函数 `engine_capabilities_are_consistent(list, n)`，对**传入的副本**判定：① 每行 `basis` 非空 ② `id` 唯一。
该谓词 SHALL NOT 读取任何全局状态（⛔ 以便测试传入变异副本做负控）。
⛔ 该谓词 SHALL NOT 承担「行数正确性」检查（行数正确性由双向零 orphan 判据承担）。

#### Scenario: basis 置空 ⇒ 谓词返回假

- **WHEN** 向谓词传入一份把某行 `basis` 置空的副本
- **THEN** 返回假

#### Scenario: id 重复 ⇒ 谓词返回假

- **WHEN** 向谓词传入一份含重复 `id` 的副本
- **THEN** 返回假
