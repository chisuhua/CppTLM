---
name: cep-invariant-modeling-batch2
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-13
  - ADR-CEP-12
状态: "🟡 **已立项 · 阻塞于 user ratify**(2026-11-20 · planner 起草 · 关联登记册 §6.3 归属表 I-6 · 前置 = Q-4 裁决 + schema 13→14 + `ADR-CEP-12` 联动修订 + user ratify)"
created: 2026-11-20
---

# cep-invariant-modeling-batch2: 2 条守门 invariant 的建模（批 2 · 跨 change 规格修订）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文**（`ADR-CEP-12` 只能**追加式**修订，⛔ 不改原文）· ⛔ **新增 invariant 数 = 0**（批 1 已把 `⭭-unmodelled` 从 5 降到 2；本批把最后 2 条变成有判定级载体 ⇒ 仍 66 条 / 编号位 67）· ⛔ **`INV-CEP-04-IC4` 永久跳空** · ⛔ **判据语义由 architect 裁决给定** · ⛔ **schema 13→14 与 `INV-CEP-12-IC1` 联动修订属规格修订面，须独立裁决 + user ratify**。

## 架构依据

批 1 之后的最后 2 条 `⭭-unmodelled`（`test/cep_invariant_deferrals.json`）：

| id | 缺口 | 触发条件原文 | 代价 |
|---|---|---|---|
| `INV-CEP-13-IC1` | 「『采用该公理为判据』**无机器可检声明项**；输入侧 vocabulary 与 `INV-CEP-12-IC1` **共享** ⇒ 不计专属证据；运行期待 litmus」 | 「`ADR-CEP-13` 决策 5 ① 的剩余转写（组合判据成为结构化声明项）+ **litmus change 立项**」 | 需 **litmus change**（运行期验证一次公理可被证伪）|
| `INV-CEP-13-IC3` | 「adapter 提前 ack 行为成为实例声明项」 | 「触发 = adapter 提前 ack 行为成为声明项；代价 = **schema 13→14** + `INV-CEP-12-IC1` 联动修订 + `ADR-CEP-12` **追加式**修订」 | 需 **schema 版本递增** + 跨 invariant 联动 |

⚠️ 这两条**与批 1 不同型**：批 1 的三条是「**载体面**缺失」（工程可做），批 2 的两条明确要求「**规格面修订**」（schema + ADR 联动 + litmus change）⇒ **审批链不同** ⇒ 拆批是必要的，不是便利。

**与 spec 的关系**：`include/tlm/cep/protocol_instance.schema.json` 是**冻结面成员**（`FROZEN_GLOBS`）⇒ schema 13→14 **触五步链**。

## 范围

- **In-Scope**：
  - `INV-CEP-13-IC1` 的载体（依 litmus change 的形态）
  - `INV-CEP-13-IC3` 的载体（依 schema 13→14 后的实例声明项）
  - `include/tlm/cep/protocol_instance.schema.json` —— 13→14（**冻结面成员** ⇒ 五步链）
  - `include/tlm/cep/protocol_instances.json` —— 实例声明项新增（⛔ **不在**冻结集）
  - `docs/adr/ADR-CEP-12.md` —— **追加式**修订（⛔ 原文不改）
  - `test/cep_invariant_deferrals.json` —— 移除这 2 条（⇒ `⭭-unmodelled` 数 2 → 0）
- **Out-Scope**：
  - ⛔ `INV-CEP-13-IC1` 的 **litmus change 本身**（⇒ 独立 change，本单元只消费其产物）
  - ⛔ `INV-CEP-12-IC1` 的语义改动（**只联动，不改语义**）
  - ⛔ 批 1 的 3 条（⇒ `cep-invariant-modeling-batch1`）
  - ⛔ `B3b-2` 计数 / 档 2 扩面

## Why

**不做的代价**：这 2 条是 `66` 条里**最后**的 `⭭-unmodelled` ⇒ 不建模 ⇒ `⭭-unmodelled` 恒 > 0 ⇒ 「66 条全部有判定级载体」永远不成立 ⇒ §6.2 S1-1 无法闭合 ⇒ 平台在 invariant 维度永久停在 61/66。

**为什么现在单独立项**：批 1 落地后，「61 → 64」的收益已经拿到；剩这 2 条的**边际成本远高于边际收益**（要动冻结面 schema + 已签批 ADR + 立 litmus change）⇒ **单独立项 + 单独立审批**，避免把一个高风险规格修订绑在一个低风险实现单元上。

**为什么不动 `ADR-CEP-12` 原文**：ADR 不可变（只追加 SU）⇒ `INV-CEP-13-IC3` 的触发条件本身就写着「`ADR-CEP-12` **追加式**修订」⇒ 形态合规；但**追加什么内容**属架构决策 ⇒ 须裁决。

## What Changes

| # | 变更 | 形态 / 约束（**具体判据由裁决给定**）|
|:--:|---|---|
| 1 | **litmus change 立项** | `INV-CEP-13-IC1` 的触发条件明写「litmus change 立项」⇒ 需一个**能证伪该公理**的运行期 change；⛔ 该 change 的设计属独立裁决（⛔ 本单元不代拟） |
| 2 | **schema 13→14** | `include/tlm/cep/protocol_instance.schema.json` —— 新增「adapter 提前 ack 行为」实例声明项；⚠️ **冻结面成员** ⇒ 五步链（授权 Entry 先落 → 产物 commit → `--update --authorized-by` → 清单单独 commit → 无参复验） |
| 3 | **`INV-CEP-12-IC1` 联动** | 该 invariant 的载体须**同步**纳入新 schema 版本（**只联动，不改语义**）⇒ 判据载体映射表重生成 |
| 4 | **`ADR-CEP-12` 追加式修订** | 追加一段说明新声明项与其判据形态；⛔ **原文逐字不改**；⛔ **不新增 invariant 定义** |
| 5 | **`protocol_instances.json` 实例声明项** | 新增字段（⛔ 该文件**不在**冻结集 ⇒ 不触五步链） |
| 6 | **deferrals 移除 2 条** | `test/cep_invariant_deferrals.json` —— `deferrals` 长度 2 → **0**；⚠️ 本单元**必须**在批 1 完成后（批 1 后该文件恰有 2 条 ⇒ 无歧义） |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」。

- [ ] **AC-1（最后 2 条**处置**落地 —— ⛔ 不是「2 条建模落地」）**：`python3 scripts/test/cep_invariant_map_check.py` rc=0 且输出「`⭭-unmodelled` 数 == **0**」；`test/cep_invariant_deferrals.json` 的 `deferrals` 长度 == **0**；⚠️ 闸判据 ③ 仍绿（载体 ∪ ⭕ == 66 ⇒ 载体须 == **66**）。⚠️ **处置 ≠ 建模**（per metis B4/B5）：其中 `INV-CEP-09-IC2` 的载体是**超出范围的负断言**（grep 恒成立 ⇒ 不是恒真占位，但**不等于**该 invariant 已被语义判定）⇒ ⛔ **不得**表述为「66 条全部可判」
- [ ] **AC-2（66/66 全有**载体** —— ⛔ 不是「终态达成」）**：`test/cep_invariant_assertion_map.json` 重生成后 `carriers` 长度 == **66**；⚠️ `INV-CEP-04-IC4` **不在**其中（永久跳空 ⇒ 66 条已定义 + 1 跳空 = 67 编号位）。<br>⛔ **禁止用负断言关闭 66/66**（per metis B5）：`INV-CEP-09-IC2` 的载体是「grep 命中数 == 0」型**负断言**（超范围断言，语义上**不算达成**）⇒ **「66/66」只能读作「66 条全部有载体」，⛔ 不得读作「66 条全部可判 / 全部达成」**；⚠️ 由此产生一条**跨单元约束**：`obj §0 DoD 7` 现写「66 条守门 invariant **机器检查 100% 覆盖**」⇒ 该措辞在 I-6 落地后会**看起来被验证为真** ⇒ **Q-2（DoD 7 / B2 口径修复面判定）必须先于本单元完成**，⛔ 否则制造 S0 |
- [ ] **AC-3（schema 版本递进的机器面）**：schema 13→14 后，`scripts/test/cep_decl_alignment_check.sh` + `test/test_cep_schema.cc` 全绿；⚠️ 反 fixture：把 schema 版本改回 13 ⇒ **按签名红**（版本一致性断言）⇒ 还原绿
- [ ] **AC-4（冻结面五步链 ×1）**：`cep_freeze_check.sh` 的 `artifact_count` **66 → 67**（新增 1 个 `protocol_instance.schema.json` 已是冻结成员 ⇒ 实际**不变** ⇒ 判据 = 冻结产物数**逐字节不变**）；`--update --authorized-by "Entry #N"` **执行一次** ⇒ 清单**单独 commit** ⇒ 无参复验 rc=0；⚠️ **台账与清单不得合并提交**
- [ ] **AC-5（ADR 追加式 · 原文零改动）**：`git diff` 对 `docs/adr/ADR-CEP-12.md` 的改动**只增行**（既有行 diff 行数 == **0**）；⚠️ 反 fixture：改 ADR 原文任一既有行 ⇒ 判红
- [ ] **AC-6（`INV-CEP-12-IC1` 只联动不改语义）**：该 invariant 的 `reason` / `trigger_condition` / `ledger_entry` **逐字未变**（`git diff` 对其载体块的语义行零改动）⇒ 判据 = 该 id 的判据措辞 diff 行数 == **0**
- [ ] **AC-7（litmus 证据可追溯）**：`INV-CEP-13-IC1` 的载体须引用 litmus change 的**归档目录 + case 名**；⚠️ litmus change **未归档** ⇒ 本单元**不得**开工（AC-1 前置）⇒ 判据 = `ls openspec/changes/archive/ | grep -c <litmus>` ≥ 1。<br>⚠️ **循环前置已拆解**（per metis B6）：本单元的**前置不是**「schema 13→14 已 ratify」（那恰是本单元的**交付物** ⇒ 自我前置）⇒ 前置改为 ① **Q-4 裁决含 schema 13→14 的形态**（字段名 / 取值域 / 联动边界 —— 只裁形态，不裁成果）② **litmus change 已归档**（由登记册队列 2 的 **I-6a** 承接，⛔ 本单元不产 litmus change）|
- [ ] **AC-8（范围纪律 · 零越权）**：`git diff --name-only` **不含** `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/architecture/**`；⚠️ `include/tlm/cep/protocol_instance.schema.json`（冻结面）**允许**改但须走 AC-4 五步链
- [ ] **AC-9（0 回归）**：`ctest` 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无新增 invariant） | — | ⛔ **新增 invariant 数 = 0** —— 本单元只把**已定义**的 2 条变成有判定级载体；⚠️ 但 **schema 版本递增（13→14）属契约面变更** ⇒ 若 spec 侧需同步，须由 builder 在 delta spec 中体现（⛔ 不在 architect 域预判） |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | 2 个载体文件（路径由裁决给定）· litmus change（**独立 change**，本单元只消费）· 台账 Entry（追加） |
| **修改文件** | `include/tlm/cep/protocol_instance.schema.json`（**冻结面**）· `include/tlm/cep/protocol_instances.json` · `docs/adr/ADR-CEP-12.md`（**只增行**）· `test/cep_invariant_deferrals.json`（**只删 2 条**）· `test/cep_invariant_assertion_map.json`（重生成） |
| **冻结面** | `include/tlm/cep/protocol_instance.schema.json` **已在** `FROZEN_GLOBS` 内 ⇒ 改动 ⇒ **一次** `--update --authorized-by` ⇒ 冻结产物数**不变**（该文件已是成员）⇒ 清单**单独 commit** |
| **闸** | `cep_invariant_map_check.sh` 判据 ③ 口径不变（载体 ∪ ⭕ == 66）· `⭭-unmodelled` 2 → 0 · `cep_decl_alignment_check.sh` 需适配 schema 版本 |
| **KPI** | **0/3 → 1/3** ⇒ 归档 ⇒ **0/3**（⚠️ litmus change 另占 1 slot ⇒ 串行 ⇒ KPI 峰值 **2/3**，仍在 ≤3 内） |
| **风险** | ① schema 版本递增牵动既有 schema 断言 ⇒ AC-3 兜底 ② `ADR-CEP-12` 追加式误改正文 ⇒ AC-5 兜底 ③ litmus change 未归档 ⇒ 本单元被 AC-7 卡住 ④ `INV-CEP-12-IC1` 语义被顺带改动 ⇒ AC-6 兜底 ⑤ 冻结面五步链漏步（台账与清单合并提交）⇒ AC-4 兜底 |

## 待裁

⛔ **本单元被三重前置阻塞**（⛔ 任何一项未落 ⇒ **不得开工**）：

| # | 前置 | 归属 |
|:--:|---|---|
| 1 | **litmus change 立项 + 归档**（`INV-CEP-13-IC1` 的触发条件明写）| architect 裁决 + 独立 change |
| 2 | **schema 13→14 的形态**（新增声明项的字段名 / 取值域 / 与 `INV-CEP-12-IC1` 的联动边界）| architect 裁决 |
| 3 | **`ADR-CEP-12` 追加式修订的内容** | architect 裁决（⛔ 原文不改）|

⚠️ 另需 **user ratify**（schema 递增属契约面变更）⇒ 归登记册 §6.4 **Q-4** 裁决范围（architect 侧给形态）+ user 侧给 ratify。
⇒ **本单元在本轮只作登记**，⛔ **不排入下一轮开工序列**（§6.4 队列 2 的 I-6 前置已写明「批 1 完成 **且** schema 13→14 + `ADR-CEP-12` 联动修订已获 user ratify」）。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 指出 G-04 范围错（3 条 vs 实测 5 条）+ 后 2 条属「跨 change 规格修订」 | ⇒ 本单元 = 批 2，**范围重估为规格修订面** |
| 2 | oracle | **REJECT** · 修法 1 | ⇒ v3 显式拆分批次构成（批 1 / 批 2）|
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | ⇒ 本单元不碰 B2 行、不碰 KPI 面 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ 本单元不含 KPI 活口径取值 |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | ⇒ 不涉及本单元 |

⇒ **本单元登记待裁，不排期**。
