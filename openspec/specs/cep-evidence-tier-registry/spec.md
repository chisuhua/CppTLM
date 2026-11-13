# cep-evidence-tier-registry Specification

## Purpose
CEP 一致性证据存在**三个强度档位**(`ADR-CEP-20` 决策 6):档 0 = 拓扑级 E2E 装置(**样本通路活性**,⛔ 非语义判据)· 档 1 = 模型级 `ConsistencyCompletionModel`(**语义判据**)· 档 2 = 协议级引擎(**协议语义**)。但:

- **跨档并列以机器可读形式发生** —— `test/cep_e2e_runs.jsonl` 3 条中**唯一** `"ic3_verdict": "✓ 通过"` 是**档 0**;`ic3_verdict` 三态词表**不分档**、产物内无 tier 字段;且 `evidence_scope`(唯一可能承载档位词的字段)**不在**对账闸的必填键内。
- **档位阶梯不在总纲 / 手册** —— `07 号` 与 `docs/guide/cep-usage.md` 均无档位阶梯与档位声明义务。
- **生产喂料映射未被任何判据见证** —— 存量负控(`test/test_cep_experiment_harness.cc:87-130` 等 5 处)**不经过** `test/test_cep_stub_cache_emit.cc:120-131` 的喂料循环。

本 capability 把「**证据档位登记**」这一**读法面**契约化,并补上**生产喂料映射的 data-value 可达性见证**。

本 capability **只约束档位的登记与读法**,⛔ **不改变任何判据口径 / 阈值 / 预登记**,⛔ **不把档 0 的 0 升格为语义判据**,⛔ **不产出 SWMR 检测力见证**(喂料通路结构不可达 —— 见 Scenario 3)。

> **演进方式**:本 spec 经由 OpenSpec delta(**ADDED**)演进,**不直接改写**本文件。
> **规范载体**:`docs/architecture/07-coherence-exploration-platform.md` 的新 `## Status Update` + `docs/guide/cep-usage.md` §12.9
> **canonical 实现**:`test/cep_e2e_run_tiers.json` + `scripts/test/cep_e2e_reconcile_check.sh`

## Requirements

### Requirement: cep-evidence-tier-registry

CEP SHALL 为每个已入库的对账 run **登记唯一档位**,并 SHALL 使**档 0** 条目的档位声明**可机判**;同时 SHALL 为**生产喂料映射**的 data-value 可达性提供**正负配对**的见证。

#### Scenario: 每个 run SHALL 有且仅有一个档位

- **WHEN** 校验 `test/cep_e2e_runs.jsonl` 的每一条目
- **THEN** 该条目 SHALL 在档位登记表中有**且仅有 1 个**档位 ⇒ ⛔ SHALL NOT 缺档位
- **AND** 档位登记表中的每个 run 标识 SHALL 存在于对账清单(**反向零 orphan**)⇒ ⛔ SHALL NOT 出现**僵尸档位登记**
- **AND** ⛔ SHALL NOT 通过改写 `test/cep_e2e_runs.jsonl` 来「补齐」档位(该文件是 append-only 台账)⇒ 档位 SHALL 走**旁挂表**

#### Scenario: 档 0 条目 SHALL 携带档位声明

- **WHEN** 校验一个**档 0** 条目
- **THEN** 其 `evidence_scope` SHALL 含档位词(`档 0` 或 `拓扑级`)⇒ ⛔ SHALL NOT 只在 ADR 内声明档位
- **AND** `evidence_scope` SHALL 是**必填键** ⇒ 缺该键的条目 SHALL 以**独立红因**判红(⛔ SHALL NOT 退化为无签名失败)

#### Scenario: 喂料映射 SHALL 以正负配对见证 data-value 可达性

- **WHEN** 校验生产喂料映射(`test/test_cep_stub_cache_emit.cc` 的喂料循环)的 data-value 可达性
- **THEN** SHALL 存在**同一输入、唯一差异**的正负配对:仅切换 `read` 的观测值(旧 / 最新)⇒ 两次 `data_value` 计数差 = 违例数
- **AND** 该见证 SHALL 调用**与生产逐字相同**的喂料路径(⛔ SHALL NOT 绕过映射直接调检测器)
- **AND** ⛔ SHALL NOT 以两段不同代码充当「注入」

#### Scenario: SWMR SHALL 登记为结构不可达而非已见证

- **WHEN** 校验档 0 的 SWMR 侧
- **THEN** SHALL 显式登记:喂料通路对写事件**只调合法写路径** ⇒ **任何**事务流都只能产出 `swmr == 0` ⇒ **SWMR 违例在喂料通路结构不可达**
- **AND** ⛔ SHALL NOT 声称「SWMR 检测力已见证」;⛔ SHALL NOT 把该登记写成「已闭合的弱点」

#### Scenario: 档位阶梯 SHALL 在总纲与手册可读,且其语义 SHALL 声明为人工评审

- **WHEN** 校验档位阶梯的载体
- **THEN** `07 号` SHALL 有**新 Status Update** 承载三档阶梯 + 档位声明义务 + **跨档聚合禁令**(⛔ 不改正文);`docs/guide/cep-usage.md` SHALL 有独立教学节(⛔ 不塞进入结义务清单)
- **AND** ⛔ SHALL NOT 声称「阶梯内容已由机器判据保证」⇒ 格式闸(索引/引用/链接)全绿 SHALL NOT 被读成语义正确
- **AND** ⛔ SHALL NOT 把档 0 的通过态与档 1 / 档 2 的强度并列引用

#### Scenario: 判据 SHALL 声明其不可约残余

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录:① 档 0 的**语义空洞**不因本能力改变 ② **SWMR 侧无见证**(结构不可达)③ 档位词表为**真源单点**(脚本常量;文档侧同词但无机器一致性)④ `cep_dod_reading_check` 的 S6 扫描面**不含**新增 SU(「附录 ·」开头)⇒ 该 SU 内的禁表述无闸可抓 ⑤ **档 2 未排期** ⇒ 强度天花板不变
- **AND** ⛔ SHALL NOT 把「档位登记已落地」表述为「一致性证据已增强」
