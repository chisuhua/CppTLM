# tasks —— `cep-evidence-tier2-minimal-slice`

> 顺序 = **A0(授权门) → A1(实测复核) → A2(观测面) → A3(判定器 + 契约级断言) → A4(档 2 登记面) → A5(总纲 SU + 手册) → A6(冻结五步链 + 归档)**。
> **门**:**G1 = A0** · **G2 = A2**(观测面非空洞)· **G3 = A3**(正负配对 + 判定行 + 断线三连)· **G4 = A4**(登记面机器面)· **G5 = A5**(总纲/手册)· **G6 = A6**(冻结 provenance + 归档)。
> ℹ️ 编号区分:本文件的 **G1~G6 = 阶段门**;design 的 **D-1~D-7 = 设计段**;proposal 的 **AC-1~AC-14 = 验收判据**。

## A0 · 授权门(G1)
- [x] A0.1 台账 Entry 授权实施(占 1 slot,KPI **0/3 → 1/3**) ⇒ ✅ 台账 **Entry #410**(同时作为实施授权与 A0~A3 回执)
- [x] A0.2 improvement(v2)+ architect 裁决(R1~R8 + 勘误)已落盘 ⇒ ✅ `.rddf/improvements/cep-evidence-tier2-minimal-slice.md` v2 + `.rddf/reviews/cep-tier2-prerequisite-rulings.md` 三段
- [x] A0.3 四件套已建 + `openspec validate --strict` rc=0 ⇒ ✅ 四件套(proposal / design / tasks / delta spec)· validate rc=0
- [x] A0.4 基线实测:`apply_event` 落点 / `view_of` const 纯查询 / `responder_mask_of` 4 bit / `kLevelCount` / 既有 `set_tracker` 调用点 ⇒ ✅ 全部落 design D-1/D-4/D-5

## A1 · 实测复核(⛔ 只读 · 不改代码)
- [x] A1.1 复核 U-1:`mesif` 的事务边界是否足以承载 per-event 观测 ⇒ ✅ `apply_event` 是**全部迁移的单点入口**(`:325`)⇒ 无需引入事务抽象
- [x] A1.2 复核 U-2:`SharerTrackerIf` 是否可查 sharer 集合 ⇒ ✅ `LocalView.sharers` 可查;`view_of` 为 **const 纯查询**;`lookup` 有副作用 ⇒ 观测点取 `view_of`
- [x] A1.3 复核「静默 no-op」三处退化 + `tracker_feeds` 照涨的假象 ⇒ ✅ 落 D-4 ⇒ AC-4 硬断言
- [x] A1.4 复核「行数据写入点 ≠ 状态迁移点」⇒ ✅ `:442` 是 data 唯一写入点 ⇒ 落 D-1 落点②
- [x] A1.5 复核既有 `set_tracker` 调用点是否会被硬断言误伤 ⇒ ✅ `test/test_cep_mesif_protocol_engine.cc:217` 用 `(kNodeIdSdma, level=0)` ⇒ 合法 ⇒ 零误伤
- [x] A1.6 ⛔ 若任一复核推翻 design D-1/D-4 ⇒ **停**并回报(⛔ 不许硬做) ⇒ ✅ 无需停

## A2 · 观测面(G2)
- [x] A2.1 `protocol_engine_base.hh`:新增 `EventObservation`(字段表按 R5)+ `operator==` ⇒ ✅ 字段表定死;⛔ **无 version 字段**;sharer 用 `std::uint64_t`
- [x] A2.2 `protocol_engine_base.hh`:观测缓冲 + 访问器 + 有界上限 + 丢弃计数 + 失效计数 ⇒ ✅ 容量 4096;`dropped_observations` / `observation_invalid`
- [x] A2.3 观测面**默认关闭** ⇒ 既有行为与性能零影响 ⇒ ✅ `observations_enabled_ = false`
- [x] A2.4 `mesif`:新增 `apply_event_as(origin, ...)`;`apply_event` 重载 ⇒ 外部直调标 `DirectInject` ⇒ ✅ 7 处内部调用点已改归因
- [x] A2.5 `mesif`:`record_transition`(入口 · 迁移前视图)⇒ ✅ 在 `sync_tracker` **之前**取 sharer 快照
- [x] A2.6 `mesif`:`record_fill`(装行**之后**)⇒ ✅ `data_before` 用 `find` 取(⛔ 不用 `operator[]`)⇒ 不改 `line_count()`
- [x] A2.7 `mesif`:`set_tracker` 加 3 硬断言 + 1 防御断言 ⇒ ✅ 失败 ⇒ `observation_invalid` +1
- [x] A2.8 ⛔ 不改 `sharer_tracker.hh` / `include/bundles/` / `docs/adr/` / `experimental/**` / `tools/render_report.py` / `scripts/test/cep_experiment_run.sh` / `test/cep_e2e_run_tiers.json` / `test/cep_experiment_harness.hh` ⇒ ✅ 零 diff(AC-7)
- [x] A2.9 ⛔ 不改既有虚函数签名;`SharerTrackerIf` 虚方法数 == 5 的既有断言仍绿 ⇒ ✅ 零改动 `sharer_tracker.hh`

## A3 · 判定器 + 契约级断言(G3)
- [x] A3.1 判定器(test 侧 · per 裁决 R2):SWMR 判定行 + data-value 判定 + 不可判标记 ⇒ ✅ `detect()`;归因分层写进 D-2
- [x] A3.2 非空洞正控:独占写 ⇒ 写时无他人 sharer ⇒ 违例 0 ⇒ ✅ 断言 `write_episodes == 1` / `swmr_violations == 0` / `observation_count() > 0`
- [x] A3.3 负控①:写时存在他人 sharer ⇒ SWMR 违例**精确 == 1**(同流 + 单差异) ⇒ ✅ 两个引擎共享同一 `HierarchicalDirectory` + 不同 node
- [x] A3.4 判定行钉死(AC-6):一次 WRITEBACK = **2 transition + 1 fill** ⇒ 写 episode 计 **1** ⇒ ✅ 三项断言全绿
- [x] A3.5 负控②:home 回陈旧值 ⇒ data-value 违例**精确 == 1** + 正控配对(回最新值 ⇒ 0) ⇒ ✅
- [x] A3.6 断线三连(AC-4):未接线 / 节点未映射 / 层级越界 ⇒ `observation_invalid` ≥ 1 + 正控零 ⇒ ✅ 4 项断言
- [x] A3.7 确定性(AC-8):同流两遍 ⇒ 观测记录**逐字相等** ⇒ ✅ `operator==` 逐条比对
- [x] A3.8 观测面默认关闭 ⇒ 零记录 + 状态迁移照常 ⇒ ✅
- [x] A3.9 ⛔ `test/cep_experiment_harness.hh` 零 diff ⇒ 档 1 既有负控逐字绿(AC-10) ⇒ ✅ 零 diff;`[cep]` 523 cases 全绿

## A4 · 档 2 登记面(新表 + T4/T5)
- [x] A4.1 新增 `test/cep_evidence_tiers.json`(档 2 的**非 run** 载体登记) ⇒ ⛔ **不进** `test/cep_e2e_run_tiers.json`(只登记 run) ⇒ ✅ `test/cep_evidence_tiers.json`(schema `cep-evidence-tiers/v1`;1 条档 2 非 run 载体登记)
- [x] A4.2 `cep_e2e_reconcile_check.sh` 增 **T4**(档 2 条目字段齐备 ∧ 含档位词)· **T5**(跨档聚合检查:不得与档 0/1 的 0 并列 / 不含「通过态」表述) ⇒ ✅ `cep_e2e_reconcile_check.sh` 增 **T4**(字段齐备 + 载体文件存在 + `run_produced/has_verdict` 纪律)· **T5a**(肯定式「通过态」)· **T5b**(夸大表述)· **T5c**(跨档 run 引用)
- [x] A4.3 T4/T5 的**反向 fixture**各 1 条(影子树 + `CEP_RECONCILE_*` 覆盖)⇒ 按签名红 + 还原绿 ⇒ ✅ 变异 **e**(肯定式通过态)· **f**(跨档 run 引用)按签名红 + 真实仓字节指纹相等 ⇒ 共 **6** 条变异全绿

## A5 · 总纲 SU + 手册
- [x] A5.1 `07 号` **末尾追加** SU(⛔ 不改正文;5 条标题约束逐条满足;阶梯表档 2 行载体 cell 更新为「协议级观测面 + 契约级断言(⭕ 无 run / 无通过态)」) ⇒ ✅ `07 号` 新 SU「Status Update 2026-11-14(档 2 协议级观测面 · per 台账 Entry #410)」(4 节 + 6 条读法边界 + 阶梯表档 2 行现行 cell)
- [x] A5.2 `scripts/cep/generate_doc_index.py` 重生成 + `cep_index_position_check` 绿 ⇒ ✅ `generate_doc_index.py --write` 重生成(一级节 **33** / SU **20**,三合一一致)+ `cep_index_position_check` rc=0
- [x] A5.3 `docs/guide/cep-usage.md` 新节落盘(档 2 的读法 + 不可判语义 + 跨档防夸大) ⇒ ✅ `docs/guide/cep-usage.md` **新 §12.10**(载体与登记面 / 四条硬边界 / 新族接入步骤 / 前置未知)+ §12.9 末尾**一行导航**
- [x] A5.4 诚实边界逐字登记(AC-14 的 5 条) ⇒ ✅ 诚实边界 6 条逐字落 SU ② + 手册 §12.10(数据维恒 0 / 无通过态 / 单族单拓扑 / 正交非强弱 / 非全覆盖 / 不可判≠零违例)

## A6 · 冻结五步链 + 归档(G6)
- [x] A6.1 授权 Entry 先落台账 ⇒ 五步链:Entry → 产物 commit → `--update --authorized-by "Entry #N"`(**一次**,归档前)→ 清单单独 commit → 无参复验 rc=0 ⇒ 冻结 **84 → 85** ⇒ ✅ 第一次 `--update`(授权 Entry #411 · 84 产物 hash 刷新 · 清单单独 commit · 无参复验 rc=0)⇒ 第二次待归档后执行
- [x] A6.2 `openspec validate --strict` rc=0 + **tasks 全勾** ⇒ `openspec archive cep-evidence-tier2-minimal-slice --yes` rc=0(✅ 已完成归档语义 · **带 delta** ⇒ 合并出 `openspec/specs/cep-evidence-tier2-observation/spec.md`) ⇒ ✅ `openspec archive cep-evidence-tier2-minimal-slice --yes` rc=0(archivedAs = `2026-11-14-cep-evidence-tier2-minimal-slice` · specsUpdated **added 1** ⇒ 合并出 `openspec/specs/cep-evidence-tier2-observation/spec.md`;⛔ **已完成归档**语义 · 带 delta · 新增 invariant 数 = 0)
- [ ] A6.3 KPI **1/3 → 0/3** + `AGENTS.md` / 台账 §5.2 的 KPI baseline 与体量数字回填
- [ ] A6.4 全量回归:`ctest` 全绿 · 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · 工作区干净
