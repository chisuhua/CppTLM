# architect 方案 —— R-3「全绿 run」缺口的根因定位与闭合路径

> **主题**:per 台账 **Entry #383** / **#391** 的 R-3(`docs/guide/cep-usage.md` §12.8 表外第 1 项 + `07 号` SU · 十五 ①)——「全绿 run」缺口。
> **状态**:✅ **根因已定位** + **闭合路径已定**;⛔ **本轮不实施**(需独立 change + 冻结面五步链)⇒ 登记为**下一批**。
> **上游**:**Entry #332 ⑧**(口径 ④ 预登记)· **#264**(B4 达成证据形态)· **#228**(B4 的证据设备落地)· **#391**(R-3 归因)

---

## 一 · 根因(实测,非文本推断)

### 1.1 判据侧

IC3 = `deadlock_violations == 0 && memory_violations == 0`(per `07 号` §4 的 IC3 口径 + `scripts/test/cep_ic3_report_states_check.sh`)。
⇒ **两个键都必须实测为 0**,任一为 `not-collected` ⇒ IC3 = ⚪ **不适用**(永不可判)。

### 1.2 数据侧(实测两个已有 run)

| run | `deadlock_violations` | `memory_violations` | IC3 |
|---|---|---|---|
| `experimental/run-2026-11-09-cep-e2e`(拓扑 A) | **0** ✅ 已采集 | **`"not-collected"`** ⛔ | ⚪ 不适用 |
| `experimental/run-2026-11-09-cep-e2e-b` | **0** ✅ 已采集 | **`"not-collected"`** ⛔ | ⚪ 不适用 |

⇒ **缺口只差一个键**:`memory_violations`。其余 13 必纳键里 `coherence_stalls` 已实测(260 / 86),`collective_stalls` / `directory_misses` 为 `not-collected` 但属**非违例类**(per B4 口径:违例类必须**实测为 0**,非违例类可为 `not-collected` 且 ⊆ 预登记)。

### 1.3 采集面(已存在,不是「缺功能」)

| 事实 | 落点 |
|---|---|
| 采集点 | `test/cep_experiment_harness.hh:430` — `metric_json("memory_violations", violations.total())` |
| 「未采集」的显式声明机制 | `test/test_cep_experiment_emit.cc:148` — 本 run 未采集 ⇒ 须显式声明(⇒ 现有 run 的 `not-collected` 是**诚实声明**,不是 bug) |
| **装置**(使单个 run 同时实测两类违例类) | `test/cep_consistency_completion_model.hh` — B4 的证据设备(per **Entry #228**) |
| 反向 fixture(IC3 三态渲染) | `scripts/test/cep_ic3_report_states_check.sh` |

⇒ **结论**:R-3 **不是「缺能力」,而是「装置已有、真实 run 未跑」** —— 与 B4 当时的形态**同型**(装置 + 合成 fixture 的通过态实例已有,**真实 run 实例缺**)。

---

## 二 · 闭合路径(4 步)

| 步 | 内容 | 产物 | 门 |
|:--:|---|---|---|
| **1** | **装置接入真实 E2E**:用 `cep_consistency_completion_model.hh` 的装置跑拓扑 **B**(`configs/cep_chi_rnf_e2e_b.json`,**带 bridge** ⇒ 有违例类样本面) | 一个 run 目录(4 件套) | `memory_violations` **实测为 0** |
| **2** | **IC3 三态判定**:该 run 的 `report.html` 渲染「**✓ 一致性闸门通过**」 | 判据更新 | `cep_ic3_report_states_check.sh` 绿 |
| **3** | **证据登记**:按 ADR-CEP-06 的 retention 口径登记(⚠️ `trace.h5` **不入库** ⇒ 登记形态须先核) | 实施笔记 + `07 号` 新 SU | 冻结件 ⇒ **五步链** |
| **4** | **DoD 读法**:把「IC3 在真实 run 上达成通过态」写进 `obj §6.2` B4 的证据行(append-only) | 台账 Entry | `cep_docs_sync_check` 绿 |

### ⚠️ 三条硬边界(不可让步)

1. **⛔ 不得为凑 IC3 而掩盖真实违例** —— 若第 1 步实测 `memory_violations > 0` ⇒ **立即停止**,按**缺陷**另立处置(不得改判据、不得改阈值、不得改预登记来「让它变绿」);
2. **⛔ 不得用合成 fixture 冒充真实 run** —— B4 当时的通过态实例是**合成**的,本项要求的正是**真实 run**;
3. **⛔ 不得把「装置接入」写成「能力新增」** —— 装置、采集点、三态 fixture **均已存在**;本项是**接线 + 取证**,⛔ 不是新功能。

### ⚠️ 未实测的前置(实施期必须先核,⛔ 不许假设)

| # | 前置 | 若不成立的后果 |
|:--:|---|---|
| P-1 | 拓扑 B 上装置能否**真的采到** `memory_violations` 样本(而非仍然 `not-collected`) | 步骤 1 直接失败 ⇒ 缺口另有根因 |
| P-2 | ADR-CEP-06 的 retention 口径是否允许「4 件套齐备但 `trace.h5` 不入库」的**登记形态** | 步骤 3 无合法登记形态 ⇒ 需先裁决口径 |
| P-3 | 「全绿 run」是否还要求**其它**条件(如 H2/H3/H6 假设全覆盖) | 步骤 2 的判据范围会被低估 |

---

## 三 · 规模与排期

| 项 | 估时 | 说明 |
|---|---|---|
| 实施(4 步) | ≈1-2 人日 | 主体是**接线 + 真跑 + 取证**,⛔ 不是新功能 |
| 冻结面 | **1 处**(`07 号` 新 SU)⇒ **五步链** | 另涉 `obj §6.2` 证据行(append-only) |
| KPI slot | ✅ 占 1 个(当前 **0/3**) | |
| 前置核对 | ≈0.5 人日 | P-1 / P-2 / P-3 三条**必须先核** |

⚠️ **本轮不实施的理由**:① 需独立 change + 冻结面五步链 + 归档 + 独立验收(四角色链);② **P-1/P-2/P-3 三条前置尚未实测**,⛔ 先立项再发现前置不成立 = 白占一个 slot;③ 本仓纪律要求「⛔ 半成品」——宁可登记清楚,不做半截。

---

## 四 · 已登记的旁证(可复核)

- `experimental/run-2026-11-09-cep-e2e{,-b}/metrics.json` —— 两 run 的 `deadlock_violations` = 0、`memory_violations` = `not-collected`;
- `test/cep_experiment_harness.hh:430` —— 采集点;
- `test/cep_consistency_completion_model.hh` —— 装置(B4 证据设备,per Entry #228);
- `scripts/test/cep_ic3_report_states_check.sh` —— IC3 三态 fixture。

**关联**:台账 **#391**(R-3 归因)· **#383** · **#332 ⑧** · **#264** · **#228**(B4 达成)· **#250** · `docs/guide/cep-usage.md` §12.8 · `07 号` SU · 十五 ①

---

## 五 · ⚠️ 订正(2026-11-13 · P-1 前置核对后 · append-only,⛔ 原文不改)

**§一 与 §二 的两处表述需精化**:

| # | 原表述 | 订正 |
|:--:|---|---|
| ① | 「**不是缺能力**……采集点与装置均已存在 ⇒ R-3 = **接线 + 真实 run 取证**」 | ⇒ **能力确实已存在**(违例检测器 `SwmrDataValueDetector` 在 harness 内:`ViolationCounts.swmr` / `.data_value` 桶 + :169 / :182 的递增点);但 ⛔ **装置 `ConsistencyCompletionModel` 与 E2E harness 不在同一条运行链**(装置的调用方只有 `test_cep_b4_evidence_device.cc` 与 `test_cep_experiment_emit.cc`)。⇒ R-3 = **在真实 E2E 场景里挂上检测器 + 提供内存操作样本 + 取证** ⇒ 规模由「纯接线」上调为「**接线 + 场景扩写**」 |
| ② | 「`memory_violations` 未采集 ⇒ 缺口只差一个键」 | ⇒ **精确机制**:该键的 `not-collected` 是**具体实验装置的诚实声明** —— `test/test_cep_experiment_emit.cc:345-348`「本装置**无内存一致性语义** ⇒ 不挂检测器 ⇒ 登记为未采集(per Entry #222 ④:不得写裸 0 而不声明)」· `test/test_cep_stub_cache_emit.cc:146`「本 run **无内存操作样本** ⇒ 无可检样本」。⇒ **不是采集点缺失**,而是「**没有装置同时满足『挂了检测器』+『有内存操作样本』**」 |

⇒ **更新后的闭合路径第 1 步**:⛔ 不是「用装置跑拓扑 B」(措辞不准确)⇒ 而是「**在真实 E2E 场景中挂 `SwmrDataValueDetector` + 产生真实内存操作样本 ⇒ 该 run 的 `memory_violations` 从 `not-collected` 变为实测值**」⇒ **P-1 的答案 = 当前 E2E 场景均不满足 ⇒ 需扩写场景**(规模 ≈1-2 人日,比原估 +0.5 人日)。
⇒ ⚠️ 三条硬边界**不变**(尤其「⛔ 不得为凑 IC3 掩盖真实违例」)。

---

## 六 · ⚠️ 订正二(2026-11-13 · 审查后 · append-only,⛔ 原文不改)

> **性质**:纯**追加**订正段。⛔ **不改** §一 ~ §五 **任何**既有文字(尤其 §一.3 的「结论」行与 §二 硬边界 2)· ⛔ **不改** §五 订正的 ①② 两处(那两处仍成立)⇒ **本段只订正「合成 / 真实 run」这组表述**。

**§一.3 与 §二 硬边界 2 的「合成」表述与已签记录相反**:

| # | 原表述 | 订正 |
|:--:|---|---|
| ① | §一.3 结论行:「与 B4 当时的形态**同型**(装置 + **合成 fixture** 的通过态实例已有,**真实 run 实例缺**)」 | ⛔ **说反了**。`obj-cep-platform.md` **§6.2 B4**(Entry #228 ③)明写 B4 的达成证据 (iii) 是「`render_report --check` 绿 且 `report.html` 渲染『✓ 一致性闸门通过』—— IC3 三态中该态**首个真实 run 实例**(此前仅合成 fixture **F1**)」;`docs/validation/2026-11-05-cep-b4-evidence-device.md` §0 同(「**该态的首个真实实例**(此前仅由合成 fixture F1 覆盖)」)⇒ 实测 `experimental/run-20261105-153418-h2h3h4h6/h3/`:`deadlock_violations = 0` + `memory_violations = 0` **均在 `_measured`** · `simulation_config.json`:`hypothesis_id = "h3"`(预登记 registered 行)· `report.html` 渲染「一致性闸门通过」⇒ ⇒ **B4 的通过态实例 = 真实 run(装置级 / 模型级)**;**合成**的是更早的 fixture **F1** ⇒ ⇒ **R-3 的缺口 = 「拓扑级(E2E 链)run 不承载 IC3 通过态」**,⛔ **不是**「真实 run 实例缺」 |
| ② | §二 硬边界 2:「**不得用合成 fixture 冒充真实 run** —— B4 当时的通过态实例是**合成**的,本项要求的正是**真实 run**」 | ⛔ 同 ① ⇒ 改为:「**不得用合成 fixture 或模型级装置实例冒充拓扑级 E2E run**」—— B4 实例是**真实 run 但属模型级**(装置 `ConsistencyCompletionModel`);合成的是更早的 fixture F1 ⇒ 边界**收紧**(多挡一层「模型级冒充拓扑级」) |

⇒ **闭合路径第 1 步的「动作」不变**(仍为「在真实 E2E 场景中挂 `SwmrDataValueDetector` + 产生真实内存操作样本 ⇒ 该 run 的 `memory_violations` 从 `not-collected` 变为实测值」),但**目标档位**须写明 = **拓扑级**(per `ADR-CEP-19` **ASM-8 ①**:拓扑级 run 的 `hypothesis_id` **须绑定 `H3`** —— 唯一双违例类均须实测的已登记行)⇒ ⛔ 绑定 H3 **不需要**改 `experimental/preregistration.json`(H3 的 `out_of_scope` = {`coherence_stalls`, `collective_stalls`, `directory_misses`},其余 10 键必须实测)。
⇒ **硬边界 1 / 3 不变**;⚠️ 本次审查另补两条实施期硬约束(**不属于**本段订正范围,落 change 的 design §D-3 / §D-7):**不得以「检测器已挂载但样本从未流入」的实测 0 充数**(须有样本计数见证)· **不得称「检测力已证」**(本项不含负控 ⇒ 证据强度弱于 B4 的三负控 + 耦合探针)。
⇒ **落地**:change `openspec/changes/cpptlm-cep-full-green-run-evidence/` 的 **proposal §前提订正**(5 条)· **design §D-1 / §D-2 A2.3 / §D-5 C4 / §D-7** · **tasks A0.5 / A2.3 / A2.5 / A4.4 / A8.2** · **spec delta 的 Purpose + Scenario「目标 run SHALL 具备证据资格」/「证据 SHALL 来自拓扑级 E2E run」**。

**关联**:台账 **Entry #394**(立项 + 五步链授权号)· **#393**(P-1 核对)· **#392**(根因首定位)· **#228**(B4 达成)· `obj §6.2 B4` · `docs/validation/2026-11-05-cep-b4-evidence-device.md` · `ADR-CEP-19` **ASM-8 ①** · `tools/render_report.py` 预登记判据④ · change `cpptlm-cep-full-green-run-evidence` proposal §前提订正

