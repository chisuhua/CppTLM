---
name: cpptlm-cep-invariant-gate-teeth
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-00
  - ADR-CEP-01
  - ADR-CEP-02
  - ADR-CEP-03
状态: "🟡 待实施（2026-11-12 起草;per 台账 Entry #359 的 D-1~D-5 + Entry #361 的 D-6~D-9 与 C3a/C3b 切片;architect 三次报告 §五 4 处裁决 + oracle 1 轮判定;metis 1 轮 12 条已逐条落盘，见文末处置对照）"
created: 2026-11-12
---

# cpptlm-cep-invariant-gate-teeth: 治理机制机器化 —— R-c / R-d 两项残留登记的牙齿

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 🟡 待实施 —— 设计已定死（Entry #361 ③④）；本草案**不重新设计**，只做 5 段打包。
> **上游**: architect 三次报告 `.rddf/reviews/cep-goal-scope-review-2026-11-12c.md`（§五 4 处裁决 + §七 oracle 4 条判定）+ 台账 Entry #359（D-1~D-5）/ #361（D-6~D-9 + C3a/C3b 切片）。
> **审查**: metis **1 轮**（对象 = D-6~D-9 设计，Entry #361 ⑥ 的 12 条已逐条处置）⇒ 本 improvement 的 metis 预算 1 轮**已消耗**，不新增第 2 轮（D-10 封盘）。
> **统一判据纪律**: 每条变异 = 「破坏 ⇒ 按**正确原因**红（红因**有签名**，⛔ 不以 rc≠0 充数）⇒ 还原 ⇒ 绿」+ **先跑基线正控**（未改前必须绿）；⛔ 不用关键词匹配（per §13.1：硬造关键词闸 = P6 闸教训重演）；⛔ 期望值不得从被检对象自身或 `NUMBER-MAP` 派生（= 自证复燃）。

## 架构依据

**这是治理线最后两个「有登记、无牙齿」的残留（R-c / R-d），不是新增能力。**

| 残留 | 契约依据（逐字可核） | 现状 |
|---|---|---|
| **R-c** = 「一决策一文档化」残余登记 + 3 条触发条件**无机器检查** | `ADR-CEP-00:434`（SU 2026-11-11 ⑤）自认：「**D0 堵法 ②（机器检查）尚未落地**…断言「架构真源含 3 份 ADR 的残留登记 + 3 条触发条件」的检查**当前不存在**（`scripts/test/` 与 `test/` 内无对应载体）⇒ **残留仍可被静默删除** ⇒ 登记为**待办**（独立 change，落非冻结面）」 | 缺口**已被契约自身承认** ⇒ 本 change 就是那条「待办」的落地 |
| **R-d** = 决策块计数口径**三个互不相等的实现** | `ADR-CEP-00:431`（同 SU ⑤）：「本口径 = 5 块 / **标题口径实测 = 4 条** / `test/cep_adr_parser.hh` 的 `decision_head` **对该 ADR 计 0** ⇒ **三者互不相等**」；规范载体 = `ADR-CEP-00:426` 的 §⑤（**唯一规范载体**） | 口径可被漂移 / 被掏空，**无任何机器判据报警** |

**为何现在做、为何只做这两个**：V1~V5 已闭合，V6「有登记、无牙齿」是**治理线唯一活口**；V3（DoD 降格 = 假绿）是最贵的后果 ⇒ 顺序永远是「**先立判据、再搬内容**」。文档治理再做一轮收益 ≈ 0 ⇒ 本 change 只做「判据机器化」，⛔ 不碰任何内容搬运。

**本 change 的产出物形态 = 2 个新闸 + 1 份快照 + 2 处冻结件的追加 SU**，落在**治理面** ⇒ 走 OpenSpec change（`cpptlm-cep-invariant-gate-teeth`，占 D1 槽 KPI 0/3 → 1/3），⛔ 不新建 ADR（「为承载一条治理残留而新立 ADR」本身即与本条残余的治理方向相悖，per `ADR-CEP-00:398`）。

## 范围

**In-Scope（7 项）**

| # | 变更 | 载体 | 形态 |
|:--:|---|---|---|
| 1 | **新闸**（R-d 计数快照 + R-c 双向对账，**同一脚本两段**） | `scripts/test/cep_decision_block_check.{sh,py}` | 职责单一（⛔ **不并入** `cep_adr_structural_check.py`：后者 4 组以「可登记例外」为模型，R-c/R-d 的对象根本不在其分母里 ⇒ 硬并会让两套语义共享一个 registry） |
| 2 | **抽影子树 helper**（⛔ 抽，**不复制**） | `scripts/test/cep_test_shadow.sh`（`reset <shadow> [extra_cp...]`） | 由 `cep_adr_structural_check.sh:52-57` 的 `shadow_reset` **提取**；两闸 `source` 之（⛔ 不留「自带 4 行」选项 = 隐性分叉路径） |
| 3 | **计数快照**（闸**只读**） | `test/cep_decision_block_snapshot.json` | 22 键（逐份 ADR）+ `total`；**更新须带台账 entry id**（授权态写） |
| 4 | **冻结件 ①** 追加 SU | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | `## Status Update 2026-11-12`：取代声明（§⑤ 的「5」与 `:431` 的「已知不一致」段**自本 SU 起作废**）+ D-3 换算表 + `### ⑥ 三份 ADR 全名重述` 块（恰 3 bullet，每行唯一一个完整 token） |
| 5 | **冻结件 ②** 追加 SU | `docs/architecture/07-coherence-exploration-platform.md` | `## Status Update 2026-11-12`，**必须插在 `:569` 的 `## §末 文档索引` 之前**（⛔ 追加到文末 ⇒ 索引闸立刻红）；声明 `:501` 的「尚未落地」失效 + 指针改指新闸；随后 `generate_doc_index.py --write` 重生成 |
| 6 | **ctest 注册 + 守门表登记** | `test/CMakeLists.txt` + `scripts/README.md` | ctest 用例 **170 → 171**；README 守门表 **32 → 33** 行（**helper 非闸**，不登记） |
| 7 | **OpenSpec change + 归档** | `openspec/changes/cpptlm-cep-invariant-gate-teeth/` → `openspec/specs/cep-*` | 立项即占 KPI **1/3**；归档后 main spec 增 1 份 ⇒ `artifact_count` **78 → 79** |

**Out-of-Scope（⛔ 硬边界，逐条来自已裁裁决）**

- ⛔ **不改任何 ADR 正文**（只允许**追加 `## Status Update`**）
- ⛔ **新 SU 内不得出现 `### 决策 ` 标题**（否则 `ADR-CEP-00` 的 `^### 决策` 计数 4 → 5 ⇒ 破坏 R-d 快照自洽）
- ⛔ **不改 `test/cep_adr_parser.hh`**（`decision_head`（`:110`）= **invariant-id 提取器，不计数**；⛔ 只在文档中声明其为该角色）
- ⛔ **不改 `scripts/test/cep_docs_sync_check.sh:156`** 的正则（**历史形态**，只登记，不追认）
- ⛔ **期望值不得从被检对象自身派生**（读 ADR 自述再断言一遍 = 自证）；R-c 期望值 = **闸内源码常量**；R-d 期望值 = **闸外快照 JSON**
- ⛔ **不用关键词匹配**（`T1`/`T2`/`T3` 字面量等）
- ⛔ **R-c 不用决策块数**（与 R-d 口径**彻底解耦**，per D-9）
- ⛔ **不做 invariant 集快照**（`-02` 恰 11 条 `CEN-Ib*` 那种逐 owner 断言 = C3 之后的可选增强，需在 Python 重实现 P1-P5 提取）
- ⛔ 不改 B3b-2 计数（仍 **0/4**）· ⛔ 不动 C4（MUST-FIX-1/2）· ⛔ 不做 T3（Q2 残），但 C3 造成的计数变化**在本 change 内预登记**（T3 统一对齐，期间不算回归）
- ⛔ 不回改台账已提交 Entry（append-only）

## Why

**两个缺口都是「契约自认、机器沉默」型** —— 缺口已被写进契约，却没有任何检查会因为它被破坏而报红。

| 缺口 | 实测（可复核） | 不修的后果 |
|---|---|---|
| **R-c 机器检查不存在** | `ADR-CEP-00:434` 自认「`scripts/test/` 与 `test/` 内无对应载体」 | 6 行登记（触发条件表 3 行 + 新 SU 重述块 3 行）**可被静默删除**，无人报警 ⇒ 治理线最后一处「有登记、无牙齿」保持敞口 |
| **R-d 三个口径互不相等** | `ADR-CEP-00:431`：口径 5 / 标题口径 **4** / `decision_head` **0** | 决策块计数可被漂移、可被「掏空正文但不改标题」，且**无一处会红** |
| 「块数 ≥ 1」类判据**无鉴别力** | metis P1-9：删 8 条决策仍绿 | 判据形式上存在、实质上是恒真 ⇒ 属 §13.1 警告的「硬造闸」退化路径 |
| **07 号 是唯一入口却写着「尚未落地」** | `07 号:501`（SU 2026-11-04(#2) 内）逐字写「D0 堵法 ② 的机器检查**尚未落地**」 | R-c 落地后成为**错误事实陈述**，且读者沿指针进城会**先读到错句** |
| 影子树「复用」是**修辞不是机制** | `shadow_reset` 在 `.sh:52-57`（bash），Python 无法 import | 落地即成第二份 5 行实现，两份的复制集合不同步 ⇒ **隐性分叉** |

**已排除的替代路径（逐条有据，⛔ 本 change 不再讨论）**

| 被否方案 | 否决理由 |
|---|---|
| 期望值从 `NUMBER-MAP` 派生 | 自证复燃（NUMBER-MAP 随合法拆分更新） |
| 关键词闸（`T1`/`T2`/`T3` 字面量） | §13.1 硬造关键词闸 = P6 闸教训重演 |
| 并入 `cep_adr_structural_check.py` | registry 语义冲突（4 组以「可登记例外」为模型） |
| R-c 用决策块数 + 换算表 | 单一失败域被破坏（R-c/R-d 耦合 ⇒ 一处错则两处皆红） |
| 给 `-01/-02/-03` 补 id 锚 | 无必要（判别力主体是反向对账，不是存在性） |

**⛔ 不可约残余（必须写进 design.md 的诚实边界，不得补救、不得静默）**

1. 闸只验「**引用良形 + 被指物存在**」，**不验**「被指物真实现了该判定」；
2. 计数类判据防不住「**块数不变但 §2 正文被原位改写**」（R-d 快照同样防不住）—— 这是计数类判据的天花板；
3. 「拆出件」覆盖**同号派生件**（`ADR-CEP-02a` / `-01-addendum`）⇒ **新编号拆出件不在 R-c 覆盖内**（靠 ①`cep_adr_structural_check.sh` 的分区完整性 + ②`ADR-CEP-00 §3` supersede 表兜底）⇒ ⛔ 不得写成「R-c 全覆盖」，否则未来会当误红报。

## What Changes

| # | 变更 | 文件 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **R-d 段**：22 份 ADR 的 §2 决策块计数，按 **D-1 四步（顺序不可换）** + 两条显式红，与快照逐份比对 | `scripts/test/cep_decision_block_check.py` | ① 先剥 ` ``` ` 围栏 → ② 再取 §2 章范围 → ③ 仅 `###` 级 → ④ 匹配 `^###\s*决策\s*\d+(?:\.\d+)?`；**块 ID = 正则全匹配的编号串**（⛔ 禁前缀 / 子串 / 整数归一：`stoi("2.1")==2` 漏计、`grep -c "决策 1"` 双命中 `决策 10`） |
| 2 | **R-d 显式红 2 条** | 同上 | §2 内出现 `#{2,6}\s*决策\s*\d` 但**非 `###` 级** ⇒ 红;出现 `决策` 但编号**非 ASCII 十进制** ⇒ 红 |
| 3 | **R-c 段（双向对账，期望值 = 闸内常量）** | 同上 | **正向**：三份精确 basename 存在 + **无同号拆出件**（`^ADR-CEP-0[123](?![0-9])` 命中但不属三份精确名 ⇒ 红）;**反向**：C3-C 新 SU 的 `### ⑥` 重述块（`### ⑥` 至下一个 `###`/`##`，**恰 3 bullet，每行唯一一个完整 token**）内 `ADR-CEP-01`/`-02`/`-03` 各 ≥1 |
| 4 | **R-c 触发条件解析** | 同上 | `### ②` 之下**第一张表**：表头含「判定方式」列 + **恰 3 数据行** + 每行该列含 ≥1 inline code span 且按 **4 类**可解析：(a) 仓内路径 `os.path.exists` (b) `Entry #N`（台账有 `^### Entry #N`）(c) **ADR 短名** `ADR-CEP-NN` → glob `docs/adr/ADR-CEP-NN-*.md` (d) 符号 = 同格**最后一个**路径 + 在该文件命中定义式 |
| 5 | **影子树 helper（抽取）** | `scripts/test/cep_test_shadow.sh` | `reset <shadow> [extra_cp...]`;由 `cep_adr_structural_check.sh` 的 `shadow_reset` 提取 ⇒ 两闸 `source` 之;⛔ **两份复制集合必须同步**（structural 需 `deferrals.json`，新闸不需要 ⇒ helper 以 `[extra_cp...]` 参数化差异） |
| 6 | **§2 提取在 Python = 受控重复（登记）** | 同上（头注） | 与 C++ 版 `test/cep_adr_parser.hh:250` 的 `read_section` 并存 ⇒ **显式登记为受控重复**（先例：`cep_adr_parser.hh:169-173` 对 `no_inv_adr_files` 的双实现自登记）;⛔ 不改 C++ 版 |
| 7 | **快照（只读）** | `test/cep_decision_block_snapshot.json` | 22 键 + `total`(=125);闸**只读**;更新 = **带台账 entry id** 的授权态写（⛔ 无授权不得改） |
| 8 | **冻结件 ① 追加 SU** | `docs/adr/ADR-CEP-00-*.md` | 取代声明 + D-3 换算表（`-01` 3→3 · `-02` **4→3** · `-03` 3→3;第三个值 `:30`「锁定 3 件事」= **历史措辞，不构成口径**）+ `### ⑥` 重述块 + **D1 第 5 步**（同步更新 R-c 闸期望值常量 ⇒ 「合法拆分 ⇒ 闸红 ⇒ 更新期望值」成为**强制函数**而非误红）;⛔ 新 SU 内不得有 `### 决策 ` 标题 |
| 9 | **冻结件 ② 追加 SU** | `docs/architecture/07-*.md` | **插在 `:569` 之前**（⛔ 非文末）+ 声明 `:501` 失效 + 指针改指新闸 + `generate_doc_index.py --write` 重生成 + 自述 **28/15 → 29/16** |
| 10 | **ctest + 守门表 + 数字预登记** | `test/CMakeLists.txt` + `scripts/README.md` | ctest **170 → 171**;守门表 **32 → 33** 行;⚠️ 磁盘 `cep_*.sh` **36 → 38**（helper 非闸）⇒ `AGENTS.md` 3 处本就写「数量以 `ls` 为准」⇒ **AGENTS.md 无需改数字**（实测订正见下） |
| 11 | **五步链（2 冻结件同一次）** | 冻结面 | **四拍**：0) 先落 builder 自己的 C3 台账 entry 1) 编辑 2 件 2) **commit 两件** 3) `--update --authorized-by "Entry #N"` 4) **commit manifest**（`frozen_at_commit ≠ HEAD` 时的 ⚠️ 是**预期**） |
| 12 | **归档 + 无条件第二次 `--update`** | `openspec/specs/cep-*` | `FROZEN_GLOBS` 含 `openspec/specs/cep-*/spec.md` 全量 glob ⇒ **必然触** ⇒ `artifact_count` **78 → 79**;归档到 `--update` 之间 `cep_freeze_check` + `cep_docs_sync_check` **预期双红** |

**canonical 实现宿主声明（防「仓内并存 4~5 个决策块定义」）**：决策块计数的 **canonical 实现 = 本次新闸的 Python 实现**;`ADR-CEP-00:431` 的「标题口径 4」= 该实现的**文档镜像**;`test/cep_adr_parser.hh:110` 的 `decision_head` = **invariant-id 提取器、不计数**（⛔ 不改）;`scripts/test/cep_docs_sync_check.sh:156` 的正则 = **历史形态**（不追认、不改）。

**⚠️ 实测订正（对 Entry #361 ⑥ P1-12 的补正，不回改该 Entry）**：该条写「新闸 ⇒ `AGENTS.md` 36→**37**」⇒ **实测不成立** —— `AGENTS.md:217` / `:231` / `:618` 三处**均写「数量以 `ls scripts/test/cep_*.sh` 为准」**，**无硬编码 36** ⇒ 真实只需对齐 `scripts/README.md` 守门表（32→33），且磁盘数 36→38 含 1 个非闸 helper（README `:81` 既有口径「表内计数 ≠ 磁盘实际，差异项以 `ls` 为准」⇒ helper 属**预期差异**，须在 T3 一并登记）。

## Acceptance

> ✅ 每条变异 = 「**先跑基线正控（必须绿）** ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」。⛔ 「rc≠0」不构成红因。

**A 组 · R-d（计数口径，4 变异）**

- [ ] **AC-1（基线正控）**：未做任何变异前 `bash scripts/test/cep_decision_block_check.sh` **rc=0**（证据落 builder 的 C3 台账 entry；⛔ 无此正控 ⇒ AC-4~AC-7 无效）
- [ ] **AC-2（R-d 正向）**：`ADR-CEP-00` 按 **D-1 四步**计数 = **4**（⛔ 非 §⑤ 口径的 5）
- [ ] **AC-3（R-d 快照）**：22 份 ADR **逐份**计数 == `test/cep_decision_block_snapshot.json` 的 22 键，且 `total` == **125**（实测基线：`-00`4 `-00A`3 `-01`3 `-02`3 `-03`3 `-04`3 `-05`4 `-06`**7** `-07`9 `-07A`4 `-08`7 `-09`8 `-11`6 `-12`10 `-13`6 `-14`6 `-15`5 `-16`5 `-17`10 `-18`6 `-19`9 `NUMBER-MAP`4）⇒ ⛔ 判据是「**逐份相等**」，⛔ 不是「≥1」（那几乎恒真，删 8 条仍绿）
- [ ] **AC-4（变异① · 顺序判据）**：在某 ADR 的 §2 内插入一个 ` ``` ` 围栏块且**块内含行首 `## `** ⇒ **必红**，红因签名 = **§2 范围被围栏内 `## ` 截断 ⇒ 决策块少计**（⛔ 若实现「先取范围后剥围栏」，此变异**不会红** ⇒ 判据实现错）
- [ ] **AC-5（变异② · 非 `###` 级）**：§2 内插入 `#### 决策 5:…` ⇒ **必红**，红因签名 = **决策标题非 `###` 级**
- [ ] **AC-6（变异③ · 非 ASCII 十进制）**：§2 内插入 `### 决策 Ⅻ` ⇒ **必红**，红因签名 = **决策编号非 ASCII 十进制**
- [ ] **AC-7（变异④ · 快照偏离）**：删掉某份 ADR 的一个 `### 决策 N` 段 ⇒ **必红**，红因签名 = **计数快照偏离**（须含文件名 + 期望值 + 实得值 + 差值）
- [ ] **AC-8（R-d 还原）**：AC-4~AC-7 的 4 条变异**逐一还原**后 **rc=0**（⛔ 4/4 还原后必须绿，否则闸有副作用）

**B 组 · R-c（双向对账，3 变异）**

- [ ] **AC-9（R-c 正向）**：三份精确 basename（`ADR-CEP-01-fabric-collective-plane.md` / `ADR-CEP-02-coherence-barrier-semantics-clarification.md` / `ADR-CEP-03-fabric-collective-plane-internals.md`）存在，且**无同号拆出件**（命中 `^ADR-CEP-0[123](?![0-9])` 而不属三份精确名 ⇒ 红）
- [ ] **AC-10（R-c 反向）**：C3-C 新 SU 的 `### ⑥` 重述块（`### ⑥` 标题至下一个 `###`/`##`）**恰 3 bullet**、每行**唯一一个完整编号 token**、`ADR-CEP-01`/`-02`/`-03` **各 ≥1** ⇒ 绿；期望值 = **闸内源码常量**（⛔ 禁从 `NUMBER-MAP` 派生）
- [ ] **AC-11（触发条件可解析）**：`### ②` 之下**第一张表**表头含「判定方式」列 + **恰 3 数据行** + 每行该列含 ≥1 inline code span 且按 **4 类**（仓内路径 / `Entry #N` / **ADR 短名 glob** / 符号）**至少一个可解析** ⇒ 绿（实测今日全绿：T1/T2 行 = `ADR-CEP-01` ⇒ 类 (c);T3 行 = `test/cep_adr_parser.hh` + `owner_specs()` ⇒ 类 (a)+(d)）
- [ ] **AC-12（变异① · 反向缺项）**：从重述块删掉一行（例 `ADR-CEP-02`）⇒ **必红**，红因签名 = **反向对账缺项**
- [ ] **AC-13（变异② · 同号拆出件 · 双闸）**：影子树造 `ADR-CEP-02a-*.md` ⇒ 新闸 **必红**（红因签名 = **存在同号拆出件**）**且** `cep_adr_structural_check.sh` **同时红** ⇒ **两条闸各报各的签名，不互相掩盖**（⛔ 判据形态不成立 ⇒ 红，⛔ 不得跳过）
- [ ] **AC-14（变异③ · 引用解析缺失）**：把 ② 中某行「判定方式」改为空 ⇒ **必红**，红因签名 = **引用解析缺失**
- [ ] **AC-15（R-c 还原）**：AC-12~AC-14 的 3 条变异**逐一还原**后 **rc=0**
- [ ] **AC-16（不扰动 R-d · metis P1-11）**：`ADR-CEP-00` 的 `^### 决策` 计数**仍 = 4**、22 份合计**仍 = 125**（⇒ 新 SU 内 ⛔ 不得出现 `### 决策 ` 标题）

**C 组 · 闸 / 冻结面 / 归档**

- [ ] **AC-17（ctest 注册）**：`ctest -N` **见新闸名** + 用例总数 **170 → 171**
- [ ] **AC-18（07 号 索引位置）**：新 SU **插在 `:569` 的 `## §末 文档索引` 之前**（⛔ 非文末）+ `python3 scripts/cep/generate_doc_index.py --write` + `bash scripts/test/cep_index_position_check.sh` **rc=0** + 自述 **28/15 → 29/16**
- [ ] **AC-19（冻结链 · 五步四拍）**：2 冻结件**同一次** `--update --authorized-by "Entry #N"` + **无参**复验 `cep_freeze_check.sh` **rc=0** + `authorized_by` 精确匹配
- [ ] **AC-20（归档 + 第二次 `--update`）**：delta 并入 `openspec/specs/cep-*` 后**无条件**第二次 `--update` ⇒ `artifact_count` **78 → 79**;⚠️ 归档到 `--update` 之间 `cep_freeze_check` + `cep_docs_sync_check` **预期双红**（登记为预期，不得当回归）
- [ ] **AC-21（0 回归）**：`ctest --test-dir build -j4` **171/171** + **全部** `cep_*.sh` 闸 rc=0 + `bash scripts/build/format.sh --check` rc=0 + `bash scripts/test/docs_sync_check.sh --strict` rc=0
- [ ] **AC-22（数字预登记 · metis P1-12）**：`scripts/README.md` 守门表 **+1 行**（32→33，含**新闸**）;磁盘 `cep_*.sh` **36→38**（含**非闸** helper `cep_test_shadow.sh`）⇒ 差异按 README `:81` 既有口径登记，**T3 统一对齐，期间不算回归**;⛔ `AGENTS.md` 数字不动（实测其 3 处均以 `ls` 为准）

## Capabilities

- **治理机制机器化（gates）**：R-c / R-d 从「**有登记**」升级为「**有牙齿的机器判据**」⇒ 治理线 V6「有登记、无牙齿」由本 change **闭合**（G5 达成）。
- **决策块口径单一化**：仓内不再并存 4~5 个「决策块」定义 —— canonical 宿主 = 新闸的 Python 实现;`ADR-CEP-00:431` 的「标题口径 4」= **文档镜像**;`decision_head` = **invariant-id 提取器**（不计数）;`cep_docs_sync_check.sh:156` = **历史形态**。
- **残余登记不可静默删除**：触发条件表 3 行 + 重述块 3 行共 **6 行**受机器守护（每条都有决定性变异 + 红因签名）。
- **影子树复用不再分叉**：`cep_test_shadow.sh` 是**具名机制**（⛔ 不是修辞）⇒ 两闸的复制集合由**同一实现**参数化 ⇒ 消除「两份各自维护」的第二类隐性分叉。
- **入口文档不说谎**：`07 号:501` 的「尚未落地」被新 SU 明确作废 ⇒ 唯一入口不再先读到错句（G1 轻度偏离已如实登记）。
- **残余诚实**：不可约残余（引用良形 ≠ 语义实现；计数类判据防不住正文原位改写；新编号拆出件不在 R-c 覆盖内）**全部写入 design.md 的诚实边界段** ⇒ ⛔ 不把「不可判 / 不空转」伪装成「已覆盖 / 已证明」。

## Impact

| 面 | 影响 |
|---|---|
| **冻结面（2 处）** | ⚠️ `docs/adr/ADR-CEP-00-*.md`（`docs/adr/ADR-CEP-*.md` glob）+ ⚠️ `docs/architecture/07-coherence-exploration-platform.md` ⇒ **同一次** `--update`（C2 先例：3 处同一次）;`artifact_count` **不变**（78，无新增件）⇒ 78→79 只发生在归档那一次 |
| **KPI 槽** | **0/3 → 1/3**（立项即占）;⛔ C4 须待本 change 归档后才可立项（D1「Proposed 并发 ≤ 1」外的 CEP 串行约束） |
| **闸 / 用例数** | ctest 注册闸 **36 → 37**;ctest 用例 **170 → 171**;磁盘 `cep_*.sh` **36 → 38**（+1 闸 +1 非闸 helper） |
| **文档** | `scripts/README.md` 守门表 **32 → 33**;`AGENTS.md` **数字不动**（实测其 3 处以 `ls` 为准）;`07 号` 索引自述 **28/15 → 29/16**（机械生成，⛔ 零手抄） |
| **归档** | `openspec/specs/cep-*` **+1 份 main spec** ⇒ `artifact_count` **78 → 79** |
| **成本收益** | **防 6 行登记被静默删除 vs 2 冻结面 + 1 槽** —— 6 行 = 触发条件表 3 数据行（T1/T2/T3）+ 新 SU `### ⑥` 重述块 3 行（`-01`/`-02`/`-03`）;成本 = 2 处冻结件（各追加 1 则 SU，走同一次五步链）+ 1 个 KPI 槽 + 2 个新脚本 |
| **下游** | C4（MUST-FIX-1/2）在其后;T3（Q2 残 / README 数字与三表划分说明）在 C3 之后（⛔ 排前必做两遍） |

## 已裁项（D-1~D-9 · ⛔ 本草案不重新讨论）

| # | 裁决 | 落点 |
|:--:|---|---|
| D-1 | R-d 机器口径 = **四步且顺序不可换**（先剥围栏 → 再取 §2 → 仅 `###` → 匹配决策号）+ **两条显式红** | AC-2 / AC-4~AC-6 / What Changes #1-2 |
| D-2 | 子决策**独立计块** + **块 ID = 正则全匹配**（⛔ 禁前缀/子串/整数归一） | AC-3 / What Changes #1 |
| D-3 | 口径值 `ADR-CEP-00` = **4**;换算 `-01` 3→3 · `-02` **4→3** · `-03` 3→3;第三个值 `:30`「锁定 3 件事」= 历史措辞 | What Changes #8 |
| D-4 | C3 **冻结面 = 是**（口径 5→4 必须落进 ADR-CEP-00 ⇒ 五步链）⇒ **被 D-7 扩为 2 处** | What Changes #4 / #11 |
| D-5 | Q2 降级为**一句话订正**（自述数字 + 三表划分说明）⇒ 排 C3 之后 | Impact（文档行） |
| D-6 | R-c = **双向对账 + 闸内常量**（正向 basename/无拆出件 · 反向重述块 · 触发条件 4 类可解析） | AC-9~AC-14 / What Changes #3-4 |
| D-7 | 冻结面 = **2 处**（`ADR-CEP-00` + `07 号`，同一次 `--update`） | What Changes #4-5 / #11 |
| D-8 | **新建** `cep_decision_block_check.{sh,py}` + **抽** `cep_test_shadow.sh` + §2 提取**登记为受控重复** | What Changes #1 / #5 / #6 |
| D-9 | R-c **不用决策块数**（与 R-d 解耦；判别力主体 = 反向对账） | What Changes #3 / Capabilities |

## 处置对照（metis 1 轮 · 12 条 · Entry #361 ⑥）

| # | metis finding | 处置（本草案落点） |
|:--:|---|---|
| P0-1 | 重述块不定界 ⇒ 反向对账恒真 | ✅ 定界为 `### ⑥` + 恰 3 bullet + 每行唯一 token（AC-10 / What Changes #3） |
| P0-2 | 三类解析首日红（T1 行唯一 span = `ADR-CEP-01`） | ✅ 补 (c) ADR 短名 glob 类 ⇒ 今日全绿且非恒真（AC-11） |
| P0-3 | 「符号」无边界（一格两路径 ⇒ 归属二义） | ✅ 规则写死 = 同格**最后一个**路径 + 定义式命中（What Changes #4） |
| P0-4 | 快照的存储 / 维护 / 授权全缺 | ✅ 落 `test/cep_decision_block_snapshot.json`，闸**只读**，更新须带 entry id（AC-3 / What Changes #7） |
| P0-5 | 125 vs 124 自相矛盾 | ✅ 实测三口径全 = **125**（无差异文件）⇒ 本草案以 **125** 为准 |
| P0-6 | C3-P0 把 planner 的活推给 builder | ✅ P0a(planner) / P0b(builder) 两步（本 improvement = P0a 产物） |
| P0-7 | 无 ctest 注册 AC / 五步链缺 commit 次序 / 归档写「条件性」 | ✅ AC-17 / AC-19（四拍）/ AC-20（**无条件**第二次 `--update`） |
| P0-8 | `07 号` 追加到文末 ⇒ 索引闸立刻红 | ✅ **插在 `:569` 之前** + 重生成 + 28/15→29/16（AC-18） |
| P0-9 | 影子树双根未定义 ⇒ 变异按错误原因红 | ✅ **文档语料 = `--root`（影子）；被指物存在性 / 台账解析 = 真实仓根**（What Changes #3-4） |
| P1-10 | 「自带 4 行」是被否决路径，不该留成选项 | ✅ D-8 钉死「抽 helper」（What Changes #5） |
| P1-11 | 缺「不扰动 R-d 快照」的 AC | ✅ **AC-16**（计数仍 4 / 合计仍 125） |
| P1-12 | 缺文档数字滞后的预登记 | ✅ **AC-22**（+ ⚠️ 实测订正：`AGENTS.md` 无硬编码 36 ⇒ 只需 README 对齐） |
| P2 | 封盘 / proposal 成本收益 / 07 号 措辞诚实边界 | ✅ **D-10 起不再新增裁决**;成本收益行已写入 **Impact**;诚实边界写入 **Why 的「不可约残余」** |
