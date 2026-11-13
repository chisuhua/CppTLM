---
name: cep-evidence-tier2-minimal-slice
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)—— 证据档位阶梯的**档 2**(协议级)**观测面 + 契约级断言**最小切片:`mesif` 一族 · sharer 可见性维 + data-value 负控见证(落 ADR-CEP-20 决策 5「判别力上限在本仓协议引擎面」;⛔ **不产 4 件套 run**)
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
  - ADR-CEP-12
  - ADR-CEP-04
  - ADR-CEP-19
状态: "🟡 待实施(2026-11-13 起草 · **v2 · 已按 architect 裁决 R1~R8 + 勘误段改写** · 已吸收 metis 正式审查 19 条 + oracle 复核 7 条;⚠️ **5 项待裁已全部裁决完毕**)"
created: 2026-11-13
---

# cep-evidence-tier2-minimal-slice: 档 2(协议级)观测面 + 契约级断言

> **范围纪律(architect 裁决后)**:⛔ **不改任何已签批 ADR** · ⛔ **新增 invariant 数 = 0** · ⛔ **不产 4 件套 run**(⇒ 不改 `experimental/preregistration.json` / `metrics.schema.json` / `tools/render_report.py` / `scripts/test/cep_experiment_run.sh`,三者/四者皆冻结面)· ⛔ **无版本维** · ⛔ **不引入协议级版本字段** ⇒ 不需 ArchForge 权威 · ⛔ 不改 bundle 语义 · ⛔ 不改 `SharerTrackerIf` 虚方法数(`count_member_virtuals == 5`)· ⛔ **不改 `include/tlm/cep/sharer_tracker.hh`** · ⛔ 检测器**留 `test/` 侧** · ⛔ 不做全族推广 · ⛔ 不动 `B3b-2` · ⛔ 不实现权限维 / 标签维 · ⛔ **不声称「档 2 强于档 1」**(两者**正交**)。
> **估时**:≈ **5-7 人日**(architect 裁决 R8;较 v1 的 7-9 下调 —— 消除 4 件套 / runner / 预登记 / 检测器搬迁四项)。
> **裁决来源**:`.rddf/reviews/cep-tier2-prerequisite-rulings.md` 的 **C′-bis 段(R1~R8)** + **C′-bis-ter 段(勘误)**;台账 **Entry #408**。

## Why

**14 个 CEP change 已全部归档,DoD 12 项里唯一未达成的是 `B3b-2 = 0/4`**(4 个受限族运行时引擎:CXL 2.0 非对称「显式冻结」/ CXL 3.0 DCOH / NVLink / UALink)—— 其「转移表真源」在**仓外或受限**,⛔ 本仓单方面推不动 ⇒ 终态 DoD Go 挂起.

⇒ 本仓**还能推进的最高价值**的一件事 = **把证据档位阶梯的天花板档(档 2 = 协议级)在���仓落地**. `ADR-CEP-20` 决策 5 已判「**判别力的强度上限在本仓的协议引擎面**(⛔ 非跨仓专属)」⇒ 天花板档**就在本仓**,而当前**只有 0 / 1 两档有载体**(`07 号` 附录 SU 2026-11-13 ① 的三档表,档 2 载体 = 「⭕ 观测面未建」).

**四条动机(按重要性)**:

| # | 动机 | 不做的代价 |
|:--:|---|---|
| **M1** | **档 2 是本仓独立可交付的判别力上限** —— 交付「天花板档已落地到可机判程度」,而非「又一份实验」 | 阶梯永远缺顶档 ⇒ 任何一致性断言的最高强度只到档 1(模型级) |
| **M2** | **为 B3b-2 的 4 个受限族预铺观测面 seam** —— 4 族解题后仍需**同一套**观测面才能产出证据;若观测结构做成 `mesif` 专有形态 ⇒ 4 族届时只能复制粘贴或把 mesif 字段塞进基类 | 本次交付是**纯消耗**(做完即废) |
| **M3** | **把 `ADR-CEP-20 §4.3` R-4「档 2 工程量未排期 ⇒ 强度天花板在可预见期不可达」从「排期债」降级为「最小切片已交付 + 全族另计」** | R-4 持续挂着 ⇒ 每次读 DoD 都要重新解释天花板为何不可达 |
| **M4** | **吸收档 0 的教训** —— 档 0 的失败**不是「0 假」**,是「**0 被读成结论**」. 档 2 会以**更强标签**(协议级)出现同样的 0 ⇒ 验收必须含**防夸大机器面** | 把 tier-0 的错误升格成 tier-2 版本 |

**⚠️ 反向动机(诚实登记,per metis)**:「active change = 0 的空窗需要产出」是本计划最大的 slop 燃料 ⇒ ⇒ **若非空洞判据(AC-1)不成立,⛔ 不立项优于立项** ⇒ **停手条件写在 AC-1**(不是写在「已完成」里)⇒ ⚠️ **M3 可绕过 AC-1**(交一份 SU + spec 即算 M3 达成)⇒ ⇒ **AC-1 必须额外含「provenance 判据」**(§4 勘误):判定窗口内**禁止 fixture 播种目录** ⇒ 「用台架喂饱判据」不再是逃逸口.

**⚠️ 档 2 与档 1 的关系(oracle 澄清 b)**:两者**正交**,⛔ **不是强弱序** —— 档 2 判**真引擎状态机 + 真目录可见性**;档 1 判**完成域模型** ⇒ ⛔ **不得**宣称「档 2 强于档 1」;档位阶梯的排序语义**已签批**为「**装置归属层级,强度不可跨档比较**」(`07 号:690`)⇒ 本 change **不改**该语义.

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| **1** | **基类 per-event 观测面**(S1) | `include/tlm/cep/protocol_engine_base.hh` | 新增 `EventObservation` 结构 + 观测缓冲 + 访问器;⛔ **不改既有虚函数签名**;⚠️ **per-event 不是 per-txn**(`apply_event` 无 `txn_id`;WRITEBACK 一次 completion = **2 条 transition + 1 条 fill**) |
| **2** | **观测字段表**(R5 · 定死) | 同上 | `{ kind, addr, event, from, to, origin, data_before, data_after, sharers_snapshot, self_mask, self_mask_valid, level, node }` —— ⛔ **无 version 字段**;`sharers_snapshot` / `self_mask` 用 `std::uint64_t`(**⛔ 不用 `responder_set_t`** ⇒ 基类**不新增** `sharer_tracker.hh` 依赖,保 M2 的 seam 面窄);`origin` 取值域 = `local_request` / `completion_fill` / `remote_snoop` / `writeback` / `back_invalidate` / `direct_inject`(⛔ `direct_inject` **不计入**任何生产流判据) |
| **3** | **`mesif` 观测点 · 两处落点**(S2 · R4) | `include/tlm/cep/mesif_protocol_engine.hh` | ① `kind = transition`:`apply_event` **入口**(`:325` —— 全部状态迁移的单点入口;读 `Rd`/`RdUnique` · 写 `Wr` · 失效 `SnpUnique`/`SnpShared`);`sharers_snapshot` = **迁移前**视图(`sync_tracker` 在 `:344` 末尾才写目录)② `kind = fill`:`on_completion` 的 `lines_[addr].data = data` **之后**(`:442`)⇒ ⛔ 不改协议语义 · ⛔ 不改 bundle · ⛔ 不动 `sharer_tracker.hh` |
| **4** | **sharer 快照取法** | 同上 | `tracker_->view_of(addr)`(`HierarchicalDirectory` 的 **const 纯查询**,与 `FullMapDirectory::view_of` 同口径)⇒ ⛔ **不得**调 `lookup`(它有副作用:首次 forward 时 `dir_.emplace` + `events_.push_back(AllocateOnFirstForward)`) |
| **5** | **装配期硬断言**(metis P0-5 + oracle 降级) | `mesif_protocol_engine.hh::set_tracker`(`:292`) | **3 条硬断言**:`tracker_ != nullptr` ∧ `responder_mask_of(node) != 0` ∧ `level < kLevelCount` ⇒ 失败 ⇒ 置观测面 `observation_invalid` ⇒ run 判「**不可判**」(⛔ **不是 0**);**1 条防御断言**:`caps.completeness == EXACT` —— ⚠️ `HierarchicalDirectory` 构造时写死 `EXACT` ⇒ 该断言在生产路径**恒真** ⇒ 标注「**非生产不变式,仅防护实现被换**」 |
| **6** | **检测器**(S4 · R2) | **留 `test/` 侧**(新测试文件内) | 沿用 `include/tlm/cep/stub_cache_endpoint.hh:68-83` 先例「实现侧只出原始事实,判定归 `test/` 的 harness」⇒ ⛔ **不动** `test/cep_experiment_harness.hh` ⇒ ⛔ **不复制**档 1 的 `SwmrDataValueDetector` 逻辑 ⇒ ⚠️ 档 2 的 **sharer 可见性判据**与档 1 的 SWMR/data-value 判据**不是同一判据** ⇒ ⛔ 不构成双源,但 ⛔ **不得**把两者混用于同一断言 |
| **7** | **非空洞判据 + 断线三连 + provenance**(AC-1) | 新测试 + 观测面 | ① ≥1 条观测 `sharers & ~self_mask != 0`(**真看到过他人 sharer**)② ≥1 个地址写序 ≥ 2(**可改为「≥1 个写 episode 完整经历 transition+fill」**,依 Q1 裁决后的口径)③ 断线三任一 ⇒ **硬失败**;④ ⚠️ **provenance**:判定窗口内**禁止 fixture 播种目录** ⇒ 每个 sharer bit 须**可溯源到 peer 引擎自身的 `apply_event → sync_tracker`** |
| **8** | **3 负控**(S5 · 勘误 §2) | 新测试 | ⛔ **M1(引擎本地非法写)不进本切片**(生产路径结构不可达 ⇒ 无合法实现形态);负控固定为:① **观测面自身失效**(未映射 node / 非法 level / `tracker_ = nullptr`)② **写时他人 sharer 注入** ③ **陈旧副本注入** —— 全部「**同流 + 单差异**」且违例数 **精确 == 1**;⚠️ 违例计数**按 `(addr, write-episode)` 归并后计**(勘误 §6)⇒ 防止 WRITEBACK 的「2 transition + 1 fill」行倍增以**错误原因**红 |
| **9** | **SWMR 判定行**(勘误 §3) | 新测试 + design | 判定行 = **`kind = transition ∧ event = Wr ∧ to = M`**(态迁移写);`kind = fill` 行**显式豁免**(它是同一写 episode 的第 3 条记录)⇒ 记入 design |
| **10** | **档 2 登记面**(S6 · R3-C3 + 勘误 §5) | **新表** `test/cep_evidence_tiers.json` + `scripts/test/cep_e2e_reconcile_check.sh`(**增 T4/T5**) | ⛔ **不进** `test/cep_e2e_run_tiers.json`(它**只登记 run**且**不在**冻结面;C8 产物**零改动**);新表登记档 2 的**非 run 载体** + T4 = 档 2 条目字段齐备 ∧ 含档位词;T5 = **自带**「不得与档 0/1 的 0 并列」+「不含『通过态』表述」两项**机器检查 + 反向 fixture**(⛔ AC-9 的 grep 闸是文档层,不足以独守) |
| **11** | **总纲 SU + 手册** | `docs/architecture/07-coherence-exploration-platform.md`(**末尾追加** SU)· `docs/guide/cep-usage.md` | 阶梯表档 2 行的载体 cell 更新为「**协议级观测面 + 契约级断言(⭕ 无 run / 无通过态)**」;⛔ `07 号` 只**追加**(+ 索引重生成)⇒ 不位移既有 `path:line` 锚点;⚠️ **标题 5 条约束**(先例 = C8 同款):⛔ 不含「拓扑级 IC3 通过态」(`cep_full_green_run_check` C5)· `## Status Update 2026-11-13` 前缀**首个**命中(`cep_dod_reading_check` 用 `next()` 取首个,**不计数** ⇒ 「恰 1 命中」是**人工纪律非机器闸**)⇒ 同日第二则用「附录 ·」形态 |
| **12** | **冻结面五步链 ×1**(S6) | `scripts/test/cep_freeze_check.sh` | 新增 `openspec/specs/cep-*/spec.md` ⇒ **自动入冻结集** ⇒ 84 → **85** ⇒ **一次** `--update`(归档前)⇒ 清单**单独 commit** ⇒ 无参复验 rc=0 |

### ⛔ 明确排除(out-of-scope)

**4 件套 run** / `hypothesis_id` / `ic3_verdict` / `deadlock_violations` / `not-collected`(R3 ⇒ 四议题一并消解)· **版本维**(R1)· **home 侧共享写序**(oracle 已排除)· 权限维(需新 invariant ⇒ 另立 ADR)· 标签维(流上无运行时标签 ⇒ ArchForge)· 检测器搬迁进 `include/`(R2)· `set_sharer` 静默 no-op 的**根治**(扩 `kResponderMappedBits` ⇒ 波及冻结面)· `B3b-2` 的 4 族 · 任何已签批 ADR 的正文改动.

### ⚠️ 全族推广的**前置未知**(metis P2-16 · 必须登记)

`responder_mask_of` 只映射 **4 个 bit**(`include/tlm/cep/sharer_tracker.hh:92-115`:bit0=SDMA · bit1=VRAM · bit2=GMmu · bit3=PCIe),`node_id ∉ {0,1,2,3}` ⇒ `set_sharer` **静默 no-op** ⇒ ⇒ **多节点族的 sharer 维度在数据表示上就不可承载** ⇒ 这是**能力缺口**(非工时问题)⇒ 须扩映射表或换 tracker 实现 ⇒ **波及冻结面** ⇒ 单独立项.

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因;⛔ 「> 0」不构成精确计数;⛔ 反 fixture 走既有影子仓机制(`scripts/test/cep_test_shadow.sh`)⇒ ⛔ **不新增新闸类型**。

- [ ] **AC-1(非空洞判据 · 本 change 的停手条件)**:正控 run 必须机器断言 —— ① ≥1 条观测 `sharers & ~self_mask != 0`(**真看到过他人 sharer**)② ≥1 个**写 episode**完整经历 `transition(Wr→M)` + `fill`(**版本推进改用 episode 完整性** —— 无版本维)③ 断线三任一(未映射 node / 非法 level / `tracker_ = nullptr`)⇒ **硬失败**(⛔ 不是 0);④ ⚠️ **provenance**:判定窗口内**禁止 fixture 播种目录** ⇒ 每个 sharer bit 须可溯源到 **peer 引擎自身的 `apply_event → sync_tracker`**(否则 ⇒ **停手回报**,⛔ 不得以「交付数量」为目标)
- [ ] **AC-2(观测面 → 判定 → 断言 三段连通)**:反向 fixture —— 拔掉任一段(观测面不产记录 / 判定不收 / 断言不落)⇒ **按签名红**;还原 ⇒ 绿
- [ ] **AC-3(负控 · 同流 + 单差异 + 精确计数)**:3 负控各自「同一输入、唯一差异」⇒ 违例数 **精确 == 1**;⚠️ 计数**按 `(addr, write-episode)` 归并后**计(⛔ 不按原始行);⛔ 负控**不得**用「直接调 `apply_event` 注入」冒充生产流可达;⛔ **M1 不在本清单**
- [ ] **AC-4(装配期硬断言)**:3 条硬断言各 1 个反 fixture ⇒ 置 `observation_invalid` ⇒ 判「不可判」⇒ **⛔ 不得**记 0;⚠️ 第 4 条(`completeness == EXACT`)标注「**非生产不变式**」且**不要求**反 fixture
- [ ] **AC-5(档 2 登记面 + 跨档聚合检查)**:新表字段齐备 + T4/T5 全绿;T5 的两项**机器检查**(不得与档 0/1 的 0 并列 / 不含「通过态」表述)各带 **1 个反向 fixture**(删/改 ⇒ 按签名红)
- [ ] **AC-6(判定行钉死)**:SWMR 判据**只**取 `kind = transition ∧ event = Wr ∧ to = M`;`kind = fill` 行**显式豁免**;⇒ 正控与负控的记录行数差异须与该口径**自洽**(⛔ 不得因 fill 行计入而多计)
- [ ] **AC-7(范围纪律 · 零成本但必须查)**:`git diff` 对 `include/bundles/` · `docs/adr/` · `include/tlm/cep/sharer_tracker.hh` · `experimental/preregistration.json` · `experimental/metrics.schema.json` · `tools/render_report.py` · `scripts/test/cep_experiment_run.sh` · `test/cep_e2e_run_tiers.json` · `protocol_instance.schema.json` 均为**空**;`grep -rn '#include[[:space:]]*"\(test/\|\.\./test\)' include/` == **空** ∧ `grep -rn 'catch_amalgamated\|cep_experiment_harness' include/` == **空**;**新增 `INV-CEP-` 数 == 0**;`SharerTrackerIf` 虚方法数 == 5 的既有断言仍绿;⚠️ **不得**用 `grep -rn "test/" include/tlm/cep/`(实测已有 13 处命中 ⇒ 假阳)
- [ ] **AC-8(确定性 + 有界性 + 计数纪律)**:同一流跑两遍 ⇒ 观测记录**逐字相等**;观测缓冲有上限 + `dropped_observations` 计数(非零 ⇒ 红);⚠️ 正控规模须 **小于**缓冲上限(避免「满足 AC-1 却被 AC-8 判红」的死结)
- [ ] **AC-9(防夸大机器面)**:新增产出物内 **grep 禁表述** —— `检测力已证` / `全族达成` / `所有族` / `档 2 强于档 1` / `通过态` ⇒ **命中即红**;⚠️ 该 grep 闸**必须自带反向 fixture**;⚠️ 逐字要求「**单族(mesif)/ 单拓扑 / 无通过态**」与「data-value 生产流恒 0 = **本装置无该形态**」同段
- [ ] **AC-10(档 1 零改动)**:⛔ `test/cep_experiment_harness.hh` **零 diff** ⇒ 档 1 既有负控(`test_cep_experiment_harness` / `test_cep_b4_evidence_device` / `test_cep_stub_cache_e2e`)**逐字绿**
- [ ] **AC-11(总纲 SU + 手册 + 索引)**:`07 号` 末尾追加 SU(5 条标题约束逐条满足;阶梯表档 2 行载体 cell 按 R3-C3 更新)+ `scripts/cep/generate_doc_index.py` 重生成 + `cep_index_position_check` 绿;`cep-usage` 新节落盘
- [ ] **AC-12(冻结面五步链 ×1)**:授权 Entry 先落台账 → 产物 commit → `--update --authorized-by "Entry #N"`(**一次**,归档前)⇒ 清单**单独 commit** ⇒ **无参复验 rc=0** ⇒ 冻结产物 **84 → 85**
- [ ] **AC-13(0 回归)**:`ctest` 全绿(⛔ **不写死数字**,以 `ctest -N` 实测为准)· 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
- [ ] **AC-14(诚实边界登记)**:新 SU + 手册 + spec 中逐字写明 —— ① data-value 生产流 **恒 0 = 本装置无该形态**(⛔ 不得称「检测力已证」)② 档 2 **无 run / 无通过态**(R3)③ **单族(mesif)/ 单拓扑 / 一次实例** ④ 档 2 与档 1 **正交**(⛔ 非强弱序)⑤ 档 2 判别力 **≠** 协议语义全覆盖(权限维 / 标签维被排除)⑥ **一次通过态实例 ≠** 所有族/所有拓扑达成

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-evidence-tier2-observation` | **ADDED** | 协议级**观测面** + 档 2 的**非 run 载体登记** + **诚实边界**登记 + 六条约束的**偏离登记**;⚠️ ⛔ **不含** 4 件套 run / ⛔ **不含** verdict 判定 ⇒ 归档时合并出 `openspec/specs/cep-evidence-tier2-observation/spec.md` |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `test/cep_evidence_tiers.json`(档 2 登记面)· `test/test_cep_tier2_observation.cc` · `openspec/changes/cep-evidence-tier2-minimal-slice/{proposal,design,tasks}.md` + `specs/cep-evidence-tier2-observation/spec.md` |
| **修改文件** | `include/tlm/cep/protocol_engine_base.hh` · `include/tlm/cep/mesif_protocol_engine.hh` · `scripts/test/cep_e2e_reconcile_check.sh`(+T4/T5)· `docs/architecture/07-coherence-exploration-platform.md`(末尾 SU + 索引)· `docs/guide/cep-usage.md` · `.rddf/roadmap/objectives/obj-cep-platform.md`(台账) |
| **零改动(AC-7 硬查)** | `include/bundles/` · `include/tlm/cep/sharer_tracker.hh` · `docs/adr/` · `experimental/**` · `tools/render_report.py` · `scripts/test/cep_experiment_run.sh` · `test/cep_e2e_run_tiers.json` · `test/cep_experiment_harness.hh` |
| **冻结面** | `07 号`(新 SU)· `openspec/specs/cep-*/spec.md`(新)· `ADR-CEP-*`(⛔ **零改动**)⇒ **84 → 85**(**一次** `--update`) |
| **闸** | ctest +1(`cep_tier2_observation` 族)· `cep_e2e_reconcile_check` +2 判据(T4/T5)· ⛔ 不新增闸脚本类型(反 fixture 走既有 shadow 机制) |
| **KPI** | 0/3 → **1/3** ⇒ 归档 ⇒ 0/3 |
| **风险** | ① data-value 生产流不可达(⇒ 已登记为「本装置无该形态」)② 版本维恒等式 ⇒ 已裁不实施 ③ 观测面无人消费 ⇒ AC-2 三段连通 ④ 静默 no-op 造假的 0 ⇒ AC-4 硬断言 + AC-1 provenance ⑤ 多节点族 sharer 维度不可承载 ⇒ 全族前置未知 ⑥ **M3 绕过停手条件** ⇒ AC-1 ④ provenance 兜底 |

## 待裁

✅ **无**(Q1~Q5 已全部由 architect 裁决 R1~R8 + 勘误段处置;台账 Entry #408)。

## metis 正式审查(第 2 轮 · 2026-11-13)处置(逐条)

| metis 建议 | 处置 |
|---|---|
| 歧义 1 观测点选 `apply_event` 入口 ⇒ `data_after` 恒旧 | ✅ **架构裁决 R4** ⇒ 增 `kind = fill` 落点(`:442` 之后) |
| 歧义 2 `self_mask` 字段缺位 | ✅ **R5** 字段表补齐 |
| 歧义 3 `observation_invalid` 无合法载体 | ✅ **R6** ⇒ 观测面自身状态位(不产 run ⇒ 不用 `metrics.json`) |
| 歧义 4 「不可判」不是 `ic3_verdict` 取值 | ✅ **消解** —— R3 不产 run ⇒ 无 verdict ⇒ 该议题消失 |
| 歧义 5 `checks_enabled` 不属观测面 | ✅ **删除**该判据(AC-1 ③ 改为 episode 完整性) |
| 歧义 6 `run_tiers` v3 会让既有闸 TypeError | ✅ **消解** —— R3 ⇒ 新表 `test/cep_evidence_tiers.json` ⇒ `run_tiers` **零改动** |
| 歧义 7 「生产流」未定义 | ✅ **定义** = 10 个 `on_*` 入口(`protocol_engine_base.hh:70-81`);⛔ 直接调 `apply_event` 注入**不算**生产流(AC-3) |
| 歧义 8 「档 2 的版本维强度低于 sharer 侧」无载体 | ✅ **消解** —— 无版本维(R1) |
| 歧义 9 `TxnObservation` 命名污染 | ✅ **R5** ⇒ 改名 `EventObservation` |
| 歧义 10 基类新增 `sharer_tracker.hh` 依赖 | ✅ **R5** ⇒ sharers 用 `std::uint64_t` |
| P0-1 SWMR 拆 3 具名判据 | ⚠️ **部分采纳** —— M2(写时他人 sharer)进切片;M3(data-value)**仅负控可达**;**M1 撤回**(勘误 §2) |
| P0-2 先裁版本维否则删掉 | ✅ **R1** ⇒ 删掉 |
| P0-3 非空洞判据 + 断线三连 | ✅ **AC-1 ①②③** |
| P0-4 检测器归属矛盾 | ✅ **R2** ⇒ 留 test 侧 |
| P0-5 `set_tracker` 装配期硬断言 | ✅ **AC-4**(3 硬 + 1 防御,依 oracle 降级) |
| P0-6 六条约束 → 映射表 + 偏离登记 | ✅ **Capabilities + AC-14** |
| P0-7 档 2 条目机器载体 | ✅ **改写**为新表 + T4/T5(⚠️ 不套用 v3 schema —— 既有闸对 `run_tiers` 的 int 假设不可破) |
| P0-8 档 2 run 不得复用 `h3` | ✅ **消解** —— 不产 run |
| P1-9 per-event 记录 + 缓冲上限 | ✅ **What Changes #1 + AC-8** |
| P1-10 runner 接线 + `not-collected` | ✅ **消解** —— 不产 run |
| P1-11 负控重命名 + T7 跨档 id | ✅ **AC-3 + R7** |
| P1-12 冻结面链路显式步骤 | ✅ **AC-12**(两次 `--update` → **一次**) |
| P1-13 确定性 / 有界性 | ✅ **AC-8** |
| P2-14 seam 目标写进 design | ✅ **What Changes #2**(协议中立字段) |
| P2-15 禁表述清单 + grep 闸 | ✅ **AC-9**(+ 勘误 §5 补机器检查) |
| P2-16 `kResponderMappedBits` 能力缺口 | ✅ **「全族推广前置未知」段** |
| 隐藏意图 M1~M4 + 反向动机 | ✅ **Why** 四动机 + 停手条件 + ⚠️ M3 绕过风险(⇒ AC-1 ④ provenance) |
| 边界:假阴 9 类 / 假阳 3 类 / 时序 4 类 | ✅ **逐条进 design 的「已知边界」段** + AC-1/AC-4/AC-8 |
| P1-10' 缺口 1 预登记死锁 | ✅ **R3** ⇒ 不产 run ⇒ 零冻结面变更 |
| P1-10' 缺口 3 正控拓扑规格 | ✅ **design 硬约束** —— 引擎数 ≥2 · `node_id ∈ {0,1,2,3}` · `level ∈ {0,1,2}` · 共享同一 `HierarchicalDirectory` 实例 · 单线程 |
| P1-10' 缺口 4 观测记录序列化格式 | ✅ **design 硬约束**(定死字段顺序 + 整数表示) |
| 缺口 6 Q3=(b) 下 AC-10 的替代判据 | ✅ **AC-10** 简化为「`test/cep_experiment_harness.hh` 零 diff + 档 1 既有负控逐字绿」 |

## oracle 复核(2026-11-13 · architect 域)处置(逐条)

| oracle 意见 | 处置 |
|---|---|
| **强制修正**:DL-2 事实有误 ⇒ 第五条路存在 ⇒ R3 理由须改述 | ✅ **C′-bis-ter 勘误段 §1** —— R3 结论维持,理由改述为「**定价后排除**」;⚠️ 理由写进 review 文档 ⇒ 防止将来重开 |
| 钉死 SWMR 判定行 | ✅ **勘误 §3 + What Changes #9 + AC-6** |
| M1 处置 | ✅ **勘误 §2** ⇒ M1 不进本切片 |
| 防 M3 钻空(禁 fixture 播种 + provenance) | ✅ **勘误 §4 + AC-1 ④** |
| R7 补洞(新表自带跨档聚合检查 + 反向 fixture) | ✅ **勘误 §5 + What Changes #10 + AC-5** |
| 按写 episode 计数 | ✅ **勘误 §6 + AC-3 + AC-8** |
| 澄清 a「强度不可跨档比较」已签批 ⇒ R3-C4 措辞作废 | ✅ **Why 段末 + AC-14 ④** |
| 澄清 b 档 2 与档 1 正交 | ✅ **Why 段末 + AC-14 ④** |
| 澄清 c `origin` 取值域 | ✅ **What Changes #2** |
| 澄清 d 冻结行号更正 | ✅ **C′-bis-ter §7 d** |
| 澄清 e 新 main spec 不得绕过 change | ✅ **Capabilities + AC-12** |
| 澄清 f metis 预算已用满 ⇒ 不采纳「metis 第 3 轮」 | ✅ **已登记偏离**(本 v2 由 architect 裁决 + oracle 复核 + planner 自查承担) |

## 关联

台账 **Entry #408**(裁决 R1~R8 + 勘误)· **#407**(architect 梳理)· **#406**(C′ 前置裁决)· **#403**(档 2 评估)· `.rddf/reviews/cep-tier2-prerequisite-rulings.md`(C′ / C′-bis / C′-bis-ter 三段)· `.rddf/reviews/cep-evidence-tier2-scale-assessment.md` · `.rddf/reviews/cep-current-state-and-forward-plan.md` · `ADR-CEP-20` 决策 1/2/3a/4/5/6 · `ADR-CEP-06` SU#4 ② + `:697` · `ADR-CEP-04 §3` · `ADR-CEP-12` · `07 号` 附录 SU 2026-11-13 ×2 + `:690` 阶梯表 · `docs/guide/cep-usage.md` §12.9 · `include/tlm/cep/sharer_tracker.hh`(`LocalView:291-296` / `view_of` / `responder_mask_of:92-115`)· `include/tlm/cep/mesif_protocol_engine.hh`(`apply_event:325` / `:442` / `:469` / `set_tracker:292` / `sync_tracker:599-620`)· `include/tlm/cep/stub_cache_endpoint.hh:68-83` · `test/cep_consistency_completion_model.hh:111-186`
