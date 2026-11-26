---
name: cep-tilelink-tree-manager-view-of
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-04
状态: "✅ **已归档**（I-7a · 2026-11-21 · 归档 `2026-11-21-cpptlm-cep-tilelink-tree-manager-view-of` · per 登记册 `obj-cep-platform-remaining.md` §6.4 队列 2）"
created: 2026-11-21
---

# cep-tilelink-tree-manager-view-of: `TreeManager` 的 const 纯查询 sharer 视图（I-7a）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **零** `include/tlm/cep/sharer_tracker.hh` 改动（⇒ `SharerTrackerIf`「恰 4 个虚方法」契约 + 两处 `count_member_virtuals == 5` 断言零影响）· ⛔ **不扩档 2**（本单元档 2 **仍单族**；扩面 = **I-7b**）· ⛔ **不改** `docs/adr/**` · `docs/architecture/**` · `openspec/specs/cep-tilelink-runtime-engine/spec.md` · `test/cep_evidence_tiers.json`

## 架构依据

档 2 观测面需要一份**与判决无关的 sharer 快照**。现状（architect 2026-11-21 实测 + metis 复核）：

| 面 | 现状 |
|---|---|
| `TreeManager::lookup` 的 `LocalView` | ⛔ **不是快照**：`ESCALATE_UP` 路径**不填 `sharers`**（`include/tlm/cep/tilelink_tree_manager.hh:180-183`）· `CHAIN` in-flight 路径**直接 `valid = false`**（`:143-151`）⇒ 拿它当观测面会**漏 sharer** / **无快照** |
| `TreeManager::lookup` 的副作用 | ✅ **无副作用** —— 全文只有 `entries_.find`（`:157`），**无** `emplace`、**无** `events_.push_back`；⚠️ 「首次 forward 时 `dir_.emplace` + 记 `AllocateOnFirstForward`」的副作用属 **`FullMapDirectory`**（`include/tlm/cep/sharer_tracker.hh:640-644`）与 `HierarchicalDirectory`（`:1048`）⇒ **不属** `TreeManager`（`mesif_protocol_engine.hh:789-791` 的注释在 mesif 成立、在 `TreeManager` 不成立）|
| `view_of` 的分布 | `FullMapDirectory`(`sharer_tracker.hh:681`) · `SparseSnoopFilter`(`:836`) · `HierarchicalDirectory`(`:1084`) · `BroadcastNullTracker`(`broadcast_null_tracker.hh:139`) · `DcohSymmetric`(`cxl_dcoh_tracker.hh:231`) ⇒ **唯独 `TreeManager` 缺**（**本单元对象**）|

⇒ 缺口 = **实现面少一个同形方法** ⇒ **I-7a** = 把它补齐，**I-7b** = 靠它建立 tracker 侧观测面。

## 范围

- **In-Scope**：`include/tlm/cep/tilelink_tree_manager.hh` —— 新增 `LocalView view_of(std::uint64_t addr) const`（与 `FullMapDirectory::view_of` **同签名**）· `test/test_cep_tilelink_adapter.cc` —— 新增 TEST_CASE（载体 + 逐字锚点，见 AC-11）
- **Out-Scope**：⛔ `include/tlm/cep/sharer_tracker.hh`（**零改动**）· ⛔ `include/tlm/cep/protocol_engine_base.hh` · ⛔ `test/cep_evidence_tiers.json`（档 2 登记面 —— **零改动**，扩面属 I-7b）· ⛔ `include/tlm/cep/tilelink_protocol_engine.hh` · ⛔ 任何 home 侧驱动组件（= I-7b）· ⛔ `docs/adr/**` · `docs/architecture/**` · ⛔ `openspec/specs/cep-tilelink-runtime-engine/spec.md` · ⛔ 新增 `[[nodiscard]]`（⛔ 5 个既有实现都没有 ⇒ 加了就要同步 5 处 ⇒ 破坏零 diff 纪律）

## Why

**不做的代价**:`TreeManager` 侧任何「快照取法」都只能落到 `lookup` ⇒ 而 `lookup` 的 `LocalView` **不是快照**（`ESCALATE_UP` 时 `sharers` 留 0 ⇒ **漏 sharer**；`CHAIN` 时 `valid = false` ⇒ **无快照**）⇒ I-7b 的档 2 观测面**无法开工** ⇒ 档 2 扩面永远停在单族。

**为什么不改接口**:给 `SharerTrackerIf` 增纯虚 `view_of` 是**三重不可行** —— ① 破 `count_member_virtuals == 5` 两处机器断言（`test/test_cep_sharer_tracker_base.cc:187`、`test/test_cep_tilelink_adapter.cc:447`）② 破 `ADR-CEP-04 §3` 的「恰 4 个虚方法」契约 ③ **违反 I-7 自己的 AC-9**（禁改 `sharer_tracker.hh`）。

**为什么不走「可选能力查询 + `dynamic_cast`」**:双接口 ⇒ 复杂度上升，而 `view_of` 的语义**仍需定义** ⇒ 无净收益。

**为什么不放进引擎**:`openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界行 + **TLE-05** 禁止引擎持有状态 / 预设 from-state ⇒ 驱动不进引擎（= I-7b 的立论）。

**为什么本单元不能只断言「三字段能读出来」**:那会退化成**无人消费的死代码 + 恒真测试** ⇒ 必须有一条「**快照 vs 判决**」的**可红**判别用例（AC-3）。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **`TreeManager::view_of`** | `LocalView view_of(std::uint64_t addr) const`；**逐字段口径**（⛔ **不是**「与 `FullMapDirectory` 语义同形」—— 两者的 `dirty` 概念不同，见 AC-2）：<br>· 从未出现 ⇒ `valid = false` · `sharers = 0` · `dirty = 0`<br>· 空条目（`set_sharer(Nothing)` 或全清后仍留条目）⇒ `valid = true` · `sharers = 0` · `dirty = 0`<br>· `Branch` 单节点 ⇒ `valid = true` · `sharers = m` · `dirty = 0`<br>· `Tip` 单节点 ⇒ `valid = true` · `sharers = m` · `dirty = m`<br>· `Tip` + 另一 `Branch` ⇒ `sharers = m1\|m2` · `dirty = m1`<br>· `Tip`→`Branch` 降级 ⇒ `sharers = m` · `dirty = 0`<br>· `forward_to` **恒 `kInvalidNode`**（⛔ 调用方**不得读取**）|
| 2 | **零副作用** | ⛔ **不得** `operator[]` / `emplace` / `insert` ⇒ 只 `find` ⇒ 读操作对 `entries_` 与事件流**零影响**（⚠️ `entries_` 的条目**只**由安装面 `set_sharer`（用 `entries_[addr]`）与 `clear_sharer`（**不** erase）改变 ⇒ 视图**不得**创建条目）|
| 3 | **快照 vs 判决的差异登记** | `view_of` 恒返回**全量**视图；`lookup` 的 `LocalView` 只在 `ANSWER_HERE` 路径填 `sharers`（`Branch + want_write` ⇒ 留 0）⇒ 二者**不是同一语义** ⇒ 供 I-7b 的 `can_claim` 引用 |
| 4 | **子树内语义** | 视图是**本子树内**的聚合（`caps.aggregation_at = TREE_PARENT`）⇒ **不覆盖**兄弟/祖先子树的持有者（`ESCALATE_UP` 路径）|
| 5 | **doc comment 三句（= I-7b 的契约面）** | ① `dirty == tips` 的语义损失（`Trunk` 折叠为 `Branch` ⇒ may-contain-dirty 在 `dirty` 上不可见）② `forward_to` 恒 `kInvalidNode` 且 ⛔ **调用方不得读取** ③ **单线程假设 / 非线程安全**（`entries_` 是 `std::map` 且无锁）|

## Acceptance

- [ ] **AC-1（签名同形）**：`TreeManager::view_of` 存在且签名与 `FullMapDirectory::view_of`（`sharer_tracker.hh:681`）**逐字同形**（`LocalView view_of(std::uint64_t) const`）⇒ ⚠️ 既有 `MemberFnType`（`test/test_cep_sharer_tracker_base.cc:162-163`）**只特化非 const 成员指针** ⇒ 须**新增 const 特化**（`template<class C, class R, class... A> struct MemberFnType<R (C::*)(A...) const> { using type = R(A...) const; };`）后比较两者的 `type`；⛔ 直接 `is_same_v` 两个**成员指针**恒红（`:765-767` 已记教训）
- [ ] **AC-2（逐字段口径表 · 不与 FullMap 对照）**：按 What Changes #1 的 6 行夹具逐行断言 `valid` / `sharers` / `dirty` / `forward_to`；⛔ **不与 `FullMapDirectory` 做逐字对照** —— ① 两者 `dirty` **概念不同**（`FullMapDirectory::Entry{sharers, dirty}` 的 `dirty` = 脏节点集 vs `TreeManager::Entry{sharers, tips}` 的 `tips` = 持 R/W 副本的节点集）② 安装面**不同构**（`FullMapDirectory::set_sharer` 无条件置位 + 清零即 `erase`；`TreeManager::set_sharer(Nothing)` 建条目留 `{0,0}` + `clear_sharer` **永不** erase）⇒ 「同一 `entries_` 状态」在两侧**无法用等价 API 序列构造** ⇒ 对照表只能写成「排除法」⇒ **假绿**
- [ ] **AC-3（快照 vs 判决 · **可红**判别用例）**：同一夹具内并置 —— `lookup(a, Intent::want_write, ...)` ⇒ `REQUIRE(r == ESCALATE_UP)` ∧ `CHECK(v.sharers == 0)`；而 `CHECK(tm.view_of(a).sharers != 0)` ⇒ **判别力**：若实现成「包一层 `lookup`」⇒ 第二句**必红** ⇒ 按正确原因红
- [ ] **AC-4（脏性口径 · 可红断言）**：① `Branch` 单节点 ⇒ `dirty == 0` ∧ `sharers != 0`（若有人「顺手修正」把 Branch 标脏 ⇒ 红）② `Tip` ⇒ `dirty == tips` ∧ `dirty ⊆ sharers` ③ `Tip`→`Branch` 降级 ⇒ `dirty == 0` ∧ `sharers == m`（防 `dirty` 与 `sharers` 被写成同一字段）；⚠️ 「`Trunk` 折叠 ⇒ may-contain-dirty 不可见」是**已登记的语义损失**，**不作为**断言主体（不可断言的东西不要写成 AC）
- [ ] **AC-5（CHAIN in-flight 期间的快照语义 · 从 I-7b 移交）**：同址 `snoop_begin` 后 ⇒ `lookup` 返 `CHAIN` ∧ `view.valid == false`；而 `tm.view_of(a).valid == true` ∧ `sharers == mask` ⇒ 断言「`view_of` 反映**安装面**真相，与 in-flight 无关」（snoop 只动 `inflight_`，不动 `entries_`）；⚠️ I-7b 已把该未定义面**显式移交本单元**（`.rddf/improvements/cep-tilelink-home-side-state-driver.md` 的 Impact「已知未定义面」②）⇒ 不补 = 台账式义务丢失
- [ ] **AC-6（零副作用 · 可红形态）**：未触碰地址 `z` ⇒ `REQUIRE_FALSE(tm.has_node(z))`；记 `n = tm.events().size()` ⇒ 调 `view_of(z)` ⇒ `CHECK_FALSE(tm.has_node(z))` ∧ `CHECK(tm.events().size() == n)`；⚠️ 另加**源码文本**判据：`view_of` 函数体**不得**含 `operator[]` / `emplace` / `insert`；⚠️ ⛔ **不得**写「对照 `lookup` 首次 forward 会 `emplace`」—— `TreeManager::lookup` **无**该副作用（`FullMapDirectory` 才有）⇒ 该对照**不存在**，照写会退化成「拿 `FullMapDirectory` 冒充 ⇒ 测错对象而绿」
- [ ] **AC-7（档 2 仍单族 · 防半步状态 · 精确对象）**：判据 = `test/cep_evidence_tiers.json` 的 **`evidence_tiers` 键数 == 1** ∧ 含 `mesif-observation`（⚠️ **不是** `coverage_map`、**不是** `coverage_scope.families`）；⇒ ⛔ 本单元**不得**顺手扩族（那是 I-7b）⇒ 防「登记了但没判据」的半截状态
- [ ] **AC-8（零 diff 面 · 带基线）**：`git diff --name-only $(git merge-base HEAD main)..HEAD` **不含** `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `docs/adr/**` · `docs/architecture/**` · `openspec/specs/cep-tilelink-runtime-engine/spec.md` · `test/cep_evidence_tiers.json`；⚠️ **不得**用无参 `git diff --name-only`（= 工作区 vs index ⇒ 与「工作区干净」互斥 ⇒ 恒绿）
- [ ] **AC-9（既有断言零回归）**：两处 `count_member_virtuals == 5` 断言仍绿（`sharer_tracker.hh` 零改动 ⇒ 结构性恒绿，⚠️ 仅作**回归哨**，**不构成判别力**）· `test/test_cep_tilelink_adapter.cc` 的 TMA-07 全绿 ⇒ **实跑复验**
- [ ] **AC-10（0 回归 · 含队列语义）**：ctest 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（数量以 `ls scripts/test/cep_*.sh | wc -l` 实测）逐个 rc=0 **且输出不含 `[SKIP]`** · `bash scripts/test/rddf_stage_gate.sh --phase queue2` rc=0 **且** `bash scripts/test/rddf_stage_gate.sh --self-test` rc=0；⚠️ ctest 注册的是**裸调用**（`test/CMakeLists.txt:286-287`）⇒ 「ctest 全绿」**不覆盖**队列 2 判据（只在 `--phase queue2` 分支执行）⇒ 两条闸须**显式**跑
- [ ] **AC-11（测试载体身份 · 逐字锚点）**：载体 = `test/test_cep_tilelink_adapter.cc`（`[cep][tilelink][adapter]`，TMA-xx 同族；该文件已有 `read_repo_file` 先例 `:442`）· TEST_CASE 名 = `CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)` · 每条 AC 在该文件内留**逐字**注释锚点（供载体表 `grep -F` 命中）⇒ AC-1..AC-10 各一条锚点串（建议前缀 `// I-7a AC-N: …`）
- [ ] **AC-12（多 Tip 如实返回）**：两个节点先后 `set_sharer(Tip)` ⇒ `view_of(addr).dirty` **如实返回两位**（⛔ 不得 `first_bit` 式实现）—— 因 `set_sharer` 只做 `|=`，「至多 1 个 Tip」是**约定不是判据**；⚠️ 与 I-7b 的 AC-6②（多 Tip ⇒ 结构无效）联动：**本单元不得隐藏该形态**
- [ ] **AC-13（载体表登记需求 · 精确行 · 落盘时机）**：⚠️ `scripts/test/rddf_ac_carrier.tsv` 属 **builder 面**（⛔ planner 只能「登记需求」不能落盘）⇒ 交付给 builder 的**精确内容**（AC 编号用**父编号 `I-7`**、锚点带 `I-7a` 前缀 ⇒ 天然「I-7 全部行 done 才算完成」，⛔ 阻止 I-7a 单独宣告 I-7 完成）：

  ```text
  I-7	test	test/test_cep_tilelink_adapter.cc	I-7a AC-1: view_of 签名与 FullMapDirectory::view_of 同形	pending
  I-7	test	test/test_cep_tilelink_adapter.cc	I-7a AC-2: view_of 逐字段口径(Tip/Branch/Nothing/空条目)	pending
  I-7	test	test/test_cep_tilelink_adapter.cc	I-7a AC-3: lookup 不是快照(ESCALATE_UP 时 sharers 留 0)	pending
  I-7	test	test/test_cep_tilelink_adapter.cc	I-7a AC-4: 脏性口径(dirty == tips;Branch ⇒ dirty == 0)	pending
  I-7	test	test/test_cep_tilelink_adapter.cc	I-7a AC-5: CHAIN in-flight 期间 view_of 仍返回安装面视图	pending
  I-7	test	test/test_cep_tilelink_adapter.cc	I-7a AC-6: 零副作用(has_node 探针 + events 长度不变)	pending
  I-7	test	test/test_cep_tilink_adapter.cc	I-7a AC-7: evidence_tiers 键仍 == 1(mesif-observation)	pending
  I-7	cmd	bash scripts/test/rddf_stage_gate.sh	--phase queue2	pending
  ```

  ⚠️ **落盘时机**：载体表对 `test` 行做 `grep -F` ⇒ 锚点串**必须已在测试文件里** ⇒ ⛔ **不得**在实施前预落这些行；正确顺序 = ① **同一 commit** 写 `view_of` + 测试（含逐字锚点）+ 追加 tsv 行（`state=pending`）② 验收通过后**单独 commit** 把 `state` 改 `done`。⚠️ 若坚持「先登记后实施」⇒ 只能用 `TODO` 行且存活 **≤ 1 工作日**（超时判红）。
  ⚠️ **⛔ 不新增独立 `I-7a` 队列槽位**：若改为独立 AC 编号，则须 builder 改 `scripts/test/rddf_stage_gate.sh` 的 3 处队列数组（主数组 + 2 处 self-test 副本），且**位置硬约束** = 必须插在**前驱已 `done`** 的位置（建议 `"I-8"` 之后）—— ⛔ **不得**插在 `I-6` / `I-7` 附近（两者现为 `pending` ⇒ 跳序判据会在 `state=done` 瞬间判红 ⇒ 队列 2 闸**永远无法通过**）
- [ ] **AC-14（AC 映射回填交办）**：裁决记录 §六 明写「AC 映射 ⏳ 待 change 立项后由 planner 回填」⇒ 本单元立项后**必须**把 AC 编号回填进 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` 的 AC 映射位（⚠️ 该文件属 architect 的 `.rddf/reviews/**` 写集 ⇒ 回填需 architect 配合或由主会话代录）

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无新增 capability） | — | ⛔ 本单元只补**一个同形查询方法**；⚠️ `view_of` **不是** `SharerTrackerIf` 的能力面扩展（⛔ 接口零改动）⇒ **不产生** spec delta ⇒ **零冻结面改动** |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `include/tlm/cep/tilelink_tree_manager.hh`（~10 行 + doc comment 三句）· `test/test_cep_tilelink_adapter.cc`（新 TEST_CASE + 逐字锚点）|
| **零改动（硬查）** | 见 AC-8 清单 |
| **冻结面** | ⛔ **零冻结面成员** ⇒ 不触五步链（⚠️ 若实施中被迫改 `sharer_tracker.hh` ⇒ **立即停下并回报** —— 形态已由 architect 裁定）|
| **闸** | ctest +1（`view_of` 语义）· ⛔ 不新增闸脚本类型 · 载体表 8 行（AC-13，builder 面）|
| **KPI** | `0/3 → 1/3` ⇒ 归档 ⇒ `0/3` |
| **已知未定义面（登记不阻塞）** | ① `entries_` **无容量上限**（只增不淘汰 ⇒ 长流下目录单调增长；`view_of` 不得引入整表拷贝）② `const` ≠ 线程安全（`entries_` 是 `std::map` 且无锁 ⇒ 多线程观测 = UB）③ `Trunk` 折叠 ⇒ 脏性丢失（已登记的语义损失）|
| **风险** | ① 误用 `operator[]` 造成条目插入 ⇒ AC-6 必红 ② 脏性口径被「顺手修正」⇒ 变成改语义（AC-4 兜底）③ 顺手扩档 2 ⇒ 半截状态（AC-7 兜底）④ 载体表预落（锚点尚不存在）⇒ `P-3` 机检红（AC-13 兜底）|

## 审查轨迹

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|:--:|---|
| 1 | **architect**（裁决记录 §四）| **形态已裁**（Q-a）| = 具体类上补 `view_of`（⛔ 不动 `SharerTrackerIf`）；I-7 拆 a/b |
| — | **oracle**（裁决记录内）| APPROVE | 处置已并入 §四 |
| — | **critic**（裁决记录内）| APPROVE_WITH_CONDITIONS | 处置已并入 §四（I-7a 口径 + sub-id 登记 + TLE-05 逐字引用）|
| 1 | **metis**（新建）| **P0 × 7 + P1 × 6 + P2 × 4** | ✅ **全部处置**：P0-1 **删掉「`TreeManager::lookup` 有副作用」这一事实错误**（实测 `lookup` 只有 `find`；副作用属 `FullMapDirectory`/`HierarchicalDirectory`）⇒ 改为真实理由「`lookup` 的 `LocalView` 不是快照」· P0-2 AC-2 取消与 `FullMapDirectory` 的逐字对照（`dirty` 概念不同 + 安装面不同构）⇒ 改**逐字段期望表**· P0-3 AC-3 重写为**可红**的快照判别 + `has_node` 探针 + 源码 grep · P0-4 AC-4 改可红断言（Branch ⇒ `dirty == 0` ∧ `sharers != 0`）· P0-5 AC-8 补 diff 基线 + AC-10 补 `--phase queue2` / `--self-test`（ctest 裸调用不覆盖队列语义）· P0-6 补 **AC-5 CHAIN in-flight 快照语义**（I-7b 已显式移交）· P0-7 补 **AC-13 载体表精确行 + 落盘时机 + 队列槽位位置硬约束**· P1-8 AC-1 补 const 成员指针特化 · P1-9 AC-11 补载体身份 + 逐字锚点 · P1-10 AC-7 精确对象（`evidence_tiers` 键数）· P1-11 AC-9 降级为回归哨 · P1-12 修 What Changes #1 错引用（`AC-6`→`AC-4`）· P1-13 doc comment 三句 · P2-14 多 Tip 断言 · P2-15 `entries_` 无上限登记 · P2-16 AC 映射回填交办 · P2-17 **不加** `[[nodiscard]]`（5 个既有实现都无 ⇒ 加了要改 5 处 ⇒ 破坏零 diff）|
