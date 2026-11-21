# design: cpptlm-cep-tilelink-tree-manager-view-of

> **裁决来源**：`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四（Q-a）· I-7 拆 a/b。
> 本文件只记「裁决内容 → 落地形态」的映射 + **实施侧实测的核正**；⛔ 不重述裁决论证（避免第二真源）。

## D-1 · `view_of` 的落点与形态

**落点**：`include/tlm/cep/tilelink_tree_manager.hh` 的 **public** 区（紧邻 `has_node` —— 同属「树结构观测面」一组；⛔ 不放进 `private` ⇒ 消费方按**具体类型**持有，per mesif 先例 `mesif_protocol_engine.hh:322-329`「按具体实现接线是诚实的」）。

**签名**：`LocalView view_of(std::uint64_t addr) const` —— 与 `FullMapDirectory::view_of`（`sharer_tracker.hh:681`）**逐字同形**。

**实现**（⛔ 只 `find` ⇒ 零副作用）：

```cpp
LocalView view_of(std::uint64_t addr) const {
    LocalView v;
    auto it = entries_.find(addr);
    if (it == entries_.end())
        return v;              // 从未出现 ⇒ valid == false
    v.valid = true;
    v.sharers = it->second.sharers;
    v.dirty = it->second.tips; // ⚠️ `dirty` 口径 = 本子树内的 Tip 集
    return v;
}
```

⚠️ `LocalView` 的默认 `forward_to == kInvalidNode` ⇒ **恒 `kInvalidNode`**（⛔ 调用方**不得读取** —— 该字段对 `view_of` 无意义）。

⚠️ `dirty = it->second.tips`：**`TreeManager::Entry` 的第二字段叫 `tips` 不叫 `dirty`**（`tilelink_tree_manager.hh:207`）⇒ 语义映射在 `LocalView` 面上叫 `dirty`、在实现面上叫 `tips` ⇒ doc comment 必须点明（否则读者会以为 `dirty` 是脏节点集 ⇒ 与 `FullMapDirectory` 混淆）。

## D-2 · doc comment 三句（What Changes #5 · I-7b 的契约面）

① `dirty == tips` 的**语义损失**：`Trunk` 折叠为 `Branch` ⇒ may-contain-dirty 在 `dirty` 上**不可见**（与 `lookup` 的 `:168-169` 同一处损失）② `forward_to` 恒 `kInvalidNode` 且 ⛔ **调用方不得读取** ③ **单线程假设 / 非线程安全**（`entries_` 是 `std::map` 且无锁）。

## D-3 · 逐字段口径表与 `lookup` 的差异登记

| 面 | `view_of` | `lookup` 的 `LocalView` |
|---|---|---|
| `sharers` | **恒** = `entries_[addr].sharers`（全量）| 只在 `Tip` / `Branch+want_read` 路径填；`ESCALATE_UP` 路径**留 0** |
| `dirty` | **恒** = `entries_[addr].tips` | 只在 `Tip` 路径填 = `tips`；其余留 0 |
| `valid` | 条目存在即为 `true`（含空条目）| CHAIN in-flight 时为 `false`；否则 `true` |
| `forward_to` | **恒 `kInvalidNode`** | `ESCALATE_UP` 时 = `tree_parent_` |

⇒ 二者**不是同一语义** ⇒ I-7b 的 `can_claim` 必须引 `view_of`（快照）而**不是** `lookup`（判决）。

## D-4 · ⚠️ 实施侧**事实核正**（3 处 AC 口径）

| AC | 原文 | 实测 | 处置 |
|:--:|---|---|---|
| **AC-5** | 「`lookup` 返 `CHAIN`」| `lookup` 的 CHAIN 路径 = `result = ANSWER_HERE` + **`hazard = CHAIN`**（`tilelink_tree_manager.hh:147-149`）⇒ CHAIN 在 `LookupHazard` 上 | 断言按真实语义：`hazard == LookupHazard::CHAIN` ∧ `v.valid == false`（既有 TMA-05 ⑤ 同款，`test_cep_tilelink_adapter.cc:323-324`）|
| **AC-6** | 「`tm.events().size()` 不变」| **`TreeManager` 无 `events()`**（`entries_` + `SnoopMechanics::inflight_`；无 `events_` 成员）⇒ 子判据**不可执行** | 零副作用由 AC-6 **已列**的两条承担：① `has_node` 探针（测「不创建条目」）② **源码文本**判据（函数体不得含 `operator[]`/`emplace`/`insert`/`erase` ∧ `entries_` 只出现在 `entries_.find(` 一处 ⇒ 同时覆盖「不改写内容」）|
| **AC-8** | 「`git diff --name-only $(git merge-base HEAD main)..HEAD`」| 本仓**直推 `main`** ⇒ `merge-base HEAD main == HEAD` ⇒ diff **恒空** ⇒ **恒绿** | 精化：「**变更集合** = `git diff --name-only <base>..HEAD` ∪ 工作区未提交改动」；`<base>` = **引入本单元锚点串的 commit 的父提交**（`git log -S "I-7a AC-1" -- test/test_cep_tilelink_adapter.cc` 定位 ⇒ 自洽，不受后续 commit 影响）。⚠️ 变更集合**须含** 2 个 In-Scope 文件 ⇒ 防「判据空转」 |

⚠️ AC-6 / AC-8 的核正**不是**放宽判据 —— 两者都是**换成可执行且有鉴别力**的形态（AC-6 覆盖面不减；AC-8 从恒绿变为能真抓越权）。

## D-5 · 载体形态（AC-11）

- **单一 TEST_CASE**（不拆多 case）：`CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)`，标签 `[cep][tilelink][adapter]`。
- **逐字锚点**：AC-1 ~ AC-10 各一条 `// I-7a AC-N: <载体表锚点串逐字>` 注释 ⇒ 载体表 `grep -F` 命中。
  ⚠️ **锚点串逐字照抄** AC-13 的载体表内容（含 AC-6 的「events 长度不变」措辞）⇒ ⛔ **不擅自改写** planner 给定的字面契约；⚠️ 但 AC-6 锚点旁**必须**紧邻一条「⚠️ 实测订正」注释（说明 `events()` 不存在 ⇒ 该子判据由另两条承担）⇒ **字面契约与事实真相并存**，不制造失真。
- **const 成员指针特化**：在载体文件内**本地**定义（⛔ **不改** `test/test_cep_sharer_tracker_base.cc` —— 不在本单元 In-Scope 内）：

  ```cpp
  template <class MPtr> struct ConstMemberFnType;
  template <class C, class R, class... A> struct ConstMemberFnType<R (C::*)(A...) const> {
      using type = R(A...) const;
  };
  ```

  ⇒ 比 `ConstMemberFnType<decltype(&TreeMgr::view_of)>::type` 与 `ConstMemberFnType<decltype(&FullMapDirectory::view_of)>::type` 的 `type`（⛔ 不比成员指针本身 —— 恒红）。

## D-6 · 载体表 8 行的落盘时机（AC-13）

① **同一 commit**：写 `view_of` + 测试（含逐字锚点）+ 追加 tsv 行（`state=pending`）② 验收通过后**单独 commit** 把 `state` 改 `done`。

⚠️ 载体表对 `test` 行做 `grep -F` ⇒ 锚点串**必须已在测试文件里** ⇒ ⛔ 不得在实施前预落这些行。

⚠️ **不新增独立 `I-7a` 队列槽位**：AC 编号用**父编号 `I-7`** ⇒ 天然「I-7 的**所有**行都 done 才算 I-7 完成」⇒ ⛔ 阻止 I-7a 单独宣告 I-7 完成（I-7 = I-7a + I-7b；I-7b 独立立项）。

## D-7 · 冻结面

**零冻结面成员** ⇒ **不触五步链**（无 `--update`、无清单 commit）。
⚠️ `include/tlm/cep/tilelink_tree_manager.hh` **不在** M1 冻结清单的 23 条 glob 内 ⇒ 改它不触发清单变更。
⚠️ 若实施中被迫改 `include/tlm/cep/sharer_tracker.hh` ⇒ **立即停下并回报**（形态已由 architect 裁定）。

## D-8 · AC 映射回填（AC-14）

裁决记录 §六 的 AC 映射位 ⏳ 待 change 立项后回填 ⇒ 本单元立项后由**主会话代录**（`.rddf/reviews/**` 在 architect 写集内，但主会话可写）。
