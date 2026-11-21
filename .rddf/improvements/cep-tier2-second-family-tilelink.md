---
name: cep-tier2-second-family-tilelink
priority: P1
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-20
  - ADR-CEP-06
  - ADR-CEP-12
状态: "⏸ **已拆分 · ⛔ 不可开工**(2026-11-21 · architect 裁决 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四 = I-7 拆 **I-7a** = `TreeManager::view_of` + **I-7b** = `.rddf/improvements/cep-tilelink-home-side-state-driver` · ⛔ 本文件规划的「**engine 侧**两处观测落点(对仗 mesif)」路径**与 `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界行 + TLE-05 冲突**(引擎不持状态 / 不携带源态;「`Cap` 源不定」是 TileLink-C **规范事实**)⇒ 该路径作废 · ⚠️ 其「**硬前置 = `CEN-Ib8`(engine egress)**」的前提被本次裁决**否掉**(驱动不进引擎 ⇒ 无 egress 可依赖)⇒ ⛔ 本文件**不得**按原样开工 · 目标由 I-7a + I-7b 承接)"
created: 2026-11-20
---

# cep-tier2-second-family-tilelink: 档 2（协议级）观测面扩面到第 2 族 `tilelink_c`

> ⛔ **⏸ 已拆分 · 不可开工（2026-11-21 · architect 裁决）**
>
> 本单元的目标（档 2 扩面到 `tilelink_c`）由两个 sub-id 承接：
>
> | sub-id | 内容 | 载体 |
> |---|---|---|
> | **I-7a** | `TreeManager::view_of`（const 纯查询）· 档 2 **仍单族** | ⏳ 待 planner 修订 AC + 登记 sub-id |
> | **I-7b** | TileLink-C home 侧**消息簿记层** + tracker 侧观测 + 档 2 第二族 | `.rddf/improvements/cep-tilelink-home-side-state-driver.md` |
>
> ⛔ 本文件原规划的「**engine 侧**两处观测落点（对仗 mesif）」路径**作废** —— `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界行 + **TLE-05** 禁止引擎持有状态 / 预设 from-state（grep 0 处 `TileLinkStateSpec::State` / transition matrix 的机械判据）。
> ⚠️ 本文件的「硬前置 = `CEN-Ib8`(engine egress)」前提被同一裁决**否掉**：驱动**不进**引擎 ⇒ 无 egress 可依赖 ⇒ 该前置**不再成立**。
> ⇒ 保留本文件作**历史留痕**（⛔ 原文不改，仅追加本段与状态行）· 开工前请以 I-7a / I-7b 为准。

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文** · ⛔ **新增 invariant 数 = 0** · ⛔ **不产 4 件套 run**（⇒ 不改 `experimental/preregistration.json` / `metrics.schema.json` / `tools/render_report.py` / `scripts/test/cep_experiment_run.sh`，四者皆**冻结面**）· ⛔ **档 2 仍 ⭔ 无 run / 无 verdict / 无通过态** · ⛔ **不宣称「档 2 强于档 1」**（两者**正交**）· ⛔ **不改 `include/tlm/cep/sharer_tracker.hh`** · ⛔ **不改 `B3b-2` 计数** · ⛔ **不做全族推广**。

## 架构依据

档 2 = 证据阶梯的**天花板档**（协议语义级，per `ADR-CEP-20` 决策 6 的三档表）。现行载体形态（`07:719-737`）：**协议级观测面 + 契约级断言** —— 载体 = `protocol_engine_base.hh` 的 `EventObservation` + `mesif_protocol_engine.hh` 的两处观测落点 + `test/test_cep_tier2_observation.cc`；载体种类 = `protocol-engine-observation`（**非 run**）⇒ ⭔ **无 `run_id` / 无 `ic3_verdict` / 无「通过态」**；覆盖范围 = **单族（`mesif`）/ 单拓扑 / 一次实例**。

**可覆盖性地图**（`test/cep_evidence_tiers.json` 的 `coverage_map`，推导规则 = `(completeness == EXACT) && (!broadcast_fallback) && (impl != 不参与)`，机器判据 = `cep_e2e_reconcile_check.sh` 的 T7）：

| 实例 | sharer tracker impl | completeness | broadcast_fallback | `tier2_applicable` |
|---|---|---|---|---|
| `mesif` | `HierarchicalDirectory` | EXACT | false | ✅ **true**（已覆盖） |
| **`tilelink_c`** | **`TreeManager`** | **EXACT** | **false** | ✅ **true**（**未覆盖** ⇒ 本单元对象）|
| `moesi_gpu` | `SparseSnoopFilter` | CONSERVATIVE | true | ❌（SWMR 假阳性）|
| `cxl_3_0_symmetric` | `DcohSymmetric` | CONSERVATIVE | true | ❌ |
| `nvlink_4` | `BroadcastNull` | EXACT | **true** | ❌（恒全广播 ⇒ 恒判违例，**不是零违例**）|
| `chi_rnf` / `cxl_2_0` / `ualink_4` | 不参与 | — | — | ❌（无 sharer-set 知识需求）|

⚠️ **两条口径纪律**（`test/cep_evidence_tiers.json:45` note + `07:725-737`）：
1. `tier2_applicable = true` **只是必要条件** —— 「本表**不表示**各族的证据强度已完成」⇒ **不得**读成「判据已就位、只差实例化」。
2. 档 2 的 **data-value 维在生产流恒 0**（无本地缓存数据版本）⇒ 其可达形态**仅限**「注入陈旧副本」的负控 ⇒ ⛔ **不得**称「检测力已证」。

⚠️ **硬依赖**：`CEN-Ib8` 的 `trigger_condition` = 「engine 侧 egress 可观测面落地」⇒ 档 2 扩面**依赖** `cep-invariant-modeling-batch1` 的 `CEN-Ib8` 落地（`tilelink_protocol_engine.hh` 目前只有 `intent_observed` 等**计数**，**无** `EventObservation` 写入）⇒ **两单元不可并行开工**。

## 范围

- **In-Scope**：
  - `include/tlm/cep/tilelink_protocol_engine.hh` —— 两处观测落点（**形态由既有 `mesif` 落点对仗**，⛔ 不新造基类面）
  - `test/test_cep_tier2_observation.cc`（或同族新测试文件）—— `tilelink_c` 的状态迁移 + sharers 快照断言 + 负控
  - `test/cep_evidence_tiers.json` —— 档 2 登记新增 `tilelink_c` 条目（**非 run** 载体）
  - `scripts/test/cep_e2e_reconcile_check.sh` —— T4/T5 对新条目的校验路径（若已通用 ⇒ 零改动）
- **Out-Scope**：
  - ⛔ 4 件套 run / `hypothesis_id` / `ic3_verdict` / `deadlock_violations` / `not-collected`
  - ⛔ 版本维（**恒 0** = 本装置无该形态）· 权限维 · 标签维
  - ⛔ `moesi_gpu` / `cxl_*` / `nvlink_4` / `chi_rnf` / `ualink_4` 任何一族
  - ⛔ `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` 的既有虚函数签名
  - ⛔ `B3b-2` 计数（`tilelink_c` 是可读规范族，不属 B3b-2 的 4 个受限族）

## Why

**不做的代价**：档 2 是**证据强度的天花板**，但当前只覆盖 **1 族 / 1 拓扑 / 一次实例** ⇒ 「协议语义级证据」在本仓实质上只是 `mesif` 一族的单点观测 ⇒ 任何「平台已达天花板档」的表述都带着「单族」限定 ⇒ 扩到 `tilelink_c` 是**唯一低成本**的扩面路径（`TreeManager` 的 caps 已 EXACT 且无广播兜底 ⇒ 可回答「没有其他 sharer」⇒ SWMR 判据有判别力）。

**为什么选 `tilelink_c` 而不是别的族**：`moesi_gpu` / `cxl_3_0_symmetric` 的 completeness = CONSERVATIVE ⇒ **不能**回答「没有其他 sharer」⇒ 档 2 的 SWMR 判据会**假阳性**；`nvlink_4` 虽 EXACT 但 `broadcast_fallback = true` ⇒ sharer 视图**恒为全广播**⇒ 「他人 sharer」恒非空 ⇒ **恒判违例**（⛔ 不是零违例，也不是不可判）⇒ 三族都**结构上不适配**档 2 ⇒ `tilelink_c` 是**唯一**可扩的族。

**为什么必须先有 `CEN-Ib8`**：档 2 的观测面依赖 engine **egress** 可观测面；`tilelink_protocol_engine.hh` 当前只有进程内计数（`intent_observed` / `cap_events` / `grow_events` / `prune_events` / `report_events`）⇒ 无 `EventObservation` 写入 ⇒ 不铺观测面 ⇒ 扩面无载体。

**为什么不顺带扩到全族**：`mesif` 的既有落点形态已被 `07:719-737` 登记为「单族 / 单拓扑 / 一次实例」⇒ 全族推广属**另立 change 评估**（per `ADR-CEP-20` 决策 6 的登记纪律）。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **`tilelink_protocol_engine.hh` 观测落点** | 两处（对仗 `mesif_protocol_engine.hh`）：① `kind = transition`：`apply_event` 入口 ② `kind = fill`：`on_completion` 之后；⛔ **不改协议语义** · ⛔ **不改 bundle** · ⛔ **不动 `sharer_tracker.hh`** |
| 2 | **sharer 快照取法** | `tracker_->view_of(addr)`（`TreeManager` 的 **const 纯查询**）⇒ ⛔ **不得**调 `lookup`（有副作用：首次 forward 时 `dir_.emplace` + `events_.push_back(AllocateOnFirstForward)`）|
| 3 | **档 2 观测面断言** | 状态迁移落点 + 写事件 + 迁移后为脏态（SWMR 判定行）；装行落点**显式豁免**（同一次写产生 2 条迁移记录 + 1 条装行记录 ⇒ 逐行计数会重复计数）；违例条件 = 「写时该地址的 sharer 快照中除本节点外仍有其他持有者」；计数按「写 episode」归并后计 |
| 4 | **装配期硬断言** | `set_tracker` 侧：3 条硬断言（node 已映射 / level 合法 / tracker 非空）⇒ 失败 ⇒ 置 `observation_invalid` ⇒ run 判「**不可判**」（⛔ **不是 0**）；+ 1 条防御断言 `caps.completeness == EXACT`（标注「**非生产不变式**」）|
| 5 | **档 2 登记条目** | `test/cep_evidence_tiers.json` —— 新增 `tilelink_c` 的档 2 条目（`carrier_kind = protocol-engine-observation` · `run_produced = false` · `has_verdict = false` · 含 `coverage_scope` / `can_claim` / `cannot_claim` / `carrier_files`）|
| 6 | **T4/T5 校验** | `scripts/test/cep_e2e_reconcile_check.sh` 的 T4（字段齐备 + 载体文件实际存在 + `run_produced == false` + `has_verdict == false`）/ T5a（禁肯定式「通过态」）/ T5b（禁夸大短语）/ T5c（禁跨档 run 引用）⇒ 对新条目生效（若已通用 ⇒ 零改动）|
| 7 | **覆盖地图自洽** | T7 判据仍绿：`coverage_map.families` 的实例集 == `protocol_instances.json` 的实例集；每实例 `impl` 与声明一致；`tier2_applicable` 与推导规则一致；caps 与**代码真源** `sharer_tracker.hh` 的 `kTrackerTraits`（6 行）逐字段一致 |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（档 2 覆盖从 1 族 → 2 族）**：`test/cep_evidence_tiers.json` 的档 2 条目数 ≥ **2**（`mesif` + `tilelink_c`）；`bash scripts/test/cep_e2e_reconcile_check.sh` 的 T4/T5/T7 全绿；⚠️ **无 `run_id` / 无 `ic3_verdict` / 无「通过态」**（grep 断言）
- [ ] **AC-2（非空洞判据 · 真看到过他人 sharer）**：`tilelink_c` **正控用例**须机器断言 —— ① ≥1 条观测 `sharers & ~self_mask != 0` ② ≥1 个**写 episode**完整经历 `transition(Wr→M)` + `fill` ③ 断线三任一（node 未映射 / 非法 level / `tracker_ == nullptr`）⇒ **硬失败**（⛔ **不是 0**）；⚠️ **术语统一（per metis 歧义 5）**：本单元 ⛔ **不产 4 件套 run**（`run_id` / `hypothesis_id` / `ic3_verdict` / `deadlock_violations` 全部不产出）⇒ 此处「正控**用例**」= **测试用例**，⛔ **不是**「4 件套 run」⇒ 与 AC-1 的「无 `run_id`」不矛盾 |
- [ ] **AC-3（provenance · 反 fixture 播种）**：判定窗口内**禁止 fixture 播种目录** ⇒ 每个 sharer bit 须可溯源到 **peer 引擎自身的 `apply_event → sync_tracker`**；⚠️ 这是**反「用台架喂饱判据」**的逃逸口（per `cep-evidence-tier2-minimal-slice` 的 M3 反向动机）
- [ ] **AC-4（负控 · 同流 + 单差异 + 精确计数）**：3 负控各自「同一输入、唯一差异」⇒ 违例数**精确 == 1**；⚠️ 计数**按 `(addr, write-episode)` 归并后**计（⛔ 不按原始行 —— WRITEBACK 的「2 transition + 1 fill」会倍增）；⚠️ 负控**不得**用「直接调 `apply_event` 注入」冒充生产流可达
- [ ] **AC-5（判定行钉死）**：SWMR 判定行**只**取 `kind = transition ∧ event = Wr ∧ to = M`；`kind = fill` 行**显式豁免**；⇒ 正控与负控的记录行数差异须与该口径**自洽**
- [ ] **AC-6（装配期硬断言）**：3 条硬断言各 1 个反 fixture ⇒ 置 `observation_invalid` ⇒ 判「**不可判**」⇒ ⛔ **不得**记 0；⚠️ 第 4 条（`completeness == EXACT`）标注「**非生产不变式**」且**不要求**反 fixture
- [ ] **AC-7（档 1 零改动）**：`test/cep_experiment_harness.hh` **零 diff** ⇒ 档 1 既有负控（`test_cep_experiment_harness` / `test_cep_b4_evidence_device` / `test_cep_stub_cache_e2e`）**逐字绿**
- [ ] **AC-8（诚实边界登记 · 禁夸大表述）**：新增产出物内 grep 禁表述 —— `检测力已证` / `全族达成` / `所有族` / `档 2 强于档 1` / `通过态` ⇒ **命中即红**；⚠️ 该 grep 闸**必须自带反向 fixture**；⚠️ 逐字要求「**单族扩展（mesif + tilelink_c）/ 单拓扑 / 无通过态**」与「data-value 生产流恒 0 = **本装置无该形态**」同段
- [ ] **AC-9（范围纪律 · 零越权）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `experimental/` · `tools/` · `scripts/test/cep_experiment_run.sh`；**新增 `INV-CEP-` 数 == 0**；`SharerTrackerIf` 虚方法数 == 5 的既有断言仍绿；⚠️ 不得用 `grep -rn "test/" include/tlm/cep/`（实测已有 13 处命中 ⇒ 假阳）
- [ ] **AC-10（0 回归）**：`ctest` 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
- [ ] **AC-11（冻结面五步链 · 归档必做 · per metis 缺失 AC 9）**：本单元归档时 delta 会合并 `openspec/specs/cep-evidence-tier2-observation/spec.md` ⇒ 该文件**在 `FROZEN_GLOBS` 内**（spec 侧是 `openspec/specs/cep-*/spec.md` **glob 全量**）⇒ 判据 = ① `bash scripts/test/cep_freeze_check.sh` 的 `artifact_count` **逐字节不变** ② `--update --authorized-by "Entry #N"` **执行一次** ③ 冻结清单**单独 commit**（⛔ 与台账 Entry 不得合并提交，per `docs/guide/cep-usage.md §12.6`）④ 无参复验 rc=0；⚠️ **未走五步链的 spec 改动 ⇒ 判红** |

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-evidence-tier2-observation` | **MODIFIED** | 档 2 观测面的覆盖族从 `mesif` 扩到 `mesif` + `tilelink_c`；⚠️ ⭔ **仍不含** 4 件套 run / ⭔ **仍不含** verdict 判定 / ⭔ **仍不含**版本维 ⇒ 归档时合并进 `openspec/specs/cep-evidence-tier2-observation/spec.md` |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `include/tlm/cep/tilelink_protocol_engine.hh`（两处观测落点）· `test/cep_evidence_tiers.json`（+1 档 2 条目）· `openspec/specs/cep-evidence-tier2-observation/spec.md`（归档时 MODIFIED 合并）· 台账 Entry（追加） |
| **零改动（硬查）** | `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh`（既有虚函数签名）· `docs/adr/**` · `docs/architecture/**` · `experimental/**` · `tools/render_report.py` · `scripts/test/cep_experiment_run.sh` · `test/cep_experiment_harness.hh` |
| **冻结面** | `openspec/specs/cep-evidence-tier2-observation/spec.md` **已在** `FROZEN_GLOBS`（`openspec/specs/cep-*/spec.md` glob 全量）⇒ 归档合并后**一次** `--update --authorized-by` ⇒ 清单**单独 commit**；⚠️ `include/tlm/cep/tilelink_protocol_engine.hh` **不在**冻结集（该 glob 只含 3 个 json）|
| **闸** | ctest +1（档 2 观测族）· `cep_e2e_reconcile_check.sh` T4/T5/T7 覆盖新条目（通用则零改动）· ⛔ 不新增闸脚本类型 |
| **KPI** | **0/3 → 1/3** ⇒ 归档 ⇒ **0/3** |
| **风险** | ① `CEN-Ib8` 未落地 ⇒ 无观测面 ⇒ **本单元被前置卡住**（AC-2 是兜底判据，不解依赖）② `TreeManager::view_of` 若非 const 纯查询 ⇒ 快照取法需改 ⇒ Q 项 ③ data-value 维恒 0 ⇒ 只能作负控可达形态 ⇒ AC-8 兜底 ④ 负控用注入冒充生产流 ⇒ AC-4 兜底 ⑤ 档 2 扩面被读成「档 2 覆盖全族」⇒ AC-8 禁表述闸兜底 |

## 待裁

| 问 | 内容 | 建议 |
|:--:|---|---|
| **Q-a** | `TreeManager` 是否提供 **const 纯查询**的 sharer 视图（`view_of`）？ | ⚠️ **已读码实测（2026-11-20）= 否** —— `include/tlm/cep/tilelink_tree_manager.hh` 只有 `lookup`（`:141`），**无 `view_of`**；且 `:36` 注释明写 lookup 假设「单次同步回答全局问题」⇒ **有副作用**。⇒ **本单元 ⛔ BLOCKED**，须先裁 ①**在具体类新增 `const` 纯查询**（⚠️ ⛔ 不动 `SharerTrackerIf` 虚方法数 = 5，per AC-9）②**用 `lookup` + 状态回滚**（⚠️ 会污染 `events_`）|
| **Q-b** | `tilelink_c` 的档 2 观测落点是否**完全对仗** `mesif`（transition + fill 两处）？ | 建议对仗 ⇒ 观测面结构可复用 ⇒ M2（为 4 受限族预铺 seam）成立 |
| **Q-c** | 扩面后档 2 的读法措辞 | 仍「**两族** / 单拓扑 / **无通过态**」；⛔ 禁「全族达成」 |

⇒ **Q-a 是架构取舍（新增纯查询 API 的形态），⛔ 不是「实施前读码」**（初稿把它读成读码细节 = 事实错误，per metis B7）⇒ **须 architect 裁决**，落 `.rddf/reviews/` 后本单元方可开工。
⇒ **Q-b 属判据细节**（不发明 invariant 语义 ⇒ 本单元**不新增** invariant）⇒ 不需 architect 裁决。
⇒ ⛔ **本单元前置 = ① `cen-invariant-modeling-batch1` 的 `CEN-Ib8` 落地（AC-2 的观测面依赖）② Q-a 裁决落盘**（初稿只写了 ①，⛔ 漏了 ② ⇒ 会开工到一半才发现缺 API）。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 指出「`tier2_applicable = true` 只是必要条件」的过度陈述 | ⇒ 本单元的 Why 段显式写入该口径纪律 |
| 2 | oracle | **REJECT** · 修法 1（I-5 前置）+ 修法 9（无机关） | ⇒ v3 显式写「S1-2 依赖 S1-1 批 1，不可独立排期」|
| 3 | critic（v2） | **REJECT** · 3 BLOCKER | ⇒ 本单元不碰 B2 行；批次归属唯一 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ 本单元不含 KPI 活口径取值 |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | ⇒ 不涉及本单元；NITS 确认 S1-2 的 data-value 写法合规 |

⇒ **本单元须待 `cep-invariant-modeling-batch1` 的 `CEN-Ib8` 落地后开工**。
