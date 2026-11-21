# tasks: cpptlm-cep-tilelink-tree-manager-view-of

> **前置**：✅ 无（planner 已起草 · metis 1 轮已过 · 裁决 §四 Q-a 已裁）
> **⚠️ 无 delta spec ⇒ 零冻结面改动 ⇒ 不触五步链**
> **⛔ 顺序**：实现 + 载体表 `pending` 行 = **同一 commit** ⇒ 验收后**单独 commit** 改 `done`

## T0 · 机械件（**缺则「已落地」是假宣称**）

- [x] T0.1 `include/tlm/cep/tilelink_tree_manager.hh`：public 区新增 `LocalView view_of(std::uint64_t addr) const`（design.md D-1 · ⛔ 只 `find`）
- [x] T0.2 同处 doc comment **三句**（design.md D-2：① `dirty == tips` 语义损失 ② `forward_to` 恒 `kInvalidNode` ⛔ 不得读取 ③ 单线程 / 非线程安全）
- [x] T0.3 `test/test_cep_tilelink_adapter.cc`：新 TEST_CASE（名 = `CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)` · 标签 `[cep][tilelink][adapter]`）
- [x] T0.4 载体内 **本地** `ConstMemberFnType`（含 const 特化）⇒ AC-1 比函数类型（⛔ 不改 `test_cep_sharer_tracker_base.cc`）

## T1 · 判据（AC-2 ~ AC-12 逐条落地）

- [x] T1.1 **AC-2**：6 行夹具逐行断言 `valid`/`sharers`/`dirty`/`forward_to`
- [x] T1.2 **AC-3**：`lookup(want_write)` ⇒ `ESCALATE_UP` ∧ `v.sharers == 0` ∧ **`view_of(a).sharers != 0`**（可红判别）
- [x] T1.3 **AC-4**：Branch ⇒ `dirty == 0` ∧ `sharers != 0` · Tip ⇒ `dirty == tips` ∧ `dirty ⊆ sharers` · Tip→Branch ⇒ `dirty == 0`
- [x] T1.4 **AC-5**：CHAIN in-flight ⇒ `hazard == CHAIN` ∧ `v.valid == false` ∧ **`view_of(a).valid == true`** ∧ `sharers == mask`
- [x] T1.5 **AC-6**：`has_node` 探针 + **源码文本**判据（函数体不得含 `operator[]`/`emplace`/`insert`/`erase` ∧ `entries_` 只出现在 `find` 一处）
- [x] T1.6 **AC-7**：`test/cep_evidence_tiers.json` 的 `evidence_tiers` **键数 == 1** ∧ 含 `mesif-observation`
- [x] T1.7 **AC-8**：**变更集合**判据（design.md D-4：`<base>` = 引入锚点串的 commit 的父提交 ∪ 工作区未提交改动）∧ 须含 2 个 In-Scope 文件
- [x] T1.8 **AC-9**：TMA-01 ~ TMA-07 全绿（回归哨）
- [x] T1.9 **AC-12**：两个节点先后 `set_sharer(Tip)` ⇒ `dirty` **如实返回两位**
- [x] T1.10 **AC-11**：AC-1 ~ AC-10 各一条**逐字**锚点注释（照抄 AC-13 载体表锚点串）+ AC-6 锚点旁的「⚠️ 实测订正」注释

## T2 · 载体表（AC-13）

- [x] T2.1 追加 **8 行**到 `scripts/test/rddf_ac_carrier.tsv`（AC 编号 = 父编号 `I-7` · 锚点带 `I-7a` 前缀 · `state=pending`）—— **与 T0/T1 同一 commit**
- [x] T2.2 ⛔ **不新增** `I-7a` 队列槽位（不改 `scripts/test/rddf_stage_gate.sh` 的 3 处队列数组）

## T3 · 决定性变异（每条须**按正确原因红** ⇒ 还原绿）

- [x] T3.1 **M-1**：`view_of` 实现成「包一层 `lookup`」⇒ AC-3 的 `view_of(a).sharers != 0` 红
- [x] T3.2 **M-2**：`view_of` 用 `entries_[addr]` ⇒ AC-6 的源码文本判据红（且 AC-6 探针红）
- [x] T3.3 **M-3**：`dirty` 写成 `sharers`（口径混同）⇒ AC-4 红
- [x] T3.4 **M-4**：AC-7 —— 临时给 `evidence_tiers` 加一个键 ⇒ 红 ⇒ 还原绿

## T4 · 0 回归（AC-10）

- [x] T4.1 ctest 全绿（⛔ 不写死数字）
- [x] T4.2 全部 `scripts/test/cep_*.sh` 逐个 rc=0 **且输出不含 `[SKIP]`**（⚠️ 须先确认 `build/bin/cpptlm_tests` 存在）
- [x] T4.3 `bash scripts/test/rddf_stage_gate.sh --phase queue2` rc=0 ∧ `--self-test` rc=0（⚠️ ctest 裸调用**不覆盖**队列语义）
- [x] T4.4 `docs_sync --strict` · `agents_md_lint` · `format --check`（⚠️ 既有 C++ 债 ⇒ 本单元**新增/触碰**文件须零违规）

## T5 · 收口

- [x] T5.1 验收报告 `.rddf/reviews/cpptlm-cep-tilelink-tree-manager-view-of-verify.md`
- [x] T5.2 台账 Entry（`.rddf/roadmap/objectives/obj-cep-platform.md`）—— 编号以落库时实测最大值 + 1 为准
- [x] T5.3 载体表 8 行 `state` → `done`（**单独 commit**）
- [x] T5.4 登记册回填：§6.4 队列 2 的 **I-7** 行（记 I-7a 已落地 · **I-7 仍未完成** ⇒ I-7b 待立项）
- [x] T5.5 **AC-14**：AC 映射回填进 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md`（主会话代录）

## T6 · 归档

- [x] T6.1 `openspec archive cpptlm-cep-tilelink-tree-manager-view-of -y`（**无 delta spec** ⇒ 不合并任何 main spec）
- [x] T6.2 归档目录 **force-add**（`.gitignore` 默认不入库归档目录）
- [x] T6.3 归档验收基线 `scripts/test/archive_verify_baseline.txt` +1

## ⛔ 本 change 不做

`sharer_tracker.hh`（**零改动**）· `protocol_engine_base.hh` · `cep_evidence_tiers.json` · `tilelink_protocol_engine.hh` · home 侧驱动组件（= I-7b）· `docs/adr/**` · `docs/architecture/**` · `cep-tilelink-runtime-engine/spec.md` · **扩档 2** · 新增 `[[nodiscard]]` · 新增 `I-7a` 队列槽位
