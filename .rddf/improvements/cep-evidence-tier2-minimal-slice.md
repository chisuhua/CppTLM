---
name: cep-evidence-tier2-minimal-slice
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)—— 证据档位阶梯的**档 2**(协议级语义)最小切片:`mesif` 一族 · sharer + data-value 两维(落 ADR-CEP-20 决策 5「判别力上限在本仓协议引擎面」)
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
  - ADR-CEP-12
  - ADR-CEP-04
  - ADR-CEP-19
状态: "🟡 待实施(2026-11-13 起草 · **v1 · planner 阶段 · 已吸收 metis 首轮 16 条建议**;⚠️ 含 **3 项待 architect 追加裁决**(见 §待裁 Q1~Q3))"
created: 2026-11-13
---

# cep-evidence-tier2-minimal-slice: 档 2(协议级语义)最小切片

> **范围纪律**:⛔ **不改任何已签批 ADR** · ⛔ **新增 invariant 数 = 0** · ⛔ 不改 bundle 语义 · ⛔ 不改 `SharerTrackerIf` 虚方法数(`count_member_virtuals == 5`) · ⛔ 不做全族推广 · ⛔ 不动 `B3b-2` · ⛔ 不实现权限维 / 标签维(需另立 ADR / 需 ArchForge 权威) · ⛔ 不改 `metrics.schema.json` 的既有字段语义.
> **估时**:≈ **7-9 人日**(metis 首轮后**上浮**:原估未含「版本维恒等式处置」「检测器单一真源化」「非空洞断线三连」「装配期硬断言」四项)。
> **⚠️ 立项前置**:**3 项待 architect 追加裁决**(§待裁 Q1~Q3)—— Q1 若判「只做 1 维」则本 change 范围收窄;Q2/Q3 影响 S3/S4 形态。

## Why

**14 个 CEP change 已全部归档,DoD 12 项里唯一未达成的是 `B3b-2 = 0/4`**(4 个受限族的运行时引擎:CXL 2.0 非对称「显式冻结」/ CXL 3.0 DCOH / NVLink / UALink)—— 其「转移表真源」在**仓外或受限**,⛔ 本仓单方面推不动 ⇒ 终态 DoD Go 挂起.

⇒ 本仓**还能推进的最高价值**的一件事 = **把证据档位阶梯的天花板档(档 2)在本仓落地**. `ADR-CEP-20` 决策 5 已判「**判别力的强度上限在本仓的协议引擎面**(⛔ 非跨仓专属)」⇒ 天花板档**就在本仓**,而当前**只有 0 / 1 两档有载体**(`07 号` 附录 SU 2026-11-13 ① 的三档表,档 2 载体 = 「⭕ 观测面未建」).

**四条动机(按重要性)**:

| # | 动机 | 不做的代价 |
|:--:|---|---|
| **M1** | **档 2 是本仓独立可交付的判别力上限** —— 交付「天花板档已落地到可机判程度」,而非「又一份实验」 | 阶梯永远缺顶档 ⇒ 任何一致性断言的最高强度只到档 1(模型级) |
| **M2** | **为 B3b-2 的 4 个受限族预铺观测面 seam** —— 4 族解题后仍需**同一套**观测面才能产出证据;若 S1 把观测结构做成 `mesif` 专有形态 ⇒ 4 族届时只能复制粘贴或把 mesif 字段塞进基类 | 本次交付是**纯消耗**(做完即废) |
| **M3** | **把 `ADR-CEP-20 §4.3` R-4「档 2 工程量未排期 ⇒ 强度天花板在可预见期不可达」从「排期债」降级为「最小切片已交付 + 全族另计」** | R-4 持续挂着 ⇒ 每次读 DoD 都要重新解释天花板为何不可达 |
| **M4** | **吸收档 0 的教训** —— 档 0 的失败**不是「0 假」**,是「**0 被读成结论**」. 档 2 会以**更强标签**(协议语义)出现同样的 0 ⇒ 验收必须含**防夸大机器面** | 把 tier-0 的错误升格成 tier-2 版本 |

**⚠️ 反向动机(诚实登记,per metis)**:「active change = 0 的空窗需要产出」是本计划最大的 slop 燃料 ⇒ ⇒ **若非空洞判据(AC-1)不成立,⛔ 不立项优于立项** ⇒ 本 change 的**停手条件**写在 AC-1(不是写在「已完成」里).

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| **1** | **基类 per-event 观测面**(S1) | `include/tlm/cep/protocol_engine_base.hh` | 新增 `TxnObservation` 记录结构 + 观测缓冲 + 访问器;⛔ **不改既有虚函数签名**;⚠️ **per-event 不是 per-txn**(metis P1-9:`apply_event` 无 `txn_id`,且 WRITEBACK 完成 = **2 条**记录)⇒ 协议中立字段(`addr` / `event_as_int` / `from` / `to` / `origin` / `data_after` / `version_after` / `sharers_snapshot` / `self_mask_valid`)⇒ **mesif 特异性留在派生类**(M2 的 seam 目标) |
| **2** | **`mesif` 观测点**(S2) | `include/tlm/cep/mesif_protocol_engine.hh` | 观测点挂 `apply_event` 入口(:325 —— **全部状态迁移的单点入口**:读 `Rd`/`RdUnique` / 写 `Wr` / 失效 `SnpUnique`/`SnpShared`);sharer 快照取 `tracker_->view_of(addr)`(`HierarchicalDirectory` 的 **const 纯查询**,⛔ **不得**调 `lookup` —— 它有副作用);⚠️ `set_line`(:302)旁路 `sync_tracker` ⇒ 观测面须登记该旁路 |
| **3** | **版本语义**(S3) | 引擎内实现侧 | ⛔ **不改 bundle** · ⛔ **不改协议语义设计** ⇒ 不需 ArchForge 权威;⚠️ **口径待 architect 追认**(Q2):per-engine 私有写序在「自读自写」下是**恒等式** ⇒ 恒 0 ⇒ 不构成判据 ⇒ 三条出路见 §待裁 |
| **4** | **检测器**(S4) | `include/tlm/cep/cep_invariant_detector.hh`(**新**,header-only,零 test 依赖) | ⚠️ **归属待 architect 追认**(Q3):新建实现侧单一真源 + `test/cep_experiment_harness.hh` 的 `SwmrDataValueDetector` 改**薄委托**(档 1 API 与行为不变)⇒ **档 1 负控零削弱**必须由既有负控全绿证明;⛔ **禁止**复制一份判据逻辑进 `include/` |
| **5** | **装配期硬断言**(新增,metis P0-5) | `mesif_protocol_engine.hh::set_tracker`(:292) | `tracker_ != nullptr` ∧ `responder_mask_of(node) != 0` ∧ `level < kLevelCount` ∧ `caps.completeness == EXACT` ⇒ 失败 ⇒ 置 `observation_invalid` ⇒ run 判「**不可判**」(⛔ **不是 0**) |
| **6** | **非空洞判据 + 断线三连硬失败**(新增,metis P0-3) | 新测试 + 实现侧 | ① 至少 1 条观测满足 `sharers & ~self_mask != 0`(真看到过「别人」);② 至少 1 个地址写序 ≥ 2;③ 至少 1 条观测 `checks_enabled == true`;④ 断线三任一出现 ⇒ **硬失败**(⛔ 不是 0) |
| **7** | **3 负控**(S5,按 metis P1-11 重定义) | 新测试 | ⛔ 删「错标完成域」(档 2 无对应物);改为:① **观测面自身失效**(未映射 node / 非法 level / `tracker_ = nullptr` ⇒ 必红)② **M2 注入**(写时存在他人 sharer)③ **M3 注入**(过期读)—— 全部「**同流 + 单差异**」,且违例数 **精确 == 1**(⛔ 不用「> 0」) |
| **8** | **六条约束 → 映射表 + 偏离登记**(S5 改写,metis P0-6) | `docs/` 侧登记 + 新 spec | ⚠️ 约束 1(in-flight 峰值)/ 约束 6(阻塞开关)在**引擎面不存在该机制** ⇒ **不适用** ⇒ 须逐条写「约束 → 档 2 对应物 → 是否适用 → 替代判据」,**⛔ 不声称 SU#4 ② 已满足**(per `ADR-CEP-06:697` 的「无法真实构造 ⇒ 停手回报 + 显式登记降格」) |
| **9** | **档 2 条目机器面**(S6,metis P0-7) | `test/cep_e2e_run_tiers.json`(**schema v3**)· `scripts/test/cep_e2e_reconcile_check.sh`(**增 T4/T5/T6** + 反向 fixture) | v3 增 `family` / `version_caliber` / `coverage_scope` / `attribution`;T4 = 档 2 条目三项齐备 ∧ 含档位词;T5 = 反向 fixture(删一项 ⇒ 按签名红);T6 = 档 2 条目 `ic3_verdict == "✓ 通过"` ⇒ 其 `deadlock_violations` 必须是**实测 0**(否则只能 ⚪);⛔ **不改 `ADR-CEP-20` 的 8 载体对账表**(不可变 ADR) |
| **10** | **run 身份与 4 件套**(S6) | `scripts/test/cep_experiment_run.sh`(增 `--tier2`)· 新 emit 用例 | ⛔ **不复用 `hypothesis_id = "h3"`**(跨档聚合禁令)⇒ 新 id(如 `h3-tier2-mesif`);`deadlock_violations` = `"not-collected"` + reason(⛔ **不得写 0** —— 引擎面无死锁机制);`observation_keys` 落 `trace.h5`;`memory_violation_kinds` 之和 == `memory_violations`(既有 `render_report.py` 已强制) |
| **11** | **总纲 SU + 手册**(S6) | `docs/architecture/07-coherence-exploration-platform.md`(**末尾追加** SU)· `docs/guide/cep-usage.md` | ⛔ `07 号` 只**追加**(+ 索引重生成)⇒ 不位移既有 `path:line` 锚点;⚠️ **标题 5 条约束**(先例 = C8 同款):⛔ 不含「拓扑级 IC3 通过态」(`cep_full_green_run_check` C5 会去新 SU 找 run 目录)· `## Status Update 2026-11-13` 前缀**恰 1 命中**(`cep_dod_reading_check` S7)⇒ 同日第二则用「附录 ·」形态 |
| **12** | **冻结面五步链 ×2**(S6) | `scripts/test/cep_freeze_check.sh` | 新增 `openspec/specs/cep-*/spec.md` ⇒ **自动入冻结集** ⇒ 84 → **85** ⇒ 归档前 `--update` + 归档后 `--update`(两次,同一授权 Entry)⇒ 清单**单独 commit** ⇒ 无参复验 |

### ⛔ 明确排除(out-of-scope)

全族推广(其余 5 引擎)· 权限维(需新 invariant ⇒ 另立 ADR)· 标签维(流上无运行时标签 ⇒ ArchForge)· `set_sharer` 静默 no-op 的**根治**(扩 `kResponderMappedBits` ⇒ 波及冻结面)· `B3b-2` 的 4 族 · `metrics.schema.json` 既有字段语义 · 任何已签批 ADR 的正文改动.

### ⚠️ 全族推广的**前置未知**(metis P2-16 · 必须登记,否则本交付会被误读成「几乎完成」)

`responder_mask_of` 只映射 **4 个 bit**(`include/tlm/cep/sharer_tracker.hh:92-115`:bit0=SDMA · bit1=VRAM · bit2=GMmu · bit3=PCIe),`node_id ∉ {0,1,2,3}` ⇒ `set_sharer` **静默 no-op** ⇒ ⇒ **多节点族的 sharer 维度在数据表示上就不可承载** ⇒ 这是**能力缺口**(非工时问题),须扩映射表或换 tracker 实现 ⇒ **波及冻结面** ⇒ 单独立项.

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因;⛔ 「> 0」不构成精确计数.

- [ ] **AC-1(非空洞判据 · 本 change 的停手条件)**:正控 run 必须机器断言 —— ① ≥1 条观测 `sharers & ~self_mask != 0`(**真看到过他人 sharer**)② ≥1 个地址写序 ≥ 2 ③ ≥1 条观测 `checks_enabled == true`;**任一不成立 ⇒ 停手回报**(⛔ 不得以「交付数量」为目标);**且**断线三任一(未映射 node / 非法 level / `tracker_ = nullptr`)出现 ⇒ **硬失败**(⛔ 不是 0)
- [ ] **AC-2(观测面 → 检测器 → `memory_violations` 三段连通)**:反向 fixture —— 拔掉任一段(观测面不产记录 / 检测器不收 / `metrics.json` 不落 `memory_violations`)⇒ **按签名红**;还原 ⇒ 绿
- [ ] **AC-3(负控 · 同流 + 单差异 + 精确计数)**:3 负控各自「同一输入、唯一差异」⇒ 违例数 **精确 == 1**(`swmr == 1` / `data_value == 1` / `observation_invalid == 1`);⛔ 不用「> 0」;⛔ 负控**不得**用「直接调 `apply_event` 注入」冒充生产流可达
- [ ] **AC-4(装配期硬断言)**:4 条断言各 1 个反 fixture(未映射 node / 非法 level / `tracker_ = nullptr` / `completeness != EXACT`)⇒ 置 `observation_invalid` ⇒ run 判「不可判」⇒ **⛔ 不得**记 0
- [ ] **AC-5(档 2 条目机器面)**:tiers 表 schema v3 三项齐备 + T4/T5/T6 全绿;T5 反向 fixture「删一项 ⇒ 按签名红」;**档 2 条目 `ic3_verdict == "✓ 通过"` ⇒ `deadlock_violations` 必须实测 0**(⛔ 引擎面无死锁机制 ⇒ 只能 `"not-collected"` + reason ⇒ 则 verdict 只能 ⚪)
- [ ] **AC-6(跨档隔离)**:档 2 run 的 `hypothesis_id` **≠ `h3`**;`cep_prereg_drift_check` rc=0(新 id 不触发 drift);⛔ 档 0 的既有 0 **不得**与档 2 的 0 在同一断言/同一段内并列引用
- [ ] **AC-7(范围纪律 · 零成本但必须查)**:`git diff` 对 `include/bundles/` · `docs/adr/` · `protocol_instance.schema.json` · `experimental/preregistration.json` · `metrics.schema.json` 为**空**;`grep -rn "test/" include/tlm/cep/` 为**空**;**新增 `INV-CEP-` 数 == 0**;`SharerTrackerIf` 虚方法数 == 5 的既有断言仍绿;`git diff --stat include/tlm/cep/sharer_tracker.hh` == **空**
- [ ] **AC-8(确定性 + 有界性)**:同一流跑两遍 ⇒ 观测记录**逐字相等**;观测缓冲有上限 + `dropped_observations` 计数(非零 ⇒ 红)
- [ ] **AC-9(防夸大机器面)**:新增产出物内 **grep 禁表述** —— `检测力已证` / `全族达成` / `所有族` / `档 0` 与档 2 的 0 并列 ⇒ **命中即红**;⚠️ 该 grep 闸**必须自带反向 fixture**(否则又是一条文档层承诺);⚠️ 逐字要求「观测侧代理版本,⛔ 非协议语义版本」与「单族(mesif)/ 单拓扑 / 一次通过态实例」同段
- [ ] **AC-10(档 1 零削弱)**:若 Q3 选「实现侧单一真源 + test 薄委托」⇒ 档 1 全部既有负控(`test_cep_experiment_harness` / `test_cep_b4_evidence_device` / `test_cep_stub_cache_e2e`)**逐字绿**;⛔ 不得「顺手重构」其判定逻辑
- [ ] **AC-11(总纲 SU + 手册 + 索引)**:`07 号` 末尾追加 SU(5 条标题约束逐条满足)+ `scripts/cep/generate_doc_index.py` 重生成 + `cep_index_position_check` 绿;`cep-usage` 新节落盘
- [ ] **AC-12(冻结面五步链 ×2)**:授权 Entry 先落台账 → 新增/归档产物 commit → `--update --authorized-by "Entry #N"`(两次:归档前 + 归档后)⇒ 清单**单独 commit** ⇒ **无参复验 rc=0** ⇒ 冻结产物 **84 → 85**
- [ ] **AC-13(0 回归)**:`ctest` 全绿(⛔ **不写死数字**,以 `ctest -N` 实测为准)· 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
- [ ] **AC-14(诚实边界登记)**:新 SU + 手册 + spec 中逐字写明 —— ① data-value 维在**生产流**的可达性状态(依 Q1 裁决)② 版本维 = **观测侧代理,⛔ 非协议语义版本**③ **单族(`mesif`)/ 单拓扑 / 一次通过态实例**④ ⛔ **不得**称「检测力已证」⑤ 档 2 判别力 **≠** 协议语义全覆盖(权限维 / 标签维被排除)

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-evidence-tier2-observation` | **ADDED** | 协议级观测面 + 档 2 档位登记机器面 + 档 2 的**诚实边界**登记;⚠️ 含「偏离登记」(六条约束中不适用者的替代判据)⇒ 归档时合并出 `openspec/specs/cep-evidence-tier2-observation/spec.md` |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `include/tlm/cep/cep_invariant_detector.hh`(若 Q3 选实现侧)· `test/test_cep_tier2_observation.cc` · `openspec/changes/cep-evidence-tier2-minimal-slice/{proposal,design,tasks}.md` + `specs/cep-evidence-tier2-observation/spec.md` |
| **修改文件** | `include/tlm/cep/protocol_engine_base.hh` · `include/tlm/cep/mesif_protocol_engine.hh` · `test/cep_e2e_run_tiers.json`(v2→v3)· `scripts/test/cep_e2e_reconcile_check.sh`(+T4/T5/T6)· `scripts/test/cep_experiment_run.sh`(+`--tier2`)· `test/cep_experiment_harness.hh`(若 Q3 选薄委托)· `docs/architecture/07-coherence-exploration-platform.md`(末尾 SU + 索引)· `docs/guide/cep-usage.md` · `.rddf/roadmap/objectives/obj-cep-platform.md`(台账) |
| **冻结面** | `07 号`(新 SU)· `openspec/specs/cep-*/spec.md`(新)· `ADR-CEP-*`(⛔ **零改动**)⇒ **84 → 85**(两次 `--update`) |
| **闸** | ctest +1(`cep_tier2_observation` 族)· `cep_e2e_reconcile_check` +3 判据 · ⛔ 不新增闸脚本(⛔ 避免闸 proliferation) |
| **KPI** | 0/3 → **1/3** ⇒ 归档 ⇒ 0/3 |
| **风险** | ① data-value 生产流不可达(⇒ Q1 可能把范围收窄到 1 维)② 版本维恒等式(⇒ Q2)③ 检测器双源(⇒ Q3 单一真源 + 档 1 零削弱 AC)④ 观测面无人消费(⇒ AC-2 三段连通)⑤ 静默 no-op 造假的 0(⇒ AC-4 硬断言)⑥ 多节点族 sharer 维度不可承载(⇒ 全族前置未知) |

## 待裁(architect 追加裁决 · 3 项)

| # | 问题 | 选项 | planner 建议 |
|:--:|---|---|---|
| **Q1** | **data-value 维在 MESIF 生产流的可达性**:引擎无本地缓存数据版本(`on_completion` 恒装最新值)⇒ 生产流 **stale read 结构不可达** ⇒ 档 2 的 data-value 维**只能**在「注入陈旧副本」的负控下可达 | (i) 只做 sharer 1 维 · (ii) 做 2 维,但 data-value **仅负控可达**,生产流的 0 登记为「结构不可达」· (iii) 扩引擎模型支持本地缓存版本(工作量大 + 可能属协议语义设计) | **(ii)** —— 与档 0 的诚实边界同型;⛔ 若选 (i) 则本 change 范围显著收窄(且档 2 仍只有一个可判维度) |
| **Q2** | **版本写序的持有方**:per-engine 私有写序在「自读自写」下是**恒等式** ⇒ 恒 0 ⇒ 不构成判据 | (a) 写序由 **home 侧模型**(`HierarchicalDirectory` 或新的实现侧写序表)维护 ⇒ 跨节点可比 ⇒ **仍属实现侧**(⛔ 非测试侧代理)· (b) 保留 per-engine 写序,登记为「结构近似恒 0,仅作负控可达性见证」,⛔ **不进**档 2 证据结论 | **(a)** —— 但需 architect 确认「home 侧维护写序」**不构成新协议语义设计**(不改 bundle / 不改流字段 / 不改协议族语义)⇒ 否则回 (b) |
| **Q3** | **检测器归属**:仓内既有先例(`stub_cache_endpoint.hh:68-83`)=「实现侧只出原始事实,判定归 test 侧 harness」;R-3 纪律原文是「⛔ 实现**不得依赖** test 头」(⛔ 不禁实现侧有判定器) | (a) 新建 `include/tlm/cep/cep_invariant_detector.hh` 单一真源 + `test/cep_experiment_harness.hh` 薄委托(⇒ AC-10 档 1 零削弱)· (b) 沿用既有先例:实现侧只出观测记录,判定留 test 侧(⛔ 与「检测器不在 test 侧」的初稿硬约束冲突,须显式撤回) | **(a)** —— 档 2 是协议语义档,判定逻辑本身是产品能力;⛔ 禁止「复制一份」 |

## metis 首轮处置(2026-11-13 · 逐条)

| metis 建议 | 处置 |
|---|---|
| P0-1 SWMR 拆 3 具名判据(M1/M2/M3),禁止合并计数 | ✅ **采纳**(改写为 AC-3/AC-5):M1 = 引擎本地非法写(生产路径结构不可达 ⇒ 只作检测力见证)· M2 = 写时目录存在他人 sharer(**归因 run 内 home 行为**)· M3 = 过期读;⛔ M1 单列不计入 IC5 |
| P0-2 先裁版本维,否则删掉 | ✅ **采纳**⇒ 升为 **Q2**(architect 裁决) |
| P0-3 非空洞判据 + 断线三连硬失败 | ✅ **采纳**⇒ **AC-1** |
| P0-4 检测器归属矛盾 | ✅ **采纳**⇒ 升为 **Q3**(architect 裁决) |
| P0-5 `set_tracker` 装配期硬断言 | ✅ **采纳**⇒ **AC-4** |
| P0-6 六条约束改写为映射表 + 偏离登记 | ✅ **采纳**⇒ What Changes #8 + **AC-14** |
| P0-7 档 2 条目机器载体(schema v3 + T4/T5/T6) | ✅ **采纳**⇒ What Changes #9 + **AC-5** |
| P0-8 档 2 run 不得复用 `h3` | ✅ **采纳**⇒ **AC-6** |
| P1-9 per-event 记录 + 缓冲上限 | ✅ **采纳**⇒ What Changes #1 + **AC-8** |
| P1-10 runner 接线 + `not-collected` 纪律 | ✅ **采纳**⇒ What Changes #10 |
| P1-11 负控重命名与重定义 | ✅ **采纳**⇒ What Changes #7 + **AC-3** |
| P1-12 冻结面链路显式步骤(两次 `--update`) | ✅ **采纳**⇒ **AC-12** |
| P1-13 确定性 / 有界性 AC | ✅ **采纳**⇒ **AC-8** |
| P2-14 seam 目标写进 design 但不实现 | ✅ **采纳**⇒ What Changes #1(协议中立字段)+ out-of-scope |
| P2-15 禁表述清单 + grep 闸(须自带反向 fixture) | ✅ **采纳**⇒ **AC-9** |
| P2-16 全族推广的 `kResponderMappedBits` 能力缺口 | ✅ **采纳**⇒ 新增「全族推广前置未知」段 |
| 隐藏意图 M1~M4 + 反向动机 | ✅ **采纳**⇒ **Why** 的四条动机 + 「非空洞不成立即停手」写入 AC-1 |
| 歧义 1「档 2 的协议语义」未定义 | ✅ **采纳**⇒ 逐维声明归因对象(引擎 / run 内 home 行为 / harness),见 What Changes #7 与 AC-14 |
| 歧义 2「SWMR 两个判据并存」 | ✅ **采纳**⇒ M1/M2/M3 三具名 + ⛔ 不声明等价 |
| 歧义 3 per-event ≠ per-txn | ✅ **采纳**⇒ What Changes #1 |
| 歧义 4 版本载体与生命周期未定 | ✅ **采纳**⇒ 归入 **Q2**;design 须定死「版本 map 键独立于 `lines_` / `to == I` 不清版本 / `set_line` 显式推进版本」 |
| 歧义 5 检测器先例冲突 | ✅ **采纳**⇒ **Q3** |
| 歧义 6 档 2 条目落点 | ✅ **采纳**⇒ 落 `test/cep_e2e_run_tiers.json` v3(⛔ 不动不可变 ADR 的载体表) |
| 歧义 7「E2E」名不副实 | ✅ **采纳**⇒ 措辞改为「**引擎级 run**」(⛔ 不称 E2E —— 两个引擎 + 测试侧 home **不经**装配闸 / 拓扑 / `ModuleFactory` 路径) |
| 歧义 8「错标完成域」无对应物 | ✅ **采纳**⇒ 删,替换为「观测面自身失效」类负控 |
| 歧义 9 六条约束是否原样适用 | ✅ **采纳**⇒ What Changes #8(映射表 + 偏离登记) |
| 边界:假阴 8 类(node_id 未映射 / level ≥ 3 / 共用 node_id / `set_line` 播种 / 版本随 `lines_.erase` 丢失 / `on_snoop_resp` 丢 data / `Wr` 在 S/F/I 状态不变 / WRITEBACK 一 txn 两事件 / egress 满时 `txn_addr_` 残留) | ✅ **全部采纳**⇒ 逐条进 design 的「已知边界」段 + AC-1/AC-4/AC-8 |
| 边界:假阳 3 类(home 未发 snoop ⇒ 归因 harness / 保守 tracker ⇒ sharer 是上界 / `forward_to` 双重语义) | ✅ **全部采纳**⇒ 归因声明 + `completeness == EXACT` 断言 + 只用 `sharers`/`dirty` |
| 边界:时序 4 类(多写者交错 / 未写过地址 / 长 run 无界 / 多线程) | ✅ **全部采纳**⇒ 「不可判」而非 0 · 缓冲上限 · 单线程声明 |

## 关联

台账 **Entry #407**(architect 梳理)· **#406**(C′ 前置裁决:U-3 + 版本口径)· **#403**(档 2 评估)· **#405**(C8 归档回执)· `ADR-CEP-20` 决策 1/2/3a/4/5/6 · `ADR-CEP-06` SU#4 ② 六条约束 + `:697` 停手口径 · `ADR-CEP-04 §3`(虚方法数契约)· `ADR-CEP-12` · `07 号` 附录 SU 2026-11-13 ×2 · `docs/guide/cep-usage.md` §12.9 · `.rddf/reviews/cep-evidence-tier2-scale-assessment.md` · `.rddf/reviews/cep-tier2-prerequisite-rulings.md` · `.rddf/reviews/cep-current-state-and-forward-plan.md`
