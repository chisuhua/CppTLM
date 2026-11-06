# cep-moesi-gpu-l4-e2e Specification

## Purpose
TBD - created by archiving change cpptlm-cep-moesi-gpu-l4-e2e. Update Purpose after archive.

## Requirements

### Requirement: cep-moesi-gpu-l4-e2e

`moesi_gpu` 实例 SHALL 具备**事务级 L4 驱动**:可被实例化(注册面)· 10 条 canonical 流**逐条**有「抵达」记账 · 本地协议事件 SHALL 走**表驱动迁移**并落到状态机 · 离开合并态(`O`)的迁移 SHALL 受**补偿**约束。

⚠️ **边界**:本 capability 只闭合「**实例化 + 记账 + 迁移 + 补偿**」;引擎的 10 个 `on_*` 按决策 M-6 **只记账**(零发射)⇒ ⛔ 本 capability **不**承诺「协议事件语义已实现」(那需要 UALink/其它协议的专用引擎)。

#### Scenario: MGE-01 —— 引擎可实例化(注册面)

- **WHEN** 短名 `MoesiGpuProtocolEngine` 注册进 ChStream 注册表,且拓扑声明该类型
- **THEN** 引擎 SHALL 可被实例化;`flow_events` 与 `line_count` 初值 SHALL 为 0

#### Scenario: MGE-02 —— 10 canonical 流逐条「抵达」记账

- **WHEN** 经 CohIf 边界的 **5** 条流被 `dispatch_cohif` 派发,其余 **5** 条被**直调** `on_*`
- **THEN** `flow_events[0..9]` SHALL **各为 1**;`unsupported_flow_rejected` SHALL 为 0

#### Scenario: MGE-03 —— 本地协议事件走表驱动迁移

- **WHEN** `submit_local_request(addr, from, event)`
- **THEN** 返回态 SHALL 等于载体表驱动的真值;`state_of(addr)` SHALL 与之一致;`adapter_transitions` SHALL 递增

#### Scenario: MGE-04 —— 离开合并态的迁移受补偿约束

- **WHEN** `from == O` 且事件离开 `O`,且**未接** sharer tracker
- **THEN** 迁移 SHALL **被拒绝**(返回原态 `O`);该地址 SHALL **不被登记**;`compensation_skipped` SHALL 递增;`compensation_queries` SHALL 为 0;`adapter_transitions` SHALL **不**递增
