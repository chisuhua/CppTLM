---
name: cep-registry-stale-cleanup
priority: P1
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
状态: "🟡 **已立项**(2026-11-20 · planner 起草 · 经 metis 1 轮 + oracle 1 轮 + critic 3 轮审查(critic 第 3 轮 APPROVE_WITH_CONDITIONS)· 关联登记册 `obj-cep-platform-remaining.md` §6.3 归属表 P-1 / P-3 / I-1)"
created: 2026-11-20
---

# cep-registry-stale-cleanup: CEP 登记册 / 台账 stale 集清理 + 摘要表指引追加式强化

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文**（只走 Status Update）· ⛔ **不动 `07 号` 正文与既有 SU**（只追加，append-only）· ⛔ **不改 `obj §4.0-a` 的 8 行摘要表**（守 Entry #334 ratify 形态）· ⛔ **不新建登记面**（本册是 canonical 登记册，只更新既有 living 节）· ⛔ **不改冻结面成员**（本单元全部落在台账 / 登记册 / 活文档 / 脚本注释，⛔ 均**不**在 `cep_freeze_check.sh` 的 `FROZEN_GLOBS` 内 ⇒ **不触五步链**）· ⛔ **不新建 OpenSpec change**（零槽位批路径，先例 = `cpptlm-cep-volume-number-guard` / `cpptlm-docs-index-mechanical-generation`）· ⛔ **不改 KPI 计数**（仍 `0/3`）· ⛔ **不改 `B3b-2` 计数**（仍 `0/4 ⭔`）。

## 架构依据

本单元处理登记册 §6.2 的 **S2-5 / S2-6 / S3-9 / S3-12** 四组缺口，全部是**读法层失真**（不是能力缺口）：

| 缺口 | 位置 | 声明 | 实测（2026-11-20） |
|---|---|---|---|
| **S2-6a** | `docs/guide/cep-usage.md:195` | 「ADR 归因（**18** 条）· doc_layer_manual_review **4**」 | `test/cep_adr_structural_deferrals.json` 实测 **19** 条 · `doc_layer_manual_review` **5** 条 |
| **S2-6b** | `obj-cep-platform.md:971`（L2 行） | 「coverage 文档 TileLink-C 行的 L4 档标注矛盾**至今未修**」 | `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md:36,39` **已改**为「◐ L4-direct ✅ / L4-wire ❌」⇒ **过期的是台账，不是文档** |
| **S2-6c** | `scripts/test/cep_freeze_check.sh:52-54` | 「⚠️ 当前 `docs/architecture/cep/` **不存在** ⇒ 本 glob 现 0 匹配，冻结产物数 69 → 69 不变」 | 该目录实有 **9** 份 `.md` 且已在 manifest 内 ⇒ **行为正确、注释误导**（后来者据此会误判 glob 未生效） |
| **S2-6d** | `obj-cep-platform-remaining.md:21`（§0.1） | active **3** · KPI **3/3** · specs **135** | active **0** · KPI **0/3** · specs **137**（三个框架治理 change 2026-11-18~19 全部归档所致） |
| **S2-5** | `obj §4.0-a` 表下指引（`:18643`） | UALink 行 2 条 ⛔ 禁令语「不在摘要内 ⇒ 见下方原文快照」 | **已有**指引，但强度不足以拦住「只看 8 行摘要表」的读者 ⇒ 需**追加式**强化（⛔ 不得改 8 行摘要表本身 —— Entry #334 ratify 形态 +「⛔ 禁令语不得摘要化丢失」） |
| **S3-9** | `obj-cep-platform-remaining.md` §1.5 | 框架治理三 change 的实施序列（118 个未勾 task） | 三个 change **已全部归档** ⇒ **整节已作废**（本单元的登记册更新时已标注） |
| **S3-12** | `obj-cep-platform.md:474` | 同行散文「`openspec/changes/` 当前 **3 个 active**」vs 同行结论「当前 Proposed 并发 = **0/3**」 | 实测 active = **0** ⇒ **散文 stale**（结论对）⇒ 行内读法陷阱 |

**为什么现在做**：登记册 §1.5（整节作废）+ §0.1（stale）若不清理，**下一个 session 会照着一个整节作废的计划派工** —— 这是本次缺口梳理里唯一「不修就会直接误导执行者」的项。

## 范围

- **In-Scope**：
  - `docs/guide/cep-usage.md` —— S2-6a 一行数字订正（活文档 ⇒ **原地改**）
  - `scripts/test/cep_freeze_check.sh:52-54` —— S2-6c 过时注释删除（脚本注释 ⇒ **原地改**，⛔ 不改判据逻辑 ⇒ rc 不变）
  - `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` —— §0.1 三数 + §1.5 作废标注 + 指向新增 §6（living 节 ⇒ **原地改**）
  - `.rddf/roadmap/objectives/obj-cep-platform.md` —— **追加式**订正 Entry（S2-6b / S3-12 / S2-5 指引强化）⇒ ⛔ **不改任何历史 Entry 原文**
- **Out-Scope**：
  - ⛔ `obj §6.2` B2 行措辞（Entry #232 授权的**改判**）⇒ 归 `Q-2`（architect 独家）
  - ⛔ `obj §4.0-a` 的 8 行摘要表本身（Entry #334 ratify 形态）
  - ⛔ 冻结面成员（`docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / `docs/architecture/cep/*.md` / `openspec/specs/cep-*/spec.md` / `experimental/*.json` / `tools/*.py` / `include/tlm/cep/protocol_instance.schema.json` / `fabric.schema.json` / `fabric.json`）⇒ **不触五步链**
  - ⛔ KPI 活口径**取值**同步（6 文件 / 7 处）⇒ 归 `cep-stage-gate-p5-hardening`（闸判据修正后由闸强制）
  - ⛔ 45 份非 CEP Purpose 占位 / 26 份正文 >500 / R-10·R-11·R-16 存废 ⇒ **仓级，非 CEP 域，仅登记**

## Why

**不做的代价**：登记册与台账是本仓唯一的「剩余任务真源」。它一旦 stale，后果不是「文档不准确」，而是**下一轮派工照着一份作废计划开工** —— 本轮梳理里已经出现过的真实事故：`obj-cep-platform-remaining.md:21` 至今写「KPI **3/3 已满** ⇒ 不得立新提案」，而实际 KPI 是 `0/3` ⇒ **按它派工会误判为「不能立项」**。

**为什么放进同一个单元**：这 7 处 stale 的**修法形态完全同型**（一行订正 + 各自一条判据），拆成 7 个 change 会产生 7 次 slot 占用与 7 份审批成本，而**零槽位批路径**（本仓先例：`cpptlm-cep-volume-number-guard`「零槽位批路径」/ `cpptlm-docs-index-mechanical-generation`「不占 OpenSpec slot」）本就是这类纯文档/注释债的既定处置。

**为什么不用「一次全清」**：其中 S2-6b / S2-5 / S3-12 落在**台账**（append-only），S2-6a 落在**活文档**（可原地改），S2-6c 落在**脚本注释**（可原地改但不得改逻辑）—— **三种修法形态的完成定义不同**（活文档/脚本 = 「旧串消失」；台账 = 「订正载体存在」）⇒ 必须**逐项判据化**，不得用一条「grep 全文无 stale」糊过去。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **S2-6a 订正** | `docs/guide/cep-usage.md:195` | 「ADR 归因（18 条）」→「（**19** 条）」；「doc_layer_manual_review 4」→「**5**」；⛔ 同一行的其余内容逐字不动 |
| 2 | **S2-6c 注释订正** | `scripts/test/cep_freeze_check.sh:52-54` | 删除「当前 `docs/architecture/cep/` 不存在 ⇒ 本 glob 现 0 匹配」一句，改为「该目录已有 9 篇子文档，glob 全量生效」；⛔ **`FROZEN_GLOBS` 一行不动** ⇒ 冻结产物数与 rc 不变 |
| 3 | **S2-6d + S3-9 登记册刷新** | `obj-cep-platform-remaining.md` §0.1 / §1.5 | §0.1 三数改实测（active 0 / KPI 0/3 / specs 137）+ 加「KPI 活口径同步面 6 文件 / 7 处」行；§1.5 顶部加**作废标注**（⛔ **正文逐字保留**）+ 闸头引用悬空提示；头部「实测日期」改 2026-11-20 + 指向 §6 |
| 4 | **S2-6b / S3-12 / S2-5 追加式订正 Entry** | `obj-cep-platform.md` **文末追加**新 Entry | 三点并入**一条**订正 Entry：① L2 行「至今未修」订正为「已改（`docs/validation/2026-11-04-cep-protocol-e2e-coverage.md:36,39`）+ 台账已回填」② `:474` 行散文「当前 3 个 active」订正为「当前 **0** 个 active（散文 stale，结论 `0/3` 正确）」③ `obj §4.0-a` 表下指引追加强化句（含可 grep 字面量）⇒ ⛔ **不改任何历史 Entry 原文**；⛔ **不改 `obj §4.0-a` 的 8 行摘要表** |
| 5 | **闸头引用转指** | `scripts/test/rddf_stage_gate.sh:3` | 引用登记册 §1.5.6 ⇒ 改为引用登记册 **§6.5**（顺序机关节）；⚠️ 归 `cep-stage-gate-p5-hardening` 的 **I-0** 实施面（该单元本就改这个闸 ⇒ 避免两单元同改一文件）⇒ **本单元只登记归属，不改该文件** |
| 6 | **登记册新增 §6**（缺口清单 + 立项计划） | `obj-cep-platform-remaining.md` §6 | ⛔ **本单元不改**（已于本轮 planner 落盘时写入）⇒ 本单元只需核对其存在与自洽 |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因；⛔ 台账侧**不要求**历史 Entry 里的旧串消失（append-only）⇒ 判据是「**订正载体存在 + grep 命中订正文本**」。

- [ ] **AC-1（S2-6a · 活文档原地改）**：`grep -cF 'ADR 归因**(18 条)' docs/guide/cep-usage.md` == **0** ∧ `grep -cF 'ADR 归因**(19 条)' docs/guide/cep-usage.md` == **1** ∧ `grep -cF 'doc_layer_manual_review 5' docs/guide/cep-usage.md` == **1**；⚠️ 反 fixture：把该行改回 `18 条` ⇒ 判红 ⇒ 还原绿；⚠️ **必须用 `-F` 且含 markdown 粗体星号**（per critic BLOCKER-2）—— 原文是 `**ADR 归因**(18 条)`（**半角括号 + 粗体**）⇒ 旧判据用**全角**「（18 条）」⇒ 命中恒为 0（**假绿**）且「（19 条）」恒为 0（**永假**）⇒ 该 AC **永不可能通过**；⚠️ 本判据的**正确形态**已在登记册 §6.4 的 **I-1** 行落地（`grep -cF 'ADR 归因**(18 条)'`）⇒ 两处须一致
- [ ] **AC-2（S2-6c · 脚本注释原地改，逻辑零动）**：`grep -c "当前 \`docs/architecture/cep/\` 不存在" scripts/test/cep_freeze_check.sh` == **0**；`bash scripts/test/cep_freeze_check.sh` 的 **rc 与冻结产物数前后逐字节相等**（`artifact_count` 不变）⇒ 证明「只改注释、不改判据」；⚠️ 反 fixture：删掉 `FROZEN_GLOBS` 里 `docs/architecture/cep/*.md` 一行 ⇒ **冻结产物数必变** ⇒ 判红 ⇒ 还原绿
- [ ] **AC-3（S2-6d + S3-9 · 登记册 living 节原地改）**：登记册 §0.1 的 active / KPI / specs 三数 == `ls openspec/changes/ | grep -v archive | wc -l` / `ls openspec/specs/ | wc -l` 实测值；§1.5 顶部含「整节已作废」字面量；头部「实测日期」== 2026-11-20；⚠️ **§1.5 正文逐字未变**（`git diff` 中 §1.5 正文段落无改动行，除新增的顶部标注块）
- [ ] **AC-4（S2-6b / S3-12 / S2-5 · 台账追加式订正 Entry）**：台账文末**新增**一条订正 Entry，且该 Entry 正文**同时含**三个可 grep 字面量：① 「L2」+「已改」+ `docs/validation/2026-11-04-cep-protocol-e2e-coverage.md` ② 「0 个 active」+「散文 stale」③ `obj §4.0-a` +「原文快照」+「见下方原文快照」；⚠️ **历史 Entry 原文零改动**（`git diff` 中 §11 的历史 Entry 段落无改动行）；⚠️ 反 fixture：删掉该订正 Entry ⇒ 三项 grep 全部落空 ⇒ 判红 ⇒ 还原绿
- [ ] **AC-5（8 行摘要表 sha256 逐字未变）**：`git diff` 对 `obj-cep-platform.md` 的改动**不触及** `obj §4.0-a` 的 8 行表体（`实例 | 状态 token | ≤40 字摘要 | Entry 锚` 表）⇒ 判据 = 表体区域 diff 行数 == **0**
- [ ] **AC-6（零冻结面 diff · 零 ADR diff）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `openspec/specs/` · `include/tlm/cep/` · `experimental/` · `tools/`；⇒ **零冻结面授权**（与 Entry #232 段落「objective **不在** `FROZEN_GLOBS` ⇒ **零冻结面授权**」同口径）
- [ ] **AC-7（零 OpenSpec change · 零 KPI 占用）**：本单元**不创建** `openspec/changes/<name>/`；`ls openspec/changes/ | grep -v archive | wc -l` 前后相等；`AGENTS.md` / `.rddf/roadmap.md` 的 KPI 活口径行**零改动**
- [ ] **AC-8（既有闸仍绿）**：`bash scripts/test/rddf_stage_gate.sh` rc=0；`ctest --test-dir build -R rddf_stage_gate` 绿；⇒ 证明本单元未破坏既有顺序机关
- [ ] **AC-9（范围纪律 · 零越权）**：`git diff` 对 `obj-cep-platform.md` **不含** §6.2 B2 行（`obj:529`）的改动 ⇒ 证明未越权改 Entry #232 授权的改判措辞；`git diff --name-only` **不含** `scripts/test/rddf_stage_gate.sh` ⇒ 证明闸头引用转指归属在 `cep-stage-gate-p5-hardening`（AC-9 的归属纪律）
- [ ] **AC-10（0 回归）**：`ctest` 全绿（⛔ **不写死数字**，以 `ctest -N` 实测为准）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
- [ ] **AC-11（裁决吸收 · Q-7 · 「同一事实 5 处」canonical 化）**：**canonical = `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账**（append-only 决策流水）；其余 4 处（structural `decision_adrs` / 快照 keys / `ADR-CEP-NUMBER-MAP` / `docs_sync ADR_LIST` / `OWNERS+NO_INV`）改为**机械生成**（生成器 = 已存在的 `scripts/cep/generate_doc_index.py`）⇒ **单改一侧即红**（沿用 `cep_decl_alignment_check.sh` 双向对账范式）；判据 = ① 4 处生成物与台账 §11 的对应集合**逐项相等** ② 改台账一侧 ⇒ 4 处生成物中至少一处**变红**（反向 fixture）；⚠️ **否决记录（per 裁决 §8.2 C-3）**：曾评估以 `test/cep_adr_structural_deferrals.json` 的 `decision_adrs` 为 canonical —— **否决**（该文件**只覆盖 ADR 全集这一个事实**，覆盖面不等；且它已被结构闸消费 ⇒ 再加一个 canonical = **制造第二真源**）
- [ ] **AC-12（裁决吸收 · Q-9 · OWNER 维度已废除 · **本单元不承担任何 OWNER 项的具体修正**）**：**用户 2026-11-20 已拍板：废除 OWNER 维度 + 8 项逐项终态**（⛔ 前提已修正 —— §1.3 实为 **8 项全部仍 live**，此前「4 项」是窄检口径）。⇒ **三分落点（per 裁决记录 §10.9.3）**：**A 类 2 项**（R-17 来源 change 归档目录缺失 · R-19 `AGENTS.md` handler 名 stale）→ `cep-doc-hygiene-kpi-and-impl-state`；**B 类 5 项**（R-12 · R-13 · R-14 · R-15 · R-20，**落点全在 `openspec/specs/`**）→ `cep-spec-stale-refs-r14-r15-r20`；**C 类 R-18** → architect（Q-12）；⚠️ **R-12 / R-13 原归 A 类是错的**（其落点在 spec 内 ⇒ A 类单元 AC-6 禁止 specs diff ⇒ 物理不可执行）。⇒ **本单元只承担「登记册 §1.3 终态表与三分裁决一致」这一条登记面义务**（重写已由 planner 2026-11-20 落盘时完成），⛔ **不承担任何一项的具体修正**（消除重复认领）；判据 = ① 登记册 §1.3 含「已废除」标记 ∨ 8 项终态表逐项有落点 ② `git diff --name-only` 中本单元**不含** `openspec/specs/**`（B 类）与 `.rddf/improvements/cep-doc-hygiene-*.md` 的 In-Scope 目标文件（A 类）③ 三分计数在登记册 §1.3 与两份承接单元中**一致**（2 / 5 / 1）

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无） | — | ⛔ **本单元零能力增量** —— 纯 stale 清理（登记册 / 台账 / 活文档 / 脚本注释）⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时**零契约面变化**（先例 = `cpptlm-cep-volume-number-guard`「零槽位批路径」） |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `docs/guide/cep-usage.md`（1 行）· `scripts/test/cep_freeze_check.sh`（注释行）· `.rddf/roadmap/objectives/obj-cep-platform-remaining.md`（§0.1 / §1.5 顶部 / 头部）· `.rddf/roadmap/objectives/obj-cep-platform.md`（**文末追加**一条订正 Entry） |
| **零改动（硬查）** | `docs/adr/**` · `docs/architecture/**` · `openspec/specs/**` · `include/tlm/cep/**` · `experimental/**` · `tools/**` · `scripts/test/rddf_stage_gate.sh`（归另一单元）· `obj §4.0-a` 的 8 行摘要表 · `obj §6.2` B2 行 |
| **冻结面** | **零**（全部落点均不在 `FROZEN_GLOBS`）⇒ **不触五步链**、**零 `--update`**、冻结产物数不变 |
| **闸** | ctest **0 新增**；`cep_freeze_check.sh` rc 不变；`rddf_stage_gate.sh` rc 不变（仅被消费） |
| **KPI** | **0/3 → 0/3**（不创建 OpenSpec change ⇒ 不占 slot） |
| **风险** | ① 追加式订正 Entry 若漏项 ⇒ 该项静默残留 ⇒ AC-4 的三 grep 兜底 ② 登记册 §1.5 标注若误改正文 ⇒ 破坏「历史快照不回改」纪律 ⇒ AC-3 的 diff 范围判据 ③ 台账改写若误触历史 Entry ⇒ 破坏 append-only ⇒ AC-4 的 diff 判据 ④ `cep_freeze_check.sh` 注释改动若误删 glob 行 ⇒ 冻结面缩小 ⇒ AC-2 的「产物数必变」反 fixture ⑤ 与 `cep-stage-gate-p5-hardening` 同改 `rddf_stage_gate.sh` ⇒ 文件冲突 ⇒ AC-9 的归属纪律把闸头转指划归后者 |

## 待裁

✅ **无** —— 本单元 4 项（P-1 / P-3 / I-1）+ 3 处闸头归属（划归 `cep-stage-gate-p5-hardening`）**全为 planner 域**（登记册 / 台账写入权 = rdd-planner，`obj:19`）⇒ ⛔ 无需 architect 判权。
⚠️ **但 S2-6d 的登记册 §0.1 数字必须与 §6.5 顺序机关的 `LIVE_FILES` 一致** ⇒ 若 `cep-stage-gate-p5-hardening` 的 Q-6 裁决**增补 `LIVE_FILES`** ⇒ 本单元的 §0.1「KPI 活口径同步面 6 文件 / 7 处」须**同步回填** ⇒ 依赖关系登记在此。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis（gap 分析） | 4 处定级错误 + 2 处违反既有禁令 + 7 项漏项 | S2-1 降级迁出本单元 ⇒ S3-10 归 Q-2；「手写分级表」方案被禁 ⇒ 本单元不产表 |
| 2 | oracle（对抗式） | **REJECT** · 10 项必修 | 修法 6（P-2 在 Q-2 裁决前越权改 Entry #232 措辞）⇒ **本单元不含该项**；修法 3（重复认领）⇒ 本单元的 S2-6a/c 与 b/d **分属不同单元** |
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | B-2（同上）⇒ 本单元不碰 B2 行；B-3（重复认领）⇒ §6.3 归属表划死 |
| 4 | critic（v3） | **REJECT** · 1 BLOCKER | BLOCKER-1（窄 pattern 伪证 KPI 面）⇒ 本单元**不含** KPI 活口径取值（归 `cep-stage-gate-p5-hardening`） |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | 2 条 SHOULD_FIX（7 处全锚 / 闸头转指归属）⇒ 闸头转指划归 `cep-stage-gate-p5-hardening`；7 处全锚落在后者 |

⇒ **本单元可在 planner 域直接实施**（⛔ 不新建 OpenSpec change ⇒ 不占 KPI slot）。
