# 验收报告 · `cpptlm-cep-tilelink-tree-manager-view-of`（`TreeManager::view_of` 纯查询视图 · 登记册 I-7a）

- **角色**：builder（**实施侧自验** —— 同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-21 · **关联单元**：登记册 §6.4 队列 2 **I-7a**（= I-7 拆分后的**前置产出单元**）
- **上游裁决**：`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四（Q-a = **具体类**上补 `view_of`，⛔ 不动 `SharerTrackerIf`）· oracle **APPROVE** + critic **APPROVE_WITH_CONDITIONS**（无 BLOCKER）
- **前置**：✅ 无（planner 已起草本 improvement · metis 1 轮已过 · 裁决 §四.2 的 Q-a 已裁）
- **归档**：`openspec/changes/archive/2026-11-21-cpptlm-cep-tilelink-tree-manager-view-of/`（**零 delta spec** ⇒ 不合并任何 main spec ⇒ **零冻结面改动** ⇒ 不触五步链）

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单 | 归档 change 的 `proposal.md` §验收标准（AC-1 ~ AC-14）|
| 形态来源 | 裁决记录 §四（Q-a）+ `.rddf/improvements/cep-tilelink-tree-manager-view-of.md`（AC-1 ~ AC-14 + What Changes #1 的 6 行夹具）|
| 设计取舍 | 同 change `design.md` D-1 ~ D-8 |
| 上游 | 登记册 §6.4 队列 2 的 **I-7** 行（旧单元 `cep-tier2-second-family-tilelink` 已 ⏸ 拆分）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| **AC-1** | `view_of` 签名与 `FullMapDirectory::view_of` 逐字同形 | **通过** | 载体内本地 `ConstMemberFnType`（含 **const 特化**，⛔ 未改 `test_cep_sharer_tracker_base.cc` —— 不在本单元 In-Scope 内）⇒ `REQUIRE(std::is_same_v<TreeViewSig, FullViewSig>)` 绿（`TreeViewSig` = `LocalView(std::uint64_t) const` = `FullViewSig`）。⚠️ 比**函数类型**（去类部分）而非成员指针（后者恒红）|
| **AC-2** | 6 行夹具逐字段口径（不与 FullMap 对照）| **通过** | 载体内 6 个独立夹具逐行断言 `valid`/`sharers`/`dirty`/`forward_to`：① 从未出现 ⇒ `false`/`0`/`0`/`kInvalidNode` ② 空条目（`set_sharer(Nothing)`）⇒ `true`/`0`/`0` ③ Branch ⇒ `true`/`m`/`0` ④ Tip ⇒ `true`/`m`/`m` ⑤ Tip+Branch ⇒ `true`/`m1\|m2`/`m1` ⑥ Tip→Branch ⇒ `true`/`m`/`0`。⛔ **未**与 `FullMapDirectory` 做逐字对照（`dirty` 概念不同 + 安装面不同构 ⇒ 对照表只能是排除法 ⇒ 假绿）|
| **AC-3** | 快照 vs 判决 · **可红**判别用例 | **通过** | `lookup(0x9100, want_write)` ⇒ `REQUIRE(r == ESCALATE_UP)` ∧ `CHECK(v.sharers == 0)`（判决视图**漏** sharer）∧ `CHECK(tm.view_of(0x9100).sharers != 0)`（快照**不漏**）⇒ 判别力由**变异 M-1** 实证（见 §决定性变异）|
| **AC-4** | 脏性口径 · 可红断言 | **通过** | ① Branch ⇒ `dirty == 0` ∧ `sharers != 0` ② Tip ⇒ `dirty == mSelf` ∧ `dirty ⊆ sharers` ③ Tip→Branch 降级 ⇒ `dirty == 0` ∧ `sharers == mSelf`（防 `dirty` 与 `sharers` 写成同一字段）。⚠️ 「`Trunk` 折叠 ⇒ may-contain-dirty 不可见」是**已登记的语义损失**，**未**写成断言主体。**变异 M-3** 实证判别力 |
| **AC-5** | CHAIN in-flight 期间的快照语义 | **通过** | 同址 `snoop_begin` 后：`lookup` 的 **`hazard == LookupHazard::CHAIN`** ∧ `v.valid == false`（判决面无快照）∧ `view_of(a).valid == true` ∧ `sharers == mSelf`（安装面真相）。⚠️ CHAIN 体现在 **`hazard`** 上（`result` 仍是 `ANSWER_HERE`）—— 见 §F-1 事实核正 |
| **AC-6** | 零副作用 · 可红形态 | **通过** | ① 未触碰地址 `z` ⇒ `REQUIRE_FALSE(tm.has_node(z))` ② 调 `view_of(z)` ⇒ `CHECK_FALSE(tm.has_node(z))`（读操作**不创建**条目）③ **源码文本**判据：`view_of` 函数体不得含 `operator[]`/`emplace`/`insert`/`erase` ∧ `entries_` 的**每次**使用都必须是**只读**访问（`.find`/`.end`）⇒ 同时覆盖「不创建条目」+「不改写内容」。⚠️ 载体表的锚点串按 planner 原文保留（「events 长度不变」），紧邻处加**实测订正**注释（`TreeManager` **无** `events()`）—— 见 §F-2 |
| **AC-7** | 档 2 仍单族 · 精确对象 | **通过** | 判据 = `test/cep_evidence_tiers.json` 的 **`evidence_tiers` 键数 == 1** ∧ 含 `mesif-observation`（实测：键数 = 1、键 = `['mesif-observation']`）。⚠️ 精确对象是 `evidence_tiers`（**不是** `coverage_map`、**不是** `coverage_scope.families`）。**变异 M-4** 实证判别力 |
| **AC-8** | 零 diff 面 | **通过（口径已精化 · 见 F-3）** | ① 接口面虚方法数仍 **== 5**（`count_member_virtuals`，加纯虚 `view_of` ⇒ 6 ⇒ 红）② `SharerTrackerIf` 类体内**无独立** `view_of` 标识符（「`view_of` 出现次数 == 「`peer_view_of` 出现次数」）③ 禁改路径（`sharer_tracker.hh` / `protocol_engine_base.hh` / `docs/adr` / `docs/architecture` / `cep-tilelink-runtime-engine/spec.md` / `cep_evidence_tiers.json`）的**工作区内容与 HEAD 一致**（`git diff --quiet HEAD --`）。⚠️ ③ 的判别力由**变异 M-4** 附带实证（改了 `cep_evidence_tiers.json` ⇒ ③ 同时红）|
| **AC-9** | 既有断言零回归 | **通过** | `count_member_virtuals(sharer_tracker.hh) == 5` ∧ `sharer_tracker.hh` 全文不含 `tree_parent`（`LocalView` 零改动）∧ `tilelink_tree_manager.hh` 不含 `virtual LocalView view_of`（新增方法**非**虚方法 ⇒ 不进接口面）⇒ TMA-01 ~ TMA-07 全绿（**231 assertions / 8 cases**）|
| **AC-10** | 0 回归 · 含队列语义 | **通过** | 见 §回归记录。⚠️ 两条队列闸**显式**跑（ctest 注册的是裸调用 ⇒ **不覆盖**队列语义）|
| **AC-11** | 测试载体身份 · 逐字锚点 | **通过** | 载体 = `test/test_cep_tilelink_adapter.cc` · TEST_CASE 名 = `CEP TreeManager: view_of 纯查询视图(Tip/Branch/Nothing/空条目/CHAIN)` · 标签 `[cep][tilelink][adapter]` · AC-1 ~ AC-12 各一条**逐字**锚点注释（前缀 `// I-7a AC-N: …`；AC-1 ~ AC-7 **逐字照抄**载体表锚点串）。⇒ P-3 机检实测**7/7 锚点命中**（`✓ P-3 [I-7] test/test_cep_tilelink_adapter.cc ← 'I-7a AC-1: …'` 等）|
| **AC-12** | 多 Tip 如实返回 | **通过** | 两个节点先后 `set_sharer(Tip)` ⇒ `view_of(addr).dirty == (mSelf\|mPeer)` ∧ `sharers == (mSelf\|mPeer)`（⛔ 非 `first_bit` 式实现）。⚠️ 与 I-7b 的 AC-6②（多 Tip ⇒ 结构无效）联动：**本单元未隐藏该形态**|
| **AC-13** | 载体表登记 · 精确行 · 落盘时机 | **通过** | `scripts/test/rddf_ac_carrier.tsv` 的 **I-7 共 9 行**（既有 1 行 doc + 新增 **8 行**：7 行 `test` + 1 行 `cmd`）· AC 编号用**父编号 `I-7`** ⇒ 天然「I-7 全部行 done 才算完成」⇒ ⛔ 未新增 `I-7a` 独立队列槽位（未改机关的 3 处队列数组）。⚠️ 落盘时机按 AC-13：① **同一 commit** 写实现 + 测试 + 追加 tsv 行（`pending`）② 验收后**单独 commit** 改 `done` |
| **AC-14** | AC 映射回填交办 | **通过** | 已回填进 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` 的 AC 映射位（**主会话代录** —— 该文件属 architect 的 `.rddf/reviews/**` 写集）|

## 决定性变异（4 条 · 全部有鉴别力证据）

| # | 变异 | 期望 | 实测红因（签名）| 判别力 |
|:--:|---|---|---|:--:|
| **M-1** | `view_of` 实现成「包一层 `lookup(want_write)`」 | 按签名红 | `test_cep_tilelink_adapter.cc:600: CHECK( tm.view_of(0x9100).sharers != 0 )` FAILED（**目标红因** = AC-3 的判别句）| ✅ |
| **M-2** | `view_of` 用 `entries_[addr]`（`operator[]` ⇒ 创建条目）| 按签名红 | `:671: CHECK_FALSE( tm.has_node(z) )` FAILED（**AC-6 探针**）∧ `:696: CHECK( is_read_only )` FAILED（**AC-6 源码文本判据**）⇒ 两条 AC-6 判据**都**有鉴别力 | ✅ |
| **M-3** | `dirty` 写成 `sharers`（口径混同）| 按签名红 | `:613: CHECK( v.dirty == 0 )` FAILED（AC-4 的 Branch 夹具）| ✅ |
| **M-4** | `evidence_tiers` 加一个键 | 按签名红 | `:710: CHECK( tiers.size() == 1u )` FAILED（**AC-7**）∧ `:746: CHECK( git_diff_quiet(forbidden) == 0 )` FAILED（**AC-8 越权判据**附带命中）| ✅ |

⚠️ **一次自纠（M-4 的首跑无效）**：M-4 首跑时**实现侧仍留着 M-3 的变异**（改了 C++ 未重建）⇒ 红因被 M-3 掩盖 ⇒ 按「还原实现 → **重建** → 确认基线绿 → 再注入 M-4 → 还原 → 复绿」重做 ⇒ M-4 的红因**独立**成立。

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| **F-1** | **AC-5 的表述偏差（事实核正）**：AC 原文写「`lookup` 返 `CHAIN`」—— 实测 CHAIN 体现在 **`hazard == LookupHazard::CHAIN`**（`result` 仍是 `ANSWER_HERE`，`tilelink_tree_manager.hh:147-149`）⇒ 断言按**真实语义**写 | ⇒ 已按真实语义实施（与既有 TMA-05 ⑤ 同款，`test_cep_tilelink_adapter.cc:323-324`）⇒ **不改** AC 文本（AC 属 planner 面），在此登记 |
| **F-2** | **AC-6 的子判据不可执行（事实核正）**：AC 写「记 `n = tm.events().size()` ⇒ 调 `view_of(z)` ⇒ `CHECK(tm.events().size() == n)`」—— 实测 **`TreeManager` 无 `events()`**（本类只有 `entries_`；in-flight 面在 `SnoopMechanics::inflight_`）⇒ 该子判据**不可执行** | ⇒ 零副作用判据由 AC-6 **已列**的两条承担（`has_node` 探针 + 源码文本判据）⇒ **覆盖面不减**。⚠️ 载体表的锚点串**逐字保留** planner 原文（字面契约不断裂）+ 锚点旁加「实测订正」注释 ⇒ **字面契约与事实真相并存** |
| **F-3** | **AC-8 的口径恒绿（事实核正）**：AC 写「`git diff --name-only $(git merge-base HEAD main)..HEAD`」—— 本仓**直推 `main`** ⇒ `merge-base HEAD main == HEAD`（实测）⇒ diff **恒空** ⇒ **恒绿**（与 AC-8 自己警示的「无参 `git diff` 恒绿」同型）| ⇒ 精化为 **3 条可执行且有鉴别力**的判据（接口虚方法数 == 5 ∧ 接口类体无独立 `view_of` ∧ 禁改路径工作区 vs HEAD 一致）。⚠️ 「**已提交**的越权」由**验收侧**承担（本报告 §变更清单 + 台账 Entry 记录实际改动文件）。**不**改 AC 文本，在此登记 |
| **F-4** | **机关缺陷（本单元落地时暴露）**：载体表的 `cmd` 行**可以**指向机关自身（per AC-13 的 I-7a 行）⇒ P-3 会 `bash -c "$target $anchor"` 调起本脚本 ⇒ 内层又跑 P-3 ⇒ **无限递归**（实测 `rc=124` 超时 ⇒ 机关直接挂）| ⇒ **已修机关**：P-3 段首加**自调用防护**（内层由 `RDDF_STAGE_GATE_INNER=1` 标记 ⇒ **整段跳过 P-3**；cmd 行执行时带上该标记）⇒ 实测递归消失（`rc=0`）且**未动** P-2a 判据（先例 = I-6 的 F-2 机关修复）|
| **F-5** | **P-5 在立项期间必红**：`_active` = `ls openspec/changes \| grep -v archive \| wc -l` ⇒ 新建 change ⇒ `_active` = 1 ⇒ P-5「活口径取值 0/3 != 实测 1/3」判红 ⇒ 并经 P-3 的 cmd 行**传染**到内层 | ⇒ **不回填**活口径（沿用 I-6 先例：立项 → 实施 → 归档，归档后 `_active` 归 0 ⇒ **自动恢复绿**）⇒ 本报告与提交信息显式登记该**暂态**，终局复验见 §回归记录 |
| **F-6** | 本 change **无独立 verifier 环节**（同一 builder 会话实施 + 自验）| ⇒ 已在报告头显式标注 |
| **F-7** | 归档时 openspec 报 warning「Change must have a What Changes section」（因我的 header 写了后缀 `## What Changes（**2 个文件 · 零 spec delta**）`）| ⇒ 已在归档目录内改为**精确** header `## What Changes`（说明移至下一行）⇒ 与无 delta 的归档先例（`2026-05-25-fix-packetpool-refcount`）header 形态一致 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 变异残留 | ✅ 4 条变异全部还原并**重建**复验：`tilelink_tree_manager.hh` diff = **+28 / −0**（纯新增，既有行零触碰）· `cep_evidence_tiers.json` **零改动**（不在变更集内）|
| 临时备份 | ✅ `/tmp/opencode/i7a_tm.bak` / `i7a_tiers.bak` / `carrier.bak` ⇒ 收口时删除 |
| 临时目录操作 | ✅ 曾临时把新 change 移出 `openspec/changes/` 以隔离 P-5 暂态 ⇒ 已**移回**（归档前）|
| 范围纪律 | ✅ `include/tlm/cep/sharer_tracker.hh` **零改动** · `protocol_engine_base.hh` 零改动 · `docs/adr/**` 零改动 · `docs/architecture/**` 零改动 · `test/cep_evidence_tiers.json` 零改动 · `openspec/specs/**` **零改动**（归档未合并任何 spec）|
| 冻结面 | ✅ **零冻结面成员**改动 ⇒ **不触五步链**（未跑 `--update`；清单 diff = **空**；无参 `cep_freeze_check` rc=0 · `provenance 自检通过`）|
| 半截状态 | ✅ F-1 ~ F-7 **全部显式登记**，⛔ 不假装「零偏离」；载体表 I-7 的 **doc 行仍 `pending`** ⇒ ⛔ I-7a **未**宣告 I-7 完成（I-7 = I-7a + I-7b，I-7b 待立项）|

## 变更清单（AC-8 的「已提交越权」验收侧凭据）

| 文件 | 性质 |
|---|---|
| `include/tlm/cep/tilelink_tree_manager.hh` | **In-Scope**（+28 / −0）|
| `test/test_cep_tilelink_adapter.cc` | **In-Scope**（新 TEST_CASE + 3 个 helper）|
| `scripts/test/rddf_ac_carrier.tsv` | 载体表 **+8 行**（I-7a 的 7 test + 1 cmd）|
| `scripts/test/rddf_stage_gate.sh` | **机关缺陷修复**（F-4：自调用防护）|
| `scripts/test/archive_verify_baseline.txt` | 归档验收基线 **33 → 34** |
| `openspec/changes/archive/2026-11-21-cpptlm-cep-tilelink-tree-manager-view-of/**` | 归档目录（4 份 · force-add）|
| `.rddf/roadmap/objectives/obj-cep-platform.md` · `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` · `.rddf/reviews/**` | 台账 Entry + 登记册回填 + AC-14 回填 |

⚠️ **零改动面已逐项核实**（AC-8 禁改清单 6 项全部零 diff）。

## 回归记录

| 项 | 结果 |
|---|---|
| `ctest --test-dir build --output-on-failure -j4` | ✅ **198/198 passed**（0 failed · 195.53 s）—— 与 I-6 后**同数**（本单元无新 ctest 测试 ⇒ 载体在既有 `cpptlm_tests` 内）|
| `./build/bin/cpptlm_tests "[cep][tilelink][adapter]"` | ✅ **231 assertions / 8 cases**（含 TMA-01 ~ TMA-07 + 新 case）|
| `./build/bin/cpptlm_tests "CEP TreeManager: view_of 纯查询视图(...)"` | ✅ **66 assertions / 1 case** |
| 全部 `scripts/test/cep_*.sh`（逐个 rc=0 ∧ **无 `[SKIP]`**）| ✅ **0 失败 / 47 · SKIP = 0**（已先确认 `build/bin/cpptlm_tests` 存在）|
| `bash scripts/test/rddf_stage_gate.sh`（默认 / `--phase queue2` / `--self-test`）| ✅ 三路径全 rc=0 —— ⚠️ 机关的 P-3 新增 7 条 `✓ P-3 [I-7] …` 命中 + 1 条 `✓ P-3 [I-7] cmd rc=0`；`✓ P-2a 队列 2 下一项 (I-7) ⇒ 无跳序` |
| `bash scripts/test/cep_freeze_check.sh`（无参）| ✅ rc=0 —— **provenance 自检通过：90 个产物的 hash 与 commit bfc053bb 的内容一致**（`artifact_count` **90** 不变；本单元**零冻结面成员**改动 ⇒ 清单 diff = **空** ⇒ **不触五步链**）|
| `bash scripts/test/cep_freeze_reverse_verify.sh` · `cep_flow_support_reverse_verify.sh` | ✅ rc=0 |
| `bash scripts/test/archive_verify_check.sh` | ✅ rc=0（基线 **34** · 本单元归档 1 件）|
| `bash scripts/test/openspec_version_check.sh` · `cep_adr_structural_check.py` | ✅ rc=0 |
| `bash scripts/test/docs_sync_check.sh --strict` · `python3 scripts/test/agents_md_lint.py --all --strict` | ✅ rc=0 |
| `openspec validate --specs --strict --json` | ✅ **138 items / 55 invalid / 47 `cep-` / `cep-` invalid = 0** ⇒ 与基线**逐项一致** |
| `git status --porcelain`（排除 `.rddf/state/`）| ✅ 提交后干净 |
| ⚠️ `scripts/build/format.sh --check` | **21 个既有 C++ 文件**（与本单元无关）。本单元触碰的 2 个文件（`include/tlm/cep/tilelink_tree_manager.hh` · `test/test_cep_tilelink_adapter.cc`）**零违规**（实测 `clang-format --dry-run --Werror` 各自 0 处）|

⚠️ **P-5 的立项期暂态已消除**：`_active`（活跃 change 数）在**归档后**归 **0** ⇒ P-5「活口径取值 == 实测」
复绿（实测机关三路径全 rc=0）⇒ 沿用 I-6 先例**未回填**活口径（见 F-5）。

## 已用轮次

**1/3**（第 1 轮；无「验收失败 → 回 builder」的返工轮，但有 **3 次实施内自纠**：① AC-6 的 `entries_` 计数判据写错（实为 **2** 处 `find`+`end`，非 1 处）⇒ 改为语义化的「每次使用都必须是只读访问」② Catch2 v3 不支持断言内 `||` ⇒ 先在变量里算好再断言 ③ **M-4 首跑无效**（改了 C++ 未重建 ⇒ 红因被 M-3 掩盖）⇒ 还原 + 重建 + 重做。三次均在提交前闭合）。
