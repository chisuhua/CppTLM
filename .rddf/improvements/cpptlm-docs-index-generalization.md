---
name: cpptlm-docs-index-generalization
priority: P2
phase: phase-1
依赖:
  - cpptlm-docs-index-mechanical-generation（已实施 2026-11-19，per 台账 Entry #504）
  - docs/README.md §5「索引不复制状态」
状态: 🟡 **已立项**（2026-11-19 · 台账 Entry #504 · KPI **保持 0/3** —— 与一期同型走「不占 OpenSpec slot」路径，按 `cep-b4-evidence-device` 先例）
created: 2026-11-19
---

# improvement: cpptlm-docs-index-generalization

**主题**：把一期（`cpptlm-docs-index-mechanical-generation`）在 `docs/adr/` 上跑通的「索引不腐化」机制，推广到 **`docs/**/README.md` 全部 15 个目录级索引**，并修掉推广前实测出的 **4 处真实漂移**。

> **决策源**：无新 ADR（同型机制先例 = 一期 + 台账 Entry #342，均未走 ADR）
> **前置**：一期已交付生成器 + 三合一闸 + 4 变异，`docs/adr/README.md` 已不可腐化
> **触发**：一期 C-4 明确「本期**不**推广」，oracle 建议「先单点跑通验证，⛔ 不复制未验证的判据形态」⇒ 一期跑通后按此推进
> **估时**：**0.5d**（闸 + 变异 ~0.3d + 修 4 处漂移 ~0.1d + 判据面落盘与验证 ~0.1d）

---

## 架构依据

| 依据 | 出处 | 用到的决策要点 |
|---|---|---|
| **一期判据形态** | `docs/adr/README.md §7` + `scripts/test/docs_adr_index_check.sh` | 三合一闸 + **变异必须跑在拷贝根**（`ADR-CEP-*` 在 M1 冻结面内，直接改真文件会留下 red 工作区）+ 「取最后一个命中」型契约的变异须**行尾追加** |
| **索引不复制状态** | `docs/README.md §5` 维护约定 | 「文档本体写状态；**索引不复制状态**（ADR 索引由机械抽取生成）」⇒ 本 improvement 是把该约定的**剩余部分**兑现 |
| **同目录 README 索引本目录 .md** | `docs/**/README.md` 现状（15 个目录实测） | 计量单位 = 「目录 D 的 README 索引 D 的顶层 `.md`」，⛔ 不递归（`designs/README.md` 是入口索引，指向子目录 README） |
| **ADR 不可变 / 冻结面** | `AGENTS.md` §DOC HYGIENE + `cep_freeze_check.sh` | 推广只碰**子目录 README + 索引行**；`07 号` 与 `cep/*` 的**正文**不动（冻结面） |

---

## Why（推广前实测的 4 处真实漂移 + 1 个结构缺口）

| # | 证据 | 实测 |
|:--:|---|---|
| W-1 | **3 份文档未登记**（覆盖缺口） | `docs/designs/dgpu-soc/` 磁盘 8 份、README 登记 5 份 ⇒ 未登记 = `14-pcie-tlp-wire-format.md` · `communication-network.md` · `multi-level-hybrid-simulation.md` |
| W-2 | **索引状态与本体重体矛盾**（真缺陷） | `docs/architecture/README.md:49` 写「✅ v2.0 已批准」，而 `09-topology-visualization-pipeline.md` 本体写「🟡 草案（2026-10-19 状态复核**回退**，per critic 评审 B4：关联 ADR-X.12 仍未签发）」 |
| W-3 | **索引携带本体没有的生命周期状态**（非缺陷，但需显式豁免） | `06-bis` 本体「🟡 v0.3 Revision」，索引「⛔ 已被 07 号 supersede」⇒ 索引**多**了 supersede 标记，与 W-2 性质不同 ⇒ 必须靠**显式豁免表**区分，⛔ 不可放宽判据把它吞掉 |
| W-4 | **同型缺陷已在别处发生过** | 一期 Entry #342（`07 号` 索引 5 个 `## ` 未列入仍绿、行号 10/23 错）与一期 G-1（`ADR-DGPU-10`「提议」未在词表）⇒ 「人工同步 + 无对账闸」的复发已被实测两次 |
| W-5 | **结构缺口：非 ADR 目录零闸** | 一期闸只覆盖 `docs/adr/README.md`；其余 14 个目录的「新增文档忘登记」**完全静默**（W-1 就是实例） |

**⛔ 为什么不做「把状态列也机械生成」**：实测 `docs/architecture/*.md` 的**状态源行有 5 种形态**（`> **状态**:` / 裸 `**状态**:` / `> **文档状态**:` / `版本：x \| 状态：y` 行内 / 章首无），且取值词表**开放**（「✅ Phase 6 完成，端到端验证通过」/「🟡 v0.4 Revision」/「已决策」…）。要机械生成就得**先规整 17 份文档的元数据**，其中 `07 号` + 9 篇 `cep/*` 在 **M1 冻结面内不可改** ⇒ 一期那套「全量重生成」**不可移植**。

**⇒ 因此二期只做可移植的两条**：① **覆盖对账**（磁盘 ⊆ README 登记）② **状态符号一致性**（✅/🟡/📋/⛔ 的**符号层**比对 —— 实测 17 份里 15 份符号一致，2 处不一致中 1 处是 W-2 真缺陷、1 处是 W-3 需豁免）。

---

## What Changes

| # | 内容 | 落点 | 性质 |
|:--:|---|---|---|
| **G1** | **判据面落盘**：新增「子目录索引登记契约」节 —— 登记形态（`](./name.md)` **或** `` `name.md` ``）、计量单位（同目录顶层 `.md`，⛔ 不递归）、状态符号比对规则、显式豁免表（**当前 1 项**：`06-bis`，理由是索引携带 supersede 生命周期）、`docs/README.md` 与 `docs/archive/**` 不适用的理由 | `docs/README.md`（新增 §7） | 新增 |
| **G2** | **闸**：`scripts/test/docs_index_consistency_check.sh` —— ① 覆盖（磁盘 ⊆ 登记）② 同目录链接可达 ③ 状态符号一致（有豁免表）④ 一次 fixture 自测（`--selftest`，**跑在拷贝根**） | `scripts/test/`（新） · `test/CMakeLists.txt`（注册 2 项） | 新增 |
| **G3** | **修 W-1**：`docs/designs/dgpu-soc/README.md` 补登记 3 份文档（按该目录既有表格形态） | `docs/designs/dgpu-soc/README.md` | 改 |
| **G4** | **修 W-2**：`docs/architecture/README.md:49` 的 `09` 状态 `✅ v2.0 已批准` → `🟡 v2.0 草案`（以**本体为准**，本体给出回退理由） | `docs/architecture/README.md` | 改 |
| **G5** | **登记**：`scripts/README.md` 的 `test/` 行补该闸 | `scripts/README.md` | 登记 |

---

## Acceptance（全部机器可判）

| # | 判据 | 手段 |
|:--:|---|---|
| AC-1 | 闸 `--check` rc=0，且**无任何目录出现「未登记」** | 闸输出 `合计未登记 = 0` |
| AC-2 | W-1 闭合：`docs/designs/dgpu-soc/` 的 8 份 `.md` 全部在 README 中登记 | 闸判据① 对该目录绿 |
| AC-3 | W-2 闭合：`09` 索引符号（🟡）== 本体系号（🟡） | 闸判据③ 对该文件绿 |
| AC-4 | **判据有牙**：4 个决定性变异全红 + 还原全绿 —— ① 删 1 行登记（未登记 ⇒ 红）② 加 1 份未登记 `.md`（⇒ 红）③ 改 1 份文档本体系号（⇒ 红）④ 把 1 条同目录链接改成不存在的文件名（⇒ 红） | `--selftest`（**拷贝根**，⛔ 零工作树污染） |
| AC-5 | **豁免表精确**：删掉 `06-bis` 豁免 ⇒ 必红（证明豁免是**必要的**，不是掩盖）；加回 ⇒ 绿 | `--selftest` 附 1 条 |
| AC-6 | **零回归**：13 个文档闸 + `ctest` 全绿；M1 冻结面 89 件 hash 一致 | 闸套件 + `cep_freeze_check.sh` |
| AC-7 | `docs/README.md §7` 的判据面与实际闸实现**口径一致**（登记形态 / 计量单位 / 符号规则 / 豁免表逐条可对照） | 人工对照（一次），此后以闸为准 |

⚠️ **AC-4 的变异必须在拷贝根**：`docs/architecture/07-*.md` 与 `docs/architecture/cep/*.md` 在 M1 冻结面内；直接改工作树真文件会让 `cep_freeze_check` / `cep_docs_sync_check` 整组 rc=1（中途崩溃即留下 red 工作区）。

---

## Capabilities

| # | 能力 | 判别力（改什么 ⇒ 必红） |
|:--:|---|---|
| **C-1** | **新增文档漏登记检出** | 目录里加 1 份 `.md` 不登记 README ⇒ 红（这是 W-5 的结构缺口，当前**完全静默**） |
| **C-2** | **索引状态与本体重矛盾检出** | 改本体状态符号不更新索引 ⇒ 红（W-2 型；当前只能靠人工复核） |
| **C-3** | **索引幽灵链接检出** | README 指向同目录不存在的 `.md` ⇒ 红（改名/删除后的残留） |
| **C-4** | **豁免可审计** | 豁免表逐条须写理由；`--selftest` 证明「删豁免必红」⇒ ⛔ 不能当消音器用 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `scripts/test/docs_index_consistency_check.sh` |
| **改** | `docs/README.md`（新增 §7 判据面）· `docs/designs/dgpu-soc/README.md`（补 3 份）· `docs/architecture/README.md`（1 处状态）· `test/CMakeLists.txt`（注册 2 项）· `scripts/README.md`（登记） |
| **零改动** | **零** ADR 正文 · **零** `07 号` / `cep/*` 正文（冻结面）· **零** 产品代码 · **零** `openspec/specs/**` |
| **风险** | 低 —— 覆盖判据是**集合包含**，对现有 15 个目录实测仅 W-1 一处不满足；状态判据用**符号层**（不是开放词表）⇒ 不会误伤 |

---

## 明确排除

| 项 | 理由 |
|---|---|
| `docs/` 根 README 的递归覆盖 | 根 README 是**总入口**（指向子目录 README），子目录各管 ⇒ 递归会让 200+ 份文件都要求登记，与「子目录自管」冲突 |
| `docs/archive/**` | 归档目录的 README 是**摘要**而非穷举（40 份归档件中登记 5 份是有意的） |
| 状态列的**机械生成**（ADR 式） | 见 §Why 末段：源行 5 种形态 + 开放词表 + 冻结面不可改元数据 |
| `docs/architecture/examples/**` 的 `.md` | 示例目录按**目录链接**登记（`examples/bundles/` 等 4 条），其内部 `.md` 不逐个登记 |

---

## 关联

台账 **Entry #504**（回填 + 本 improvement 立项）· 一期 `.rddf/improvements/cpptlm-docs-index-mechanical-generation.md` · `docs/adr/README.md §7`（一期判据面）· `scripts/test/docs_adr_index_check.sh`（一期闸，本闸的模板）· `docs/README.md §5`（索引不复制状态的既有约定）
