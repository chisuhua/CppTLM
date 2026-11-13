# cpptlm-cep-full-green-run-evidence: 「拓扑级全绿 run」缺口的闭合 —— 让 IC3 在**拓扑级 E2E run** 上落在「通过」态

> **状态**: 🟡 **Proposed**(2026-11-13 立项 —— per 用户口令「给 R-3 任务创建 openspec change」;KPI 并发 **0/3 → 1/3**)
> ⚠️ **2026-11-13 审查后修订**(critic round 1 + builder 独立核对):本 change 的**前提陈述**与**判据边界**已订正 —— 缺口口径由「真实 run 的通过态实例」改为「**拓扑级(E2E 链)run** 的通过态实例」;AC 与新闸判据据此收紧。逐条见 §前提订正。
> **立项依据**: 台账 **Entry #383**(R-3 归因)· **#392**(根因首定位)· **#393**(P-1 前置核对 + 根因精化)· architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 订正;⚠️ 其 §一.3 / 硬边界 2 的「合成」表述**说反了**,见 §前提订正)· improvement `.rddf/improvements/cpptlm-cep-full-green-run-evidence.md`
> **上游裁决**: IC3 三态口径(per `07 号` §4 + `ADR-CEP-06` SU#4 §①)· B4 达成证据形态(per **Entry #228** + `docs/validation/2026-11-05-cep-b4-evidence-device.md`)· 「不得写裸 0 而不声明」(per **Entry #222** ④)· 拓扑级 run 的 `hypothesis_id` 绑定(per `ADR-CEP-19` **ASM-8 ①** + `tools/render_report.py` 预登记判据④)
> **⛔ 范围纪律**: 本 change **只做「拓扑级真实 run 的 IC3 通过态证据」**;⛔ 不改 IC3 判据 / 不改阈值 / 不改预登记(`experimental/preregistration.json` **零 diff**)· ⛔ 不新建 ADR · ⛔ 不改 `B3b-2`
> **估时**: ≈1.5-3 人日(含前置核对 ≈0.5d;⚠️ 若 A1/P-3 判定需扩围 ⇒ **停并回报重立项**,不扩本 change)

## 前提订正(2026-11-13 · 审查后 · append-only)

| # | 原表述 | 订正 |
|:--:|---|---|
| ① | Why:「**一致性闸门(IC3)至今没有一个「真实 run」的通过态实例**」⇒「B4 的证据链**缺最后一环**」 | ⛔ **不成立**。已签记录方向相反:`obj-cep-platform.md` **§6.2 B4**(Entry #228 ③)与 `docs/validation/2026-11-05-cep-b4-evidence-device.md` §0 均明写 B4 的 **h3 v2 run** 是「IC3 三态中该态**首个真实 run 实例**(此前仅合成 fixture **F1**)」⇒ 实测 `experimental/run-20261105-153418-h2h3h4h6/h3/`:`memory_violations = 0` + `deadlock_violations = 0` **均在 `_measured`** · `hypothesis_id = "h3"`(预登记 registered 行)· `report.html` 渲染「一致性闸门通过」⇒ ⇒ **本 change 的缺口是「拓扑级(E2E 链)」缺口,不是「真实 run 实例」缺口** |
| ② | Out of Scope ②:「B4 当时的通过态实例是**合成的**」 | ⛔ **说反了**。B4 的实例是**真实 run**(装置级 / 模型级);**合成**的是更早的 fixture F1(`cep_ic3_report_states_check.sh` 的三态 fixture)⇒ 硬边界改为「⛔ 不得用**合成 fixture 或模型级装置实例**冒充**拓扑级 E2E run**」 |
| ③ | 新闸 G4:「⛔ 非合成 fixture —— run 目录须是真实 E2E run(可机判:含真实 `simulation_config.json` 的拓扑标识 + 非 fixture 标记)」 | ⛔ **判据方向错**:模型级 h3 run **也不是** fixture ⇒ 该判据分不开两者 ⇒ 改为**拓扑链身份锚**(C4,见 design §D-5):run 须在 `test/cep_e2e_runs.jsonl` 有条目且 `config_file` ∈ `configs/cep_chi_rnf_e2e_*.json` ⇒ 模型级 h3 run(未入库 · jsonl 无条目)**自动落选** |
| ④ | 冻结面 = **1 处**(`07 号` 新 SU) | ⇒ **3 处**:`07 号` 新 SU + `ADR-CEP-19` 一则 Status Update(取代其 SU ② 的「⛔ 拓扑级未闭合『全绿 run』」状态陈述)+ `ADR-CEP-06` 一则 Status Update(订正其 SU 2026-11-09 §② 的「框架验证 run 证据边界」五句 —— 本 change 翻转 `run_class` / `not_an_experiment_run` / `evidence_scope` / `hypothesis_id` 后该五句与事实不符)⇒ 另:归档后新增 main spec 会被 `openspec/specs/cep-*/spec.md` glob **自动入冻结集** ⇒ 归档时需**第二次** `--update` |
| ⑤ | AC-9:「⛔ **无 ADR 变更**」 | ⇒ 改为「⛔ 不改任何 ADR **决策 / 正文**;⛔ 仅**追加** `ADR-CEP-19` 一则 Status Update」 |

✅ **architect 域订正已落**(2026-11-13 · per 用户裁决):`.rddf/reviews/cep-full-green-run-gap-rootcause.md` **§六 订正二**(纯追加,+19 行 / 0 删除;⛔ §一 ~ §五 逐字未动)已订正其 §一.3 结论行与 §二 硬边界 2 的「合成」表述 ⇒ **本 change 与 architect 方案的口径现已一致**,无需再等裁决。

## Why

**拓扑级(E2E 链)run 不承载 IC3 通过态** —— 模型级装置(H3 v2)已有真实 run 通过态(B4),但**拓扑级 E2E 链**上的 run 恒为 ⚪ **不适用** ⇒ 拓扑级「全绿 run」缺口未闭合。

| 缺口 | 实测 | 后果 |
|---|---|---|
| **拓扑级 run 的 `memory_violations` 恒 `not-collected`** | 两个已有 E2E run(`experimental/run-2026-11-09-cep-e2e{,-b}/metrics.json`):`deadlock_violations` = **0**(已采集)· `memory_violations` = **`"not-collected"`** | IC3 判据 = `deadlock_violations == 0 && memory_violations == 0` ⇒ **永不可判** ⇒ ⚪ 恒不适用 |
| **该 run 自陈不适格** | 两个 run 的 `simulation_config.json`:`"hypothesis_id": "framework-e2e"` · `"not_an_experiment_run": true` · `"run_class": "framework-verification"` · `"evidence_scope": "…⛔ 非 B4 证据;⛔ 非一致性安全断言(IC3=⚪ 不适用)"` | ⛔ 该 run **不得**作一致性安全断言证据(per `test/test_cep_stub_cache_emit.cc:10-15` 的装置自陈)⇒ **要成为合格证据须改判据绑定**(见下),不是「补个数字」 |
| **根因(精确)** | `memory_violations` 的 `not-collected` 是**装置的诚实声明** —— `test/test_cep_experiment_emit.cc:345-348`「本装置**无内存一致性语义** ⇒ 不挂检测器 ⇒ 登记为未采集(per Entry #222 ④)」· `test/test_cep_stub_cache_emit.cc:146`「本 run **无内存操作样本**」 | ⛔ **不是采集点缺失**(检测器 `SwmrDataValueDetector` 在 `test/cep_experiment_harness.hh` **已存在**) |
| **缺的是「装置」** | ⛔ **没有装置同时满足「挂了检测器」+「有真实内存操作样本」** | ⇒ R-3 = **接线 + 场景扩写 + 取证** —— ⛔ 不是「纯接线」 |
| **装置不在 E2E 链上** | `ConsistencyCompletionModel` 的调用方**只有** `test_cep_b4_evidence_device.cc` 与 `test_cep_experiment_emit.cc` | ⇒ **模型级**通过态实例已有(B4)· **拓扑级**实例缺 |

**为什么是现在**:B4 已达成,但其通过态实例是**模型级**的;R-3 是把「模型级」补到「**拓扑级**」的唯一路径,且**不依赖任何外部条件**(与 `B3b-2` 的外部阻塞相反 ⇒ 可单方面推进)。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **场景扩写 + 判据绑定**:让拓扑级 E2E 场景**产生内存操作样本** + **挂上违例检测器** + **把 `hypothesis_id` 绑定到已登记行** ⇒ `memory_violations` 由 `not-collected` 变为**实测值**,且该 run 由「装置自陈不适格」转为「合格证据」 | `test/cep_experiment_harness.hh`(挂载点)+ `test/test_cep_stub_cache_emit.cc`(emit 侧自陈字段)+ E2E 场景与 `configs/cep_chi_rnf_e2e_*.json` | ⚠️ **具体挂载点须在 A2 定位后确定**(⛔ 不许预设);⚠️ **实测 > 0 ⇒ 停止**(见 Out of Scope) |
| 2 | **IC3 三态判定** | 该 run 的 `report.html` + `cep_ic3_report_states_check.sh` + `tools/render_report.py --check` | **拓扑级**真实 run 渲染「**✓ 一致性闸门通过**」⇒ 且 `--check` 的预登记判据④ 绿(⇒ `hypothesis_id` 必须是**已登记**行) |
| 3 | **证据登记 + 可回溯** | `docs/architecture/07-coherence-exploration-platform.md` **新 SU**(冻结件 ⇒ **五步链**)· `docs/adr/ADR-CEP-19-*.md` **一则 Status Update**(取代其 SU ② 的「不承载全绿 run」状态陈述)· `docs/adr/ADR-CEP-06-*.md` **一则 Status Update**(订正其 SU 2026-11-09 §② 的「框架验证 run 证据边界」五句)· 目标 run 的**两件套入库** + `test/cep_e2e_runs.jsonl` 条目 + **台账散文段**(台账载体两处并存,per `ADR-CEP-06` SU 2026-11-09 ③) | ⛔ 不改正文;登记形态须先核 **P-2**;⛔ `trace.h5` / `report.html` **不入库**(retention 丙) |
| 4 | **DoD 读法** | `.rddf/roadmap/objectives/obj-cep-platform.md` §6.2 B4 证据行(append-only) | 追加「IC3 在**拓扑级 E2E run** 上达成通过态」⚠️ 须带「拓扑级/E2E」限定词(与既有行「首个真实 run 实例」并存不冲突) |
| 5 | **新闸** | `scripts/test/cep_full_green_run_check.sh` + `test/CMakeLists.txt` + `scripts/README.md` | 判据 **C1 / C1' / C2 / C3 / C4 / C5**:两违例类实测 + **样本活性见证** + 实测为 0 + IC3 通过态 + **拓扑链身份锚**(⛔ 非合成 fixture / 非模型级实例)+ `07 号` 新 SU 存在;配**每个判据各 1 条**决定性变异(共 **6 条**);ctest **175 → 176**;守门表 **36 → 37** |

**Out of Scope(⛔ 硬边界)**

- ⛔ **不得为凑 IC3 掩盖真实违例** —— 若实测 `memory_violations > 0` ⇒ **立即停止**,按**缺陷**另立处置(⛔ 不得改判据 / 阈值 / 预登记来「让它变绿」)
- ⛔ **不得用合成 fixture 或模型级装置实例冒充拓扑级 E2E run**(B4 的 h3 v2 实例是**真实 run 但属模型级**;合成的是更早的 fixture F1)
- ⛔ **不得把「接线 + 场景扩写」写成「能力新增」**(检测器已存在)
- ⛔ **不得删 `not_collected` 的登记**而不接上产出者(那会让 `metric_json` 输出 0 ⇒ **裸 0 的假绿**,违反 **Entry #222** ④)
- ⛔ **不得以「挂了检测器但样本从未流入」的实测 0 充数**(⇒ 必须有**样本计数**的机器见证,见 AC-2)
- ⛔ **不得声称「检测力已证」** —— 除非注入式负控到位(本 change ⛔ 不含负控 ⇒ 证据强度弱于 B4 的三负控 + 耦合探针)
- ⛔ 不改 IC3 判据 / 不改 `metrics.schema.json` / 不改预登记(**`experimental/preregistration.json` 零 diff**)· ⛔ 不改 `B3b-2 = 0/4` · ⛔ 不新建 ADR · ⛔ 不动 `§4.0` 排期值 · ⛔ 不改任何 ADR **决策 / 正文**(仅追加 `ADR-CEP-19` 一则 Status Update)

## 验收标准(Acceptance Criteria)

> **判据纪律**:每条变异 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」;⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（两条前置已核 + 前提订正已落）**:**P-2**(ADR-CEP-06 retention 丙口径是否覆盖本 run 的登记形态)与 **P-3**(「全绿 run」的完整判据范围)逐条给出结论并落盘,**且**本 proposal §前提订正 的口径(「拓扑级(E2E 链)」)已在 design / tasks / spec / improvement 四处一致 ⇒ ⛔ 未核不得进入 A2
- [ ] **AC-2（违例类被实测 + 有样本活性见证）**:目标 run 的 `metrics.json` 中 `memory_violations` 为**实测数值**(⛔ **不是** `"not-collected"`),且 `deadlock_violations` 为**实测数值** ⇒ 两键都在 `measured` 内;**且** `observation_keys` 含一个**样本计数**键(如 `memory_ops_checked_addr_count`,值 **> 0**)⇒ 证明「检测器**有样本可检**」⇒ ⛔ 不得以「挂了但样本从未流入」的实测 0 充数;⚠️ 该键名 ⛔ 不得与 13 必纳键同名(per `cep_b4_observation_keys_check.sh`)
- [ ] **AC-3（实测为 0）**:两违例类实测值 **== 0** ⇒ ⚠️ **若 > 0 ⇒ 停止并按缺陷另立处置**(⛔ 不得改判据/阈值/预登记)
- [ ] **AC-4（IC3 通过态 · 拓扑级 E2E run）**:该 run 的 `report.html` 渲染「**✓ 一致性闸门通过**」⇒ `cep_ic3_report_states_check.sh` 绿 + `tools/render_report.py --check` 绿;**且** 该 run 带**拓扑链身份锚**(在 `test/cep_e2e_runs.jsonl` 有条目且 `config_file` ∈ `configs/cep_chi_rnf_e2e_*.json`)⇒ ⛔ 模型级 h3 run **不满足**本 AC(它满足的是 B4)⇒ ⛔ 反向 fixture 必须证明「合成 fixture / 模型级实例冒充拓扑级 run」会被新闸判红
- [ ] **AC-5（4 件套齐备 + 登记形态合法 + 可对账）**:4 件套按 **P-2 的结论**齐备且登记形态合法(⚠️ `trace.h5` 的 retention 口径逐字遵守)⇒ `cep_experiment_run.sh` 的 4 件套校验绿;**且** 目标 run 落**顶层** `experimental/run-<ts>-<tag>/`(⛔ 嵌套子目录不入库,per `.gitignore:116-119`)· `metrics.json` + `simulation_config.json` **已入库**(git tracked)· `test/cep_e2e_runs.jsonl` 条目**必填 9 键**齐备(per `cep_e2e_reconcile_check.sh` 的 `required`:`run_id` / `config_sha256` / `metrics_sha256` / `metrics_13` / `verification_mode` / `verification_scope` / `ic3_verdict` / `hypothesis_id` / `topology_tag`)+ `ic3_verdict` ∈ 三态词表(`✓ 通过` / `❌ 违例 fail` / `⚪ 不适用`) ⇒ `cep_e2e_reconcile_check.sh` **双向闸绿**;**且** **台账散文段**已落(obj §11 一条 Entry 描述该 run 的 7 字段 —— per `ADR-CEP-06` SU 2026-11-09 ③「载体 = 两处并存」)
- [ ] **AC-6（证据登记 + 五步链）**:`07 号` **新 SU** 落盘(⛔ 不改正文;⛔ 须**显式取代** SU·十五 ① 的「不承载全绿 run」口径)+ `ADR-CEP-19` **一则 Status Update** 落盘(⛔ 不改决策 / 正文)+ `ADR-CEP-06` **一则 Status Update** 落盘(订正其 SU 2026-11-09 §② 的「框架验证 run 证据边界」五句 —— 本次装置形态转变的如实登记)⇒ 走五步链(Entry 先落 → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验)⇒ 冻结清单 provenance 成立 ⇒ 冻结面 = **3 处**
- [ ] **AC-7（DoD 读法更新 + 活文本同步）**:`obj §6.2` B4 证据行**追加**(append-only)「IC3 在**拓扑级 E2E run** 上达成通过态」+ 指向新 SU 与 run 目录 ⇒ `cep_docs_sync_check --strict` 绿;**且** 下列活文本逐条同步或显式登记不改:`configs/cep_chi_rnf_e2e_{a,b}.json` 的 `description` · `docs/guide/cep-usage.md` §12.8 · `AGENTS.md` 的 main spec 计数
- [ ] **AC-8（新闸有牙）**:新闸判据齐备(C1 / C1' / C2 / C3 / C4 / C5)+ **每个判据各 1 条决定性变异 = 6 条**(破坏 `memory_violations` 的实测性 ⇒ 必红;删样本计数键 ⇒ 必红;实测值改非 0 ⇒ 必红;IC3 降级为不适用 ⇒ 必红;run 换成模型级 / fixture ⇒ 必红;移除 `07 号` 的新 SU ⇒ 必红)⇒ 全部**按签名红** + 还原 ⇒ 绿
- [ ] **AC-9（0 回归 + 范围纪律）**:`ctest` **176/176** · 新增闸自身 rc=0 且**全部** `scripts/test/cep_*.sh` 逐个 rc=0(⛔ **不写死绝对数**;以 `ls scripts/test/cep_*.sh` 实测为准并回填) · format / docs_sync / doc_ref / agents_md_lint / freeze / **reconcile** 全 rc=0 · ⛔ 未改 IC3 判据 / 阈值 / 预登记(`experimental/preregistration.json` **零 diff**) / `metrics.schema.json` / retention 条款 · ⛔ `B3b-2` 未动 · ⛔ 未改任何 ADR 决策 / 正文

## 成本收益

**消「拓扑级 E2E run 上 IC3 恒 ⚪ 不适用」vs 1 处场景扩写 + 1 处检测器挂载 + 1 处判据绑定 + 1 个 run 产物 + 2 则 SU + 1 个新闸**。

收益侧是**拓扑级**「全绿 run」缺口的闭合:模型级(B4)⇒ **拓扑级**(本项),且**不依赖任何外部条件**。

## 诚实边界(⛔ 不可约残余)

1. 本项**只覆盖「一次拓扑级 E2E run 的通过态实例」** ⇒ ⛔ **不等于**「所有协议族 / 所有拓扑都达成」;
2. `trace.h5` 的 retention 口径(丙)下 **不入库** ⇒ 「4 件套的『齐备』」只能以**运行期**形态成立 ⇒ 该形态的合规性取决于 **P-2** 的结论;
3. 一次通过态**不构成统计意义上的「全绿」** ⇒ ⛔ 措辞必须限定为「**一次拓扑级 E2E run 的通过态实例**」;
4. 本项**不改**任何判据 ⇒ 若未来发现违例类语义有缺口,仍须另立 change;
5. ⚠️ **本项的证据强度弱于 B4**:B4 有三负控 + 耦合探针 + watchdog 非空洞(SU#4 ② 六条非空洞性约束),本项**不含**负控 ⇒ ⛔ **不得**表述为「检测力已证」,⛔ 且「模型级 ≠ 协议级」的口径限定须随引用重复(per `docs/validation/2026-11-05-cep-b4-evidence-device.md` §5);
6. ⚠️ **CEP 实现侧无内存操作语义**(实测 `include/tlm/cep/*.hh` 对 `memory_read` / `memory_write` / `MemoryOp` **0 命中**)⇒ 本项的内存样本由**测试侧**从 run 的真实事务流(`StubCacheEndpointTLM` 的 per-txn `addr` / `is_write`)导出 ⇒ 该 0 的含义是「**本次 run 的地址 / 写序观察未发现违例**」,**不是**协议级一致性证明。

## 关联

台账 **Entry #393**(P-1 核对 + 根因精化)· **#392**(根因首定位)· **#383**(R-3 归因)· **#264** · **#228**(B4 达成 + 证据装置)· **#222**(④ 不得写裸 0)· architect 方案 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(⚠️ 其「合成」表述待 architect 域订正,见 §前提订正)· `07 号` SU · 十五 ① · `ADR-CEP-19` SU ② · `ADR-CEP-19` **ASM-8 ①**(`hypothesis_id` 须绑定 H3)· `docs/guide/cep-usage.md` §12.8 · `docs/validation/2026-11-05-cep-b4-evidence-device.md`
