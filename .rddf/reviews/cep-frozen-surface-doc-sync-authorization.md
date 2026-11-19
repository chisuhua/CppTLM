# 冻结面文档同步 —— 授权请求包

> **性质**: architect 产出的**授权请求包**（非审稿报告、非产物）—— 面向 **planner**（落台账 Entry）与 **builder**（执行五步链）。
> **日期**: 2026-11-19
> **作者**: architect（主会话）
> **审查**: oracle 1 轮（机制设计，**APPROVE + 4 条件**，条件已全部采纳）
> **本回合 architect 零触碰冻结面** —— `cep_freeze_check.sh` 无草稿模式，工作区一改冻结产物即 sha256 漂移转红 ⇒ 故只产出本包。

---

## 一 · 要解决的问题（实测事实）

| 项 | 文档声明 | 实测 | 处置 |
|---|---|---|---|
| **D3** | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md:473` 的 R-d 行：「22 份 ADR 的 §2 决策块计数逐份 == 快照(· 22 键 + `total`=125)」 | `test/cep_decision_block_snapshot.json` = **23 键 / total=131**（求和自洽） | ⛔ **冻结面内** ⇒ 需授权链 |
| **D4** | `docs/architecture/07-coherence-exploration-platform.md` 的 4 处 + `docs/architecture/cep/{core-traffic-gen,simulation-and-experiment}.md` 各 1 处，共 **6 条** Markdown 链接相对深度错 | `../cep/*` 解析到不存在的 `docs/cep/`；`../adr/*` 解析到不存在的 `docs/architecture/adr/` | ⛔ **冻结面内** ⇒ 需授权链 |

**D3 的根因已闭环**：台账 **Entry #431** 的 **W2** = 快照 `files` 加 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`；`count` **22→23**；`total` **125→131**；`authorized_by` → `Entry #431`；同批 **W3** 同步改了 delta spec（`cep-invariant-gate` 的「实测 125 / 22」→「131 / 23」）。
⇒ **决策未变、快照已授权更新、唯 ADR 的 SU 声明未同步** ⇒ 纯**声明漂移**，处置 = **追加 SU 声明新值**（⛔ 不改原引用行）。

---

## 二 · 冻结面归属（实测）

| 文件 | 在 M1 冻结清单 | 归属 |
|------|:---:|------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | ✅ 显式条目 | 冻结 |
| `docs/architecture/07-coherence-exploration-platform.md` | ✅ 显式条目 | 冻结 |
| `docs/architecture/cep/core-traffic-gen.md` | ✅ 显式条目 | 冻结 |
| `docs/architecture/cep/simulation-and-experiment.md` | ✅ 显式条目 | 冻结 |

清单口径（2026-11-19 实测）：`artifact_count` = **89**；显式 89 条 sha256 与 `globs` **23** 条展开的 89 个文件**完全重合**；`exclusions` = **0**。

---

## 三 · 待授权变更清单（4 文件 / 7 处）

### 3.1 D3 —— `ADR-CEP-00` 追加 `## Status Update`（**不改任何原引用行**）

> **落点**：`docs/adr/ADR-CEP-00-platform-charter-and-governance.md` **文末追加**（该 ADR 现有 **17** 个追加式 SU，最新 SU 内部已显式声明「只引用，⛔ 不改 SU 2026-11-11 `### ②` 的原表(append-only)」⇒ 本次沿用同一纪律）。

**拟追加正文（逐字稿）**：

```markdown
## Status Update 2026-11-19(per 台账 Entry #431 的 W2 已授权更新)—— **R-d 快照当前值声明**(⛔ 不改上方 `### ③` 表的原引用行)

⚠️ **本段只声明当前值,⛔ 不改**上方 `### ③` 机器判据表 R-d 行的「22 份 ADR · 22 键 + `total`=125」—— 那是 **2026-11-12** 时的历史值,按本 ADR 的 append-only 纪律原样保留。

**R-d 快照的当前权威值** = `test/cep_decision_block_snapshot.json` 实测:

| 项 | 历史值(本 SU 上方 R-d 行) | **当前值** | 变更授权 |
|---|---|---|---|
| 参与判据的 ADR 份数 | 22 | **23** | 台账 **Entry #431** · W2 |
| 快照 `count` | 22 | **23** | 同上 |
| 快照 `total` | 125 | **131** | 同上 |
| 快照 `files` 键集合 | 22 键 | **23 键**(新增 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`) | 同上 |
| 快照 `authorized_by` | — | **`Entry #431`** | 同上 |

⇒ **增量为 `ADR-CEP-20` 的 6 个 §2 决策块**(期望值 6 由闸自己的 `analyze_section2` 实测得出,⛔ 禁从被检对象外部 `grep` 推导 —— per Entry #431 W2 备注)。

⛔ **本段不构成新判据**:R-d 的判据形态(逐份比对 + 两条显式红 + 围栏危险标记 + 闸外快照)自 2026-11-12 起不变;本段只把**期望值**对齐到已授权的快照当前值。

**关联**:台账 **Entry #431**(W2 快照更新授权 · W3 delta spec 同步)· `test/cep_decision_block_snapshot.json` · `scripts/test/cep_decision_block_check.{sh,py}`
```

### 3.2 D4 —— 6 条链接相对深度修正（逐字 before → after）

| # | 文件 | 行 | before | after |
|---|------|---|--------|-------|
| 1 | `docs/architecture/07-coherence-exploration-platform.md` | 183 | `](../cep/coherence-interface.md)` | `](./cep/coherence-interface.md)` |
| 2 | 同上 | 256 | `](../cep/protocol-engine.md)` | `](./cep/protocol-engine.md)` |
| 3 | 同上 | 279 | `](../cep/simulation-and-experiment.md)` | `](./cep/simulation-and-experiment.md)` |
| 4 | 同上 | 294 | `](../cep/simulation-and-experiment.md)` | `](./cep/simulation-and-experiment.md)` |
| 5 | `docs/architecture/cep/core-traffic-gen.md` | 16 | `](../adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md)` | `](../../adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md)` |
| 6 | `docs/architecture/cep/simulation-and-experiment.md` | 78 | `](../adr/ADR-CEP-06-experiment-protocol.md)` | `](../../adr/ADR-CEP-06-experiment-protocol.md)` |

⚠️ **链接文字（link text）不变** —— 只改 href；文字里的仓库根相对路径（`docs/architecture/cep/...`）本就正确。
⚠️ **07 号的 `../adr/*` 链接是对的**（`docs/architecture/` 与 `docs/adr/` 同级）⇒ **不在本包范围**，⛔ 勿误改。

---

## 四 · 台账 Entry 草案（**planner 落** `obj-cep-platform.md §11`）

> **owner = planner**（architect 无 `.rddf/roadmap/**` 写集）。
> **拟标题**：`### Entry #502 — 2026-11-19 [architect](**CEP 冻结面文档同步授权** —— ADR-CEP-00 R-d 快照当前值声明(22→23 / 125→131，授权源 Entry #431) + 6 条相对链接深度修正)`
>
> **必须写明（供 `--authorized-by` 校验）**：
> - 授权范围 = **本包 §三 的 4 个文件 / 7 处**，⛔ 超出即越权
> - 变更性质 = **声明同步 + 链接形态修正**，⛔ **不触碰任何决策内容**
> - `--authorized-by` 引用 = **`Entry #502`**
> - 判据 = 清单 diff **恰为 4 个文件 hash 变更**；多一个少一个 ⇒ 停下重报

---

## 五 · 五步执行序列（owner 明确，⛔ 不得合并提交）

| 步 | 动作 | owner | 判据 |
|:--:|------|-------|------|
| 1 | 落台账 **Entry #502**（本包 §四） | **planner** | `obj-cep-platform.md §11` 出现 `Entry #502`，且含 `--authorized-by` 依据 |
| 2 | 提交产物 = 本包 §三 的 4 文件 diff | **builder** | commit 产物；⛔ 台账与产物**不得**同一 commit |
| 3 | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #502"` | builder | 清单 diff **恰 4 个文件 hash 变更** |
| 4 | 清单**单独** commit | **builder** | ⛔ 不得与产物合并提交 |
| 5 | 无参复验 `bash scripts/test/cep_freeze_check.sh` | builder | rc=0 且 provenance 一致 |

⚠️ **步骤 3 必须在步骤 2 之后** ⇒ 否则 `frozen_at_commit` 指向错误 commit，闸 rc=1 阻断。
⚠️ 步骤 4 之后 `frozen_at_commit` 落后 HEAD 一个 commit 是**常态且不阻断**（闸仅告警）⇒ ⛔ **不要**为消除该告警重复 `--update`。
⚠️ 步骤 2 执行前，工作区会短暂处于「冻结面已改、清单未更新」状态 ⇒ `cep_freeze_check` / `cep_docs_sync_check` 会红 ⇒ 这是**预期中间态**，⛔ 不得跳过步骤 3。

---

## 六 · 验收判据（全部机器可判）

| # | 判据 | 手段 |
|---|------|------|
| AC-01 | 6 条链接全部可达 | 全仓 Markdown 链接扫描：这 6 条从「broken」消失 |
| AC-02 | `ADR-CEP-00` 的 R-d 当前值声明存在 | `grep` 该 SU 段命中 `23` / `131` / `Entry #431` |
| AC-03 | ADR 原引用行**未被改** | `git diff` 显示 `:473` 行**无改动**（只新增文末段） |
| AC-04 | 清单 diff 恰 4 文件 | `--update` 后 `git diff --stat` 恰 4 行 |
| AC-05 | 冻结面复验绿 | 无参 `cep_freeze_check.sh` rc=0 + provenance 一致 |
| AC-06 | 关联闸不回归 | `cep_docs_sync_check.sh` / `cep_decision_block_check.sh` / `cep_doc_ref_check.sh` rc=0 |

---

## 七 · 明确**不**包含在本包的项目（各归其位）

| 项 | 归属 | 理由 |
|---|------|------|
| **5 条伪链接**（`07 号` 的 `../adr/ADR-CEP-19-*.md` / `-04-*` / `-06-*`，含 `*` 字面永不解析） | **另立台账 Entry** | 属**独立缺陷类**（伪链接 ≠ 相对深度错）⇒ 最小变更原则，不混入本包 |
| `docs/ONBOARDING.md:310`「22 份 ADR 锚点」→ 实测 **24** | **builder** | `docs/ONBOARDING.md` 不在 architect 写集 |
| `scripts/README.md` 自述「31+2+2=35」→ 实测 **44 行** | **builder** | `scripts/**` 不在 architect 写集 |
| `docs/adr/README.md` 生成器 + 闸（D5 机制） | **builder** | 生成器落 `scripts/`（builder 写集）；契约已钉死在 `docs/adr/README.md §7`（architect 写集）⇒ **契约已就位，实现待落地** |
| `docs/adr/README.md` 的**可复现性缺口**（生成器未落仓 ⇒ 索引不可重生成） | **builder** | 同上；`docs/adr/README.md` 已**如实标注该缺口为「未治理项」**，未作未兑现承诺 |

---

**维护**: architect（本包）· planner（Entry #502）· builder（五步链）
