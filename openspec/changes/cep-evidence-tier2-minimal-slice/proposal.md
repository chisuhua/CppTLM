# cep-evidence-tier2-minimal-slice: 档 2(协议级)观测面 + 契约级断言

> **状态**:🟡 in-progress(2026-11-13 建;**A0~A3 已完成**,A4~A6 待办 ⇒ ⛔ **未归档**)
> **规范载体**:`.rddf/improvements/cep-evidence-tier2-minimal-slice.md`(v2)· `.rddf/reviews/cep-tier2-prerequisite-rulings.md`(C′ / C′-bis / C′-bis-ter 三段裁决)· 台账 Entry **#408**(裁决)/ **#409**(草稿 v2)
> **⛔ 范围纪律**:不改任何已签批 ADR · 新增 invariant 数 = **0** · **不产 4 件套 run** · 无版本维 · 不改 bundle · 不改 `SharerTrackerIf` 虚方法数 · 不改 `sharer_tracker.hh` · 检测器留 `test/` 侧 · 不动 `B3b-2`

## Why

**14 个 CEP change 已全部归档;DoD 12 项里唯一未达成的是 `B3b-2 = 0/4`**(4 个受限族运行时引擎 —— CXL 2.0 非对称「显式冻结」/ CXL 3.0 DCOH / NVLink / UALink;其「转移表真源」在**仓外或受限**,⛔ 本仓单方面推不动)⇒ 终态 DoD Go 挂起.

⇒ 本仓**还能推进的最高价值**的一件事 = **把证据档位阶梯的天花板档(档 2 = 协议级)在本仓落地**。`ADR-CEP-20` 决策 5 已判「**判别力的强度上限在本仓的协议引擎面**(⛔ 非跨仓专属)」⇒ 天花板档**就在本仓**;而当前三档里**只有档 0 / 档 1 有载体**(`07 号:696` 的档 2 机器载体 cell 写「⭕ 观测面未建」).

**为什么不是「产一份 run」**:四条产 run 的路逐条走死 —— ① 复用 `hypothesis_id = h3` ⇒ H3 的 `out_of_scope` 不含 `deadlock_violations` ⇒ 必须实测 ⇒ 引擎面无死锁形态 ⇒ 只��**人造 0**;② 新增假设行 ⇒ 改冻结面 `experimental/preregistration.json` + 真源 `§9.1 ②` ⇒ **B7 从 8/8 退到 8/9** ⇒ DoD 退步;③ `not-collected` ⇒ `ic3_verdict()` 必返 ⚪ ⇒「档 2 通过态」不可得;④ 写实测 0 = 人造 0。⇒ ⇒ **档 2 = 观测面 + 契约级断言,⛔ 不产 run**(裁决 R3;「第五条路」经 oracle 复核 = **定价后排除**,理由见 review 文档 C′-bis-ter §1)。

**为什么不是「再加一个 0」**:档 0 的失败**不是「0 假」**,是「**0 被读成结论**」。档 2 会以**更强标签**(协议级)出现同样的 0 ⇒ 本 change 的判据里含**非空洞判据 + provenance + 不可判语义 + 跨档防夸大**四道机制。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **基类 per-event 观测面** | `include/tlm/cep/protocol_engine_base.hh` | 新增 `EventObservation` 结构 + 有界观测缓冲 + 访问器;⛔ 不改既有虚函数签名;⛔ **无 version 字段**;⛔ sharer 字段用 `std::uint64_t` ⇒ 基类**不新增** `sharer_tracker.hh` 依赖;观测面**默认关闭** ⇒ 既有行为与性能零影响 |
| 2 | **`mesif` 两处观测落点** | `include/tlm/cep/mesif_protocol_engine.hh` | ① `kind = transition`:`apply_event` **入口**(`sharers_snapshot` = **迁移前**视图);② `kind = fill`:`on_completion` 装行**之后**;sharer 快照取 `HierarchicalDirectory::view_of`(**const 纯查询**)⇒ ⛔ 不用有副作用的 `lookup`;归因经 `apply_event_as(origin, ...)` 传递 ⇒ 外部直调一律标 `DirectInject` |
| 3 | **装配期硬断言** | `mesif_protocol_engine.hh::set_tracker` | 3 条硬断言(目录未接线 / 节点未映射到 sharer 位 / 层级越界)⇒ 置观测面**不可判**;+ 1 条防御断言(`completeness == EXACT`,⚠️ 生产路径恒真) |
| 4 | **判定器 + 契约级断言**(test 侧) | `test/test_cep_tier2_observation.cc`(**新**) | 实现侧只出原始事实 ⇒ 判定留 `test/` 侧(沿用 `stub_cache_endpoint.hh:68-83` 先例);7 cases:非空洞正控 / SWMR 负控(精确 == 1)/ 判定行钉死 / data-value 负控 / 断线三连 / 确定性 / 默认关闭 |

## 验收标准(Acceptance Criteria)

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因;⛔ 「> 0」不构成精确计数。

- [ ] **AC-1(非空洞判据 · 停手条件)**:正控 run 须机器断言 —— ① ≥1 个**写 episode**完整经历「transition(Wr→M)+ fill」② 断线三任一 ⇒ **硬失败**(⛔ 不是 0)③ ⚠️ **provenance**:判定窗口内**禁止 fixture 播种目录** ⇒ 每个 sharer bit 须可溯源到 **peer 引擎自身的 `apply_event → sync_tracker`**(否则 ⇒ **停手回报**)
- [ ] **AC-2(观测面 → 判定 → 断言 三段连通)**:反向 fixture —— 拔掉任一段 ⇒ **按签名红**;还原 ⇒ 绿
- [ ] **AC-3(负控 · 同流 + 单差异 + 精确计数)**:3 负控各自「同一输入、唯一差异」⇒ 违例数 **精确 == 1**;⚠️ 计数按 `(addr, write-episode)` 归并后计(⛔ 不按原始行);⛔ **M1(引擎本地非法写)不进本切片**
- [ ] **AC-4(装配期硬断言)**:3 条硬断言各 1 个反 fixture ⇒ 置 `observation_invalid` ⇒ 判「不可判」⇒ **⛔ 不得**记 0;⚠️ 第 4 条标注「非生产不变式」且**不要求**反 fixture
- [ ] **AC-5(档 2 登记面 + 跨档聚合检查)**:新表字段齐备 + T4/T5 全绿;两项**机器检查**(不得与档 0/1 的 0 并列 / 不含「通过态」表述)各带 **1 个反向 fixture**
- [ ] **AC-6(判定行钉死)**:SWMR 判据**只**取 `kind = transition ∧ event = Wr ∧ to = M`;`kind = fill` 行**显式豁免**;⇒ 一次 WRITEBACK = 2 transition + 1 fill 但**写 episode 计 1**
- [ ] **AC-7(范围纪律 · 零成本但必须查)**:`git diff` 对 `include/bundles/` · `docs/adr/` · `include/tlm/cep/sharer_tracker.hh` · `experimental/**` · `tools/render_report.py` · `scripts/test/cep_experiment_run.sh` · `test/cep_e2e_run_tiers.json` · `test/cep_experiment_harness.hh` · `protocol_instance.schema.json` 均为**空**;`grep -rn '#include[[:space:]]*"\(test/\|\.\./test\)' include/` == 空 ∧ `grep -rn 'catch_amalgamated\|cep_experiment_harness' include/` == 空;**新增 `INV-CEP-` 数 == 0**;`SharerTrackerIf` 虚方法数 == 5 的既有断言仍绿
- [ ] **AC-8(确定性 + 有界性)**:同一流跑两遍 ⇒ 观测记录**逐字相等**;缓冲有上限 + `dropped_observations` 计数(非零 ⇒ 判不可判)
- [ ] **AC-9(防夸大机器面)**:新增产出物内 grep 禁表述 —— `检测力已证` / `全族达成` / `所有族` / `档 2 强于档 1` / `通过态` ⇒ **命中即红**;⚠️ 该闸须自带反向 fixture
- [ ] **AC-10(档 1 零改动)**:⛔ `test/cep_experiment_harness.hh` **零 diff** ⇒ 档 1 既有负控逐字绿
- [ ] **AC-11(总纲 SU + 手册 + 索引)**:`07 号` 末尾追加 SU(5 条标题约束逐条满足;阶梯表档 2 行载体 cell 更新)+ 索引重生成 + `cep_index_position_check` 绿;`cep-usage` 新节落盘
- [ ] **AC-12(冻结面五步链 ×1)**:授权 Entry 先落台账 → 产物 commit → `--update --authorized-by "Entry #N"`(**一次**,归档前)⇒ 清单**单独 commit** ⇒ 无参复验 rc=0 ⇒ 冻结产物 **84 → 85**
- [ ] **AC-13(0 回归)**:`ctest` 全绿(⛔ 不写死数字)· 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · 工作区干净
- [ ] **AC-14(诚实边界登记)**:新 SU + 手册 + spec 中逐字写明 —— ① data-value 生产流恒 0 = 本装置无该形态 ② 档 2 **无 run / 无通过态** ③ **单族(mesif)/ 单拓扑** ④ 档 2 与档 1 **正交** ⑤ 档 2 判别力 **≠** 协议语义全覆盖

## 成本收益

- **估时** ≈ **5-7 人日**(裁决 R8;较 v1 的 7-9 下调 —— 消除 4 件套 / runner / 预登记 / 检测器搬迁四项)
- **收益** 档 2 载体从「⭕ 未建」变成「可机判」;**为 B3b-2 的 4 族预铺观测面 seam**;R-4 从「排期债」降级为「最小切片已交付 + 全族另计」
- **代价** 零冻结面变更(除归档时的新 main spec)· 零 DoD 代价(B7 分母不变)· 档 2 **不产 run**(与档 0/1 形态不同 ⇒ 须显式登记)

## 诚实边界(⛔ 不可约残余)

1. **档 2 ⭔ 不产 run** ⇒ ⛔ 无「通过态」· ⛔ 不得宣称「档 2 强于档 1」(两者**正交**);
2. **data-value 维在生产流恒 0** = 「本装置无该形态」⇒ ⛔ 不得称「检测力已证」;
3. **权限维 / 标签维被排除** ⇒ 档 2 判别力 **≠** 协议语义全覆盖;
4. **多节点族的 sharer 维度不可承载**(`responder_mask_of` 只映射 4 bit)⇒ 全族推广是**能力缺口**,非工时问题;
5. **本 change 不证明档 2 可实施** —— AC-1 不成立 ⇒ ⛔ **停手**(不立项优于立项)。

## 关联

台账 **#409**(草稿 v2)/ **#408**(裁决 R1~R8 + 勘误)/ **#407**(architect 梳理)/ **#406**(C′ 段)/ **#403**(档 2 评估)· `.rddf/reviews/cep-tier2-prerequisite-rulings.md` · `ADR-CEP-20` 决策 5/6 · `ADR-CEP-06` SU#4 ② · `07 号:690,696` · `include/tlm/cep/protocol_engine_base.hh` · `include/tlm/cep/mesif_protocol_engine.hh` · `include/tlm/cep/sharer_tracker.hh`
