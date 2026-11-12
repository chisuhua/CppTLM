# design —— `cpptlm-cep-full-green-run-evidence`

> **定位**:实施契约。根因与闭合路径落 `.rddf/reviews/cep-full-green-run-gap-rootcause.md`(含 §五 订正);本文件记**实施面细节**与**判据设计**。
> ⚠️ **A1/A2/A3 三步是「前置核对 + 定位」** —— ⛔ **未核完不得改代码**(design §D-2 的「不许预设挂载点」)。

## D-1 · 现状(实测 · 本 change 的事实基座)

| 事实 | 落点 |
|---|---|
| 违例**检测器**已存在 | `test/cep_experiment_harness.hh:135-140`(`ViolationCounts.swmr` / `.data_value`)+ :169 / :182(递增点) |
| `metric_json` 的「未采集」判定 | `test/cep_experiment_harness.hh:374-378` —— 键在 `not_collected` ⇒ `"not-collected"`,否则输出实测值 |
| `not_collected` 的登记由**调用方**做 | `test/test_cep_experiment_emit.cc:345-348` · `test/test_cep_stub_cache_emit.cc:146` · `test/test_cep_stub_cache_e2e.cc:560/568/772` |
| 装置 `ConsistencyCompletionModel` 的调用方 | 仅 `test/test_cep_b4_evidence_device.cc` · `test/test_cep_experiment_emit.cc` ⇒ ⛔ **不在 E2E 运行链上** |
| IC3 三态 fixture | `scripts/test/cep_ic3_report_states_check.sh` |
| 真实 run 现状 | `experimental/run-2026-11-09-cep-e2e{,-b}/metrics.json`:`deadlock_violations` = 0 · `memory_violations` = `not-collected` |

⇒ **缺的不是能力、不是采集点、也不是接线** ⇒ 缺的是「**同时满足『挂了检测器』+『有真实内存操作样本』』的装置**」。

## D-2 · 实施面(⛔ 三处**待定位**,不许预设)

| # | 待定位项 | 定位方式 | 若定位失败 |
|:--:|---|---|---|
| **A1/P-2** | ADR-CEP-06 的 retention 条款 —— 「4 件套齐备但 `trace.h5` 不入库」是否允许 | 读 `docs/adr/ADR-CEP-06-*.md` 的 retention 段 | ⇒ **停**并先裁决口径(⛔ 不许自行发明登记形态) |
| **A1/P-3** | 「全绿 run」的**完整**判据范围(是否还要求 H2/H3/H6 假设全覆盖等) | 读 `obj §6.2` B4 段 + `ADR-CEP-06` SU#4 §① | ⇒ 扩写判据范围(⛔ 不许只做 IC3 一项就宣称「全绿」) |
| **A2** | harness 的**检测器挂载点** + E2E 场景里**真实内存读写**的产生路径 | 读 `test/cep_experiment_harness.hh` 的 `violations` 填充链 + `stub_cache_core` 的 `memory_read/memory_write` 路径 | ⇒ **停**并回报(⛔ 不许猜一个挂载点就改) |

## D-3 · 场景扩写的判据(形态要求)

- **必须**:目标场景**真的产生内存操作样本**(⇒ `memory_violations` 的检测器**有样本可检**)⇒ 该键**不在** `not_collected` 内;
- **必须**:检测器**挂上**且**实际参与计数** ⇒ 输出**实测数值**(0 也是数值,⛔ **不是** `"not-collected"`);
- ⛔ **不得**:把 `not_collected` 里的键**删掉**而不接上产出者(那会让 `metric_json` 输出 0 ⇒ **裸 0 的假绿**,违反 **Entry #222** ④);
- ⚠️ **零违例是结果,不是目标** ⇒ 若实测 > 0 ⇒ **停止**(见 proposal 的 Out of Scope)。

## D-4 · 4 件套与登记形态

- 产出链 = `cep_experiment_run.sh`(--out <dir>)/ `tools/run_sweep.py` ⇒ 4 件套 + `cep_experiment_run.sh` 的 4 件套校验;
- ⚠️ **登记形态以 A1/P-2 的结论为准** ⇒ ⛔ 本文件**不预设**形态;
- ⚠️ `trace.h5` 的入库口径 ⛔ 不在本 change 内放宽(⛔ 不改 retention 条款)。

## D-5 · 新闸设计(`scripts/test/cep_full_green_run_check.sh`)

| # | 判据 | 变异 ⇒ 期望签名 |
|:--:|---|---|
| **G1** | **真实 run 存在**且其 `metrics.json` 的两违例类为**实测数值**(⛔ 非 `not-collected`) | 把 `memory_violations` 改回 `not-collected` ⇒ `违例类未实测` |
| **G2** | 两违例类实测值 **== 0** | 改成 1 ⇒ `违例类非零` |
| **G3** | IC3 = **通过态**(⛔ **非** ⚪ 不适用 / ❌ 违例) | 判据降级为不适用 ⇒ `IC3 非通过态` |
| **G4** | **⛔ 非合成 fixture** —— run 目录须是**真实 E2E run**(可机判:含真实 `simulation_config.json` 的拓扑标识 + 非 fixture 标记) | 把 run 换成 fixture ⇒ `合成 fixture 冒充真实 run` |
| **G5** | `07 号` 的**新 SU** 存在且指向该 run | 移除新 SU ⇒ `证据登记缺失` |

⚠️ **G4 的可判性**:若「真实 run」与「合成 fixture」在文件形态上**不可区分** ⇒ 本判据**不可实现** ⇒ ⛔ 须在 A2/A3 阶段一并解决(⇒ **不得**用「我说是真的」作判据)。

## D-6 · 冻结面

**1 处**(`07 号` 新 SU)⇒ **五步授权链**:Entry 先落 → 产物 commit → `--update --authorized-by "Entry #N"` → 清单 commit → 无参复验。
⚠️ `test/` / `scripts/` / `configs/` / `experimental/` **均不在冻结集** ⇒ 其改动**不触发**清单变更。

## D-7 · 硬边界(逐字进 tasks)

1. ⛔ **不得为凑 IC3 掩盖真实违例** —— 实测 > 0 ⇒ 停止 + 按缺陷另立处置;
2. ⛔ **不得用合成 fixture 冒充真实 run**;
3. ⛔ **不得把「接线 + 场景扩写」写成「能力新增」**;
4. ⛔ **不得删 `not_collected` 的键**而不接上产出者(裸 0 假绿,违反 Entry #222 ④);
5. ⛔ 不改 IC3 判据 / 阈值 / 预登记 / `metrics.schema.json` / retention 条款。

## D-8 · 不可约残余(随交付落盘)

1. 一次通过态实例 **≠**「所有协议族 / 所有拓扑达成」;
2. `trace.h5` 不入库 ⇒ 「4 件套齐备」只能是 run 级形态 ⇒ 合规性取决于 A1/P-2;
3. 措辞必须限定为「**一次真实 run 的通过态实例**」;
4. G4 的可判性取决于「真实 run」与「合成 fixture」在文件形态上是否可区分(⇒ A2/A3 须一并解决)。

## 铁律

1. **先核前置,后改代码**(A1/A2/A3)。
2. **零违例是结果,不是目标** ⇒ > 0 即停。
3. **真实 run 不可由 fixture 冒充** ⇒ 判据必须机判(G4)。
4. **不改任何判据** ⇒ 只加证据。
5. **证据先于宣称** ⇒ `07 号` 新 SU + `obj` 证据行必须可回溯到 run 目录。
