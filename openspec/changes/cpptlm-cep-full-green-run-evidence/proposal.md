# cpptlm-cep-full-green-run-evidence: 「全绿 run」缺口的闭合 —— 让 IC3 在**真实 run** 上落在「通过」态

> **状态**: 🟡 **Proposed**(2026-11-13 立项 —— per 用户口令「给 R-3 任务创建 openspec change」;KPI 并发 **0/3 → 1/3**)
> **立项依据**: 台账 **Entry #383**(R-3 归因)· **#392**(根因首定位)· **#393**(P-1 前置核对 + 根因精化)· architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 订正)· improvement `.rddf/improvements/cpptlm-cep-full-green-run-evidence.md`
> **上游裁决**: IC3 三态口径(per `07 号` §4 + `ADR-CEP-06` SU#4 §①)· B4 达成证据形态(per **Entry #228**)· 「不得写裸 0 而不声明」(per **Entry #222** ④)
> **⛔ 范围纪律**: 本 change **只做「真实 run 的 IC3 通过态证据」**;⛔ 不改 IC3 判据 / 不改阈值 / 不改预登记 / 不改 B4 口径 · ⛔ 不新建 ADR · ⛔ 不改 `B3b-2`
> **估时**: ≈1-2.5 人日(含前置核对 ≈0.5d)

## Why

**一致性闸门(IC3)至今没有一个「真实 run」的通过态实例** —— 它在所有真实 E2E run 上都是 ⚪ **不适用**,因此 B4 的证据链缺最后一环。

| 缺口 | 实测 | 后果 |
|---|---|---|
| **`memory_violations` 恒 `not-collected`** | 两个已有 run(`experimental/run-2026-11-09-cep-e2e{,-b}/metrics.json`):`deadlock_violations` = **0**(已采集)· `memory_violations` = **`"not-collected"`** | IC3 判据 = `deadlock_violations == 0 && memory_violations == 0` ⇒ **永不可判** ⇒ ⚪ 恒不适用 |
| **根因(精确)** | `memory_violations` 的 `not-collected` 是**装置的诚实声明** —— `test/test_cep_experiment_emit.cc:345-348`「本装置**无内存一致性语义** ⇒ 不挂检测器 ⇒ 登记为未采集(per Entry #222 ④)」· `test/test_cep_stub_cache_emit.cc:146`「本 run **无内存操作样本**」 | ⛔ **不是采集点缺失**(检测器 `SwmrDataValueDetector` 在 `test/cep_experiment_harness.hh:135-140/169/182` **已存在**) |
| **缺的是「装置」** | ⛔ **没有装置同时满足「挂了检测器」+「有真实内存操作样本」** | ⇒ R-3 = **接线 + 场景扩写 + 取证** —— ⛔ 不是「纯接线」 |
| **装置不在 E2E 链上** | `ConsistencyCompletionModel` 的调用方**只有** `test_cep_b4_evidence_device.cc` 与 `test_cep_experiment_emit.cc` | ⇒ 合成通过态实例已有(B4 当时),**真实 run 实例缺** |

**为什么是现在**:B4 已达成但其「通过态」是**合成**的;R-3 是把「纸面已达成」变成「**真实 run 已达成**」的唯一路径,且**不依赖任何外部条件**(与 `B3b-2` 的外部阻塞相反 ⇒ 可单方面推进)。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **场景扩写**:让真实 E2E 场景**产生内存操作样本** + **挂上违例检测器** ⇒ `memory_violations` 由 `not-collected` 变为**实测值** | `test/cep_experiment_harness.hh`(挂载点)+ E2E 场景与 `configs/cep_*_e2e_*.json` | ⚠️ **具体挂载点须在 A2 定位后确定**(⛔ 不许预设);⚠️ **实测 > 0 ⇒ 停止**(见 Out of Scope) |
| 2 | **IC3 三态判定** | 该 run 的 `report.html` + `cep_ic3_report_states_check.sh` | 真实 run 渲染「**✓ 一致性闸门通过**」 |
| 3 | **证据登记** | `docs/architecture/07-coherence-exploration-platform.md` **新 SU**(冻结件 ⇒ **五步链**) | ⛔ **不改正文**;登记形态须先核 **P-2**(retention) |
| 4 | **DoD 读法** | `.rddf/roadmap/objectives/obj-cep-platform.md` §6.2 B4 证据行(append-only) | 追加「IC3 在**真实 run** 上达成通过态」 |
| 5 | **新闸** | `scripts/test/cep_full_green_run_check.sh` + `test/CMakeLists.txt` + `scripts/README.md` | 判据:真实 run 存在 + 两违例类**实测为 0** + IC3 = 通过态 + **⛔ 非合成 fixture**;配决定性变异;ctest **175 → 176**;守门表 **36 → 37** |

**Out of Scope(⛔ 硬边界)**

- ⛔ **不得为凑 IC3 掩盖真实违例** —— 若实测 `memory_violations > 0` ⇒ **立即停止**,按**缺陷**另立处置(⛔ 不得改判据 / 阈值 / 预登记来「让它变绿」)
- ⛔ **不得用合成 fixture 冒充真实 run**(B4 当时的通过态实例是合成的;本项要的正是真实 run)
- ⛔ **不得把「接线 + 场景扩写」写成「能力新增」**(检测器已存在)
- ⛔ 不改 IC3 判据 / 不改 `metrics.schema.json` / 不改预登记 / 不改 B4 口径 · ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不新建 ADR · ⛔ 不动 `§4.0` 排期值

## 验收标准(Acceptance Criteria)

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（两条前置已核）**:**P-2**(ADR-CEP-06 retention 是否允许「4 件套齐备但 `trace.h5` 不入库」的登记形态)与 **P-3**(「全绿 run」的完整判据范围)逐条给出结论并落盘 ⇒ ⛔ 未核不得进入 A2
- [ ] **AC-2（违例类被实测）**:目标 run 的 `metrics.json` 中 `memory_violations` 为**实测数值**(⛔ **不是** `"not-collected"`),且 `deadlock_violations` 为**实测数值** ⇒ 两键都在 `measured` 内
- [ ] **AC-3（实测为 0）**:两违例类实测值 **== 0** ⇒ ⚠️ **若 > 0 ⇒ 停止并按缺陷另立处置**(⛔ 不得改判据/阈值/预登记)
- [ ] **AC-4（IC3 通过态 · 真实 run）**:该 run 的 `report.html` 渲染「**✓ 一致性闸门通过**」⇒ `cep_ic3_report_states_check.sh` 绿;⛔ **反向 fixture 必须证明「合成 fixture 冒充真实 run」会被新闸判红**
- [ ] **AC-5（4 件套齐备 + 登记形态合法）**:4 件套按 **P-2 的结论**齐备且登记形态合法(⚠️ `trace.h5` 的 retention 口径逐字遵守)⇒ `cep_experiment_run.sh` 的 4 件套校验绿
- [ ] **AC-6（证据登记 + 五步链）**:`07 号` **新 SU** 落盘(⛔ 不改正文)· 走五步链(Entry 先落 → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验)· 冻结清单 provenance 成立
- [ ] **AC-7（DoD 读法更新）**:`obj §6.2` B4 证据行**追加**(append-only)「IC3 在**真实 run** 上达成通过态」+ 指向新 SU 与 run 目录 ⇒ `cep_docs_sync_check --strict` 绿
- [ ] **AC-8（新闸有牙）**:新闸判据齐备 + **≥3 条决定性变异**(破坏 `memory_violations` 的实测性 ⇒ 必红;把 run 换成合成 fixture ⇒ 必红;移除 `07 号` 的新 SU ⇒ 必红)⇒ 全部**按签名红** + 还原 ⇒ 绿
- [ ] **AC-9（0 回归 + 范围纪律）**:`ctest` **176/176** · `cep_*.sh` **42/42** rc=0 · format / docs_sync / doc_ref / lint / freeze 全 rc=0 · ⛔ 未改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` · ⛔ `B3b-2` 未动 · ⛔ 无 ADR 变更

## 成本收益

**消「IC3 在所有真实 run 上恒 ⚪ 不适用」vs 1 处场景扩写 + 1 处检测器挂载 + 1 个 run 产物 + 1 则 `07 号` SU + 1 个新闸**。

收益侧是 **B4 证据链的最后一环**:「纸面已达成」⇒「**真实 run 已达成**」,且**不依赖任何外部条件**。

## 诚实边界(⛔ 不可约残余)

1. 本项**只覆盖「一次真实 run 的通过态实例」** ⇒ ⛔ **不等于**「所有协议族/所有拓扑都达成」;
2. `trace.h5` 的 retention 口径若不允许入库 ⇒ **4 件套的「齐备」只能以 run 级形态成立** ⇒ 该形态的合规性取决于 **P-2** 的结论;
3. 一次通过态**不构成统计意义上的「全绿」** ⇒ ⛔ 措辞必须限定为「**一次真实 run 的通过态实例**」;
4. 本项**不改**任何判据 ⇒ 若未来发现违例类语义有缺口,仍须另立 change。

## 关联

台账 **Entry #393**(P-1 核对 + 根因精化)· **#392**(根因首定位)· **#383**(R-3 归因)· **#264** · **#228**(B4 达成 + 证据设备)· **#222**(④ 不得写裸 0)· architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md` · `07 号` SU · 十五 ① · `docs/guide/cep-usage.md` §12.8
