# cpptlm-cep-invariant-gate-teeth: 治理机制机器化 —— R-c / R-d 两项残留登记的牙齿

> **状态**: 🟡 **Proposed**(2026-11-12 立项 —— per 用户口令「启动 C3 实施」+「同意你的建议，请推进」;KPI 并发 **0/3 → 1/3**)
> **立项依据**: `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md`(7 段草稿 + **22 条 AC** · HARD-GATE rc=0)· 台账 **Entry #362**(P0a 落账)· **Entry #361**(D-6~D-9 + C3a/C3b 切片 + metis 12 条)· **Entry #359**(D-1~D-5)· `.rddf/reviews/cep-goal-scope-review-2026-11-12c.md`(§五 4 处裁决 + §七 oracle 4 条判定)
> **上游决策(已裁决,本 change ⛔ 不重新讨论)**: **D-1**(R-d 四步口径 + 两条显式红)· **D-2**(子决策独立计块 + 块 ID 全匹配)· **D-3**(口径值 4 + 三值换算表)· **D-4/D-7**(冻结面 = **2 处** · 同一次 `--update`)· **D-5**(Q2 降级为一句话订正 · 排本 change 之后)· **D-6**(R-c 双向对账 + 闸内常量)· **D-8**(新建闸 + 抽 `cep_test_shadow.sh` + §2 提取登记为受控重复)· **D-9**(R-c 不用决策块数)
> **估时**: ≈3-5 人日(C3a ≈0.5d · C3b ≈1.5d · C3-E 五步链 ≈0.5d · C3-F 归档 ≈0.5d)

## Why

**两个缺口都是「契约自认、机器沉默」型** —— 缺口已被写进契约，却没有任何检查会因为它被破坏而报红。

| 缺口 | 契约依据（逐字可核） | 实测 | 不修的后果 |
|---|---|---|---|
| **R-c** = 「一决策一文档化」残余登记 + 3 条触发条件**无机器检查** | `ADR-CEP-00:434`（SU 2026-11-11 ⑤）：「**D0 堵法 ②（机器检查）尚未落地**…断言「架构真源含 3 份 ADR 的残留登记 + 3 条触发条件」的检查**当前不存在**（`scripts/test/` 与 `test/` 内无对应载体）⇒ **残留仍可被静默删除** ⇒ 登记为**待办**」 | `scripts/test/` 内无任何 `decision_block` / `residual` 闸 | 6 行登记（触发条件表 3 行 + 新 SU 重述块 3 行）**可被静默删除**，无人报警 |
| **R-d** = 决策块计数口径**三个互不相等的实现** | `ADR-CEP-00:431`：口径 **5** / 标题口径 **4** / `test/cep_adr_parser.hh` 的 `decision_head` **计 0** ⇒ **三者互不相等**;规范载体 = `:426` 的 §⑤（**唯一规范载体**） | 22 份 ADR 的 `^### 决策` 计数合计 **125**（三口径实测全一致） | 决策块计数可被漂移 / 可被「掏空正文但不改标题」，**无一处会红** |

**为什么是现在**：V1~V5 已闭合，V6「有登记、无牙齿」是**治理线唯一活口**;V3（DoD 降格 = 假绿）是最贵的后果 ⇒ 顺序永远是「**先立判据、再搬内容**」。文档治理再做一轮收益 ≈ 0 ⇒ 本 change 只做**判据机器化**，⛔ 不碰任何内容搬运。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **新闸**（R-d 计数快照 + R-c 双向对账，**同一脚本两段**） | `scripts/test/cep_decision_block_check.{sh,py}` | ⛔ **不并入** `cep_adr_structural_check.py`（后者 4 组以「可登记例外」为模型，R-c/R-d 的对象不在其分母里 ⇒ 两套语义共享一个 registry 比两个脚本更糟） |
| 2 | **抽影子树 helper**（⛔ 抽，**不复制**） | `scripts/test/cep_test_shadow.sh`（`reset <shadow> [extra_cp...]`） | 由 `cep_adr_structural_check.sh:52-57` 的 `shadow_reset` **提取**;两闸 `source` 之;⛔ 不留「自带 4 行」选项 |
| 3 | **计数快照**（闸**只读**） | `test/cep_decision_block_snapshot.json` | 22 键（逐份 basename）+ `total`(=125);更新须带台账 `Entry #N` 授权号 |
| 4 | **冻结件 ①** 追加 SU | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | `## Status Update 2026-11-12`：取代声明（§⑤ 的「5」与 `:431` 的「已知不一致」段**自本 SU 起作废**）+ D-3 三值换算表 + `### ⑥ 三份 ADR 全名重述` 块（**恰 3 bullet，每行唯一一个完整 token**）;⛔ 新 SU 内**不得**出现 `### 决策 ` 标题 |
| 5 | **冻结件 ②** 追加 SU | `docs/architecture/07-coherence-exploration-platform.md` | `## Status Update 2026-11-12`，**必须插在 `:569` 的 `## §末 文档索引` 之前**（⛔ 追加到文末 ⇒ 索引闸立刻红）;声明 `:501` 的「尚未落地」失效 + 指针改指新闸;随后 `generate_doc_index.py --write` 重生成（自述 **28/15 → 29/16**） |
| 6 | **ctest 注册 + 守门表登记** | `test/CMakeLists.txt` + `scripts/README.md` | ctest 用例 **170 → 171**;守门表 **32 → 33** 行（**helper 非闸**，不登记）;⚠️ 磁盘 `cep_*.sh` **36 → 38** |
| 7 | **契约 delta** | `openspec/changes/cpptlm-cep-invariant-gate-teeth/specs/cep-invariant-gate/spec.md` | **ADDED** capability `cep-invariant-gate` ⇒ 归档后落 `openspec/specs/cep-invariant-gate/spec.md` ⇒ **触** `openspec/specs/cep-*/spec.md` 冻结 glob ⇒ `artifact_count` **78 → 79** |
| 8 | **冻结面五步链** | 冻结面 | **四拍**：0) 先落 builder 自己的 C3 台账 entry 1) 编辑 2 件 2) **commit 两件** 3) `--update --authorized-by "Entry #N"` 4) **commit manifest** |

> ⚠️ **冻结面 = 2 处**（D-7，订正 #359 的 D-4）：`docs/adr/ADR-CEP-00-*.md`（`docs/adr/ADR-CEP-*.md` glob）+ `docs/architecture/07-coherence-exploration-platform.md` ⇒ **同一次** `--update`（C2 先例：3 处同一次）。
> ✅ `scripts/test/cep_decision_block_check.{sh,py}` / `scripts/test/cep_test_shadow.sh` / `test/cep_decision_block_snapshot.json` **均不在**冻结集（已核 manifest globs）。
> ⚠️ **「2 处」不改变 artifact_count**：两处都是**既有**产物的重算（78 不变）⇒ 78→79 只发生在**归档那一次**（新 main spec 入 glob）。

## 验收标准(Acceptance Criteria)

> 判据来源 = `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md` 的 **22 条 AC**（**逐条同源**，本节为转录，⛔ 不新增、不放宽）。
> **统一判据纪律**:每条变异 = 「**先跑基线正控（必须绿）** ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」;⛔ **「rc≠0」不构成红因**;⛔ 不用关键词匹配;⛔ 期望值不得从被检对象自身派生。

**A 组 · R-d（计数口径 · 4 变异）**

- [ ] **AC-1（基线正控）**:未做任何变异前 `bash scripts/test/cep_decision_block_check.sh` **rc=0**（证据落 builder 的 C3 台账 entry;⛔ 无此正控 ⇒ AC-4~AC-7 无效）
- [ ] **AC-2（R-d 正向）**:`ADR-CEP-00` 按 **D-1 四步**计数 = **4**（⛔ 非 §⑤ 口径的 5）
- [ ] **AC-3（R-d 快照）**:22 份 ADR **逐份**计数 == `test/cep_decision_block_snapshot.json` 的 22 键，且 `total` == **125** ⇒ ⛔ 判据是「**逐份相等**」，⛔ 不是「≥1」
- [ ] **AC-4（变异① · 围栏危险）**:某 ADR 的 §2 内插入一个 ` ``` ` 围栏块且**块内含行首 `## `** ⇒ **必红**，红因签名 = **围栏内含行首 `## `（会截断 §2 范围 ⇒ 决策块静默少计）**
- [ ] **AC-5（变异② · 非 `###` 级）**:§2 内插入 `#### 决策 5:…` ⇒ **必红**，红因签名 = **决策标题非 `###` 级**
- [ ] **AC-6（变异③ · 非 ASCII 十进制）**:§2 内插入 `### 决策 Ⅻ` ⇒ **必红**，红因签名 = **决策块编号非法（须 ASCII 十进制且全匹配）**
- [ ] **AC-7（变异④ · 快照偏离）**:删掉某份 ADR 的一个 `### 决策 N` 段 ⇒ **必红**，红因签名 = **计数快照偏离**（须含文件名 + 期望值 + 实得值 + 差值）
- [ ] **AC-8（R-d 还原）**:AC-4~AC-7 的 4 条变异**逐一还原**后 **rc=0**

**B 组 · R-c（双向对账 · 3 变异）**

- [ ] **AC-9（R-c 正向）**:三份精确 basename 存在，且**无同号拆出件**（命中 `^ADR-CEP-0[123](?![0-9])` 而不属三份精确名 ⇒ 红）
- [ ] **AC-10（R-c 反向）**:新 SU 的 `### ⑥` 重述块**恰 3 bullet**、每行**唯一一个完整编号 token**、`ADR-CEP-01`/`-02`/`-03` **各 ≥1** ⇒ 绿;期望值 = **闸内源码常量**（⛔ 禁从 `NUMBER-MAP` 派生）
- [ ] **AC-11（触发条件可解析）**:`### ②` 之下**第一张表**表头含「判定方式」列 + **恰 3 数据行** + 每行该列含 ≥1 inline code span 且按 **4 类**（仓内路径 / `Entry #N` / **ADR 短名 glob** / 符号）**至少一个可解析** ⇒ 绿
- [ ] **AC-12（变异① · 反向缺项）**:从重述块删掉一行（例 `ADR-CEP-02`）⇒ **必红**，红因签名 = **重述块缺项**
- [ ] **AC-13（变异② · 同号拆出件 · 双闸）**:影子树造 `ADR-CEP-02a-*.md` ⇒ 新闸 **必红**（签名 = **存在同号拆出件**）**且** `cep_adr_structural_check.sh` **同时红** ⇒ **两条闸各报各的签名，不互相掩盖**
- [ ] **AC-14（变异③ · 引用解析缺失）**:把某行「判定方式」改为空 ⇒ **必红**，红因签名 = **引用解析缺失**
- [ ] **AC-15（R-c 还原）**:AC-12~AC-14 的 3 条变异**逐一还原**后 **rc=0**
- [ ] **AC-16（不扰动 R-d）**:`ADR-CEP-00` 的 `^### 决策` 计数**仍 = 4**、22 份合计**仍 = 125**

**C 组 · 闸 / 冻结面 / 归档**

- [ ] **AC-17（ctest 注册）**:`ctest -N` **见新闸名** + 用例总数 **170 → 171**
- [ ] **AC-18（07 号 索引位置）**:新 SU **插在 `:569` 之前** + `generate_doc_index.py --write` + `cep_index_position_check.sh` **rc=0** + 自述 **28/15 → 29/16**
- [ ] **AC-19（冻结链 · 五步四拍）**:2 冻结件**同一次** `--update --authorized-by "Entry #N"` + **无参**复验 `cep_freeze_check.sh` **rc=0** + `authorized_by` 精确匹配
- [ ] **AC-20（归档 + 第二次 `--update`）**:delta 并入 `openspec/specs/cep-*` 后**无条件**第二次 `--update` ⇒ `artifact_count` **78 → 79**;⚠️ 归档到 `--update` 之间 `cep_freeze_check` + `cep_docs_sync_check` **预期双红**（登记为预期，不得当回归）
- [ ] **AC-21（0 回归）**:`ctest --test-dir build -j4` **171/171** + **全部** `cep_*.sh` 闸 rc=0 + `format.sh --check` rc=0 + `docs_sync_check.sh --strict` rc=0
- [ ] **AC-22（数字预登记）**:`scripts/README.md` 守门表 **+1 行**（32→33）;磁盘 `cep_*.sh` **36→38**（含**非闸** helper）⇒ 按 README `:81` 既有口径登记，**T3 统一对齐，期间不算回归**;⛔ `AGENTS.md` 数字不动（实测其 3 处均以 `ls` 为准）

## 成本收益

**防 6 行登记被静默删除 vs 2 冻结面 + 1 槽** —— 6 行 = 触发条件表 **3 数据行**（T1/T2/T3）+ 新 SU `### ⑥` 重述块 **3 行**（`-01`/`-02`/`-03`）;成本 = **2 处冻结件**（各追加 1 则 SU，走同一次五步链）+ **1 个 KPI 槽** + 2 个新脚本 + 1 份快照。

## 诚实边界（⛔ 不可约残余，必须落进 design.md）

1. 闸只验「**引用良形 + 被指物存在**」，**不验**「被指物真实现了该判定」;
2. 计数类判据防不住「**块数不变但 §2 正文被原位改写**」⇒ 这是计数类判据的天花板;
3. 「拆出件」覆盖**同号派生件** ⇒ **新编号拆出件不在 R-c 覆盖内**（靠 `cep_adr_structural_check.sh` 分区完整性 + `ADR-CEP-00 §3` supersede 表兜底）⇒ ⛔ 不得写成「R-c 全覆盖」。

## 关联

台账 **Entry #362**(P0a)· **#361**(D-6~D-9 + 切片)· **#359**(D-1~D-5)· **#355**(C2 · 五步链先例)· **#338**(R-c 授权)· `ADR-CEP-00:396-:436`(SU ①~⑤)· `:431`(已知不一致)· `:434`(R-c 缺口自认)· `07 号:501`(旧规格)· `07 号:569`(索引)· `scripts/test/cep_adr_structural_check.sh:52-57`(shadow_reset)· `test/cep_adr_parser.hh:110`/`:169-173`/`:250` · `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md`
