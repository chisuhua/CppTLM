# cep-sparse-snoop-filter Specification

## Purpose
定义 `SparseSnoopFilter` 的**行为实现**契约:它是 CEP 平台上第 3 种 `SharerTracker` 行为实现,
落 `include/tlm/cep/sharer_tracker.hh`,**消费**已冻结的 caps / hazard 契约
(`ADR-CEP-04 §2` / `§4`),并为 `moesi_gpu` 的 `O→WRITEBACK` 投影损失提供**保守补偿**路径。

> **归属**:本能力**不服务** CXL 实例(CXL 的 peer 集平凡为空 ⇒ 其 `sharer_tracker` 为「不参与」,
> per `cep-cxl-nonsymmetric` CNA-03)⇒ 本能力服务 **`moesi_gpu`**(台账 §8.1 **R11**)。
>
> **⚠️ 语义纪律**:`SparseSnoopFilter` 的 caps 是 `CONSERVATIVE`(宁多探不漏探 = **不精确**)⇒
> 它只能给出「**可能**还有其他 sharer」的保守近似,**无法**精确恢复 `O` 态携带的
> 「**没有**其他 sharer」⇒ 补偿路径 SHALL 判**保守补偿**,SHALL **NOT** 声称等价恢复。

---

## ADDED Requirements

### Requirement: cep-sparse-snoop-filter

CEP SHALL 提供 `SparseSnoopFilter` 的**行为实现**:SHALL 落 `include/tlm/cep/sharer_tracker.hh` ·
SHALL 组合 `SharerTrackerIf` + `SnoopMechanics` · SHALL **消费**(不重推)`ADR-CEP-04 §2`/`§4` 的
caps 与 hazard 契约 · SHALL 具备 **CONSERVATIVE** 语义 · SHALL 为 `moesi_gpu` 的
`O→WRITEBACK` 合并损失提供**可执行**的保守补偿路径。

#### Scenario: SSF-01 —— caps / behavior SHALL **逐字段**等于 `ADR-CEP-04` 的冻结契约

- **WHEN** 核对 `SparseSnoopFilter` 的 `caps` 与 `behavior`
- **THEN** 子条件 ① SHALL 成立:`caps` SHALL 为
  `scope = SYSTEM` / `completeness = CONSERVATIVE` / `authority = GLOBAL` / `aggregation_at = HOME`
- **AND** 子条件 ② SHALL 成立:`behavior.hazard_under_contention` SHALL 为 `LookupHazard::STALL`
- **AND** 子条件 ③ SHALL 成立:`behavior.broadcast_fallback` SHALL 为 `true`
- **AND** 子条件 ④ SHALL 成立:以上 SHALL 与 `kTrackerTraits[SparseSnoopFilter]` **逐字段相同**
  (⇒ **不得**在本类内另写一份 caps 字面量)

#### Scenario: SSF-02 —— **CONSERVATIVE** 语义:SHALL **不**给出「无其他 sharer」的**精确**结论

- **WHEN** 查询一个地址的 sharer 视图
- **THEN** 子条件 ① SHALL 成立:filter **无该地址条目** ⇒ SHALL 返回 **`BROADCAST`**
  (SHALL **不**返回 `ANSWER_HERE` —— 无条目意味着「**可能**有别人」)
- **AND** 子条件 ② SHALL 成立:条目含**除自己以外**的 bit ⇒ SHALL 返回 `BROADCAST`
  或 `FORWARD_TO`(有脏持有者提示时)
- **AND** 子条件 ③ SHALL 成立:条目**确实存在**且**唯一**可能的 sharer 是脏持有者 ⇒ SHALL 返回
  `FORWARD_TO`(该持有者);可能的 sharer > 1 ⇒ SHALL 返回 `BROADCAST`
- **AND** 子条件 ③' SHALL 成立:SHALL **永不**返回 `ANSWER_HERE` ——
  `completeness = CONSERVATIVE` ⇒ filter **不能**回答「**没有**其他 sharer」⇒ 该回答只能由
  **EXACT** 目录给出(⇒ 此即本实现与 `FullMapDirectory` 的**可观测差异**)
- **AND** 子条件 ④ SHALL 成立:contention 在飞 ⇒ SHALL 返回 `LookupHazard::STALL`
- **AND** 子条件 ⑤ SHALL 成立:以上 SHALL 有**反向 fixture** 证明有鉴别力
  (构造「filter 说独占、实际另有 sharer」⇒ **必须因正确原因红** —— 漏探 = 正确性失败)

> **语义**:CONSERVATIVE = 宁多探不漏探。**漏探**是正确性失败,**多探**只是代价 ⇒
> 默认必须偏保守。

#### Scenario: SSF-03 —— `moesi_gpu` 的 `O→WRITEBACK` 补偿路径 SHALL 有**可执行**判据

- **WHEN** `moesi_gpu` 的转换把 `O` 合并投影为 `WRITEBACK`(丢失「没有其他 sharer」信息)
- **THEN** 子条件 ① SHALL 成立:SHALL **查询** SF 以取得保守结论
- **AND** 子条件 ② SHALL 成立:SHALL **登记**该结论(可观测)
- **AND** 子条件 ③ SHALL 成立:该路径 SHALL **判「保守补偿」**,SHALL **NOT** 声称「等价恢复」
- **AND** 子条件 ④ SHALL 成立:`moesi_gpu` 的既有测试 SHALL **0 回归**;
  若 `CONSERVATIVE` 语义导致 snoop 数变化 ⇒ SHALL **显式登记 + 重基线**(不得静默改期望值)

> **语义**:R11 降级的三条件(V-1 ∧ V-2 ∧ V-3)中的 **V-2** 就是本场景 ⇒
> 本场景不达成 ⇒ R11 **不得**降级(per `ADR-CEP-04` 的 `## Status Update(2026-11-01)`)。
