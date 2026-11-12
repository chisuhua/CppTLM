# tasks —— `cpptlm-cep-invariant-gate-teeth`

> 顺序 = **A0(授权门) → A1(影子树 helper) → A2(R-d 闸 + 4 变异) → A3(ctest + 守门表) → A4(冻结件① SU) → A5(冻结件② SU + 索引) → A6(冻结件 commit) → A7(R-c 段 + 3 变异) → A8(五步链 --update + manifest commit) → A9(契约 delta) → A10(0 回归) → A11(归档 + 第二次 --update)**;
> 门:**G1 = A0** · **G2 = A2**(R-d 有牙)· **G3 = A4+A5**(2 冻结件 SU 落盘)· **G4 = A7**(R-c 有牙)· **G5 = A8**(冻结链)· **G6 = A9~A11**(delta + 归档)。
> 里程碑:**M1 helper 就位 → M2 R-d 有牙 → M3 冻结件落盘 → M4 R-c 有牙 → M5 冻结链闭合 → M6 归档**。

> ⚠️ **TDD 5 步纪律**(per `rdd-workflow-writing-plans`):每条判据 = **红(先写变异) → 绿(实现) → 净(0 回归)**;
> ⛔ **不得**先写实现再补变异(那样变异是「照着实现写的」⇒ 无法证明判据有牙)。
> ⚠️ **AC 逐条映射见 `proposal.md` 的「验收标准」段**(22 条 · 与 `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md` 同源);本文件是**可勾选载体**。

---

## A0 · 授权门(G1)

- [x] A0.1 台账 Entry 授权本 change 的实施(占 **D1 的 1 个 Proposed 槽位** —— 立项时即占,当前 **1/3**);⚠️ **C4 须待本 change 归档后才可立项**(CEP 串行约束)
      > ⚠️ **授权号 = 本 change 的 builder Entry**(A8.3 的 `--authorized-by` 指向它)⇒ ⛔ **不用 `#361`**(`#361` = 设计裁决,非实施授权)
- [x] A0.2 `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md` 已落盘 + **HARD-GATE rc=0**(per Entry #362 ①)
      > ✅ 实测:`pre_create_brainstorm_check.sh` ⇒ `✅ brainstorm HARD-GATE satisfied`(rc=0)
- [x] A0.3 `.rddf/state/.planner-handoff.json` 已由 **canonical 写者**产出(per Entry #362 ② 的 B-3 收口步)
      > ✅ 实测:`schema=planner-handoff-v1` / `recommended_route=complex` / `awaiting_builder=["add-cpptlm-cep-invariant-gate-teeth"]`
- [x] A0.4 **立项前**确认 D-1 四步提取口径与快照值一致(Entry #361 ⑤ 的「立项前」项)
      > ✅ 实测:22 份 **逐份**计数与快照全等,合计 **125**;**三口径**(全文件 / 先剥围栏取 §2 / 先取范围剥围栏)**逐份相等**;显式红 **0**;危险围栏 **0**
- [x] A0.5 基线零改动确认(`git status` 仅本 change 的产物)

---

## A1 · 影子树 helper 抽取(per design **D-2**)

- [x] A1.1 新建 `scripts/test/cep_test_shadow.sh`:`reset <shadow_dir> [extra_cp...]`
      > ⚠️ 必复制集合 = `docs/adr`(两闸共用,单一真源);差异复制 = `extra_cp...` 参数化
- [x] A1.2 `cep_adr_structural_check.sh:52-57` 的 `shadow_reset()` **改为** `source` helper + 调 `reset "$SHADOW" test/cep_adr_structural_deferrals.json`
      > ⛔ **不复制第二份**;⛔ 新闸**不得**自带复制逻辑
- [x] A1.3 helper 头注登记「**共享影子树机制** ⇒ 改动须同步两闸」
- [x] A1.4 **回归**:`cep_adr_structural_check.sh` 无参 rc=0(⛔ 抽取不得改变既有闸行为)

---

## A2 · R-d 闸(计数口径 · 4 变异)(per design **D-3 / D-4 / D-8**)

- [x] A2.1 **红 · 快照文件**:`test/cep_decision_block_snapshot.json` 落盘(22 键 + `total`=125 + `authorized_by`)
- [x] A2.2 **绿 · 实现**:`cep_decision_block_check.py` 的 R-d 段 —— 四步口径 + 块 ID 全匹配 + 显式红 ①② + 围栏危险标记 ③
- [x] A2.3 **绿 · 快照校验**:`schema` / `count` / `total` / `authorized_by`（台账 Entry 必须存在）
- [x] A2.4 **基线正控**(AC-1):无变异 ⇒ rc=0
- [x] A2.5 **红 · AC-4**(围栏危险):某 ADR 的 §2 内插 ` ``` ` 围栏块且块内含行首 `## ` ⇒ 必红,签名 `围栏内含行首 ##`
- [x] A2.6 **红 · AC-5**(非 `###` 级):插 `#### 决策 5:…` ⇒ 必红,签名 `决策块标题非 ### 级`
- [x] A2.7 **红 · AC-6**(非 ASCII 十进制):插 `### 决策 Ⅻ` ⇒ 必红,签名 `决策块编号非法`
- [x] A2.8 **红 · AC-7**(快照偏离):删某份 ADR 的一个 `### 决策 N` ⇒ 必红,签名 `决策块计数快照偏离`(含文件名/期望/实得/差值)
- [x] A2.9 **还原**(AC-8):4 条逐一还原 ⇒ rc=0
- [x] A2.10 **AC-2 / AC-3** 机器证据:`ADR-CEP-00` = 4;22 份逐份 == 快照;`total` = 125

---

## A3 · ctest 注册 + 守门表(per design **D-1**)

- [x] A3.1 `test/CMakeLists.txt` 注册新闸 ⇒ `ctest -N` 见其名 + 总数 **170 → 171**(AC-17)
- [x] A3.2 `scripts/README.md` 守门表 **+1 行**(32→33,含**新闸**);⚠️ **helper 非闸** ⇒ 不登记(AC-22)
- [x] A3.3 数字预登记:磁盘 `cep_*.sh` **36→38**;⛔ `AGENTS.md` 数字不动(实测其 3 处以 `ls` 为准)⇒ 记入 builder Entry ⇒ **T3 对齐,期间不算回归**

---

## A4 · 冻结件 ① —— `ADR-CEP-00` 追加 SU 2026-11-12(per design **D-5.3**)

- [x] A4.1 追加 `## Status Update 2026-11-12`(**⛔ 只追加**;⛔ 不改任何既有正文 / 既有 SU)
- [x] A4.2 **取代声明**:`§⑤(:426-:434)` 的「唯一规范载体」由本 SU 继承;§⑤ 文本**保留为历史**;其「**5**」与 `:431` 的「已知不一致」段**自本 SU 起作废**
- [x] A4.3 **D-3 三值换算表**:`-01` 3→3 · `-02` **4→3** · `-03` 3→3;第三个值 `:30`「锁定 3 件事」= **历史措辞,不构成口径**
- [x] A4.4 `### ⑥ 三份 ADR 全名重述` 块:**恰 3 bullet**,每行**唯一一个完整编号 token**（`ADR-CEP-01` / `-02` / `-03`）
- [x] A4.5 **D1 第 5 步**:同步更新 R-c 闸的期望值常量 ⇒「合法拆分 ⇒ 闸红 ⇒ 更新期望值」成为**强制函数**而非误红
- [x] A4.6 ⛔ **新 SU 内不得出现 `### 决策 ` 标题** ⇒ `ADR-CEP-00` 的 `^### 决策` 计数**仍 = 4**(AC-16)

---

## A5 · 冻结件 ② —— `07 号` 追加 SU 2026-11-12 + 索引重生成(per design **D-5.3**)

- [x] A5.1 追加 `## Status Update 2026-11-12`,**必须插在 `:569` 的 `## §末 文档索引` 之前**(⛔ 非文末)
- [x] A5.2 声明 `07 号:501` 的「D0 堵法 ② 的机器检查**尚未落地**」**失效** + 指针改指新闸
- [x] A5.3 措辞诚实边界:「新增内容不位移内容锚,索引区整体后移;`07 号:684` 越界是**既有债**、不在本 change 修」
- [x] A5.4 `python3 scripts/cep/generate_doc_index.py --write` 重生成(⛔ 零手抄)
- [x] A5.5 `cep_index_position_check.sh` **rc=0** + 自述 **28/15 → 29/16**(AC-18)

---

## A6 · 冻结件 commit(五步链 拍 2)

- [x] A6.1 落 **builder 自己的 C3 台账 entry**(五步链 拍 0 的前置;⛔ 不改 `#361`)
- [ ] A6.2 `git commit` **2 件冻结件**(⚠️ **产物 commit 必须先于 `--update`**)

---

## A7 · R-c 段(双向对账 · 3 变异)(per design **D-5 / D-6**)

- [x] A7.1 **绿 · 正向**:三份精确 basename 存在 + **无同号拆出件**(语料 = `--root`)
- [x] A7.2 **绿 · 反向**:新 SU 的 `### ⑥` 块**恰 3 bullet** + 每行唯一 token + 三 token 各 ≥1(期望值 = **闸内常量**)
- [x] A7.3 **绿 · 触发条件**:`### ②` 之下第一张表 ⇒ 表头含「判定方式」+ **恰 3 数据行** + 每行 ≥1 code span 且按 **4 类**(a 路径 / b `Entry #N` / c ADR 短名 glob / d 符号)**至少一个可解析**(D-6)
- [x] A7.4 **基线正控**:无变异 ⇒ rc=0
- [x] A7.5 **红 · AC-12**(反向缺项):删重述块一行 ⇒ 必红,签名 `重述块缺项`
- [x] A7.6 **红 · AC-13**(同号拆出件 · **双闸**):影子树造 `ADR-CEP-02a-*.md` ⇒ 新闸必红(签名 `存在同号拆出件`)**且** `cep_adr_structural_check.sh` **同时红** ⇒ 两闸各报各的签名
- [x] A7.7 **红 · AC-14**(引用解析缺失):某行「判定方式」清空 ⇒ 必红,签名 `引用解析缺失`
- [x] A7.8 **还原**(AC-15):3 条逐一还原 ⇒ rc=0
- [x] A7.9 **AC-16** 复核:`ADR-CEP-00` = **4**;22 份合计 = **125**(新 SU 未扰动 R-d)

---

## A8 · 冻结面五步链(G5)

- [x] A8.1 拍 3:`bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`(**2 冻结件同一次**)
- [x] A8.2 拍 4:**commit manifest**(`frozen_at_commit ≠ HEAD` 的 ⚠️ 是**预期**)
- [x] A8.3 **无参复验** `cep_freeze_check.sh` **rc=0** + `authorized_by` 精确匹配(AC-19)
- [x] A8.4 ⚠️ `artifact_count` 此时**仍 = 78**(无新增件)⇒ 78→79 只发生在 A11

---

## A9 · 契约 delta

- [ ] A9.1 `openspec/changes/cpptlm-cep-invariant-gate-teeth/specs/cep-invariant-gate/spec.md`(**ADDED** capability)
- [ ] A9.2 `openspec validate cpptlm-cep-invariant-gate-teeth` 通过

---

## A10 · 0 回归与验收(AC-21)

- [ ] A10.1 `ctest --test-dir build -j4` **171/171**
- [ ] A10.2 **全部** `cep_*.sh` 闸 rc=0(含新闸)
- [ ] A10.3 `bash scripts/build/format.sh --check` rc=0
- [ ] A10.4 `bash scripts/test/docs_sync_check.sh --strict` rc=0 + `python3 scripts/test/agents_md_lint.py` rc=0
- [ ] A10.5 ⛔ **不变式复核**:真实仓**零**变异残留(`git status` 干净,变异只在影子树)

---

## A11 · 归档(M6 · AC-20)

- [ ] A11.1 `openspec archive cpptlm-cep-invariant-gate-teeth` ⇒ delta 并入 `openspec/specs/cep-invariant-gate/spec.md`
- [ ] A11.2 ⚠️ 归档后 `cep_freeze_check` + `cep_docs_sync_check` **预期双红** ⇒ 登记为**预期**,⛔ 不得当回归
- [ ] A11.3 **无条件第二次 `--update --authorized-by "Entry #N"`** ⇒ `artifact_count` **78 → 79**
- [ ] A11.4 **commit**(manifest + 归档产物)
- [ ] A11.5 `openspec validate --specs --strict` 通过 + 落 **verifier 验收报告** `.rddf/reviews/cpptlm-cep-invariant-gate-teeth-verify.md`

---

## ⛔ 明确不做

- ⛔ **不改任何 ADR 正文**(只追加 `## Status Update`)
- ⛔ **新 SU 内不得出现 `### 决策 ` 标题**
- ⛔ **不改** `test/cep_adr_parser.hh`(⛔ `decision_head` = invariant-id 提取器,不计数)
- ⛔ **不改** `scripts/test/cep_docs_sync_check.sh:156` 的正则(历史形态,只登记)
- ⛔ **期望值不得从被检对象自身 / `NUMBER-MAP` 派生**
- ⛔ **不用关键词匹配**
- ⛔ **R-c 不用决策块数**
- ⛔ **不做 invariant 集快照**(C3 之后的可选增强)
- ⛔ 不改 B3b-2 计数(仍 **0/4**)· ⛔ 不动 C4 · ⛔ 不做 T3(Q2 残)
- ⛔ **不在真实仓做变异**(影子树是唯一变异场所)
- ⛔ 不回改台账已提交 Entry(append-only)
