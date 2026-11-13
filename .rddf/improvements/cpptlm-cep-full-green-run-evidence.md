---
name: cpptlm-cep-full-green-run-evidence
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— 「拓扑级全绿 run」缺口的闭合(IC3 拓扑级 E2E run 通过态)
依赖:
  - ADR-CEP-06
  - ADR-CEP-05
  - ADR-CEP-19
状态: "🟡 待实施（2026-11-13 起草;per 台账 **Entry #383**(R-3 归因)+ **#392**(根因首定位)+ **#393**(P-1 前置核对 + 根因精化);architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 订正;⚠️ 其 §一.3 / 硬边界 2 的「合成」表述**说反了** —— 见下 §前提订正);⚠️ **2026-11-13 审查后修订**:缺口口径由「真实 run 的通过态实例」订正为「**拓扑级(E2E 链)** run 的通过态实例」;⛔ 本 change **不改任何判据 / 阈值 / 预登记** —— 只补「拓扑级真实 run 的通过态实例」这一证据面)）"
created: 2026-11-13
---

# cpptlm-cep-full-green-run-evidence: 「拓扑级全绿 run」缺口的闭合 —— 让 IC3 在**拓扑级 E2E run** 上落在「通过」态

> **范围纪律**:⛔ **不改** IC3 判据 / 阈值 / 预登记(`experimental/preregistration.json` **零 diff`) / `metrics.schema.json` / retention 条款 · ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不新建 ADR · ⛔ 不动 `§4.0` 排期值 · ⛔ 不改任何 ADR **决策 / 正文**。
> **估时**:≈1.5-3 人日(含前置核对 ≈0.5d;⚠️ 若 P-3 判定需扩围 ⇒ 停并回报重立项)

## 前提订正(2026-11-13 · 审查后 · append-only)

| # | 原表述 | 订正 |
|:--:|---|---|
| ① | 「**一致性闸门(IC3)至今没有一个「真实 run」的通过态实例**」⇒「B4 的证据链**缺最后一环**」 | ⛔ **不成立**。`obj §6.2 B4`(Entry #228 ③)与 `docs/validation/2026-11-05-cep-b4-evidence-device.md` §0 均明写 B4 的 **h3 v2 run** 是「IC3 三态中该态**首个真实 run 实例**(此前仅合成 fixture **F1**)」⇒ 实测 `experimental/run-20261105-153418-h2h3h4h6/h3/`:两违例类实测 0 且均在 `_measured` · `hypothesis_id = "h3"` · `report.html` 渲染「一致性闸门通过」⇒ **缺口 = 拓扑级(E2E 链)缺口** |
| ② | 「Entry #228 \| B4 的达成证据形态(装置 + **合成** fixture 的通过态实例)」 | ⛔ **说反了**。B4 实例 = **真实 run**(装置级 / 模型级);**合成**的是更早的 fixture F1 |
| ③ | 新闸 G4「⛔ 非合成 fixture」 | ⛔ **判据方向错**(模型级 run 也不是 fixture)⇒ 改为**拓扑链身份锚**(C4) |

## 架构依据

| 依据 | 内容 |
|---|---|
| 台账 **Entry #393** | P-1 前置核对 + 根因**精化**(⛔ 装置 `ConsistencyCompletionModel` 与 E2E harness **不在同一运行链**) |
| 台账 **Entry #392** | 根因首定位(`memory_violations` 恒 `not-collected`) |
| `ADR-CEP-06` SU#4 §① | IC3 三态口径(`deadlock_violations == 0 && memory_violations == 0` ⇒ 通过) |
| 台账 **Entry #222** ④ | 「**不得写裸 0 而不声明**」⇒ 违例类的 `not-collected` 是**诚实声明**,⛔ 不是 bug |
| 台账 **Entry #228** + `docs/validation/2026-11-05-cep-b4-evidence-device.md` §0 | B4 的达成证据形态 = **装置级(模型级)真实 run** 的通过态实例(「首个真实 run 实例」;此前仅合成 fixture F1)—— ⇒ 本项要补的是**拓扑级 E2E run** 实例 |
| `ADR-CEP-19` **ASM-8 ①** | 拓扑级 run 的 `hypothesis_id` **须绑定 `H3`**(唯一双违例类均须实测的已登记行)⇒ ⛔ 绑定**不需要**改预登记 |
| `tools/render_report.py:178-183` | 预登记判据④:`hypothesis_id` **无登记行 ⇒ `--check` 判红** ⇒ 拓扑级 run 要合格**必须**绑定 |
| architect 方案 | `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(根因 + 4 步路径 + 3 条硬边界 + 2 条待核前置;⚠️ 其「合成」表述待 architect 域订正) |

## 范围

| 面 | 内容 |
|---|---|
| ✅ 在范围内 | ① **前置核对**(P-2 retention 丙口径 / P-3 「全绿 run」完整判据范围)+ **前提订正落盘** ② **挂载点 / 内存样本路径 / 判据绑定**定位 ③ **场景扩写 + 检测器挂载 + 样本活性见证**(⇒ 违例类被实测)④ 一次拓扑级真实 run + 4 件套 + **两件套入库 + 双向对账** ⑤ IC3 通过态判定 ⑥ `07 号` 新 SU + `ADR-CEP-19` 一则 Status Update(**冻结面 2 处 ⇒ 五步链**)+ `obj §6.2` 证据行 ⑦ 新闸 + **每判据 1 条**决定性变异 |
| ⛔ 不在范围内 | ① 改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款 ② 为凑 IC3 掩盖真实违例 ③ 用合成 fixture 或模型级实例冒充拓扑级 E2E run ④ 删 `not_collected` 的登记而不接产出者 ⑤ 以「挂了但样本从未流入」的实测 0 充数 ⑥ 声称「检测力已证」 ⑦ 新建 ADR / 改 ADR 决策与正文 |

## Why

**拓扑级(E2E 链)run 不承载 IC3 通过态** —— 模型级装置(H3 v2)已有真实 run 通过态(B4),但**拓扑级**run 恒 ⚪ 不适用。

| 事实 | 实测 |
|---|---|
| 拓扑级 run 的 `memory_violations` 恒 `not-collected` | 两个已有 run 均是 ⇒ IC3 **永不可判** ⇒ ⚪ 不适用 |
| 该 run **自陈不适格** | `simulation_config.json`:`hypothesis_id = "framework-e2e"` · `not_an_experiment_run = true` · `run_class = "framework-verification"` · `evidence_scope`「⛔ 非一致性安全断言(IC3=⚪ 不适用)」 |
| `deadlock_violations` 已采集且 = 0 | ⇒ **缺口只差一个键**(且差的是**资格**,不只是数字) |
| 检测器**已存在** | `test/cep_experiment_harness.hh:135-140/169/182`(`SwmrDataValueDetector` 的 swmr / data_value 桶) |
| `not-collected` 是**装置的诚实声明** | `test/test_cep_experiment_emit.cc:345-348`「本装置**无内存一致性语义** ⇒ 不挂检测器」· `test/test_cep_stub_cache_emit.cc:146`「本 run **无内存操作样本**」 |
| 装置不在 E2E 链上 | `ConsistencyCompletionModel` 的调用方**只有** B4 证据设备与 emit 测试 |

⇒ **缺的不是能力、不是采集点、也不是接线** ⇒ 缺的是「**同时满足『挂了检测器』+『有真实内存操作样本』+『绑定已登记判据行』』的装置**」⇒ R-3 = **接线 + 场景扩写 + 取证**。

## What Changes

| # | 变更 | 落点 | 约束 |
|:--:|---|---|---|
| 1 | **场景扩写 + 判据绑定** | `test/cep_experiment_harness.hh` + `test/test_cep_stub_cache_emit.cc` + E2E 场景 / `configs/cep_*_e2e_*.json` | 违例类由 `not-collected` → **实测数值**;`hypothesis_id` 绑定 H3;⛔ 不得删登记而不接产出者;⛔ 不得零样本充数 |
| 2 | **拓扑级真实 run + 4 件套 + 入库 + 双向对账** | `experimental/run-<ts>-<tag>/` + `test/cep_e2e_runs.jsonl` | 形态以 **P-2** 结论为准;两件套入库 ⇒ 双向闸绿 |
| 3 | **IC3 通过态** | `report.html` + `cep_ic3_report_states_check.sh` + `render_report --check` | 渲染「✓ 一致性闸门通过」+ 预登记判据④ 绿 |
| 4 | **证据登记** | `07 号` 新 SU + `ADR-CEP-19` 一则 Status Update(**冻结面 2 处 ⇒ 五步链**)+ `obj §6.2` 证据行(append-only) | ⛔ 不改正文 |
| 5 | **新闸** | `scripts/test/cep_full_green_run_check.sh` | C1 / C1' / C2 / C3 / C4 / C5 + **每判据 1 条**决定性变异(共 **6 条**);ctest **175 → 176**;守门表 **36 → 37** |

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1**:P-2 / P-3 结论落盘 **+ 前提订正在 4 处文档一致** ⇒ ⛔ 未核不得进入 A2
- [ ] **AC-2**:两违例类为**实测数值**(⛔ 非 `not-collected`)且在 `measured` 内 **+ 样本活性见证 > 0**
- [ ] **AC-3**:两违例类实测 **== 0** ⇒ ⚠️ **> 0 ⇒ 停止**并按缺陷另立处置
- [ ] **AC-4**:IC3 = **通过态** + 渲染「✓ 一致性闸门通过」+ **拓扑链身份锚** ⇒ ⛔ 模型级 / fixture 冒充必红
- [ ] **AC-5**:4 件套按 P-2 结论齐备 **+ 两件套入库 + `jsonl` 条目 + 双向闸绿**
- [ ] **AC-6**:`07 号` 新 SU + `ADR-CEP-19` 一则 Status Update + **五步链** provenance 成立(冻结面 **2 处**)
- [ ] **AC-7**:`obj §6.2` B4 证据行 append-only 更新(带「拓扑级」限定词)+ `cep_docs_sync_check --strict` 绿 + 活文本同步清单逐条处置
- [ ] **AC-8**:新闸 **每判据各 1 条决定性变异**按签名红 + 还原绿
- [ ] **AC-9**:`ctest` **176/176** · 全部 `cep_*.sh` 逐个 rc=0(⛔ 不写死绝对数) · format / docs_sync / doc_ref / lint / freeze / reconcile 全 rc=0 · ⛔ 未改判据 / 阈值 / 预登记 / schema / retention · ⛔ `B3b-2` 未动 · ⛔ 未改 ADR 决策与正文

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-full-green-run-evidence` | **ADDED** | 「**一次拓扑级 E2E run** 的 IC3 通过态实例」这一**证据面** |

## Impact

| 项 | 内容 |
|---|---|
| **冻结面** | ✅ **2 处**(`07 号` 新 SU + `ADR-CEP-19` 一则 Status Update)⇒ **五步链**;⚠️ 归档后新 main spec 自动入集 ⇒ 第二次 `--update` |
| **KPI slot** | ✅ 占 **1 个**(当前 1/3)⇒ 归档后回填 0/3 |
| **风险** | ① 实测 > 0 ⇒ **停止**(硬边界)② 「拓扑级身份」若在入库后**不可机判** ⇒ ⛔ 须在 A2/A3 解决 ③ `trace.h5` 不入库 ⇒ 4 件套只能**运行期**形态(依赖 P-2)④ 取得证据资格若需改**预登记** ⇒ ⛔ 停并回报 ⑤ P-3 若要求扩围 ⇒ ⛔ 停并回报**重立项** |
| **不可约残余** | ① 一次拓扑级通过态 **≠** 全面达成 ② 措辞须限定为「一次拓扑级 E2E run 的通过态实例」 ③ **模型级 ≠ 协议级** ④ ⛔ 不得称「检测力已证」(证据强度弱于 B4) |

## 已裁项

| # | 裁决 | 落点 |
|:--:|---|---|
| 1 | **不改任何判据** —— 只补证据面 | 本草案 §范围 / §AC-9 |
| 2 | 硬边界(不掩盖违例 / 不用 fixture 或模型级实例冒充 / 不写成能力新增 / 不删登记 / 不用零样本充数 / 不称检测力已证) | architect 方案 §二 · §五 + 本草案 §范围 |
| 3 | **缺口口径 = 拓扑级**(非「真实 run 实例」)—— 订正自「合成」说反 | 本草案 §前提订正 + change proposal §前提订正 |

## 关联

台账 **#393** · **#392** · **#383** · **#264** · **#228** · **#222** · architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md` · `docs/validation/2026-11-05-cep-b4-evidence-device.md` · `ADR-CEP-19` ASM-8 ① · `openspec/changes/cpptlm-cep-full-green-run-evidence/`
