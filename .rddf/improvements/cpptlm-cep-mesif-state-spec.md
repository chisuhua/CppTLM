---
name: cpptlm-cep-mesif-state-spec
priority: P1
phase: phase-10
主题: MESIF 状态机规约 + 5→4 perm 投影表 —— 补齐 ArchForge 侧两处「全仓 0 命中」缺口(队列 #9 `cpptlm-cep-mesif-adapter` 的规约层前置)
依赖:
  - ADR-CEP-05
  - ADR-CEP-12
  - ADR-CEP-04
  - ADR-CEP-02
状态: 🟡 **已转正**(2026-10-29 —— change 目录已建;KPI 并发 **0/3 → 1/3**,per Entry #97)
created: 2026-10-29
---

# improvement: cpptlm-cep-mesif-state-spec

> **状态**: 🟡 **已转正**(2026-10-29,Entry #97)· **change 目录已建**
> **队列**: `obj-cep-platform §4.0` **队列 #9**(`cpptlm-cep-mesif-adapter`)的**第 0 个 change**(规约层)· **不新增队列行** —— 一个队列行可承载多个 openspec change / capability(per 队列 #3 / #4 / #8 先例)⇒ **顺序 / 季度口径不变**(五处同改不触发)
> **与队列 #9 的分工**:本 change = **规约层**(状态机 + 投影表的**口径来源**与**推导依据**);队列 #9 `cpptlm-cep-mesif-adapter` = **实现层**(adapter + 引擎 + 装配)。⇒ 本 change 是队列 #9 的**前置**,不是它的替代。
> **关联 ADR**: `ADR-CEP-05`(Stub 只暴露稳定状态 + `perm` 4 值投影)· `ADR-CEP-12`(实例声明 schema + 权威分区/迁移)· `ADR-CEP-04`(SharerTracker caps)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)
> **估时**: 4 周(规约推导 + 判据 + 反向 fixture + 跨仓追认登记)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1,per `obj §5.4`)

---

## Why

队列 #9 `cpptlm-cep-mesif-adapter` 的 **R-1 阻塞已于 2026-10-29 裁决落地**(Entry #96:新增第 6 种 SharerTracker 实现 `HierarchicalDirectory` + `mesif` 声明权威迁到本仓)。**但 R-1 只解了「目录形态怎么表达」,没解「协议语义从哪来」** —— 两个**独立**缺口经 ArchForge 全仓检索确认**零命中**:

| # | 缺口 | 证据(ArchForge 全仓检索,2026-10-29) |
|---|------|------------------------------|
| **G1** | **MESIF 状态机规约不存在** | 只有 **5 态词表** `M/E/S/I/F`(`25-nsa-hardware.md:598` / `32 号:213` / `32 号:725` / `25-nsa:539` 的目录条目位域);**无**状态转换表 / 事件表 —— 检索 `state_transition` / `状态转换表` / `transition table` / `状态机` 在 `25-nsa-hardware.md` 内 **0 命中**;`25-nsa §4.4` 至今仍标「**选型待讨论**(开放问题)」(`25-nsa:592` / `:611`)。**对照**:`moesi_gpu` 侧有完整 `uint8_t state_transition[6][6]`(`09 号 §3.3:140-157`) |
| **G2** | **MESIF 的 5→4 `perm` 投影表不存在** | 唯一的投影表是 **MOESI/GPU 的 6 态 → 4 值**(`32 号 §7.2:582-597`);MESIF 侧**仅 1 条**单句(`32 号:529`:`F` 态 → `WRITEBACK`),`M/E/S/I` 四态的对应关系**无任何原文**;检索 `mesif.*projection` / `mesif.*投影` / `5.*->.*4.*perm` **0 命中** |

**为什么不能直接「照 MOESI 推」**:MOESI 的 6 态与 MESIF 的 5 态**状态词表都不同**(6 态 `I/S/E/M/O/T` vs 5 态 `M/E/S/I/F`,per `32 号:213` 的「冲突点 1」)⇒ 同名态的**语义**未必同构(如 MESIF 的 `F`(Forward)= 单 Owner + 多 Sharer,per `25-nsa:598-600`,在 MOESI 词表里**没有对应态**)。⇒ 照搬会产出**看似规约、实则杜撰**的转换表。

**为什么不能等 ArchForge 补**:ArchForge 侧的 `25-nsa §4.4` 是**开放问题**态,`ADR-SOC-25` 的返工清单(`ADR-SOC-25:283`)要求它「收敛为 MESIF/mesif 实例」但**尚未落地**;且 §4.4 本身是**选型开放问题** ⇒ 跨仓无界阻塞(per 队列 #8 的同款机理)。

**为什么必须是独立 change 而不是队列 #9 的一部分**:队列 #9 是**实现层**(adapter + 引擎 + 装配 + 判据),而本 change 交付的是**规约层的口径来源与推导依据** —— 二者的**验收物不同**(规约表 + 推导依据 + 跨仓追认登记 vs 可运行插件)。混在一起 ⇒ 规约的**待追认性质**会被实现的「已落地」掩盖 ⇒ 跨仓债失去可见性。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `openspec/specs/cep-mesif-state-spec/spec.md` | **MESIF 状态机规约 + 5→4 投影表**的契约(capability `cep-mesif-state-spec`)—— 状态集合 / 事件集合 / 转换表 / 投影表 / **推导依据** / **待追认标记** |
| `include/tlm/cep/mesif_state_spec.hh` | 规约的**可执行载体**(与 `chi_rnf_adapter.hh` 同款形态:表驱动 + 编译期锁)—— 使规约**可被判据检查**,而非只存在于文档 |
| `test/test_cep_mesif_state_spec.cc` | 规约判据 + **反向 fixture** + 正控 |

### 修改

| 文件 | 改什么 |
|------|--------|
| `openspec/specs/cep-chi-rnf-subset/spec.md` | **MODIFIES** —— CR-06 ③ 的措辞「本仓 SHALL **不**新增 SharerTracker 实现」与 R-1 裁决(新增第 6 种 `HierarchicalDirectory`,per `ADR-CEP-04 §13`)**冲突** ⇒ 改为「『不参与』**不是**一种实现」(**语义**不变,去掉与行数耦合的措辞) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | **队列 #9 行**登记「第 0 个 change(规约层)」+ 状态列更新(§4.2 估时列同步)—— **不新增队列行** ⇒ 顺序 / 季度口径不变 |
| `.rddf/roadmap/phases/phase-10.md` · `.rddf/roadmap.md` · `AGENTS.md` | KPI 当前态 **0/3 → 1/3**(per「五处同改」规则) |

### 边界(**零改动**)

- `protocol_instance.schema.json`(**M1 冻结面**)**零改动** —— 本 change 不新增 schema 字段。
- `adapter_schema_contracts.hh` **零改动** —— 判据本体不动。
- `protocol_instances.json` 的 `mesif` 声明 **零改动** —— R-1 落地时已改毕(Entry #96);本 change 只补**规约**。
- `CohIfBundle` **零改动** · `perm` 仍 4 值 · `CATEGORY_*` 仍 3 类。

---

## Non-Goals

- ❌ **不**实现 `MesifAdapter` / `MesifProtocolEngine` —— 那是队列 #9 的**实现层**。
- ❌ **不**替 ArchForge 决定 MESIF 协议选型 —— `25-nsa §4.4` 的开放问题由 ArchForge 闭合;本 change 只保证「**即使** ArchForge 闭合前,本仓已有一份**显式标注待追认**的规约」。
- ❌ **不**修改 ArchForge 仓的任何文件 —— 跨仓对齐由 ArchForge 侧提案承接(见 `ArchForge:docs/architecture/41-cep-alignment-debt-cleanup-proposal.md`(跨仓))。
- ❌ **不**扩 `perm` 4 值锁 —— MESIF 的 5 态必须**投影**进现有 4 值,不得新增第 5 值(per `ADR-CEP-02` CEN-Ib11)。
- ❌ **不**引入新的 SharerTracker 实现 —— R-1 已在 `ADR-CEP-04 §13` 裁决完毕(第 6 种 `HierarchicalDirectory`),本 change 只消费它。

---

## Acceptance

| # | 判据 | 可执行形态 |
|---|------|-----------|
| **AC-1** | MESIF **5 态**的**每一态**都有 4 值 `perm` 投影(无多、无少);`I → NONE` | `test_cep_mesif_state_spec.cc` 逐态断言 |
| **AC-2** | **瞬态无投影** —— 规约若声明瞬态,其投影必为空(per `INV-CEP-05-IC1`) | 同上 + 反向 fixture(给瞬态加投影 ⇒ 红) |
| **AC-3** | 转换表**完备** —— 每个(态 × 事件)组合都有定义(无空洞) | 同上;反向 fixture(挖一格 ⇒ 红) |
| **AC-4** | **写只在独占态合法**(与 CHI 的 IHI0050 §B4.1 同型)⇒ 共享态上的写**状态不变** | 同上;反向 fixture(共享态写 ⇒ 状态突变 ⇒ 红) |
| **AC-5** | `perm` 秩**单调** —— 升级路径上的秩不降(UNIQUE → WRITEBACK 严格升高) | 同上;反向 fixture(秩下降 ⇒ 红) |
| **AC-6** | 规约的**每一格**都带**推导依据**(本地推导 / ArchForge 原文 / 行业惯例)且**标注待追认状态** | 同上:每格 `basis` 非空;反向 fixture(清空 basis ⇒ 红) |
| **AC-7** | **正控**:`chi_rnf` / `moesi_gpu` 的既有投影**零回归** | 既有 `[cep][chi-rnf]` / `[cep][moesi-gpu-adapter]` 标签全绿 |
| **AC-8** | **CR-06 ③ 措辞修正**:`cep-chi-rnf-subset` 的 main spec 不再写「不新增 SharerTracker 实现」 | spec 文本断言 + `openspec validate --strict` rc=0 |

---

## Capabilities

| capability | 动作 | 内容 |
|---|---|---|
| `cep-mesif-state-spec` | **ADDED** | MESIF 状态机规约 + 5→4 `perm` 投影表 + **推导依据** + **待追认标记** |
| `cep-chi-rnf-subset` | **MODIFIES** | CR-06 ③ 的措辞修正(去行数耦合) |

---

## Impact

- **冻结面**:`openspec/specs/cep-chi-rnf-subset/spec.md` 是**已归档 change 合并出的 main spec** ⇒ 属**真源层** ⇒ 改动走 **MODIFIES** 流程(本 change 的 delta spec 声明)+ 归档时合并;**不进** M1 冻结集(冻结集只含 Stage 0 收口时的产物快照,per Entry #90 / #96 先例)。
- **跨仓债**:规约的每一格都标注**待追认** ⇒ ArchForge 侧闭合 `25-nsa §4.4` 后,本仓只需**逐格比对**即可对齐(对齐提案见 ArchForge `ArchForge:docs/architecture/41-cep-alignment-debt-cleanup-proposal.md`(跨仓))。
- **队列关系**:本 change(#15)**不解除**队列 #9 的「余 2 项缺口」登记 —— 它**正是**那 2 项缺口的处置载体 ⇒ 归档后队列 #9 的该登记项可销号。
- **KPI**:Stage 1 = **1 slot**(0/3 → 1/3);Stage 1 计数 10 → 11,合计 14 → 15。
