# cep-full-green-run-evidence Specification (delta)

## Purpose

CEP 的一致性闸门(IC3)此前在**所有真实 E2E run** 上恒为 ⚪ **不适用** —— `memory_violations`
被装置登记为 `"not-collected"` ⇒ IC3 的三态判据(`deadlock_violations == 0 && memory_violations == 0`)
**永不可判** ⇒ B4 的证据链缺「**真实 run** 的通过态实例」这一环。

本 capability 把「**一次真实 run 的通过态实例**」这一**证据面**契约化,
并使其实例由**单一闸**机器守住。

本 capability **只约束证据的存在与可判性**,⛔ **不改变任何判据口径 / 阈值 / 预登记**
(`ADR-CEP-06` 的 IC3 三态与 retention 条款均不变)。

> **演进方式**:本 spec 经由 OpenSpec delta(**ADDED**)演进,**不直接改写**本文件。
> **规范载体**:`docs/architecture/07-coherence-exploration-platform.md` 的新 `## Status Update`
> **canonical 实现**:`scripts/test/cep_full_green_run_check.sh`

## ADDED Requirements

### Requirement: cep-full-green-run-evidence

CEP SHALL 具备**一次真实 run** 的一致性闸门(IC3)**通过态**实例,且该实例 SHALL 可机判。

#### Scenario: 违例类 SHALL 被实测而非登记为未采集

- **WHEN** 校验目标 run 的 `metrics.json`
- **THEN** `memory_violations` 与 `deadlock_violations` SHALL 是**实测数值**(⛔ **不是** `"not-collected"`)
- **AND** 两键 SHALL 位于 `measured` 内
- **AND** ⛔ SHALL NOT 通过「删除 `not_collected` 的登记」来制造 0 ⇒ 那是无产出者的**裸 0**

#### Scenario: 通过态 SHALL 以零违例为结果而非目标

- **WHEN** 目标 run 的两违例类实测完成
- **THEN** 两键实测值 SHALL 为 **0** ⇒ IC3 = **通过态**
- **AND** ⛔ 若实测值 **> 0** ⇒ SHALL **停止**并按**缺陷**另立处置 ⇒ ⛔ SHALL NOT 通过改判据 / 阈值 / 预登记使其变绿

#### Scenario: 证据 SHALL 来自真实 run

- **WHEN** 校验该通过态实例的来源
- **THEN** run SHALL 为**真实 E2E run** ⇒ ⛔ **合成 fixture SHALL NOT** 冒充真实 run
- **AND** 该区分 SHALL **可机判** ⇒ ⛔ SHALL NOT 以「人工声明其为真」作判据

#### Scenario: 证据 SHALL 可回溯且被机器守住

- **WHEN** 校验证据登记
- **THEN** 架构文档 SHALL 有**新 Status Update** 指向该 run(⛔ 不改正文)⇒ 冻结面走五步授权链
- **AND** objective 的 B4 证据行 SHALL **追加**(append-only)该实例
- **AND** 单一闸 SHALL 判:违例类已实测 · 实测为 0 · IC3 为通过态 · 来源非 fixture · 登记存在 ⇒ 并配**决定性变异**

#### Scenario: 判据 SHALL 声明其不可约残余

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录:① 一次通过态实例 **≠**「所有协议族 / 所有拓扑达成」
      ② `trace.h5` 不入库 ⇒「4 件套齐备」只能以 run 级形态成立 ③ 措辞 SHALL 限定为
      「**一次真实 run 的通过态实例**」
- **AND** ⛔ SHALL NOT 把「一次通过」表述为「全面达成」
