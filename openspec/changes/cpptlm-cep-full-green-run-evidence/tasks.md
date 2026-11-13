# tasks —— `cpptlm-cep-full-green-run-evidence`

> 顺序 = **A0(授权门) → A1(前置核对 P-2/P-3 + 前提订正落盘) → A2(挂载点 + 内存样本路径 + 判据绑定定位) → A3(场景扩写 + 检测器挂载 + 样本见证) → A4(真跑 + 4 件套 + 入库 + 双向对账) → A5(IC3 通过态 + 报告) → A6(证据登记 + 五步链) → A7(DoD 读法 + 活文本同步) → A8(新闸 + 变异) → A9(0 回归 + 归档)**。
> 门(阶段门编号):**G1 = A0** · **G2 = A3**(违例类被实测 + 样本见证)· **G3 = A5**(IC3 通过态)· **G4 = A6**(证据登记)· **G5 = A8**(新闸有牙)· **G6 = A9**(0 回归 + 归档)。
> ℹ️ **编号区分**:本文件的 **G1~G6 = 阶段门**;design §D-5 的 **C1 / C1' / C2 / C3 / C4 / C5 = 新闸判据**(共 6 条);`obj §6.2` 还有 B4 时代的 **G1~G4** ⇒ **三套编号互不相同**,勿混读。
> ⚠️ **A1/A2 是「核对与定位」,⛔ 未完成不得改代码**(design §D-2)。
> ⚠️ **口径**:目标 = 「**拓扑级(E2E 链)** run 的 IC3 通过态实例」;**模型级**通过态实例已由 B4 达成 ⇒ ⛔ 本 change 不得宣称「首次具备真实 run 通过态实例」。

## A0 · 授权门(G1)
- [x] A0.1 台账 Entry 授权实施(占 1 slot,KPI **0/3 → 1/3**)⇒ ✅ **Entry #394** 已落(2026-11-13)
- [x] A0.2 improvement + architect 方案已落盘 ⇒ ✅ `.rddf/improvements/cpptlm-cep-full-green-run-evidence.md` + `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 / §六 订正)
- [x] A0.3 四件套已建 + `openspec validate --strict` rc=0 ⇒ ✅ 实测 rc=0
- [x] A0.4 基线实测:两个已有 E2E run 的 `memory_violations` = `not-collected`、`deadlock_violations` = 0;`simulation_config.json` 的 `hypothesis_id` = `framework-e2e` / `not_an_experiment_run` = `true` / `evidence_scope` 含「⛔ 非一致性安全断言」(复核 design §D-1)⇒ ✅ **实测复核完成**(2026-11-13 · 两 run 逐项一致;`-b` 的延迟族 5 键**已实测** = 5 ⇒ 佐证 design §A2.5)
- [x] A0.5 **台账追记**(append-only):`Entry #394` 的冻结面枚举 **1 处 → 3 处**(追加 `ADR-CEP-19` + `ADR-CEP-06` 各一则 Status Update)+ 追记 §前提订正 的口径 ⇒ ✅ **Entry #395** 已落(2026-11-13)⇒ ⛔ 五步链 `--update` 授权号仍用 `Entry #394`(其冻结面范围以 #395 追记为准)

## A1 · 前置核对(⛔ 不许跳过 · design §D-2)
- [x] A1.1 **P-2**:读 `ADR-CEP-06` **SU 2026-11-08 · 二**(retention **丙**)⇒ 结论落盘(「运行期 4 件套 + 入库 2 件」的登记形态是否合法)⇒ ✅ **结论:登记形态合法**(运行期 4 件套与入库 2 件**并存**,丙口径明确 IC1~IC5 判定面不依赖 `trace.h5`;且 **SU 2026-11-09 ③** 已追认「首批 = 有台账条目的 run(首批 = 框架验证 run)」⇒ **框架验证 run 入库有先例**;⛔ 无需新裁决)· per **Entry #395 ⑤**
- [x] A1.2 **P-3**:读 `obj §6.2` B4 段 + `ADR-CEP-06` SU#4 §① + `ADR-CEP-19` SU ② ⇒ 「全绿 run」的完整判据范围落盘 ⇒ ✅ **结论:不构成扩围**(判据 = 两级,run-validity / hypothesis-verdict **不可互相继承**;B4 形态 = SU#4 §① 三条**单 run**同时满足,⛔ 不含「H2/H3/H6 假设全覆盖」(那是 B7 的 8/8 口径);`ADR-CEP-19` SU ② 的**双重成因**现已双双闭合(框架层回程已修 + 拓扑 A/B `emit_completions: true` ⇒ 延迟族已实测)⇒ **拓扑级只剩 IC3 一项** = 本 change 范围)· per **Entry #395 ⑤**
- [x] A1.3 **前提订正落盘**:proposal §前提订正 的 5 条(缺口口径 = 拓扑级 / 「合成」说反了 / C4 拓扑链身份锚 / 冻结面 3 处 / AC-9 的 ADR 口径)在 **design / tasks / spec / improvement** 四处一致 ⇒ ✅ **四处已一致**(2026-11-13 复审实测;「冻结面」一处由 2 处改 **3 处**,见 Entry #395 ④)
- [x] A1.4 ⛔ 若 P-2 / P-3 任一不成立 ⇒ **停**并回报 ⇒ ✅ **停点门未触发**:P-2 与 P-3 **均不成立**(即均**不构成**扩围/否决)⇒ **可进入 A2**;⚠️ A1 核对浮出两条**新增约束**并已回写 change:① **冻结面 2 处 → 3 处**(加 `ADR-CEP-06` SU 2026-11-09 §② 五句边界订正)② **台账载体 = 两处并存**(jsonl + 台账散文段,per SU 2026-11-09 ③)· per **Entry #395 ⑤**

## A2 · 挂载点 + 内存样本路径 + 判据绑定定位(⛔ 只读)
- [x] A2.1 定位 harness 的 `violations` 填充链 + **检测器挂载点** ⇒ ✅ **挂载点 = emit 侧一句** `m.violations = detector.counts();`(与 `test_cep_experiment_emit.cc:231` 同型)⇒ harness 只**写出**(`:430-432`)不**填充**(`:345` 是成员)⇒ ⇒ **`test/cep_experiment_harness.hh` 零改动** · per design §D-2 A2.1
- [x] A2.2 定位 E2E 场景里**真实内存读写**的产生路径 ⇒ ✅ 真路径 = `StubCacheEndpointTLM::handle_cpu_ingress()`(`include/tlm/cep/stub_cache_endpoint.hh:142`)→ `core_.cpu_write(addr,data)` / `core_.cpu_read(addr)`(`include/tlm/cep/stub_cache_core.hh:187`/`:171`)⇒ ⚠️ **订正**:`memory_read`/`memory_write` API **不存在**(实测 0 命中)⇒ 样本源 = `txn_stamps()`,⚠️ **缺 `is_write` / `data`** ⇒ 需给 `TxnStamp` 增 2 个原始事实字段 · per design §D-2 A2.2
- [x] A2.3 **「拓扑级身份」的可承载性三问**(design §D-2 A2.3):① 该 run 绑定了哪个**已登记**假设行(`tools/render_report.py:178-183` 硬要求)② `not_an_experiment_run` / `run_class` 的自陈现状 ③ `evidence_scope` 是否允许作一致性安全断言证据 ⇒ 三问**现状均为「不合格」** ⇒ 须逐条给出「改点」 ⇒ ✅ **三问改点全部定位**(`test/test_cep_stub_cache_emit.cc` 的 `simulation_config.json` 写出处:`hypothesis_id` :177 / `run_class` :179 / `not_an_experiment_run` :180 / `evidence_scope` :181-183)· per design §D-2 A2.3/A2.5
- [x] A2.4 判定「拓扑链身份锚」(C4)在**入库后**是否可机判(`test/cep_e2e_runs.jsonl` 的 `config_file` / `config_sha256` / `simulation_config_sha256` + `experimental/preregistration.json` 的已登记行)⇒ ⛔ 若不可判 ⇒ **停**并回报(⛔ 不得用「我说是真的」作判据) ⇒ ✅ **可机判**(既有 2 条 jsonl 条目已含全部所需键)· per design §D-5 C4
- [x] A2.5 定位 **`hypothesis_id` 绑定点** + `not_an_experiment_run` / `run_class` / `evidence_scope` 三处自陈字段的改点(per `ADR-CEP-19` **ASM-8 ①**)⇒ 确认**绑定 H3 不需要改 `experimental/preregistration.json`**(✅ **一手依据已就位**:design §A2.5 —— `ADR-CEP-06` SU 2026-11-09 §② 当初「不绑 H3」的理由 = 延迟族无样本(回程不可达)⇒ 该前提已被 `cpptlm-cep-dual-port-inbound-dispatch` 消除 ⇒ 绑定 H3 **不**违反「未列入 out-of-scope 者必须实测」)⇒ ⚠️ **A3 之前可做一次探测性 `tools/render_report.py --check`**(既有 run-b:延迟族 5 键**已实测** ⇒ 延迟族不越界;但其 `memory_violations` 仍 `not-collected` ⇒ H3 的 `out_of_scope` 只含 3 键 ⇒ **预登记越界检查必判红 ⇒ 该红是 A3 要消除的「预期内红」,⛔ 不是停点**)⇒ ⛔ **判绿判据在 A4.3**(A3 之后);⛔ 若 **A4.3** 仍判红 ⇒ **停**并回报 ⇒ ✅ **可实现**
- [x] A2.6 ⛔ 若 A2.4 / A2.5 的结论为「不可实现」⇒ **停**并回报 ⇒ ✅ **停点门未触发**(A2.4 可机判 + A2.5 可实现)⇒ **可进入 A3**;⚠️ A2 另浮出**证据强度**事实(该 0 是「本装置无该形态」的 0 且**双重空洞**:无 sharer 语义 ⇒ SWMR 结构上不可达;写数据恒 0 + 无版本 ⇒ data-value 无判别力)⇒ 已精化 design §D-3 + proposal 诚实边界 6(⛔ 不得称「检测力已证」;⛔ 要有判别力需另立 change)

## A3 · 场景扩写 + 检测器挂载 + 样本见证(G2)
- [x] A3.1 目标场景**真的产生内存操作样本** ⇒ 检测器**有样本可检** ⇒ ✅ 实测:`memory_ops_checked_addrs` **20 个去重地址**(TrafficGen SEQUENTIAL 下 写 = `cur_addr_ % 8 == 0` ⇒ 写/读样本均非空)
- [x] A3.2 检测器**挂上**且**实际参与计数** ⇒ 输出**实测数值** ⇒ ✅ 实测:`memory_violations = 0`(**实测值**,⛔ 非 `"not-collected"`)· `memory_violation_kinds = {swmr: 0, data_value: 0}` · 两违例类均在 `_measured`
- [x] A3.3 **样本活性见证**:`metrics.json` 的 `observation_keys` 含样本**地址列表**键 `memory_ops_checked_addrs`(其**长度** = 被检测的内存操作样本计数 ⇒ 实测 **20 > 0**;⛔ 零样本不得充数)⇒ ⛔ 键名不得与 13 必纳键同名(实测不撞);⛔ 零 `metrics.schema.json` 改动 ✅
- [x] A3.4 ⛔ **不得**删 `not_collected` 的键而不接产出者(裸 0 假绿,Entry #222 ④)⇒ ✅ `memory_violations` 的登记**与**检测器接线在**同一次改动**内完成(检测器在接线前即有 `REQUIRE_FALSE(memory_ops_addrs.empty())` 见证)
- [x] A3.5 ⛔ 不改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` ⇒ ✅ `cep_prereg_drift_check` / `cep_freeze_check` / `cep_ic3_report_states_check` 全 rc=0;`experimental/preregistration.json` **零 diff**
- [x] A3.6 **装置自陈字段翻转 + 预登记绑定**(A2.3 / A2.5 定位的「改点」的实施项)⇒ ✅ `test/test_cep_stub_cache_emit.cc` 的 `simulation_config.json`:`hypothesis_id` → `"h3"`(per `ADR-CEP-19` ASM-8 ①)· `run_class` / `not_an_experiment_run` **两字段移除**(与 h2/h3/h4/h6 同形)· `evidence_scope` 改写为**拓扑级 + 强度限定**;⚠️ 另同步 `:154` 的 `ic3_verdict` 断言 `NotApplicable` → `Pass`;**文件头诚实边界段**同步改写(原「本 run 不是实验证据 run / IC3 ⚪ 不适用」已成假)
⇒ **A3 验证**:构建 73/73 · `[cep]` **515 cases / 11707 assertions** 全绿 · `ctest` **175/175** 全绿 · `cep_freeze_check` rc=0 · 实测产物落 `/tmp/opencode/a3-verify`(**非** A4 正式 run)

## A4 · 真跑 + 4 件套 + 入库 + 双向对账
- [x] A4.1 用 `cep_experiment_run.sh` / `tools/run_sweep.py` 跑出目标 run 目录(**顶层** `experimental/run-<ts>-<tag>/`;⛔ 嵌套子目录不入库,per `.gitignore:116-119`)⇒ ✅ `experimental/run-20261113-151310-cep-full-green/`;⚠️ **A2 漏查**:`cep_experiment_run.sh` 只覆盖 h2/h3/h4/h6 ⇒ 补 `--topology` 模式(复用同一 `emit_trace`,⛔ 不复制实现)
- [x] A4.2 4 件套按 **A1.1 的结论**齐备(⚠️ `trace.h5` 的 retention 逐字遵守)⇒ ✅ 运行期 4 件套齐备(`metrics.json` / `simulation_config.json` / `trace.h5` 17488 B / `report.html` 3239 B);`git check-ignore` 实测:前 2 件 **TRACKABLE** · `trace.h5` / `report.html` **IGNORED**(= retention 丙)
- [x] A4.3 `cep_experiment_run.sh` 的 4 件套校验绿 + `tools/render_report.py --check` 绿(⚠️ 后者的预登记判据④ 要求 `hypothesis_id` 是**已登记**行)⇒ ✅ 两者均 rc=0(`not-collected` = {`collective_stalls`, `directory_misses`} ⊆ H3 `out_of_scope` ⇒ 无越界)
- [x] A4.4 **入库 + 对账**:`metrics.json` + `simulation_config.json` **已入库**(git tracked)· `test/cep_e2e_runs.jsonl` 追加 1 条(**必填 9 键**齐备,per `cep_e2e_reconcile_check.sh` 的 `required`:`run_id` / `config_sha256` / `metrics_sha256` / `metrics_13` / `verification_mode` / `verification_scope` / `ic3_verdict` / `hypothesis_id` / `topology_tag`)+ `ic3_verdict` ∈ 三态词表 + `metrics_13` 逐键结论 ⇒ `cep_e2e_reconcile_check.sh` **双向闸绿**(⛔ 只 commit 不加条目 ⇒ 零 orphan 判红);**且** **台账散文段**已落(obj §11 一条 Entry 描述该 run 的 7 字段 —— per `ADR-CEP-06` SU 2026-11-09 ③「载体 = 两处并存」⇒ ⛔ 只落 jsonl 即不满足该 SU,机器闸不兜底 ⇒ 须人工核对)⇒ ✅ 条目已加(`run_id = cep-full-green-2026-11-13` · `ic3_verdict = "✓ 通过"`)· 双向闸 **3 条目 ↔ 3 run · 0 问题** · 台账散文段 = **Entry #396**

## A5 · IC3 通过态(G3)
- [x] A5.1 两违例类实测值 **== 0**(⚠️ **> 0 ⇒ 停止**并按缺陷另立处置 —— 硬边界 1)⇒ ✅ `deadlock_violations = 0` · `memory_violations = 0`(swmr=0 / data_value=0)
- [x] A5.2 `report.html` 渲染「**✓ 一致性闸门通过**」⇒ ✅ 逐字实测 `<strong>✓ 一致性闸门通过</strong>`(命中 1 次)
- [x] A5.3 `cep_ic3_report_states_check.sh` 绿 ⇒ ✅ **4/4**(F1 通过 / F2 不适用 / F3 违例 / F4 连带洞);另 `render_report.py --check` 走**通过**分支(⛔ 非「不适用」分支)· rc=0

## A6 · 证据登记 + 五步链(G4)
- [x] A6.1 `07 号` **新 SU**(插在 `## §末 文档索引` **之前**;⛔ 不改正文)⇒ 索引重生成 ✅(⚠️ **口径订正**:07 号 自身**无**「不承载全绿 run」的陈述可取代 ⇒ 本则**新增**拓扑级 IC3 通过态事实 + **指向**真正持有该陈述的三处载体(`ADR-CEP-19` SU 2026-11-09·一 ② / `cep-usage.md` §12.8 / config A `description`);⚠️ **指针漂移已登记为残余** —— 仓内引用的「`07 号` SU · 十五/十六 ①」在当前 07 号 **不可解析**(实测 `grep "十五"` 0 命中;17 则 SU 的索引 B 序号与标题内「则/·N」标签互不对应)⇒ 三处 SU 一律用**可解析锚点**)· `generate_doc_index.py --write` + `--check` 均 rc=0
- [x] A6.2 `ADR-CEP-19` **一则 Status Update**(⛔ 不改决策 / 正文)⇒ 取代其 SU ② 的「⛔ 拓扑级未闭合『全绿 run』」状态陈述;**且** `ADR-CEP-06` **一则 Status Update**(⛔ 不改决策 / 正文)⇒ 订正其 SU 2026-11-09 §② 的「框架验证 run 证据边界」五句(本次装置形态转变的如实登记,per design §D-6) ✅ 两则均已追加(append-only,原文逐字未动)
- [x] A6.3 五步链:Entry 先落 → 产物 commit → `--update --authorized-by` → 清单 commit → 无参复验 ⇒ ✅ **五步全绿**:① Entry #397(`3e08536f`)② 产物(`51cf7f16`:三处 SU + 07 号索引重生成 + A7.1)③ `--update --authorized-by "Entry #394"`(授权校验通过 · 81 产物)④ 清单单独 commit(`2555fb03` · provenance = `51cf7f16`)⑤ **无参复验 rc=0**(81 产物 hash 全部一致)
- [x] A6.4 冻结清单 provenance 成立(冻结面 = **3 处**;⚠️ 归档后新 main spec 自动入集 ⇒ 第二次 `--update`)⇒ ✅ 3 处 sha256 已更新(ADR-CEP-06 → 49a2f31a… / ADR-CEP-19 → b58a4a53… / 07 号 → 5c637db0…);产物数 81 不变;`authorized_by = "Entry #394"`(范围以 Entry #395 追记为准)

## A7 · DoD 读法 + 活文本同步
- [x] A7.1 `obj §6.2` B4 证据行**追加**(append-only)「IC3 在**拓扑级 E2E run** 上达成通过态」+ 指向新 SU 与 run 目录 ⇒ ⚠️ 须带「拓扑级/E2E」限定词(与既有行「首个真实 run 实例」并存不冲突) ⇒ ✅ 已追加(📌 2026-11-13 追加(拓扑级档位)· 含 run 目录 / `Entry #396` / `07 号` 新 SU / 口径限定 / 「不改既有『首个真实 run 实例』表述」)
- [x] A7.2 `cep_docs_sync_check.sh --strict` 绿 ⇒ ✅ rc=0
- [x] A7.3 **活文本同步清单**逐条同步或显式登记不改:`configs/cep_chi_rnf_e2e_{a,b}.json` 的 `description` · `docs/guide/cep-usage.md` §12.8 · `AGENTS.md` 的 main spec 计数 ⇒ ✅ 逐条处置:**A 的 description** 已改(原「仍不承载全绿 run:IC3 = ⚪ 不适用」已成假 ⇒ 改为「现承载拓扑级 IC3 通过态」+ 口径限定);**B 的 description** **无需改**(其 `description` 无假陈述 —— ADR 侧「B 不承载全绿」指的是 **canonical 10 流的无损投影**,与本 run 无关);**`cep-usage` §12.8** 已改(「至今未闭合」→「拓扑级已闭合」+ 载体指针 + 口径限定);**`AGENTS.md` main spec 计数** ⏸ **随 A9 归档执行**(归档前新增 main spec 尚未存在,现在改会写错)

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
