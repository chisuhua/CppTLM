# CEP 实验：H2 / H3 / H6 首轮结果（2026-11-04）

**性质**：假设验证实验（`obj-cep-platform.md §9.1` 的 H2 / H3 / H6）
**执行**：builder，2026-11-04
**verification_mode**：`bounded_random`（三组均是抽样规模，非穷尽）

---

## 0. 一句话结论

在**固定 RN 侧行为、仅变化 home 侧目录策略**的模型内，H2 / H3 / H6 三条判据均成立；但三条的适用范围都窄于字面表述，**不得**外推为「协议级结论」（见 §2 口径限定）。

---

## 1. 判据来源（逐字）

| 假设 | 验证手段 | 成功判据 |
|---|---|---|
| **H2** | `avg_latency_cycles` + `directory_misses` 对比 | 3 策略延迟差异 **> 20%** |
| **H3** | `deadlock_violations == 0` + `memory_violations == 0` | **0 违例（1000+ 事务）** |
| **H6** | `vc_stall_per_vc` + `deadlock_violations` | **0 死锁 + collective stall 不占 coherence VC** |

来源：`.rddf/roadmap/objectives/obj-cep-platform.md` §9.1。

---

## 2. 口径限定（⚠️ 必须随任何引用重复）

1. **H2 不是协议对比**。三策略是**同一 home 侧目录策略对象**的三种注入
   （`FullMapDirectory` / `DcohSymmetric` / `BroadcastNullTracker`），RN 侧固定。
   ⇒ 结论 = 「**目录策略**对延迟的影响」；
   ⛔ **不是**「CHI 协议 vs CXL 协议的延迟差」。
2. **H6 是最小 VC 模型**。单级仲裁、无 credit 环流、无多级/跨 VC 依赖图。
   ⇒ 结论 = 「共享同一 VC 且 coherence 固定优先时会饿死 collective；分离 VC 可避免」；
   ⛔ 完整 fabric 仲裁属 Stage 4+（`ADR-CEP-09`）。
3. **既有登记义务**：`ADR-CEP-09` SU 2026-11-02 §④ 的
   「D∪E 合并后 `GrantAck` 可能被 `Grant` 饿死**未建模**」—— 本实验**不覆盖**，
   引用 H6 结论时必须原样带上。
4. **verification_mode = `bounded_random`**：按 `ADR-CEP-06 §3`，
   **不得**表述为「已证明无死锁 / 无违例」，只能说「在该抽样规模内未观察到」。

---

## 3. 实验装置（新增文件，均在 `test/`，不入 `include/tlm/cep/`）

| 文件 | 职责 |
|---|---|
| `test/cep_experiment_harness.hh` | `LatencyCollector`（延迟分布）+ `SwmrDataValueDetector`（双不变量）+ `ProgressWatchdog`（活性）+ `ExperimentMetrics`（13 必纳 JSON） |
| `test/cep_directory_experiment.hh` | `HomeNodeModel`：lookup → 按 verdict 展开 snoop → 聚合延迟 |
| `test/cep_vc_fabric_model.hh` | 最小 VC 模型（隔离 / 共享 + 每 VC stall） |
| `test/test_cep_experiment_harness.cc` | 观测层判据（14 cases） |
| `test/test_cep_h2_h3_experiments.cc` | H2 / H3 判据 + 反向 fixture |
| `test/test_cep_h6_vc_isolation.cc` | H6 判据 + 负控 + 反向 fixture |
| `test/test_cep_experiment_emit.cc` | 4 件套产出（仅 `CEP_EXP_DIR` 设置时） |
| `scripts/test/cep_experiment_run.sh` | 跑三组实验 → 补 `trace.h5` → `render_report.py` → `--check` |

**为什么放 `test/` 而不是 `include/tlm/cep/`**：实验装置不是契约面。放进 `include/` 会让每次
harness 修订都要走冻结面授权（per `examples/cep_poc_spike_s3.cc` 的 spike 硬边界先例）。

**延迟参数来源**：`snoop_latency = 2` 取自 `include/tlm/cep/stub_cache_core.hh:158`
的既有参数化值；`lookup_latency = 4` / `data_return_latency = 6` 是本模型显式声明的常量
（只影响绝对量级，不影响策略间相对差）。

---

## 4. H2 结果

固定 workload：同一地址 200 次读，初始安装 2 个 sharer（Sdma / Vram，S 态）。

| 策略 | avg latency (cycles) | snoop 展开 |
|---|---:|---|
| 集中 `FullMapDirectory`（EXACT） | **4** | 0（`ANSWER_HERE`） |
| 分布 `DcohSymmetric`（CONSERVATIVE） | **14** | 2（多 sharer ⇒ `BROADCAST`） |
| 广播 `BroadcastNullTracker`（无目录） | **18** | 4（恒 `BROADCAST`，全体） |

| 对比 | 相对差 | 判据（>20%） |
|---|---:|---|
| 集中 vs 分布 | **250%** | ✅ |
| 分布 vs 广播 | **28.6%** | ✅ |
| 集中 vs 广播 | **350%** | ✅ |

**反向 fixture**：三策略全部指向 `FullMapDirectory` ⇒ 相对差 **0%** ⇒ 「>20%」判据**不成立**
（证明上表不是恒真）。
**可复现性**：同一配置重跑两次，avg 完全一致。

---

## 5. H3 结果

`DcohSymmetric`，region = `[0x1000, 0x2000)`，**1200 事务**（每轮安装 1~3 个真实 sharer）。

| 指标 | 实测 | 判据 |
|---|---:|---|
| 漏探次数（snoop 集 ⊉ 真实 sharer 集） | **0** | 0 |
| `memory_violations` 的 `swmr` 分桶 | **0** | 0 |
| `memory_violations` 的 `data_value` 分桶 | **0** | 0 |
| 事务数 | **1200** | ≥1000 |

**反向 fixture**：
- 跳过 invalidate 连续写 10 次 ⇒ `swmr == 10`（检测器有牙）；
- 读到过期版本 ⇒ `data_value == 1`；
- 检测器被变异成空检查（`checks_enabled=false`）跑同一违例 workload ⇒ 检出 **0**
  （证明上面的非零值不是构造出来的）。
- 从未写过的地址被读 ⇒ 不判违例（防恒真）。

---

## 5bis. 判据分支的**测量状态**登记(2026-11-04 追加,per 台账 **Entry #219** ③ 缺口 C / `objective §9.1` ① 表)

> **性质**:**只勘误不重跑** —— 本节登记「每条判据分支**到底测没测**」,
> ⛔ **不改**上文 §5 / §6 的任何实测值,⛔ **不重写**结论段。
> ⇒ 历史 run 的 `metrics.json` **不动**(它们是历史产物);重跑留给下一个实验批次兑现新契约。

| 假设 | 判据分支 | 依赖观测 | **测量状态** | 说明 |
|---|---|---|---|---|
| **H2** | 3 策略延迟差异 > 20% | `avg_latency_cycles`(3 策略) | ✅ **已测量** | 4 / 14 / 18 cycles ⇒ 250% / 28.6% / 350% |
| **H2** | *directory_misses 对比* | `directory_misses` | ⏭ **未测量** | ⛔ **全仓无产出者**(实测:仅 schema / tools / 测试断言命中,**无 emit 赋值点**)⇒ 该半支 ⏭ 未测量,**不得**计入 H2 结论(⚠️ **不是**「不成立」—— 「不成立」是 ❌ 的专有语义 = 已实测未达标;本例是**从未被测量**) |
| **H3** | 0 违例 · **分支 1** | `memory_violations`(含 `memory_violation_kinds` 的 SWMR + data-value 双分桶) | ✅ **已测量** | swmr=0 · data_value=0 |
| **H3** | *分支 2* | `deadlock_violations` | ⏭ **未测量** | ⚠️ **h3 的 emit 从未设 `deadlock_detected`**(实测:仅 h6 设)⇒ 该分支**从未被测量**,⛔ **不继承 pass** |
| **H3** | *漏探(superset)判定* | `superset_violations` | ✅ **已测量** | 0 漏探。⚠️ 该观测**不在** `metrics.json` 的 13 必纳内(落在 `verification_scope` 文本)⇒ **不受** `not-collected` 机制覆盖 |
| **H6** | 0 死锁 + collective stall 不占 coherence VC | `deadlock_violations` + `vc_stall_per_vc` + `collective_stalls` | ✅ **已测量** | 独立 VC 死锁=0;共享 VC 负控 backlog=2000 死锁=1 |

**⚠️ 为什么现在补这一节**:上文 §1「判据来源(逐字)」逐字抄了 `objective §9` 的判据原文
(H3 = 「`deadlock_violations == 0` + `memory_violations == 0`」),
而**实测证明其中一支从未被测量** ⇒ 单读上文会得到「两条分支都测了」的错觉。
⇒ per `ADR-CEP-06` **SU#1** §④ 的两级 verdict(与 SU#2 §③ 的 gloss):
**run-validity 层放行**(该 run 本身合法)· **hypothesis-verdict 层拦截**(该判据 = 未测量)。

**关联**: 台账 **Entry #219** ③ 缺口 C · **#218**(§9.1 改造 + 载体裁决)· **#212**(本轮实验)·
`ADR-CEP-06` **SU#1** §④ / **SU#2** §③ · `objective §9.1` ① / ③ 表

---

## 6. H6 结果

注入速率：coherence 2/cycle、collective 1/cycle；跑 **2000 cycle**；每 VC 服务 1/cycle。

| 配置 | collective backlog | collective 在 coherence VC 上的等待 | 死锁 |
|---|---:|---:|---:|
| **独立 VC**（collective=VC3） | **0** | **0** | **0** |
| **共享 VC**（均为 VC0，负控） | **2000** | **>0** | **1** |
| **假隔离**（`isolate=true` 但 VC 撞车） | — | >0 | **1** |

**反向 fixture 说明**：
- 「假隔离」配置证明判据看的是**实际 VC 归属**，不是布尔标志；
- 无 collective 流量时共享 VC **不**误报死锁（防恒真）。

---

## 7. 4 件套链路

```bash
bash scripts/test/cep_experiment_run.sh
# → experimental/run-<ts>-h2h3h6/{h2,h3,h6}/ 各 4 件套
#   simulation_config.json + trace.h5 + metrics.json + report.html
#   render_report.py --check 逐目录通过
```

`trace.h5` 由 `h5py` 写为**真实 HDF5**，但内容为 **run 级摘要**（本模型不产逐周期信号），
文件 attributes 显式标注 `grain = "run-summary"` 与说明文字 ⇒ **不冒充** cycle-level trace。

---

## 8. 未覆盖 / 遗留（不在本轮结论内）

| 项 | 状态 |
|---|---|
| **H1**（seam 隔离） | 有契约级证据（`test_cep_adapter_schema_contracts.cc`），**未**以「假设结论」形式登记 |
| **H4**（完成域判别容纳两流） | 未做实验 |
| **H5**（SHARP 0 误分类） | 有契约级证据（`fail-stop` + `divert_counter` 字段断言），未登记结论 |
| **H8**（4 件套 + 13 metric 足以支撑取舍） | 未做；且 `tools/aggregate.py` / `plot.py` / `recommend.py` 缺失 ⇒ 无「建议输出」载体 |
| 6 个 ProtocolEngine | 运行时仅 `chi_rnf` / `mesif` / `moesi_gpu` / `CohIfNull`；CXL DCOH / TileLink-C / NVLink / UALink **只有 adapter 契约**（per oracle 2026-11-04：规约载体无转移表 ⇒ 补引擎会诱发推理填空，本次不补） |
| `trace.h5` 工具链 | 本机原无 HDF5；`h5py` 已装（3.16.0）。**仓内无 CI 承诺** ⇒ 新环境需重装，属可复现性风险 |
| HDF5 内容语义 | 现为 run 级摘要；若后续要做逐周期 trace，需在模型里加事件流 |

## 9. 复现

```bash
cd /workspace/project/CppTLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 --target cpptlm_tests
./build/bin/cpptlm_tests "[cep][exp]"          # 26 cases / 1319 assertions
bash scripts/test/cep_experiment_run.sh        # 4 件套 + 契约校验
```

---

## 10. 勘误(2026-11-05,per 台账 **Entry #226** + `ADR-CEP-06` **SU#4**)—— H3 装置升级为 v2

> ⚠️ **本文只勘误不重跑** —— 下述事实由 **H3 v2 装置**同参数**重证**,
> 逐字来源 + 口径限定 + 机器锚见 `docs/validation/2026-11-05-cep-b4-evidence-device.md`。

| 项 | 本文档(v1,2026-11-04) | 现行(v2,2026-11-05) |
|---|---|---|
| H3 **分支 1**（`memory_violations == 0`） | ✅ 1200 事务 · swmr=0 · data_value=0 | ✅ **同参数重证**(1200 事务 · swmr=0 · data_value=0 · 漏探=0)—— 数值不变 |
| H3 **分支 2**（`deadlock_violations == 0`） | ⏭ **未测量**(本装置无死锁检测器) | ✅ **首次实测 0**(v2 复合装置:一致性 × 完成域合流)⇒ 该分支不再「未测量」 |
| 装置形态 | 纯一致性装置(**v1**) | **v2** 复合装置(周期化事务流 + 真实在途窗口 + 实例级观测键) |
| 延迟语义 | home 侧 lookup 成本(avg 14) | **在途窗口**(完成周期 − 签发周期,avg 2)⇒ ⛔ **v1/v2 不可比** |
| v1 形态可否再跑 | —— | ⛔ **不保留为可跑变体**(其 `deadlock=not-collected` 与收缩后的 H3 预登记互斥,per SU#4 §① 流程闸)⇒ 可追溯性由**重证**承担 |

⚠️ **H2 / H6 结论不受影响**:两装置零触碰,13 必纳 metric + 声明集**逐项不变**(实测比对)。

---
