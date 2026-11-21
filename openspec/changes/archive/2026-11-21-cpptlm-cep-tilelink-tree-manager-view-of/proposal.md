# proposal: cpptlm-cep-tilelink-tree-manager-view-of —— `TreeManager` 的 const 纯查询 sharer 视图（I-7a）

> **来源**：`.rddf/improvements/cep-tilelink-tree-manager-view-of.md`（登记册 §6.4 队列 2 的 **I-7a**）
> **形态来源**：architect 裁决 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四（Q-a = **具体类**上补 `view_of`，⛔ 不动 `SharerTrackerIf`）· oracle **APPROVE** + critic **APPROVE_WITH_CONDITIONS**（无 BLOCKER）
> **拆分背景**：I-7 拆 **a / b**（a = 本单元；b = home 侧状态驱动，独立立项 `.rddf/improvements/cep-tilelink-home-side-state-driver.md`）
> **前置**：✅ 无（planner 已起草本 improvement · metis 1 轮已过 · 裁决 §四.2 的 Q-a 已裁）
> **⛔ 实施侧不得自决字段名 / 语义 / 取值**（形态已由裁决给定）。

> **📌 归档语义**：**✅ 已完成归档**（实现落地 · **契约面零改动** ⇒ **无 delta spec** ⇒ `.openspec.yaml` 声明 `skip_specs: true`）。
> ⚠️ 这**不是**「⏸ 暂存归档」（后者 = 实现未启动）—— 本单元的 `view_of` **确实**在仓内，只是不需要写 delta。

## Why

档 2 观测面需要一份**与判决无关的 sharer 快照**。但 `TreeManager::lookup` 的 `LocalView` **不是快照**：

| 路径 | `lookup` 的实际行为 | 后果 |
|---|---|---|
| `ESCALATE_UP` | `view.sharers` **留 0**（`tilelink_tree_manager.hh:180-183` 只填 `forward_to`）| **漏 sharer** |
| `CHAIN` in-flight | `view = LocalView{}` ⇒ `valid == false`（`:143-151`）| **无快照** |

⇒ I-7b 的 tracker 侧观测面**无法开工** ⇒ 档 2 扩面永远停在单族（`mesif-observation`）。

**缺口 = 实现面少一个同形方法**：`FullMapDirectory`(`sharer_tracker.hh:681`) · `SparseSnoopFilter`(`:836`) · `HierarchicalDirectory`(`:1084`) · `BroadcastNullTracker`(`broadcast_null_tracker.hh:139`) · `DcohSymmetric`(`cxl_dcoh_tracker.hh:231`) 都有 `view_of` —— **唯独 `TreeManager` 缺**。

**为什么不改接口**：给 `SharerTrackerIf` 增纯虚 `view_of` 是**三重不可行** —— ① 破 `count_member_virtuals == 5` 两处机器断言（`test/test_cep_sharer_tracker_base.cc:187`、`test/test_cep_tilelink_adapter.cc:447`）② 破 `ADR-CEP-04 §3` 的「恰 4 个虚方法」契约 ③ 违反 I-7 自己的 AC-9（禁改 `sharer_tracker.hh`）。

**为什么不走「可选能力查询 + `dynamic_cast`」**：双接口 ⇒ 复杂度上升，而 `view_of` 的语义**仍需定义** ⇒ 无净收益（裁决 Q-a 已裁）。

## What Changes

> **2 个文件 · 零 spec delta**（本单元**不产生** capability 语义变更 ⇒ `.openspec.yaml` 声明 `skip_specs: true`）

| # | 变更 | 约束 |
|:--:|---|---|
| 1 | **`include/tlm/cep/tilelink_tree_manager.hh`** —— 新增 `LocalView view_of(std::uint64_t addr) const` | **逐字同形** `FullMapDirectory::view_of`（`LocalView view_of(std::uint64_t) const`）· **零副作用**（⛔ 不得 `operator[]` / `emplace` / `insert` ⇒ 只 `find`）· `forward_to` **恒 `kInvalidNode`**（⛔ 调用方**不得读取**）· doc comment **三句**（① `dirty == tips` 的语义损失 ② `forward_to` 恒无效 ③ **单线程假设 / 非线程安全**）|
| 2 | **`test/test_cep_tilelink_adapter.cc`** —— 新增 1 个 TEST_CASE | TEST_CASE 名 = `CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)` · 标签 `[cep][tilelink][adapter]` · AC-1 ~ AC-10 各留**逐字**锚点注释（供载体表 `grep -F` 命中）|

**逐字段口径（What Changes #1 的 6 行夹具）**：

| 夹具 | `valid` | `sharers` | `dirty` |
|:--|:--:|:--:|:--:|
| 从未出现 | `false` | `0` | `0` |
| 空条目（`set_sharer(Nothing)` 或全清后条目仍在）| `true` | `0` | `0` |
| `Branch` 单节点 | `true` | `m` | `0` |
| `Tip` 单节点 | `true` | `m` | `m` |
| `Tip` + 另一 `Branch` | `true` | `m1\|m2` | `m1` |
| `Tip`→`Branch` 降级 | `true` | `m` | `0` |

**⛔ 不与 `FullMapDirectory` 做逐字对照**：两者 `dirty` **概念不同**（`FullMapDirectory::Entry{sharers, dirty}` 的 `dirty` = 脏节点集 vs `TreeManager::Entry{sharers, tips}` 的 `tips` = 持 R/W 副本的节点集）且安装面**不同构** ⇒ 对照表只能是「排除法」⇒ **假绿**。

## Out-of-Scope

⛔ `include/tlm/cep/sharer_tracker.hh`（**零改动**）· ⛔ `include/tlm/cep/protocol_engine_base.hh` · ⛔ `test/cep_evidence_tiers.json`（档 2 登记面 —— **零改动**，扩面属 I-7b）· ⛔ `include/tlm/cep/tilelink_protocol_engine.hh` · ⛔ 任何 home 侧驱动组件（= I-7b）· ⛔ `docs/adr/**` · `docs/architecture/**` · ⛔ `openspec/specs/cep-tilelink-runtime-engine/spec.md` · ⛔ 新增 `[[nodiscard]]`（5 个既有实现都没有 ⇒ 加了要同步 5 处 ⇒ 破坏零 diff 纪律）· ⛔ **扩档 2**（防「登记了但没判据」的半截状态）

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **⚠️ 本 change 无 delta spec** ⇒ **零冻结面改动** ⇒ **不触五步链**（若实施中被迫改 `sharer_tracker.hh` ⇒ **立即停下并回报**）。

### ⚠️ 实施侧**事实核正**（3 处 AC 口径的实测订正 · 已登记）

| AC | 原文口径 | 实测 | 处置 |
|:--:|---|---|---|
| **AC-5** | 「同址 `snoop_begin` 后 ⇒ **`lookup` 返 `CHAIN`**」| `TreeManager::lookup` 的 CHAIN 路径是 `result = ANSWER_HERE` + **`hazard = CHAIN`**（`tilelink_tree_manager.hh:147-149`）⇒ CHAIN 在 **`LookupHazard`** 上，⛔ 不在 `LookupResult` 上 | 断言按**真实语义**写：`hazard == LookupHazard::CHAIN` ∧ `v.valid == false`（既有 TMA-05 ⑤ 同款，`test_cep_tilelink_adapter.cc:323-324`）|
| **AC-6** | 「记 `n = tm.events().size()` ⇒ 调 `view_of(z)` ⇒ `CHECK(tm.events().size() == n)`」| **`TreeManager` 无 `events()`**（实测：`entries_` + `SnoopMechanics::inflight_`；`events_` 不是本类的成员）⇒ 该子判据**不可执行** | 零副作用判据由 AC-6 **已列**的两条承担：`has_node` 探针（未创建条目）+ **源码文本**判据（函数体不得含 `operator[]`/`emplace`/`insert`/`erase` ∧ `entries_` 只出现在 `find` 一处）⇒ **覆盖「不创建条目」+「不改写内容」两面** |
| **AC-8** | 「`git diff --name-only $(git merge-base HEAD main)..HEAD` 不含 …」| 本仓是**直推 `main`** 的 workflow ⇒ `merge-base HEAD main == HEAD` ⇒ 该 diff **恒为空** ⇒ **恒绿**（与 AC-8 自己警示的「无参 `git diff` 恒绿」同型）| 判据精化为「**变更集合** = `git diff --name-only <base>..HEAD` ∪ 工作区未提交改动」，其中 `<base>` = **引入本单元锚点串的 commit 的父提交**（由 `git log -S "<锚点串>"` 定位 ⇒ 自洽且不受后续 commit 影响）|

- [ ] **AC-1（签名同形）**：`TreeManager::view_of` 的**函数类型**与 `FullMapDirectory::view_of` 逐字一致（`LocalView view_of(std::uint64_t) const`）；⚠️ 既有 `MemberFnType`（`test/test_cep_sharer_tracker_base.cc:162-163`）**只特化非 const 成员指针** ⇒ 须在载体文件内**新增 const 特化**（⛔ **不改** `test_cep_sharer_tracker_base.cc` —— 不在本单元 In-Scope 内）；⛔ 直接 `is_same_v` 两个**成员指针**恒红 ⇒ 必须去类部分再比
- [ ] **AC-2（逐字段口径表 · 不与 FullMap 对照）**：6 行夹具逐行断言 `valid` / `sharers` / `dirty` / `forward_to`；⛔ **不与** `FullMapDirectory` 做逐字对照（`dirty` 概念不同 + 安装面不同构）
- [ ] **AC-3（快照 vs 判决 · **可红**判别用例）**：同一夹具内并置 —— `lookup(a, Intent::want_write, ...)` ⇒ `REQUIRE(r == ESCALATE_UP)` ∧ `CHECK(v.sharers == 0)`；而 `CHECK(tm.view_of(a).sharers != 0)` ⇒ **判别力**：若实现成「包一层 `lookup`」⇒ 第二句**必红**
- [ ] **AC-4（脏性口径 · 可红断言）**：① `Branch` 单节点 ⇒ `dirty == 0` ∧ `sharers != 0` ② `Tip` ⇒ `dirty == tips` ∧ `dirty ⊆ sharers` ③ `Tip`→`Branch` 降级 ⇒ `dirty == 0` ∧ `sharers == m`；⚠️ 「`Trunk` 折叠 ⇒ may-contain-dirty 不可见」是**已登记的语义损失**，**不作为**断言主体
- [ ] **AC-5（CHAIN in-flight 期间的快照语义）**：同址 `snoop_begin` 后 ⇒ `lookup` 的 **`hazard == LookupHazard::CHAIN`** ∧ `v.valid == false`；而 `tm.view_of(a).valid == true` ∧ `sharers == mask` ⇒ 断言「`view_of` 反映**安装面**真相，与 in-flight 无关」（snoop 只动 `inflight_`，不动 `entries_`）；⚠️ 见上表「AC-5 事实核正」
- [ ] **AC-6（零副作用 · 可红形态）**：未触碰地址 `z` ⇒ `REQUIRE_FALSE(tm.has_node(z))`；调 `view_of(z)` ⇒ `CHECK_FALSE(tm.has_node(z))`；⚠️ 另加**源码文本**判据：`view_of` 函数体**不得**含 `operator[]` / `emplace` / `insert` / `erase` ∧ `entries_` 在函数体内**只出现在 `entries_.find(` 一处**；⚠️ 见上表「AC-6 事实核正」（⛔ 不写「对照 `lookup` 首次 forward 会 `emplace`」—— `TreeManager::lookup` **无**该副作用 ⇒ 该对照**不存在**，照写会退化成「拿 `FullMapDirectory` 冒充 ⇒ 测错对象而绿」）
- [ ] **AC-7（档 2 仍单族 · 防半步状态 · 精确对象）**：判据 = `test/cep_evidence_tiers.json` 的 **`evidence_tiers` 键数 == 1** ∧ 含 `mesif-observation`（⚠️ **不是** `coverage_map`、**不是** `coverage_scope.families`）
- [ ] **AC-8（零 diff 面 · 带基线）**：**变更集合**（见上表「AC-8 事实核正」）**不含** `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/adr/` · `docs/architecture/` · `openspec/specs/cep-tilelink-runtime-engine/spec.md` · `test/cep_evidence_tiers.json`；⚠️ 变更集合**必须含** `include/tlm/cep/tilelink_tree_manager.hh` 与 `test/test_cep_tilelink_adapter.cc`（否则判据空转）
- [ ] **AC-9（既有断言零回归）**：两处 `count_member_virtuals == 5` 断言仍绿（`sharer_tracker.hh` 零改动 ⇒ 结构性恒绿，⚠️ 仅作**回归哨**，**不构成判别力**）· `test/test_cep_tilelink_adapter.cc` 的 TMA-01 ~ TMA-07 全绿 ⇒ **实跑复验**
- [ ] **AC-10（0 回归 · 含队列语义）**：ctest 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（数量以 `ls scripts/test/cep_*.sh | wc -l` 实测）逐个 rc=0 **且输出不含 `[SKIP]`** · `bash scripts/test/rddf_stage_gate.sh --phase queue2` rc=0 **且** `bash scripts/test/rddf_stage_gate.sh --self-test` rc=0；⚠️ ctest 注册的是**裸调用** ⇒ 「ctest 全绿」**不覆盖**队列 2 判据 ⇒ 两条闸须**显式**跑
- [ ] **AC-11（测试载体身份 · 逐字锚点）**：载体 = `test/test_cep_tilelink_adapter.cc`（`[cep][tilelink][adapter]`）· TEST_CASE 名 = `CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)` · AC-1 ~ AC-10 各一条**逐字**锚点注释（前缀 `// I-7a AC-N: …`，**逐字内容照抄** AC-13 的载体表锚点串）
- [ ] **AC-12（多 Tip 如实返回）**：两个节点先后 `set_sharer(Tip)` ⇒ `view_of(addr).dirty` **如实返回两位**（⛔ 不得 `first_bit` 式实现）—— 因 `set_sharer` 只做 `|=`，「至多 1 个 Tip」是**约定不是判据**；⚠️ 与 I-7b 的 AC-6②（多 Tip ⇒ 结构无效）联动：**本单元不得隐藏该形态**
- [ ] **AC-13（载体表登记 · 精确行 · 落盘时机）**：`scripts/test/rddf_ac_carrier.tsv` 追加 **8 行**（AC 编号用**父编号 `I-7`**、锚点带 `I-7a` 前缀 ⇒ 天然「I-7 全部行 done 才算完成」，⛔ 阻止 I-7a 单独宣告 I-7 完成）；⚠️ **落盘时机**：① **同一 commit** 写 `view_of` + 测试（含逐字锚点）+ 追加 tsv 行（`state=pending`）② 验收通过后**单独 commit** 把 `state` 改 `done`；⚠️ **⛔ 不新增独立 `I-7a` 队列槽位**（否则须改 `scripts/test/rddf_stage_gate.sh` 的 3 处队列数组 + 位置硬约束）
- [ ] **AC-14（AC 映射回填交办）**：把 I-7a 的 AC 编号回填进 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` 的 AC 映射位（⚠️ 该文件属 architect 的 `.rddf/reviews/**` 写集 ⇒ 回填需 architect 配合或由**主会话代录**）
