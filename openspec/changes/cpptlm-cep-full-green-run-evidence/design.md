# design —— `cpptlm-cep-full-green-run-evidence`

> **定位**:实施契约。根因与闭合路径落 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 订正 + **§六 订正二**——§六 已把其 §一.3 / 硬边界 2 的「合成」表述订正为「**拓扑级(E2E 链)** 缺口」,与本文档口径一致);本文件记**实施面细节**与**判据设计**。
> ⚠️ **A1/A2/A3 三步是「前置核对 + 定位」** —— ⛔ **未核完不得改代码**(design §D-2 的「不许预设挂载点」)。
> ⚠️ **口径**:本 change 的缺口 = 「**拓扑级(E2E 链)** run 不承载 IC3 通过态」;**模型级**装置(H3 v2)已有真实 run 通过态(per `obj §6.2 B4` Entry #228 ③ + `docs/validation/2026-11-05-cep-b4-evidence-device.md` §0)⇒ ⛔ **不得**把本项表述为「首次具备真实 run 通过态实例」。
> ℹ️ **编号约定**:本文件的**闸判据**编号 = **C1 / C1' / C2 / C3 / C4 / C5**(共 6 条);`tasks.md` 的**阶段门**编号 = **G1~G6**;`obj §6.2` 还有 B4 时代的 **G1~G4** ⇒ **三套编号互不相同**,勿混读。

## D-1 · 现状(实测 · 本 change 的事实基座)

| 事实 | 落点 |
|---|---|
| 违例**检测器**已存在 | `test/cep_experiment_harness.hh:135-140`(`ViolationCounts.swmr` / `.data_value`)+ :169 / :182(递增点) |
| `metric_json` 的「未采集」判定 | `test/cep_experiment_harness.hh:374-378` —— 键在 `not_collected` ⇒ `"not-collected"`,否则输出实测值 |
| `not_collected` 的登记由**调用方**做 | `test/test_cep_experiment_emit.cc:345-348`(H4 装置)· `test/test_cep_stub_cache_emit.cc:146`(E2E 装置)· `test/test_cep_stub_cache_e2e.cc:560/568/772`(**单测反向 fixture**,非真实产出) |
| 装置 `ConsistencyCompletionModel` 的调用方 | 仅 `test/test_cep_b4_evidence_device.cc` · `test/test_cep_experiment_emit.cc` ⇒ ⛔ **不在 E2E 运行链上** |
| **模型级**通过态实例(**已达成**,非本 change 目标) | `experimental/run-20261105-153418-h2h3h4h6/h3/`:`deadlock_violations = 0` + `memory_violations = 0`(**均在 `_measured`**)· `simulation_config.json`:`hypothesis_id = "h3"` · `report.html` 渲染「一致性闸门通过」⇒ per `obj §6.2 B4`(iii)「IC3 三态中该态**首个真实 run 实例**(此前仅合成 fixture F1)」 |
| **拓扑级**现状(本 change 的起点) | `experimental/run-2026-11-09-cep-e2e{,-b}/metrics.json`:`deadlock_violations` = 0 · `memory_violations` = `not-collected` ⇒ IC3 = ⚪ 不适用 |
| **拓扑级 run 的装置自陈** | 两个 run 的 `simulation_config.json`:`hypothesis_id = "framework-e2e"` · `run_class = "framework-verification"` · `not_an_experiment_run = true` · `evidence_scope = "…⛔ 非 B4 证据;⛔ 非一致性安全断言(IC3=⚪ 不适用)"`;装置侧同源声明见 `test/test_cep_stub_cache_emit.cc:10-15` |
| **判据绑定的机器强制** | `tools/render_report.py:178-183`(`preregistration_problems` 判据④):`hypothesis_id` **无登记行 ⇒ `--check` 判红** ⇒ 拓扑级 run 若要 `--check` 绿,**必须**绑定已登记行 |
| **拓扑级 run 的唯一已登记行** | `ADR-CEP-19` **ASM-8 ①**「run 的 `hypothesis_id` **须绑定 `H3`**(唯一双违例类均须实测的已登记行)」;`experimental/preregistration.json` H3 行 `out_of_scope = [coherence_stalls, collective_stalls, directory_misses]` ⇒ ⛔ **绑定 H3 不需要改 `preregistration.json`** |
| **IC3 三态 fixture** | `scripts/test/cep_ic3_report_states_check.sh`(F1 通过 / F2 不适用 / F3 违例 / F4 连带洞) |
| **retention 丙(入库口径)** | `ADR-CEP-06` SU 2026-11-08 · 二:只入库 `metrics.json` + `simulation_config.json`,**范围 = 有台账条目的 run**;`trace.h5` / `report.html` **不入库**;IC1~IC5 判定面**不依赖** `trace.h5` |
| **入库的可机判载体** | `test/cep_e2e_runs.jsonl`(现有 2 条 · `ic3_verdict` 三态词表 `{"✓ 通过", "❌ 违例 fail", "⚪ 不适用"}`)· 双向闸 `scripts/test/cep_e2e_reconcile_check.sh`(9 键齐备 + sha256 相等 + **零 orphan 双向**)· `.gitignore:116-119` 只放开**顶层** `experimental/run-*/{metrics.json,simulation_config.json}`(⛔ 嵌套子目录不入库) |

⇒ **缺的不是能力、不是采集点、也不是接线** ⇒ 缺的是「**同时满足『挂了检测器』+『有真实内存操作样本』+『绑定已登记判据行』』的装置**」。

## D-2 · 实施面(⛔ 四处**待定位**,不许预设)

| # | 待定位项 | 定位方式 | 若定位失败 |
|:--:|---|---|---|
| **A1/P-2** | ADR-CEP-06 **SU 2026-11-08 · 二**(retention **丙**)是否覆盖本 run 的登记形态(运行期 4 件套 + 入库 2 件) | 读 `docs/adr/ADR-CEP-06-experiment-protocol.md` 的 retention 裁定段 | ⇒ **停**并先裁决口径(⛔ 不许自行发明登记形态) |
| **A1/P-3** | 「全绿 run」的**完整**判据范围(是否还要求 H2/H3/H6 假设全覆盖等) | 读 `obj §6.2` B4 段 + `ADR-CEP-06` SU#4 §① + `ADR-CEP-19` SU ② | ⇒ **扩写判据范围 ⇒ 停并回报重立项**(⛔ 不许在本 change 内扩围:会突破 KPI slot 与估时) |
| **A2** | harness 的**检测器挂载点** + E2E 场景里**真实内存读写**的产生路径 | 读 `test/cep_experiment_harness.hh` 的 `violations` 填充链 + `stub_cache_core` 的 `memory_read/memory_write` 路径 | ⇒ **停**并回报(⛔ 不许猜一个挂载点就改) |
| **A2.5** | 拓扑级 run 的 **`hypothesis_id` 绑定点**(绑定 H3 后 `not_an_experiment_run` / `run_class` / `evidence_scope` 三处自陈字段的改点) | 读 `test/test_cep_stub_cache_emit.cc` 的 `simulation_config.json` 写出处 + `tools/render_report.py` 判据④ | ⇒ **停**并回报(⛔ 不许绕过预登记绑定) |

### A2.1 / A2.2 · 实测定位结论(2026-11-13 · A2 阶段)

**A2.1 挂载点 = emit 侧一句,harness 零改动**:
- `ExperimentMetrics::violations`(`test/cep_experiment_harness.hh:345`,类型 `ViolationCounts`)+ 写出点 `:430-432`(`metric_json("memory_violations", violations.total())` + `memory_violation_kinds`)⇒ **harness 只负责「写出」,不负责「填充」**;
- ⛔ **全仓唯一的填法** = `test/test_cep_experiment_emit.cc:231` `r.metrics.violations = positive.detector().counts();`(**整份 counts 拷贝**)⇒ ⇒ **挂载点 = emit 侧同型一句** `m.violations = detector.counts();` ⇒ ⇒ **`test/cep_experiment_harness.hh` 零改动**。

**A2.2 真实内存读写路径(⚠️ 本 design 的 API 名需订正)**:
- 真路径 = `StubCacheEndpointTLM::handle_cpu_ingress()`(`include/tlm/cep/stub_cache_endpoint.hh:142`)→ `:150-154` 取 `addr` / `is_write` → `core_.cpu_write(addr, data)` / `core_.cpu_read(addr)`(`include/tlm/cep/stub_cache_core.hh:187` / `:171`,真操作 `lines_[addr].data`)⇒ ⇒ **真实内存读写样本存在**;
- ⚠️ **订正**:本文件 §D-2 A2 原写「`stub_cache_core` 的 `memory_read/memory_write` 路径」⇒ ⛔ 该 API **不存在**(实测 `memory_read` / `memory_write` 在 `include/tlm/cep/*.hh` **0 命中**)⇒ 实际 API = **`cpu_read` / `cpu_write`**;
- 样本来源 = `txn_stamps()`(`TxnStamp{txn_id, addr, issue_cycle, complete_cycle, completed, hit, rejected, id_space}`)⇒ ⚠️ **缺 `is_write` 与 `data`** ⇒ ⇒ 需给 `TxnStamp` **增 2 个原始事实字段**(⛔ 只增不删 ⇒ 现有 8 处断言不破);
- 样本规模:`TrafficGenTLM::issueRequest()`(`include/tlm/traffic_gen_tlm.hh:201`)SEQUENTIAL 下 `is_write = (cur_addr_ % 8 == 0)` ⇒ 每 2 个地址 1 次写;addr 区间 0x1000~0x2000 ⇒ 写 / 读样本均非空。

### A2.3 / A2.5 · 可承载性三问的**改点**(全部定位)

| 字段 | 落点(emit 侧 `simulation_config.json` 写出处) | 现状 | 目标 |
|---|---|---|---|
| `hypothesis_id` | `test/test_cep_stub_cache_emit.cc:177` | `"framework-e2e"` | `"h3"`(per `ADR-CEP-19` ASM-8 ①) |
| `run_class` | `:179` | `"framework-verification"` | 新值(⛔ 须与 `ADR-CEP-06` SU 2026-11-09 ② 的口径在 ADR SU 中对齐) |
| `not_an_experiment_run` | `:180` | `true` | `false` |
| `evidence_scope` | `:181-183` | 「⛔ 非 B4 证据;⛔ 非一致性安全断言(IC3=⚪ 不适用)」 | 如实改写(须含**拓扑级** + 强度限定,见 §D-3 语义边界条) |

⇒ **绑定 H3 不需要改 `experimental/preregistration.json`**(H3 已 `registered`,`out_of_scope` = {`coherence_stalls`, `collective_stalls`, `directory_misses`})⇒ 绑定后 `_not_collected` 必须 ⊆ 该 3 键 ⇒ `memory_violations` **必须实测**(A3 目标)且 **延迟族 5 键必须实测**(拓扑 A/B 回程已闭合 ⇒ 实测成立)。
⇒ ⚠️ **另 1 处同文件断言**须同步:`test/test_cep_stub_cache_emit.cc:154` `REQUIRE(ic3_verdict(m) == NotApplicable)` ⇒ A3 后须改为 `Pass`(⛔ 不得只改 metrics 而留旧断言)。

### A2.5 · 「不绑 H3」的原理由已失效(一手依据 · 2026-11-13 A1 核对浮出)

`ADR-CEP-06` **SU 2026-11-09(Entry #304)§②** 明写拓扑级 run 当初**不绑 `H3`** 的理由:
「H3 的预登记 out-of-scope = {`coherence_stalls`, `collective_stalls`, `directory_misses`} ⇒ **延迟族 5 键必须实测**;而本装置**无已完成往返**(框架层 dual-port 入向路由恒落 PE 侧 ⇒ 回程不可达)⇒ 延迟族**无样本** ⇒ 绑 H3 会违反『未列入 out-of-scope 者**必须实测**』」。

⇒ **该前提已被消除**:框架层回程已由 change `cpptlm-cep-dual-port-inbound-dispatch` 修复(双端口适配器入向改为**按端口组**路由)+ 拓扑 A/B 的 home 已配 `emit_completions: true`(实测 `configs/cep_chi_rnf_e2e_{a,b}.json`)⇒ **延迟族 5 键已实测**(实测 `run-2026-11-09-cep-e2e-b`:`avg/p50/p95/p99/max` 全为数值 `5`)⇒ ⇒ **绑 H3 不再违反「未列入 out-of-scope 者必须实测」** ⇒ 且 H3 的 out-of-scope 集**恰好**是那 3 个非违例类 ⇒ ⇒ **绑定 H3 不需要改 `experimental/preregistration.json`**(该文件 M1 冻结集内 ⇒ 零 diff)。
⇒ ⛔ **但**:该 SU §② 的**五句证据边界**随之与事实不符 ⇒ 须由 **D-6 的第 3 处冻结面**(`ADR-CEP-06` 一则 Status Update)如实登记本次**形态转变**(⛔ 不得静默翻转自陈字段)。

### A2.3 · 「拓扑级身份」的可判性(三问的判定依据 · ⚠️ 问的是**可承载性**,不是「可区分性」)

⚠️ **订正**:原问「真实 run 与合成 fixture 在文件形态上是否可区分」⇒ 答案**是**(可区分),但区分开后得到的是**相反**结论 ⇒ **问错了问题**。正确的三问:

| # | 问 | 判据来源 | 现状 |
|:--:|---|---|---|
| ① | 该 run **绑定了哪个已登记假设行**? | `experimental/preregistration.json` 的 `hypotheses` 键(`tools/render_report.py:178-183` 硬要求) | `framework-e2e` ⇒ **无登记行** ⇒ `--check` 判红 ⇒ **不合格** |
| ② | 该 run **自陈是不是实验 run**? | `simulation_config.json` 的 `not_an_experiment_run` / `run_class` | `true` / `framework-verification` ⇒ **不合格** |
| ③ | 该 run **的 `evidence_scope` 是否允许作一致性安全断言证据**? | 同上 | 「⛔ 非一致性安全断言(IC3=⚪ 不适用)」⇒ **不合格** |

⇒ **现状三项全为「不合格」** ⇒ 本 change 的实质 = **让拓扑级 run 取得证据资格**(绑定 H3 + 翻转自陈 + 补 `memory_violations` 实测)+ 取证。
⇒ ⚛️ **若 A2.3 判定「取得资格」需要改动 `experimental/preregistration.json`(即 H3 的 out-of-scope 集合不够)⇒ 停并回报**(⛔ 本 change 明令不改预登记)。

## D-3 · 场景扩写的判据(形态要求)

- **必须**:目标场景**真的产生内存操作样本**(⇒ `memory_violations` 的检测器**有样本可检**)⇒ 该键**不在** `not_collected` 内;
- **必须**:检测器**挂上**且**实际参与计数** ⇒ 输出**实测数值**(0 也是数值,⛔ **不是** `"not-collected"`);
- **必须(活性见证)**:产出侧登记**样本计数**(落在 `metrics.json` 的 `observation_keys` 附加键,如 `memory_ops_checked_addr_count`,值 **> 0**)⇒ 机器可证「检测器**有样本可检**」⇒ ⛔ **堵住「挂了但样本从未流入 ⇒ 实测 0」的平凡满足**;⚠️ 该键名 ⛔ 不得与 13 必纳键同名(per `cep_b4_observation_keys_check.sh`);⚠️ `observation_keys` 是**附加键** ⇒ 零 `metrics.schema.json` 改动;
- ⛔ **不得**:把 `not_collected` 里的键**删掉**而不接上产出者(那会让 `metric_json` 输出 0 ⇒ **裸 0 的假绿**,违反 **Entry #222** ④);
- ⚠️ **零违例是结果,不是目标** ⇒ 若实测 > 0 ⇒ **停止**(见 proposal 的 Out of Scope);
- ⚠️ **语义边界(必须随登记重复 · A2 实测精化)**:CEP 实现侧**无内存一致性不变量语义**(实测 `include/tlm/cep/*.hh` 对 `memory_read` / `memory_write` / `MemoryOp` **0 命中**;真实路径 = `cpu_read` / `cpu_write`,per §D-2 A2.2)⇒ 样本由**测试侧**从 run 的真实事务流(`StubCacheEndpointTLM` 的 per-txn `addr` / `is_write` / `data`)导出 ⇒ ⇒ 该 0 是「**本装置无该形态**」的 0,**双重空洞**:① 本装置**无 sharer 语义**(无 directory / 无 sharer tracker)⇒ **SWMR 违例结构上不可达**;② `TrafficGenTLM::issueRequest()` 的 `req.data.write(0)` **恒 0** + home 回程只回显 `p.data`(`include/tlm/cep/cohif_responder.hh:164`)⇒ **无版本语义** ⇒ data-value 检查**无判别力** ⇒ ⇒ ⛔ **不得**表述为「已证明协议层无死锁 / 无违例」,⛔ **不得**称「检测力已证」(per `docs/validation/2026-11-05-cep-b4-evidence-device.md` §5 的同款口径限定)。⚠️ **要获得判别力需另立 change**(写侧非恒定数据 + 版本字段 + 注入式负控 ⇒ **能力新增**,⛔ 不在本 change 范围)。

## D-4 · 4 件套与登记形态

- 产出链 = `cep_experiment_run.sh`(--out <dir>)/ `tools/run_sweep.py` ⇒ 4 件套 + `cep_experiment_run.sh` 的 4 件套校验;
- ⚠️ **登记形态以 A1/P-2 的结论为准** ⇒ ⛔ 本文件**不预设**形态;
- ⚠️ `trace.h5` 的入库口径 ⛔ 不在本 change 内放宽(⛔ 不改 retention 条款);
- **入库形态(retention 丙 · A4 承接)**:目标 run 落**顶层** `experimental/run-<ts>-<tag>/`(⛔ 嵌套子目录不入库,per `.gitignore:116-119`)⇒ 只入库 `metrics.json` + `simulation_config.json` ⇒ `test/cep_e2e_runs.jsonl` 追加 1 条(9 键 + `ic3_verdict = "✓ 通过"` + `metrics_13` 逐键结论)⇒ `cep_e2e_reconcile_check.sh` **双向闸绿**(⛔ 只 commit 两件套而不加 jsonl 条目 ⇒ **零 orphan 判红** ⇒ AC-9 失败)。
- ⚠️ **台账载体 = 两处并存**(per `ADR-CEP-06` **SU 2026-11-09(Entry #304)③**):7 字段的载体 = **台账散文段**(obj §11 的一条 Entry,描述该 run 的 7 字段)+ **`test/cep_e2e_runs.jsonl`** ⇒ ⇒ A4 须**两处同落**,⛔ 只落 jsonl 即不满足该 SU(机器闸只校验 jsonl ⇔ 台账散文段是**契约面**要求,机器不兜底 ⇒ 须人工核对)。

## D-5 · 新闸设计(`scripts/test/cep_full_green_run_check.sh`)

> ℹ️ 闸判据编号 = **C1 / C1' / C2 / C3 / C4 / C5**(共 **6 条**判据 ⇒ 6 条决定性变异);勿与 `tasks.md` 的阶段门 G1~G6 混读。

| # | 判据 | 变异 ⇒ 期望签名 |
|:--:|---|---|
| **C1** | **真实 run 存在**且其 `metrics.json` 的两违例类为**实测数值**(⛔ 非 `not-collected`) | 把 `memory_violations` 改回 `not-collected` ⇒ `违例类未实测` |
| **C1'** | **样本活性见证**:`observation_keys` 的样本计数键 **> 0** | 删掉该键 / 置 0 ⇒ `检测器无样本见证` |
| **C2** | 两违例类实测值 **== 0** | 改成 1 ⇒ `违例类非零` |
| **C3** | IC3 = **通过态**(⛔ **非** ⚪ 不适用 / ❌ 违例) | 判据降级为不适用 ⇒ `IC3 非通过态` |
| **C4** | **拓扑链身份锚** —— run 须在 `test/cep_e2e_runs.jsonl` 有条目,其 `config_file` ∈ `configs/cep_chi_rnf_e2e_*.json` **且** `config_sha256` == 磁盘该 config 的 sha256 **且** `simulation_config_sha256` == run 内 `simulation_config.json` 的 sha256 **且** `hypothesis_id` ∈ 已登记行 | 把 run 换成**模型级 h3 run** / **合成 fixture** ⇒ `非拓扑级 E2E run` |
| **C5** | `07 号` 的**新 SU** 存在且指向该 run | 移除新 SU ⇒ `证据登记缺失` |

⚠️ **C4 的可判性**:判据锚在**已入库 + 已登记**两个载体上(`test/cep_e2e_runs.jsonl` + `experimental/preregistration.json`)⇒ 模型级 h3 run(嵌套目录 · 未入库 · jsonl 无条目)与合成 fixture(无 jsonl 条目)**均自动落选** ⇒ ⛔ **不依赖**「文件形态偶然差异」。
⚠️ **C4 与「装置级实例」的边界**:绑定 H3 后,`framework-e2e` 装置与模型级装置**共用** H3 行 ⇒ C4 靠**拓扑链身份锚**而非**假设行**区分二者 ⇒ ⛔ 闸不得以「`hypothesis_id` == h3」作为拓扑级判据。
⚠️ **既有条目口径提示**(本 change 范围外的既有数据 · ⛔ 不返工):`test/cep_e2e_runs.jsonl` 现有 2 条的 `config_file` **同为** `configs/cep_chi_rnf_e2e_a.json`(run-b 的 `run_id` 带 `-b`,但其配置文件仍是 A)⇒ ⇒ C4 的 glob 口径**只判「是否拓扑级」**,⛔ **不用于区分拓扑 A / B**(两者皆拓扑级 ⇒ 皆应通过 C4)⇒ 实施期**勿**把 `config_file` 当作 A/B 判别器用。

## D-6 · 冻结面

**3 处**(`07 号` 新 SU + `ADR-CEP-19` 一则 Status Update + `ADR-CEP-06` 一则 Status Update)⇒ **五步授权链**:Entry 先落 → 产物 commit → `--update --authorized-by "Entry #N"` → 清单 commit → 无参复验。
⚠️ **为何是 3 处**(A1/P-3 核对时浮出 · 2026-11-13):本 change 翻转拓扑级 run 的 `run_class` / `not_an_experiment_run` / `evidence_scope` / `hypothesis_id` ⇒ `ADR-CEP-06` **SU 2026-11-09(Entry #304)§②** 的「框架验证 run 的证据边界」**五句**(⛔ 不得作 B4 证据 · ⛔ 不得改 hypothesis 结论 · ⛔ 不得作一致性安全断言(IC3 = ⚪ 不适用)· 用途**仅** = 可对账 · 不在受监管的实验 emit 路径上)将**与事实不符** ⇒ 必须**追加**一则订正(⛔ 不回改原文)⇒ **冻结面 = 3 处**。
⚠️ 归档时新增 main spec `openspec/specs/cep-full-green-run-evidence/spec.md` 会被 `openspec/specs/cep-*/spec.md` glob **自动入冻结集** ⇒ 归档后需**第二次** `--update`(同一授权号)⇒ 冻结产物数会变 ⇒ ⛔ 实施期须以 `cep_freeze_check.sh` 无参复验的实测为准。
⚠️ `test/` / `scripts/` / `configs/` / `experimental/run-*/` **不在冻结集** ⇒ 其改动**不触发**清单变更;⚠️ 但 `experimental/preregistration.json` 与 `experimental/metrics.schema.json` **在**冻结集内 ⇒ 本 change 对二者**零 diff**。

## D-7 · 硬边界(逐字进 tasks)

1. ⛔ **不得为凑 IC3 掩盖真实违例** —— 实测 > 0 ⇒ 停止 + 按缺陷另立处置;
2. ⛔ **不得用合成 fixture 或模型级装置实例冒充拓扑级 E2E run**;
3. ⛔ **不得把「接线 + 场景扩写」写成「能力新增」**;
4. ⛔ **不得删 `not_collected` 的键**而不接上产出者(裸 0 假绿,违反 Entry #222 ④);
5. ⛔ **不得以「挂了检测器但样本从未流入」的实测 0 充数**(必须有样本计数见证);
6. ⛔ **不得声称「检测力已证」**(本项不含负控);
7. ⛔ 不改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款;⛔ 不改任何 ADR **决策 / 正文**。

## D-8 · 不可约残余(随交付落盘)

1. 一次拓扑级通过态实例 **≠**「所有协议族 / 所有拓扑达成」;
2. `trace.h5` 不入库 ⇒ 「4 件套齐备」只能是**运行期**形态 ⇒ 合规性取决于 A1/P-2;
3. 措辞必须限定为「**一次拓扑级 E2E run 的通过态实例**」;
4. **模型级 ≠ 协议级** —— 该 0 = 「本次 run 的地址 / 写序观察未发现违例」,⛔ 不得表述为「已证明协议层无死锁 / 无违例」;
5. ⛔ **不得称「检测力已证」** —— 证据强度弱于 B4(三负控 + 耦合探针 + watchdog 非空洞);
6. C4 的可判性依赖「目标 run 已入库 + `jsonl` 有条目 + 假设行已登记」三件事同时成立 ⇒ 任一不成立 ⇒ 闸对目标 run 判红(**保守失败**,非假绿)。

## 铁律

1. **先核前置,后改代码**(A1/A2/A3)。
2. **零违例是结果,不是目标** ⇒ > 0 即停。
3. **拓扑级身份不可由 fixture 或模型级实例冒充** ⇒ 判据必须机判(C4)。
4. **不改任何判据** ⇒ 只加证据 + 只加绑定。
5. **证据先于宣称** ⇒ `07 号` 新 SU + `obj` 证据行必须可回溯到 run 目录 ⇒ 且 run 的两件套**已入库**(retention 丙)⇒ 否则「可回溯」不成立。
6. **口径随引用重复** ⇒ 拓扑级限定 + 模型级≠协议级 + 不得称检测力已证,三句必须跟着新 SU / 证据行 / 新闸的说明一起走。
