---
name: cpptlm-cep-full-green-run-evidence
priority: P1
phase: phase-10
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台 —— 「全绿 run」缺口的闭合(IC3 真实 run 通过态)
依赖:
  - ADR-CEP-06
  - ADR-CEP-05
状态: "🟡 待实施（2026-11-13 起草;per 台账 **Entry #383**(R-3 归因)+ **#392**(根因首定位)+ **#393**(P-1 前置核对 + 根因精化);architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 订正);⛔ 本 change **不改任何判据 / 阈值 / 预登记** —— 只补「真实 run 的通过态实例」这一证据面)）"
created: 2026-11-13
---

# cpptlm-cep-full-green-run-evidence: 「全绿 run」缺口的闭合 —— 让 IC3 在**真实 run** 上落在「通过」态

> **范围纪律**:⛔ **不改** IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款 · ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不新建 ADR · ⛔ 不动 `§4.0` 排期值。
> **估时**:≈1-2.5 人日(含前置核对 ≈0.5d)

## 架构依据

| 依据 | 内容 |
|---|---|
| 台账 **Entry #393** | P-1 前置核对 + 根因**精化**(⛔ 装置 `ConsistencyCompletionModel` 与 E2E harness **不在同一运行链**) |
| 台账 **Entry #392** | 根因首定位(`memory_violations` 恒 `not-collected`) |
| `ADR-CEP-06` SU#4 §① | IC3 三态口径(`deadlock_violations == 0 && memory_violations == 0` ⇒ 通过) |
| 台账 **Entry #222** ④ | 「**不得写裸 0 而不声明**」⇒ 违例类的 `not-collected` 是**诚实声明**,⛔ 不是 bug |
| 台账 **Entry #228** | B4 的达成证据形态(装置 + **合成** fixture 的通过态实例)—— 本项要补的是**真实 run** 实例 |
| architect 方案 | `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(根因 + 4 步路径 + 3 条硬边界 + 2 条待核前置) |

## 范围

| 面 | 内容 |
|---|---|
| ✅ 在范围内 | ① **前置核对**(P-2 retention 登记形态 / P-3 「全绿 run」完整判据范围)② **挂载点与内存样本路径定位** ③ **场景扩写 + 检测器挂载**(⇒ 违例类被实测)④ 一次真实 run + 4 件套 ⑤ IC3 通过态判定 ⑥ `07 号` 新 SU(**冻结件 ⇒ 五步链**)+ `obj §6.2` 证据行 ⑦ 新闸 + 决定性变异 |
| ⛔ 不在范围内 | ① 改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款 ② 为凑 IC3 掩盖真实违例 ③ 用合成 fixture 冒充真实 run ④ 删 `not_collected` 的登记而不接产出者 ⑤ 新建 ADR |

## Why

**一致性闸门(IC3)至今没有一个「真实 run」的通过态实例** —— 在所有真实 E2E run 上恒 ⚪ 不适用。

| 事实 | 实测 |
|---|---|
| `memory_violations` 恒 `not-collected` | 两个已有 run 均是 ⇒ IC3 **永不可判** |
| `deadlock_violations` 已采集且 = 0 | ⇒ **缺口只差一个键** |
| 检测器**已存在** | `test/cep_experiment_harness.hh:135-140/169/182`(`SwmrDataValueDetector` 的 swmr / data_value 桶) |
| `not-collected` 是**装置的诚实声明** | `test/test_cep_experiment_emit.cc:345-348`「本装置**无内存一致性语义** ⇒ 不挂检测器」· `test/test_cep_stub_cache_emit.cc:146`「本 run **无内存操作样本**」 |
| 装置不在 E2E 链上 | `ConsistencyCompletionModel` 的调用方**只有** B4 证据设备与 emit 测试 |

⇒ **缺的不是能力、不是采集点、也不是接线** ⇒ 缺的是「**同时满足『挂了检测器』+『有真实内存操作样本』』的装置**」⇒ R-3 = **接线 + 场景扩写 + 取证**。

## What Changes

| # | 变更 | 落点 | 约束 |
|:--:|---|---|---|
| 1 | **场景扩写 + 检测器挂载** | `test/cep_experiment_harness.hh` + E2E 场景 / `configs/cep_*_e2e_*.json` | 违例类由 `not-collected` → **实测数值**;⛔ 不得删登记而不接产出者 |
| 2 | **真实 run + 4 件套** | `experimental/<run>/` | 形态以 **P-2** 结论为准 |
| 3 | **IC3 通过态** | `report.html` + `cep_ic3_report_states_check.sh` | 真实 run 渲染「✓ 一致性闸门通过」 |
| 4 | **证据登记** | `07 号` 新 SU(冻结件 ⇒ 五步链)+ `obj §6.2` 证据行(append-only) | ⛔ 不改正文 |
| 5 | **新闸** | `scripts/test/cep_full_green_run_check.sh` | G1~G5 + ≥3 条决定性变异;ctest **175 → 176**;守门表 **36 → 37** |

## Acceptance

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1**:两条前置(P-2 / P-3)逐条结论落盘 ⇒ ⛔ 未核不得进入 A2
- [ ] **AC-2**:两违例类为**实测数值**(⛔ 非 `not-collected`)且在 `measured` 内
- [ ] **AC-3**:两违例类实测 **== 0** ⇒ ⚠️ **> 0 ⇒ 停止**并按缺陷另立处置
- [ ] **AC-4**:IC3 = **通过态** + `report.html` 渲染「✓ 一致性闸门通过」;⛔ 反向 fixture 证明「合成 fixture 冒充真实 run」会红
- [ ] **AC-5**:4 件套按 P-2 结论齐备且登记形态合法
- [ ] **AC-6**:`07 号` 新 SU + **五步链** provenance 成立
- [ ] **AC-7**:`obj §6.2` B4 证据行 append-only 更新 + `cep_docs_sync_check --strict` 绿
- [ ] **AC-8**:新闸 **≥3 条决定性变异**按签名红 + 还原绿
- [ ] **AC-9**:`ctest` **176/176** · `cep_*.sh` **42/42** · format / docs_sync / doc_ref / lint / freeze 全 rc=0 · ⛔ 未改判据 / 阈值 / 预登记 / schema / retention · ⛔ `B3b-2` 未动 · ⛔ 无 ADR 变更

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-full-green-run-evidence` | **ADDED** | 「**一次真实 run** 的 IC3 通过态实例」这一**证据面** |

## Impact

| 项 | 内容 |
|---|---|
| **冻结面** | ✅ **1 处**(`07 号` 新 SU)⇒ **五步链** |
| **KPI slot** | ✅ 占 **1 个**(当前 0/3)⇒ 归档后回填 0/3 |
| **风险** | ① 实测 > 0 ⇒ **停止**(硬边界)② 「真实 run」与「合成 fixture」若文件形态不可区分 ⇒ G4 不可实现 ⇒ ⛔ 须在 A2/A3 解决 ③ `trace.h5` 不入库 ⇒ 4 件套只能 run 级形态(依赖 P-2) |
| **不可约残余** | ① 一次通过态 **≠** 全面达成 ② 措辞须限定为「一次真实 run 的通过态实例」 |

## 已裁项

| # | 裁决 | 落点 |
|:--:|---|---|
| 1 | **不改任何判据** —— 只补证据面 | 本草案 §范围 / §AC-9 |
| 2 | 硬边界 3 条(不掩盖违例 / 不用 fixture 冒充 / 不写成能力新增) | architect 方案 §二 · §五 |

## 关联

台账 **#393** · **#392** · **#383** · **#264** · **#228** · **#222** · architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md` · `openspec/changes/cpptlm-cep-full-green-run-evidence/`
