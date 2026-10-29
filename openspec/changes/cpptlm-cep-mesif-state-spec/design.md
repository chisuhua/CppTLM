# D-α —— MESIF 规约的**推导口径**(本 change 的核心决策)

## 决策

**MESIF 的 5 态 `M/E/S/I/F` 规约由本仓「本地推导」,每格标注依据 + 待追认状态;不等待 ArchForge 闭合 `25-nsa §4.4`。**

## 为什么是「本地推导 + 标注待追认」

| 方案 | 结论 | 理由 |
|---|---|---|
| **A′** 本地推导 + **逐格标注依据** + 逐格标注**待追认** | ✅ **采纳** | ① 队列 #9 由此**可解冻**(否则永远等一个开放问题);② **诚实**:每格都写明依据来源(ArchForge 原文 / 行业惯例 / 本仓推导)⇒ 读者能区分「有据」与「推导」;③ **可对齐**:ArchForge 闭合后逐格比对即可,差异即待追认项;④ 跨仓债**可见**(不是「假装已有规约」) |
| **B** 等 ArchForge 闭合 `25-nsa §4.4` 再做 | ❌ 否决 | `25-nsa §4.4` 是**选型开放问题**(`25-nsa:592` / `:611`),`ADR-SOC-25:283` 的返工清单尚未落地 ⇒ **无界阻塞**(per 队列 #8 同款机理) |
| **C** 直接照搬 MOESI 的 6×6 转换表,按同名字态对齐 | ❌ 否决 | 状态词表**不同**(6 态 `I/S/E/M/O/T` vs 5 态 `M/E/S/I/F`,per `32 号:213`)⇒ 同名态语义**未必同构**;MESIF 的 `F`(Forward = 单 Owner + 多 Sharer,per `25-nsa:598-600`)在 MOESI 词表里**无对应** ⇒ 照搬产出**杜撰**规约 |
| **D** 只写 5 态词表 + 一句「投影待定」 | ❌ 否决 | 词表**已在** ArchForge(`25-nsa:598`)⇒ 本仓重复它**零增量**;且队列 #9 需要的是**转换表**才有判据可写 |

## 推导口径(逐条依据)

| 规约元素 | 依据 | 依据强度 |
|---|---|---|
| 5 态 `M/E/S/I/F` 的**存在** | ArchForge `25-nsa:598` / `32 号:213` / `32 号:725` | **一手原文** |
| `F`(Forward)= 单 Owner + 多 Sharer | ArchForge `25-nsa:599-600` | **一手原文** |
| `F` → `WRITEBACK` 投影 | ArchForge `32 号:529` | **一手原文** |
| `F` 在 Owner 转移时**清空** | ArchForge `25-nsa:553` / `32 号:529` | **一手原文** |
| 目录条目含 `State(12 bits, MESIF 编码)` | ArchForge `25-nsa:539` | **一手原文** |
| `M/E/S/I` 的 4 值投影 | **本仓推导** —— 同名态与 MOESI 同义(`M`=Modified 脏独占 / `E`=Exclusive 干净独占 / `S`=Shared 多副本 / `I`=Invalid)⇒ 投影 `M→WRITEBACK` / `E→UNIQUE` / `S→READ` / `I→NONE` | **推导**(待追认) |
| 状态转换表 | **本仓推导** —— 以 MOESI 6×6(`09 号 §3.3`)为**结构模板**,按 MESIF 的 5 态与「Forward 语义」裁剪 | **推导**(待追认) |
| 写只在独占态合法 | ArchForge `25-nsa:552-554`(Remote Atomic 必须先查 Directory 确认 Owner 唯一)+ 同型先例 IHI0050 §B4.1(CHI) | **类比推导**(待追认) |

> ⚠️ **推导的边界**:上表标「推导」的每一格,在实现载体里都**必须**带 `basis` 字段(非空)+ `pending_uptick` 标记 ⇒ 判据 AC-6 逐格检查。

---

# D-β —— 可执行载体形态(与 `chi_rnf_adapter.hh` 同款)

`include/tlm/cep/mesif_state_spec.hh` 的形态:

- `enum class State { I = 0, S = 1, E = 2, M = 3, F = 4 }` —— **值序刻意**让 `E = 2` / `M = 3`(与 CHI 的 `UC=2` / `UD=3` 同款)⇒ `E + Wr → M` 的目标态恰为 **3** ⇒ 可**复用**队列 #6 的 AC-05 判据形态(输入是 POD,判据本体零改动)。
- `enum class Event { Rd, RdUnique, Wr, Upgrade, SnpShared, SnpUnique }` —— 与 CHI 的 6 事件**同构**(MESIF 是 snoop 型 + 目录型;目录查询是**另**一条路径,不属状态机转换)。
- `kStateTransition[5][6]` —— **表驱动**,每格附 `basis`。
- `perm_of(State, Perm&)` —— 5 态**全部**有投影;**瞬态无**(本规约**不声明瞬态** ⇒ AC-2 平凡成立,但载体**保留**瞬态列表以支持未来追认后扩展)。
- 三条编译期锁:① 转换表**完备**(每格非「未定义」)② 5 态投影齐备 ③ `E + Wr → M` 存在且秩升高。

---

# D-γ —— 判据实现位置(单一真源纪律)

**判据本体**放 `test/test_cep_mesif_state_spec.cc`(与 `test_cep_chi_rnf_subset.cc` 同款),**不**改 `adapter_schema_contracts.hh`(那是**实例面**的契约,不是**规约面**的)。⇒ 与队列 #8 的「判据本体零改动」纪律同构。

---

# D-δ —— 跨仓追认登记

规约载体里每格带 `basis` + `pending` 标记;另出一份**追认清单**(本 change 的交付物之一),逐格列出「ArchForge 侧需要确认/补写什么」⇒ ArchForge 侧的对齐提案承接(`ArchForge:docs/architecture/41-cep-alignment-debt-cleanup-proposal.md`(跨仓))。
