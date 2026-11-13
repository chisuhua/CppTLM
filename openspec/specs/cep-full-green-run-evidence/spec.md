# cep-full-green-run-evidence Specification

## Purpose
CEP 的**拓扑级(E2E 链)run** 此前恒为 ⚪ **不适用** —— `memory_violations`
被装置登记为 `"not-collected"`(且该 run 自陈 `not_an_experiment_run = true` /
`evidence_scope` 「⛔ 非一致性安全断言」)⇒ IC3 的三态判据
(`deadlock_violations == 0 && memory_violations == 0`)在拓扑链上**永不可判**。

本 capability 把「**一次拓扑级 E2E run 的通过态实例**」这一**证据面**契约化,
并使其实例由**单一闸**机器守住。

本 capability **只约束证据的存在与可判性**,⛔ **不改变任何判据口径 / 阈值 / 预登记**
(`ADR-CEP-06` 的 IC3 三态与 retention 条款均不变;`experimental/preregistration.json` 零 diff)。

> ⚠️ **口径(不得读反)**:**模型级**装置(H3 v2)已有真实 run 通过态实例
> (per `obj §6.2 B4` Entry #228 ③ + `docs/validation/2026-11-05-cep-b4-evidence-device.md` §0
> —— 「IC3 三态中该态**首个真实 run 实例**」;此前仅**合成 fixture F1** 覆盖)。
> ⇒ 本 capability **不**宣称「首次具备真实 run 通过态实例」,只覆盖**拓扑级**这一档。
> ⚠️ **本 capability 的证据强度弱于模型级**:不含负控 ⇒ ⛔ 不得称「检测力已证」。

> **演进方式**:本 spec 经由 OpenSpec delta(**ADDED**)演进,**不直接改写**本文件。
> **规范载体**:`docs/architecture/07-coherence-exploration-platform.md` 的新 `## Status Update`
> **canonical 实现**:`scripts/test/cep_full_green_run_check.sh`

## Requirements

### Requirement: cep-full-green-run-evidence

CEP SHALL 具备**一次拓扑级 E2E run** 的一致性闸门(IC3)**通过态**实例,
该实例 SHALL **可机判**、SHALL **可回溯**,且其 SHALL **具备证据资格**
(绑定已登记判据行 + 装置自陈允许作一致性安全断言证据)。

#### Scenario: 违例类 SHALL 被实测而非登记为未采集

- **WHEN** 校验目标 run 的 `metrics.json`
- **THEN** `memory_violations` 与 `deadlock_violations` SHALL 是**实测数值**(⛔ **不是** `"not-collected"`)
- **AND** 两键 SHALL 位于 `measured` 内
- **AND** SHALL 存在**样本活性见证**(产出侧登记被检测的内存操作样本计数,值 **> 0**)
  ⇒ ⛔ SHALL NOT 以「检测器已挂载但样本从未流入」的实测 0 充数
- **AND** ⛔ SHALL NOT 通过「删除 `not_collected` 的登记」来制造 0 ⇒ 那是无产出者的**裸 0**

#### Scenario: 通过态 SHALL 以零违例为结果而非目标

- **WHEN** 目标 run 的两违例类实测完成
- **THEN** 两键实测值 SHALL 为 **0** ⇒ IC3 = **通过态**
- **AND** ⛔ 若实测值 **> 0** ⇒ SHALL **停止**并按**缺陷**另立处置 ⇒ ⛔ SHALL NOT 通过改判据 / 阈值 / 预登记使其变绿

#### Scenario: 目标 run SHALL 具备证据资格

- **WHEN** 校验目标 run 是否可作一致性安全断言的证据
- **THEN** `simulation_config.json` 的 `hypothesis_id` SHALL 绑定**已登记**的假设行
  (⛔ 无登记行 ⇒ 报告层预登记判据④ 判红)
- **AND** 装置自陈 SHALL **不再**声明「非实验 run / 非一致性安全断言」⇒ ⛔ 仍带该自陈的 run SHALL NOT 计入
- **AND** ⛔ SHALL NOT 通过**扩张预登记 out-of-scope 集合**来换取「已实测」形态
  ⇒ 预登记 SHALL 保持零 diff

#### Scenario: 证据 SHALL 来自拓扑级 E2E run 且该区分可机判

- **WHEN** 校验该通过态实例的来源
- **THEN** 该 run SHALL 是**拓扑级 E2E run** ⇒ ⛔ **合成 fixture SHALL NOT** 冒充,⛔ **模型级装置实例 SHALL NOT** 冒充
- **AND** 该区分 SHALL **可机判** ⇒ 须有**拓扑链身份锚**(run 在对账清单中有条目,
  且其拓扑配置文件标识指向已登记的拓扑级配置)
- **AND** ⛔ SHALL NOT 以「人工声明其为真」作判据

#### Scenario: 证据 SHALL 可回溯且被机器守住

- **WHEN** 校验证据登记
- **THEN** 架构文档 SHALL 有**新 Status Update** 指向该 run(⛔ 不改正文)⇒ 冻结面走五步授权链
- **AND** objective 的 B4 证据行 SHALL **追加**(append-only)该实例,⛔ 且该追加语句 SHALL 携带**拓扑级**限定词
- **AND** 该 run 的两件证据文件 SHALL **入库**且在对账清单中有条目 ⇒ 双向对账闸 SHALL 绿
  (⛔ 仅入库而不登记 ⇒ 零 orphan 判红)
- **AND** 单一闸 SHALL 判:违例类已实测 · 有样本活性见证 · 实测为 0 · IC3 为通过态 · 来源为拓扑级 E2E run · 登记存在 ⇒ 并配**决定性变异**

#### Scenario: 判据 SHALL 声明其不可约残余

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录:① 一次拓扑级通过态实例 **≠**「所有协议族 / 所有拓扑达成」
      ② `trace.h5` 不入库 ⇒「4 件套齐备」只能以**运行期**形态成立 ③ 措辞 SHALL 限定为
      「**一次拓扑级 E2E run 的通过态实例**」 ④ **模型级 ≠ 协议级** —— 该 0 SHALL NOT 被表述为
      「已证明协议层无死锁 / 无违例」 ⑤ ⛔ SHALL NOT 称「检测力已证」(本能力不含负控)
- **AND** ⛔ SHALL NOT 把「一次通过」表述为「全面达成」
