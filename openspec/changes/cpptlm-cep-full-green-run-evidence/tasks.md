# tasks —— `cpptlm-cep-full-green-run-evidence`

> 顺序 = **A0(授权门) → A1(前置核对 P-2/P-3 + 前提订正落盘) → A2(挂载点 + 内存样本路径 + 判据绑定定位) → A3(场景扩写 + 检测器挂载 + 样本见证) → A4(真跑 + 4 件套 + 入库 + 双向对账) → A5(IC3 通过态 + 报告) → A6(证据登记 + 五步链) → A7(DoD 读法 + 活文本同步) → A8(新闸 + 变异) → A9(0 回归 + 归档)**。
> 门(阶段门编号):**G1 = A0** · **G2 = A3**(违例类被实测 + 样本见证)· **G3 = A5**(IC3 通过态)· **G4 = A6**(证据登记)· **G5 = A8**(新闸有牙)· **G6 = A9**(0 回归 + 归档)。
> ℹ️ **编号区分**:本文件的 **G1~G6 = 阶段门**;design §D-5 的 **C1 / C1' / C2 / C3 / C4 / C5 = 新闸判据**(共 6 条);`obj §6.2` 还有 B4 时代的 **G1~G4** ⇒ **三套编号互不相同**,勿混读。
> ⚠️ **A1/A2 是「核对与定位」,⛔ 未完成不得改代码**(design §D-2)。
> ⚠️ **口径**:目标 = 「**拓扑级(E2E 链)** run 的 IC3 通过态实例」;**模型级**通过态实例已由 B4 达成 ⇒ ⛔ 本 change 不得宣称「首次具备真实 run 通过态实例」。

## A0 · 授权门(G1)
- [ ] A0.1 台账 Entry 授权实施(占 1 slot,KPI **0/3 → 1/3**)
- [ ] A0.2 improvement + architect 方案已落盘
- [ ] A0.3 四件套已建 + `openspec validate --strict` rc=0
- [ ] A0.4 基线实测:两个已有 E2E run 的 `memory_violations` = `not-collected`、`deadlock_violations` = 0;`simulation_config.json` 的 `hypothesis_id` = `framework-e2e` / `not_an_experiment_run` = `true` / `evidence_scope` 含「⛔ 非一致性安全断言」(复核 design §D-1)
- [ ] A0.5 **台账追记**(append-only):`Entry #394` 的冻结面枚举 **1 处 → 2 处**(追加 `ADR-CEP-19` 一则 Status Update)+ 追记 §前提订正 的口径 ⇒ ⛔ 追记未落则**不得**用 `Entry #394` 授权号对第 2 处冻结件走 `--update`

## A1 · 前置核对(⛔ 不许跳过 · design §D-2)
- [ ] A1.1 **P-2**:读 `ADR-CEP-06` **SU 2026-11-08 · 二**(retention **丙**)⇒ 结论落盘(「运行期 4 件套 + 入库 2 件」的登记形态是否合法)
- [ ] A1.2 **P-3**:读 `obj §6.2` B4 段 + `ADR-CEP-06` SU#4 §① + `ADR-CEP-19` SU ② ⇒ 「全绿 run」的完整判据范围落盘
- [ ] A1.3 **前提订正落盘**:proposal §前提订正 的 5 条(缺口口径 = 拓扑级 / 「合成」说反了 / C4 拓扑链身份锚 / 冻结面 2 处 / AC-9 的 ADR 口径)在 **design / tasks / spec / improvement** 四处一致 ⇒ ⛔ 未核不得进入 A2
- [ ] A1.4 ⛔ 若 P-2 / P-3 任一不成立 ⇒ **停**并回报(⛔ 不许自行发明口径;⛔ 判据范围若需扩围 ⇒ 停并回报**重立项**,不在本 change 内扩围)

## A2 · 挂载点 + 内存样本路径 + 判据绑定定位(⛔ 只读)
- [ ] A2.1 定位 harness 的 `violations` 填充链 + **检测器挂载点**
- [ ] A2.2 定位 E2E 场景里**真实内存读写**的产生路径(`stub_cache_core` 的 `memory_read/memory_write` 等)
- [ ] A2.3 **「拓扑级身份」的可承载性三问**(design §D-2 A2.3):① 该 run 绑定了哪个**已登记**假设行(`tools/render_report.py:178-183` 硬要求)② `not_an_experiment_run` / `run_class` 的自陈现状 ③ `evidence_scope` 是否允许作一致性安全断言证据 ⇒ 三问**现状均为「不合格」** ⇒ 须逐条给出「改点」
- [ ] A2.4 判定「拓扑链身份锚」(C4)在**入库后**是否可机判(`test/cep_e2e_runs.jsonl` 的 `config_file` / `config_sha256` / `simulation_config_sha256` + `experimental/preregistration.json` 的已登记行)⇒ ⛔ 若不可判 ⇒ **停**并回报(⛔ 不得用「我说是真的」作判据)
- [ ] A2.5 定位 **`hypothesis_id` 绑定点** + `not_an_experiment_run` / `run_class` / `evidence_scope` 三处自陈字段的改点(per `ADR-CEP-19` **ASM-8 ①**)⇒ 确认**绑定 H3 不需要改 `experimental/preregistration.json`** ⇒ ⚠️ **A3 之前可做一次探测性 `tools/render_report.py --check`**(既有 run-b:延迟族 5 键**已实测** ⇒ 延迟族不越界;但其 `memory_violations` 仍 `not-collected` ⇒ H3 的 `out_of_scope` 只含 3 键 ⇒ **预登记越界检查必判红 ⇒ 该红是 A3 要消除的「预期内红」,⛔ 不是停点**)⇒ ⛔ **判绿判据在 A4.3**(A3 之后);⛔ 若 **A4.3** 仍判红 ⇒ **停**并回报
- [ ] A2.6 ⛔ 若 A2.4 / A2.5 的结论为「不可实现」⇒ **停**并回报

## A3 · 场景扩写 + 检测器挂载 + 样本见证(G2)
- [ ] A3.1 目标场景**真的产生内存操作样本** ⇒ 检测器**有样本可检**
- [ ] A3.2 检测器**挂上**且**实际参与计数** ⇒ 输出**实测数值**
- [ ] A3.3 **样本活性见证**:`metrics.json` 的 `observation_keys` 含样本计数键(如 `memory_ops_checked_addr_count`,值 **> 0**)⇒ ⛔ 键名不得与 13 必纳键同名;⛔ 零 `metrics.schema.json` 改动
- [ ] A3.4 ⛔ **不得**删 `not_collected` 的键而不接产出者(裸 0 假绿,Entry #222 ④)
- [ ] A3.5 ⛔ 不改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json`

## A4 · 真跑 + 4 件套 + 入库 + 双向对账
- [ ] A4.1 用 `cep_experiment_run.sh` / `tools/run_sweep.py` 跑出目标 run 目录(**顶层** `experimental/run-<ts>-<tag>/`;⛔ 嵌套子目录不入库,per `.gitignore:116-119`)
- [ ] A4.2 4 件套按 **A1.1 的结论**齐备(⚠️ `trace.h5` 的 retention 逐字遵守)
- [ ] A4.3 `cep_experiment_run.sh` 的 4 件套校验绿 + `tools/render_report.py --check` 绿(⚠️ 后者的预登记判据④ 要求 `hypothesis_id` 是**已登记**行)
- [ ] A4.4 **入库 + 对账**:`metrics.json` + `simulation_config.json` **已入库**(git tracked)· `test/cep_e2e_runs.jsonl` 追加 1 条(**必填 9 键**齐备,per `cep_e2e_reconcile_check.sh` 的 `required`:`run_id` / `config_sha256` / `metrics_sha256` / `metrics_13` / `verification_mode` / `verification_scope` / `ic3_verdict` / `hypothesis_id` / `topology_tag`)+ `ic3_verdict` ∈ 三态词表 + `metrics_13` 逐键结论 ⇒ `cep_e2e_reconcile_check.sh` **双向闸绿**(⛔ 只 commit 不加条目 ⇒ 零 orphan 判红)

## A5 · IC3 通过态(G3)
- [ ] A5.1 两违例类实测值 **== 0**(⚠️ **> 0 ⇒ 停止**并按缺陷另立处置 —— 硬边界 1)
- [ ] A5.2 `report.html` 渲染「**✓ 一致性闸门通过**」
- [ ] A5.3 `cep_ic3_report_states_check.sh` 绿

## A6 · 证据登记 + 五步链(G4)
- [ ] A6.1 `07 号` **新 SU**(插在 `## §末 文档索引` **之前**;⛔ 不改正文;⛔ 须**显式取代** SU·十五 ① 的「不承载全绿 run」口径)⇒ 索引重生成
- [ ] A6.2 `ADR-CEP-19` **一则 Status Update**(⛔ 不改决策 / 正文)⇒ 取代其 SU ② 的「⛔ 拓扑级未闭合『全绿 run』」状态陈述
- [ ] A6.3 五步链:Entry 先落 → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验
- [ ] A6.4 冻结清单 provenance 成立(冻结面 = **2 处**;⚠️ 归档后新 main spec 自动入集 ⇒ 第二次 `--update`)

## A7 · DoD 读法 + 活文本同步
- [ ] A7.1 `obj §6.2` B4 证据行**追加**(append-only)「IC3 在**拓扑级 E2E run** 上达成通过态」+ 指向新 SU 与 run 目录 ⇒ ⚠️ 须带「拓扑级/E2E」限定词(与既有行「首个真实 run 实例」并存不冲突)
- [ ] A7.2 `cep_docs_sync_check.sh --strict` 绿
- [ ] A7.3 **活文本同步清单**逐条同步或显式登记不改:`configs/cep_chi_rnf_e2e_{a,b}.json` 的 `description` · `docs/guide/cep-usage.md` §12.8 · `AGENTS.md` 的 main spec 计数

## A8 · 新闸 + 变异(G5)
- [ ] A8.1 `scripts/test/cep_full_green_run_check.sh`:**C1 / C1' / C2 / C3 / C4 / C5**(共 6 条判据)
- [ ] A8.2 **每个判据各 1 条决定性变异 = 6 条**:① 违例类改回 `not-collected` ⇒ `违例类未实测` ② 删样本计数键 ⇒ `检测器无样本见证` ③ 实测值改非 0 ⇒ `违例类非零` ④ IC3 降级为不适用 ⇒ `IC3 非通过态` ⑤ run 换成模型级 / fixture ⇒ `非拓扑级 E2E run` ⑥ 移除 `07 号` 新 SU ⇒ `证据登记缺失`
- [ ] A8.3 全部**按签名红** + 还原 ⇒ 绿;真实仓字节指纹前后相等
- [ ] A8.4 `test/CMakeLists.txt` +1 ⇒ ctest **175 → 176**;`scripts/README.md` 守门表 **36 → 37**

## A9 · 0 回归 + 归档(G6)
- [ ] A9.1 `ctest --test-dir build -j4` **176/176**
- [ ] A9.2 新增闸自身 rc=0 + **全部** `scripts/test/cep_*.sh` 逐个 rc=0(⛔ **不写死绝对数**;以 `ls scripts/test/cep_*.sh` 实测为准并回填;⚠️ `cep_test_shadow.sh` 在磁盘但**不在** README 三表 ⇒ 本次只登记该缺口,不在本 change 内修)
- [ ] A9.3 format / docs_sync / doc_ref / agents_md_lint / freeze / **reconcile** 全 rc=0
- [ ] A9.4 `openspec archive`(✅ 已完成归档语义)⇒ delta **ADDED** 合并进 `openspec/specs/cep-full-green-run-evidence/`;⇒ 归档后**第二次** `cep_freeze_check.sh --update --authorized-by`(新 main spec 自动入冻结集)
- [ ] A9.5 KPI 活口径 **1/3 → 0/3**

## ⛔ 明确不做
- ⛔ 为凑 IC3 掩盖真实违例 · ⛔ 用合成 fixture 或模型级装置实例冒充拓扑级 E2E run · ⛔ 删 `not_collected` 的键而不接产出者 · ⛔ 以「挂了检测器但样本从未流入」的实测 0 充数 · ⛔ 声称「检测力已证」 · ⛔ 改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款 · ⛔ 改任何 ADR **决策 / 正文** · ⛔ 改 `B3b-2` · ⛔ 新建 ADR · ⛔ 动 `§4.0`
