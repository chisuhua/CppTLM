# ADR-CEP-06: Coherence Exploration Platform 实验输出协议

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 实验输出 / metric / sweep / A-B 对比报表**,解 metis G1 "探索平台没有探索"盲点
> **关联真源**:
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §15(演进 seam)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节

---

## 1. 背景

### 1.1 metis G1 盲点 — 探索平台没有"实验"

CEP 系列 ADR(CEP-00 ~ CEP-05)定义了 6 层骨架、canonical 10 流清单、5 种 Sharer Tracker 实现、SHARP 内部接口、Stub Cache 接口 — **全是接口管道**。但 metis 评审指出:

> **零处定义"一次探索实验是什么"** — 没有 metric、没有 sweep、没有 A/B 对比输出。用户真正想要的是"能给协议/拓扑选型决策提供证据的仿真台",但 ADR 交付的是"能编译的接线图"。

**核心问题**:如果 Stage 1~3 做完仍是空转(没有可对比的产物),CEP 失去存在意义。

### 1.2 本 ADR 的定位

本 ADR **不**定义具体实验(那是 builder 阶段 OpenSpec),**只**定义:

- **实验输出契约**(每次实验必产出什么文件)
- **metric 集合**(可观测指标的统一命名)
- **A/B 对比格式**(协议/拓扑/目录策略对比的输出 schema)
- **不可观测项的兜底**

---

## 2. 决策

### 决策 1:每次 CEP 实验 = 1 个 run,产出 4 件套

每次 `cpptlm-cep` 实验运行产生以下 4 件套(per `experimental/` 目录):

```
experimental/<run_id>/
  ├── simulation_config.json      # JSON 配置快照(输入)
  ├── trace.h5                   # HDF5 trace(per cycle per txn 事件)
  ├── metrics.json               # 关键 metric 数值
  └── report.html                # A/B 对比报表(可由 tools 渲染)
```

**run_id 格式**:`{protocol}-{topology}-{directory}-{timestamp}`(per `ADR-CEP-06 §2.1`)

### 决策 2:metric 集合(分必纳 vs 分可选)

### 决策 2.1 必纳 metric(`metrics.json` 必有 13 个字段)

| Metric | 类型 | 单位 | 含义 |
|--------|------|------|------|
| `total_cycles` | int | cycles | 仿真总周期数 |
| `total_transactions` | int | count | 完成事务总数 |
| `transactions_per_cycle` | float | txn/cycle | 吞吐 |
| `avg_latency_cycles` | float | cycles | 平均事务延迟(request→completion) |
| `p50_latency_cycles` | float | cycles | 50 分位延迟 |
| `p95_latency_cycles` | float | cycles | 95 分位延迟 |
| `p99_latency_cycles` | float | cycles | 99 分位延迟 |
| `max_latency_cycles` | float | cycles | 最大延迟 |
| `coherence_stalls` | int | count | coherence 事务因 Ordering/credit/snoop 等待的周期数 |
| `collective_stalls` | int | count | collective 事务因 group rendezvous 等待的周期数 |
| `directory_misses` | int | count | directory lookup 失败次数(per `ADR-CEP-04 §4` hazard 语义) |
| `deadlock_violations` | int | count | 死锁违例次数(应为 0,否则实验 fail) |
| `memory_violations` | int | count | 一致性违例次数(应为 0,否则实验 fail) |

### 决策 2.2 可选 metric(`metrics.json` 可选字段,4 个)

| Metric | 类型 | 含义 |
|--------|------|------|
| `link_utilization_per_link` | dict | 每条 link 的 utilization 0-1 |
| `vc_stall_per_vc` | dict | 每个 VC 的 stall 周期数 |
| `snoop_fanout_per_request` | dict | snoop 请求的 fanout 分布 |
| `directory_size_distribution` | dict | 目录条目数 vs 时间分布 |

**未达成** = `not-collected`(per oracle 评审建议,机器可查,不允许静默 — per oracle v0.3 S2 修订:初版"null"措辞与 not-collected 互斥)

### 决策 3:A/B 对比报表格式(`report.html`)

`report.html` 是由 `tools/render_report.py`(per `cpptlm` Python 库)从 `metrics.json` + `trace.h5` 生成的 HTML 报表,包含:

1. **summary 表**:本次实验与 baseline run 的 13 个必纳 metric 对比(delta %)
2. **latency CDF 图**:p50/p95/p99 延迟累积分布
3. **per-protocol breakdown**(CHI/CXL/TileLink/NVLink/UALink 各自渲染)
4. **violation badges**:`deadlock_violations > 0` 或 `memory_violations > 0` 时显示红色告警

### 决策 4:不可观测项的兜底契约

per oracle 评审建议,任何必纳 metric 不可获得时(`experimental/` 工具缺位、HDF5 trace 损坏等),**必须标注 `not-collected` 而非省略或写 0**:

```json
{
  "p99_latency_cycles": "not-collected",
  "_not_collected_reason": "trace.h5 missing p99 bin",
}
```

**禁止**:省略字段(破坏必纳契约)/ 写 0(违反诚实原则)/ 写 -1(后续计算异常)

### 决策 5:实验输出与 simulation_mode 关系(per ADR-CEP-07)

- `simulation_mode=functional` 时,延迟 = 0,**`avg_latency_cycles` 必须等于 0**(违反 = 实现 bug)
- `simulation_mode=timing` 时,延迟 > 0,`avg_latency_cycles` 是 timing 推进的产物

---

## 3. 不变量

> **⚠️ v0.3 补充(per 调研 F22 / oracle 全面架构审查 Q7)**:`memory_violations` 的"应 0"**须对齐 SWMR + data-value 双不变量** —— ProtoGen 体系中一致性由两条不变量定义(per ProtoGen ISCA 2018 §4.1.8):
> 1. **SWMR**(Single-Writer-Multiple-Reader):任一时刻某地址要么一个写者,要么多个读者
> 2. **data-value invariant**:某 epoch 起点的值 = 上一写 epoch 末的值
>
> 二者在**逻辑时间**上合起来 = per-location sequential consistency。只说"值不一致"不足以覆盖 SWMR 违例(如两个同时写者)。


> **⚠️ v0.3 规模上限契约(per 调研 F22 / F27)**:一次实验的**验证深度**必须显式声明,不得隐式假定"已穷尽验证"。
>
> **一手依据**:*Gen 族的死锁自由验证在极端配置下需 **up to 1TB RAM and hours of computation time**(per HeteroGen 仓库 README);ProtoGen 的 Murϕ 验证仅能覆盖 **3 caches**(内存上限,ISCA 2018 p.12);HieraGen 需 **256GB + hash compaction**(漏检概率 < 0.001%)。
>
> **因此**:
> 1. `metrics.json` 之外,实验元数据**必须**声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} 与 `verification_scope`(规模上限描述)
> 2. **禁止**把 `bounded_random` / `sampled` 的结果表述为"已证明无死锁 / 无违例" —— `deadlock_violations == 0` 只代表"在声明的 `verification_scope` 内未观测到"
> 3. `INV-CEP-06-IC3` 的"应 0"语义**随 `verification_mode` 变化**,须在报告中一并呈现
> 4. 平台**不承诺**"任意配置都能穷尽验证" —— 这是规模事实,不是实现缺陷


- **INV-CEP-06-IC1**:每次 CEP 实验产出 4 件套(`simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`),缺一件 = 实验 fail
- **INV-CEP-06-IC2**:`metrics.json` 含 13 个必纳字段 + 可选字段,缺必纳字段 = 实验 fail
- **INV-CEP-06-IC3**:`deadlock_violations == 0 && memory_violations == 0`,否则 = 一致性违例,实验 fail
- **INV-CEP-06-IC4**:不可观测 metric 标 `not-collected` + reason 字段,禁止省略/0/-1

---

**INV-CEP-06-IC5**(新增,2026-10-20):`memory_violations` 的判定**必须**覆盖 **SWMR + data-value 双不变量**;报告须能区分违例类型(SWMR 违例 / data-value 违例)。

## 4. sweep / A-B 对比模式

## 4.1 sweep 模式(per `tools/run_sweep.py`)

- **输入**:sweep 维度(protocol ∈ {CHI, CXL, TileLink, NVLink, UALink} / topology ∈ {mesh, ring, crossbar} / directory ∈ {full-map, sparse, broadcast} ...)
- **执行**:对每个维度组合跑 1 个 run,产出 N 个 4 件套
- **聚合**:`tools/aggregate.py` 生成 `experimental/<sweep_id>/sweep_summary.csv`(每行 = 1 个 run,每列 = 13 必纳 metric)
- **可视化**:`tools/plot.py` 生成 `protocol_rank.png` / `topology_rank.png` / `directory_rank.png`

## 4.2 A/B 对比模式(per `tools/ab_compare.py`)

- **输入**:baseline run 路径 + target run 路径
- **输出**:`experimental/ab_<timestamp>/`:
  - `delta_metrics.json`(13 个 metric 的 delta%)
  - `latency_cdf_overlay.png`(两个 CDF 叠加)
  - `report.html`(A vs B summary 表)
- **决策辅助**:`tools/recommend.py` 输出 "推荐用 A / 推荐用 B / 不显著" 提示(per `policy_threshold.json` 配置)

---

## 5. 与 CEP-07 仿真语义关系

| 仿真语义 | 对实验输出的影响 |
|---------|----------------|
| `simulation_mode=functional` | `avg_latency_cycles == 0` 必须成立;`trace.h5` cycle 字段恒为 0(per oracle v0.3 S3 修订:schema 不变,cycle=0 常量) |
| `simulation_mode=timing` | `avg_latency_cycles > 0` 是产物;`trace.h5` 含 per-cycle 事件 |
| group-scoped completion | `collective_stalls` 反映 rendezvous 等待 |
| requester-scoped completion | `coherence_stalls` 反映 pairwise 等待 |

---

## 6. 抓取点

1. **`simulation_config.json` 校验**:必含 `simulation_mode` + `protocol` + `topology` + `directory` 字段(缺失 = 配置错)
2. **HDF5 trace schema**:`txn_id` / `cycle` / `event_type` / `addr` 4 字段必含(per ADR-X.1)
3. **`report.html` 模板化**:基线模板锁住,A/B 对比 / sweep summary / 单 run 三种视图
4. **`tools/render_report.py` 必须开源**(避免 vendor lock-in)
5. **`policy_threshold.json` 配置项**:`deadlock_violations == 0` / `memory_violations == 0` 硬性;`avg_latency_score > 10%` 才标"显著退化"

---

## 7. 关联 ADR

- 基础:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
- 配套:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射)
- 配套:`ADR-CEP-07-simulation-and-integration-semantics.md`(仿真语义)
- 配套:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
- 关联:后续 `cpptlm-cep-stub-cache-core-base` 等 OpenSpec change

---

## 8. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 实验输出契约 | 4 件套(simulation_config.json + trace.h5 + metrics.json + report.html)| ✅ 已拍板 |
| 必纳 metric | 13 字段 | ✅ 已拍板 |
| 可选 metric | 4 字段(未达成 = not-collected,禁止省略/0/-1)| ✅ 已拍板 |
| 报表格式 | `report.html` + `tools/render_report.py` | ✅ 已拍板 |
| 一致性违例 fail 闸 | `deadlock_violations == 0` && `memory_violations == 0` | ✅ 已拍板 |
| sweep / A-B 模式 | `tools/run_sweep.py` / `tools/ab_compare.py` | ✅ 已拍板 |
| 与仿真语义关系 | functional → latency=0;timing → latency 是产物 | ✅ 已拍板 |

---

## 9. v0.1 修订记录(本 ADR 初版)

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。

## Status Update 2026-11-04(#1)—— 决策 4 / 决策 5 的**可执行化**裁决(载体 + 两级 verdict + 预登记闸)

**主题**:Stage 0~3 全部归档后,一次全平台梳理暴露 `ADR-CEP-06` 与其实现层的**三方冲突**:
决策 4 的兜底契约在本 ADR 的**字面示例**里**不可被本 ADR 自己规定的 schema 表达**,
决策 5 的「timing ⇒ 延迟 > 0」在测试层**零断言** ⇒ 实测出现「明知不采却写 0」的 run。
本则裁决**载体形态**与**判定口径**;⛔ **本则不改决策 4 / 决策 5 的任何既有条款**,
只**追加**使其可执行所必需的分支与口径定义。

**性质**:architect 域的架构级裁决(本则 §①~§④ = 裁决;§⑤ = 落地分工)。
**关联**: 台账 **Entry #215** / **#216**(前序 architect 裁决)· `07 号` Status Update 2026-11-04(同一裁决的真源 1 同步)
· `experimental/metrics.schema.json`(冻结面)· `tools/{render_report,ab_compare,run_sweep}.py`(冻结面)
· `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`(受本则影响的历史产出)

---

### ① 冲突的三方证据(实测)

| 层 | 位置 | 实测结论 |
|---|---|---|
| **ADR(本文件)** | 决策 4(`:90-101`) | 不可观测 metric **必须**标 `"not-collected"` + reason;**禁止**省略 / 写 0 / 写 -1 |
| **ADR(本文件)** | 决策 5(`:103-108`) | `functional` ⇒ 延迟 **必须 = 0**;`timing` ⇒ 延迟 **> 0** |
| **schema**(冻结面) | `experimental/metrics.schema.json` | 13 个 `required` 字段类型**全部** `number`/`integer`,**无 `oneOf`/`anyOf`** ⇒ `"not-collected"` **不可表达**。实测 `jsonschema` 校验 ADR 决策 4 的示例 ⇒ **FAIL**(`'not-collected' is not of type 'number'`)。`_not_collected_reason` 是**单字符串**且**不在 `required`** ⇒ 多 metric 未采集时无法区分是哪个 |
| **tools**(冻结面) | `render_report.py` / `ab_compare.py` / `run_sweep.py` 各自的 `metric_type_problems()` | 3 件各自持有一份**复制粘贴**的 `REQUIRED_METRICS` + `metric_type_problems()` ⇒ 非数值即判红 ⇒ 决策 4 的兜底在工具层**同样不可执行** |
| **测试层** | `test/test_cep_simulation_mode_lock.cc:98-106` | 该用例名为「不可观测 metric 的 not-collected 契约存在」,但**只断言 `_not_collected_reason` 是 string 型属性存在** ⇒ **不断言 13 字段可承载 `"not-collected"`** ⇒ 测试在给缺口**盖章** |
| **实测产出** | `experimental/run-20261104-215837-h2h3h6/{h2,h3,h6}/metrics.json` | h2/h3 的 5 个 metric(`coherence_stalls`/`collective_stalls`/`directory_misses`/`deadlock_violations`/`memory_violations`)是**结构零**(字段在、产出者从未赋值);h6 的 5 个延迟 metric 同样结构零,而 h6 的 `verification_scope` **自陈**「本实验不采延迟(avg/p50/p95 恒 0)」⇒ **明知不采却写 0** |

⇒ 决策 4 的兜底契约**从未有过可执行载体**;决策 5 的 timing 分支**纯靠 emit 自觉**。

---

### ② 载体裁决:**A(值即类型)+ per-metric `_not_collected` map** —— 三选一中的唯一自洽项

**裁决**:13 个必纳字段的取值**类型**放宽为「数值 **或** `"not-collected"`」;**新增** `_not_collected`(object,per-metric reason map);
**废弃**单字符串 `_not_collected_reason`(并存 = 两种形状双真源)。

**为什么不是 B(伴随标注,13 字段恒为数值 + 清单登记)**:
B 的机器闸只能查「登记一致性」,**查不出「本该采集却漏采集」** ——
实测 0 与未采集的 0 在**数据层同形**,B 无法区分 ⇒ B 把「结构零冒充实测」从**数据层**挪到**清单层**,缺口未闭合。

**为什么不是 C(必采类 / 条件类拆分 required 集)**:
C 语义最准,但①必改 `required` 集 ⇒ 破 `test/test_cep_simulation_mode_lock.cc:68-95` 的**集合相等**断言;
②「现在没人测」≠「不该测」(`directory_misses` 空转是**产出者缺位**,不是 metric 不该存在)——
收窄必纳集 = 把「产出者缺位」误判为「契约错」,重蹈「实现超前/滞后于声明」的老路。

**为什么 A 需要 per-metric map(而非单字符串)**:
单字符串在「多个 metric 未采集」时**语义上不可判定**(无法区分是哪个没采)⇒ 不是可选优化,是正确性下限。

**配套**:3 件冻结 tools 的 `metric_type_problems()` 改为**接受** `"not-collected"`,
但**必须**同时要求 `_not_collected` 中有该 metric 的 reason ⇒ **reason 缺失 ⇒ 判红**。

---

### ③ 决策 5 裁决:**三分支**(h6 的 `simulation_mode` 标签**无误**,缺的是决策 5 的分支)

**裁决**(决策 5 原文两条**保留**,追加第 3 条):

| `simulation_mode` | 延迟族(`avg`/`p50`/`p95`/`p99`/`max`) | 判据 |
|---|---|---|
| `functional` | **必须 = 0** | 决策 5 原文(保留) |
| `timing` **且**未预声明不采延迟 | **必须 > 0** | 决策 5 原文(保留) |
| `timing` **且**实验设计**预先声明**不采延迟(`verification_scope` 明示) | **必须 `"not-collected"`**;⛔ **禁止写 0** | **本则追加** |

**⚠️ branch 3 的判定锚 = `simulation_config.json`**(per critic 2026-11-04 SHOULD_FIX —— 实测 3 件 tools
均只从 `simulation_config.json` 读 verification 声明,`render_report.py` 有显式「唯一真源」纪律注释):
`metrics.json` 内的 `verification_scope` 副本**不参与判定**。
⇒ 否则本则 §④ 自己立的「预登记载体不在 `metrics.json`(被试者自证的报表面)」原则,
在延迟族上会被**构造性绕过** —— emit 端只要在自己的产物里写一句「本实验不采延迟」即可自证豁免。

**⛔ 明确不采的替代路径**:把 h6 改标 `functional` ——
`simulation_mode` 是**仿真的属性**不是**报表的属性**;h6 确实是 2000 cycle 的 timing 推进,
改标 functional 会在数据里写入**第二个谎言**,并污染「functional ⇒ 必须 = 0」的反向检查(真 functional run 反而显得可疑)。

---

### ④ 「全绿」的两级 verdict 裁决(B4 可判定性的前提)

**问题**:`objective §6.2` 的 B4 写「13 必纳 metric 全绿」而**「全绿」无定义** ⇒ B4 不可判定。

**裁决**:拆成**两级**,两级不可互相继承:

| 级别 | 判据 | 载体 |
|---|---|---|
| **run-validity** | ① 4 件套齐备 ② 13 必纳**键**齐备 ③ 每个值**要么**是真实采集的数值、**要么**是 `"not-collected"` 且 `_not_collected` 有该 metric 的 reason ④ `not-collected` 集合 **⊆** 实验设计**预登记**的 out-of-scope 集合 ⑤ 违例类(`deadlock_violations`/`memory_violations`)若为 `"not-collected"` ⇒ 该 run 的 IC3 判定为**「不适用」**,**不得**作为任何一致性安全断言的证据 | `metrics.json` + schema + tools 闸 |
| **hypothesis-verdict** | objective §9 每个假设的判据**逐条**标注「测量了 / 未测量 / fail」;判据所需 metric 为 `not-collected` ⇒ 该判据 = **未测量**,⛔ **不继承 pass** | `objective §9`(判据-指标需求表) |

**④ 的预登记闸是堵「反向自欺」的唯一手段**:
若 emit 端可**事后自由**标 `not-collected`,则「结构零冒充实测」只是被替换成「`not-collected` 冒充预声明」—— 自欺升级而非消除。
⇒ **预登记集合的载体在 objective / plan 域(评审过的设计面)**,**不在** `metrics.json`(被试者自证的报表面)。
⇒ 结构性保证:同一实验**不能**既把某 metric 预登记为 out-of-scope、又在同一实验的假设判据里引用它。

**④ 的违例类规则(订正裁决初稿中的禁令)**:
违例类**允许** `"not-collected"` —— 若禁止,emit 的唯一合规出路就是**把未测量写成 0**
(= h3 实际发生的失败:0 被读作「测了,没死锁」)。正确规则是**允许 + 连带证据降级**:
not-collected ⇒ IC3 判定「不适用」+ 该 run 不得作一致性安全断言的证据 + 报告层须渲染对应 badge
(扩展 `render_report.py` 已有 violation badge 机制,**不新建**)。

**④ 的 B4 读取约束(收窄下游自由度 —— per critic 2026-11-04 SHOULD_FIX)**:
本则确立 run-validity 的可判定性,但 B4 的**落地措辞**属 objective 域(planner)⇒ 追加**读取约束**,
避免「run-validity 绿 ⇒ B4 绿」的降格读取:
- ⛔ **B4 的达成 SHALL NOT 仅依赖 run-validity**;
- ⛔ **违例类(`deadlock_violations`/`memory_violations`)为 `"not-collected"` 的 run,
  SHALL NOT 计入 B4 的达成证据**(否则「不适用」会被读成「通过」);
⇒ 即「13 项中 5 项 not-collected 的 run」**不足以**让 B4 变绿 —— 违例类必须**实测为 0**。

---

### ⑤ 落地分工(⛔ 本则只裁决,不代做)

| 域 | 动作 | 需冻结面授权 |
|---|---|:---:|
| **planner / architect** | objective §9 改造为「**判据-指标需求表**」(每假设逐条列所需 metric)—— **预登记闸的真源修复,先于一切 emit 侧改动** | — |
| **builder** | schema(`oneOf` + per-metric `_not_collected` map + 废单字符串字段)+ 3 件冻结 tools(`metric_type_problems` 接受 not-collected + 要求 reason + `not-collected ⊆` 预登记校验)+ `test/test_cep_simulation_mode_lock.cc` 更新(`:98-106` 改钉新形状;`:68-95` 的集合相等断言**不变**) | ✅ **是** |
| **builder** | `aggregate.py` / `plot.py` / `recommend.py` 的哨兵值防护(三件**不在** `FROZEN_GLOBS` ⇒ 自由改) | — |
| **builder** | ⚠️ **`openspec/specs/cep-core-contracts/spec.md` 与新契约对齐** —— 该 spec 在 `FROZEN_GLOBS` 内;`:116` 仍要求**已废弃**的 `_not_collected_reason`,`:121` 的 IC3 Scenario 仍写「`==0` 才判 pass」而**无「不适用」分支** ⇒ 与本则 §②/§④ 的新契约**直接互斥**。⛔ **不可留待日后** —— 两件冻结面互斥后再修需**两次**授权且漂移期无限 | ✅ **是** |
| **builder** | **补机器断言**(本则落地前**零覆盖**):① `"not-collected"` 值可过 schema(正例)② 缺 reason ⇒ 判红(反向 fixture)③ 违例类 not-collected ⇒ IC3 判定「不适用」(正向)+ ④ 决策 5 三分支各有断言 | — |
| **architect** | 已落盘 h2/h3/h6 的处置 = **只勘误不重跑**:`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` 追加「判据分支测量状态」小节(H3 的 `deadlock_violations` 分支 = **未测量**;h2 的 `directory_misses` 半支 = **未测量**);⛔ **不改历史 run 的 `metrics.json`**(作为历史产物不动);⛔ **不重写结论段** | — |
| **architect** | `07 号` Status Update(真源 1 同步本则 §②~§④)+ 台账 §10 的 B4 相关行 + `§6.2` 的「全绿」口径落地 | — |

### ⑥ 正文勘误指针表(裁决前文本 → **以本则为准**)

⚠️ 本则**不改本 ADR 正文**(ADR 不可变纪律);以下位置是**裁决前的历史文本**,
单读正文 / 写新 gate 的读者须以**本则**为准 ——
(先例教训:`test/test_cep_simulation_mode_lock.cc` 那个名为「not-collected 契约存在」的用例
**只断言 reason 字段形状、不断言值契约** ⇒ 给缺口盖章 ⇒ 本表的作用就是**消灭这类「按旧文本建 gate」的入口**)。

| 位置 | 裁决前文本(历史) | 本则的裁决后口径 |
|---|---|---|
| 本 ADR 决策 4 的 JSON 示例(`:92-101`) | `"_not_collected_reason": "trace.h5 missing p99 bin"`(单字符串) | §②:改用 **per-metric `_not_collected` map**;单字符串字段**废弃** |
| 本 ADR 决策 2.2 可选 metric 行(`:196`) | 「未达成 = `not-collected`」 | §②:**同样适用于 13 必纳 metric**(取值类型放宽为「数值 **或** `"not-collected"`」) |
| 本 ADR `INV-CEP-06-IC4`(`:133`) | 「不可观测 metric 标 `not-collected` + reason 字段,禁止省略/0/-1」 | §②:**载体**由本则给出(值即类型 + per-metric map);**语义不变** |
| 本 ADR 决策 5(`:103-108`) | 两条分支 | §③:**追加第 3 条**(timing + 预声明不采延迟 ⇒ 必须 `not-collected`,禁写 0) |
| `07 号` `:285`(IC4 摘录) / `:521`(兜底契约段) | 同上旧形状 | 同上(07 号 Status Update #2 同步) |
| `openspec/specs/cep-core-contracts/spec.md` `:116` / `:121` | 旧形状 + IC3 无「不适用」分支 | §②/§④:**须与新契约对齐**(§⑤ 已列入 builder 域 + 冻结面授权) |

**⑦ 关联**: 台账 **Entry #215** / **#216** · `07 号` Status Update 2026-11-04(同一裁决)·
`experimental/metrics.schema.json` · `tools/{render_report,ab_compare,run_sweep}.py` ·
`test/test_cep_simulation_mode_lock.cc` · `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`

## Status Update 2026-11-04(#2)—— 预登记 out-of-scope 集合的**机器可读载体**裁决 + §④「结构性保证」的**解释性 gloss**

**主题**:SU#1 §④ 的 run-validity 判据④要求「`not-collected` 集合 **⊆** 实验设计**预登记**的 out-of-scope 集合」,
但**未指定该集合的载体** ⇒ builder 域无法实现该闸(tools 现有约定只读 `experimental/`,**从不**读 `.rddf/` 下的路线图层)。
本则裁决**载体形态**,并给出 SU#1 `:296` 一句必要的**解释性 gloss**。
⛔ **本则不改 SU#1 的任何既有裁决**,只**追加**载体与 gloss。

**性质**:architect 域的架构级裁决(per 台账 **Entry #217** ⑥ 动作 #1 的落地配套)。
**关联**: 本文件 **SU#1** · `07 号` Status Update 2026-11-04(#2) ·
`objective §9.1`(判据-指标需求表 · **预登记真源**)· `scripts/test/cep_freeze_check.sh`(`FROZEN_GLOBS`)

---

### ① 载体裁决:**真源 / 机器投影**双层(复用仓内真源原则 + M1 冻结机制)

| 层 | 载体 | 域 | 角色 |
|---|---|---|---|
| **真源**(人读 + 评审面) | `objective §9.1` 的**预登记 out-of-scope 表** | planner / architect(**评审过的设计面**) | **不动** —— 本则不迁走它 |
| **机器投影** | **`experimental/preregistration.json`**(逐假设 `hypothesis_id` → `out_of_scope` 集 + `status: registered \| pending`) | builder 消费,**冻结面** | tools 判据④ 的**读本** |

**为什么投影放 `experimental/` 顶层是安全的**:emit 只写 **run 子目录**
(`test/test_cep_experiment_emit.cc` 的 `create_directories(out)`,`out` = `$CEP_EXP_DIR`)
⇒ **结构上碰不到**顶层文件;而人类改动被下面的门禁 ①② 双catch。
先例:`experimental/metrics.schema.json` 一直在该目录且**已在** M1 冻结集内。

**⛔ 为什么不是「run 自报 out-of-scope」**(即把集合写进 `simulation_config.json`):
`simulation_config.json` 与 `metrics.json` 由**同一个 emit 进程背靠背写出**
(实测 `test/test_cep_experiment_emit.cc` 的两个 `ofstream` 相邻)⇒ 按 SU#1 §④ 的 rationale,
二者**同属「被试者自证的报表面」家族** ⇒ run 自报 out-of-scope ⇒
「`not-collected` 冒充预声明」这条自欺升级通道**形同虚设**。
⇒ 且即便加了外部 ⊆ 约束,run 自报字段也**零信息量**(外部表已能推得该集合)⇒ 纯增一个可伪报面。

**⛔ 为什么不是「tools 直接读 `objective §9.1` 的 markdown 表」**:
① 破既有分层(tools 只读 `experimental/` + 自有 `policy_threshold.json`);
② objective 是**台账散文**,非机器可读格式 ⇒ tools 要写 markdown 解析器;
③ ⚠️ **最坏后果 = 校验静默失效** —— planner 域改散文(该文件自身有 append-only churn)
⇒ tools 解析到错误表格 ⇒ 闸**常绿**而无人察觉。

### ② 三道配套门禁(缺一 ⇒ 投影可被绕过)

| # | 门禁 | 落点 | 防什么 |
|---|---|---|---|
| **①** | `experimental/preregistration.json` **加入 `FROZEN_GLOBS`**(`scripts/test/cep_freeze_check.sh`)⇒ 改动须 `--update --authorized-by <台账 entry>` | builder 域 | **防「人类事后改表」** —— 签发者 = architect/planner 的台账裁决,授权链与 M1 全体一致(先例:`include/tlm/cep/fabric.json` **派生刷新型**成员) |
| **②** | **drift 校验**(新 script + ctest 注册,先例 `cep_decl_alignment_check`,`test/CMakeLists.txt:553`):解析 `objective §9.1` ② 表 ↔ JSON,**逐假设集合相等** ⇒ 任一侧单改即 red | builder 域 | **防「双份漂移」** —— 两闸互为锁:改 JSON 绕过评审 ⇒ ① 的授权记录暴露;改 §9.1 想静默生效 ⇒ ② red |
| **③** | **emit whitelist 保持闭合**:实测 `CEP_EXP_ID` 只认识 `h2`/`h3`/`h6`(`test/test_cep_experiment_emit.cc`)⇒ **H4 / H8 在受监管路径上物理不可产出**;tools 判据④ 对「声称的假设无已登记行」或「行 `status = pending`」**判红** | builder 域 | **防「未预登记先产出」** —— 登记 H4 预登记行的同一 PR 才允许加 `run_h4`,顺序锁 |

**⚠️ 门禁 ③ 的能力边界(坦承,不掩饰)**:闸控制的是 **validity 不是 existence** ——
谁都能手搓一个 run 目录放数字,但不会有 **validity-green** 的报告 ⇒ 产物在**消费侧**被 containment。
这正合本 ADR 的模型(validity 层才是门)。

### ③ ⚠️ SU#1 `:296` 的**解释性 gloss**(字面按读会与已批准的 `objective §9.1` 互斥)

**SU#1 `:296` 原文**:
> ⇒ 结构性保证:同一实验**不能**既把某 metric 预登记为 out-of-scope、又在同一实验的假设判据里引用它。

**⚠️ 问题(实测)**:该句按**字面**读,禁止的正是 `objective §9.1` 已批准的 **H3 型组合** ——
① 表的 H3 判据**引用** `deadlock_violations`(判据的 deadlock 分支),② 表的 H3 **预登记**它为 out-of-scope。
若 builder 按字面实现闸,会写出与已批准 §9.1 **直接互斥**的判据。

**gloss(本则追加,不改原文)**:
> **该「结构性保证」作用于 **verdict 层**,不是 registration 时或 run-validity 时的禁令** ——
> 即:同一实验**可以**既预登记某 metric 为 out-of-scope、又在假设判据里引用它,
> 但该判据的 verdict **永远** = **「未测量」**,⛔ **永不**产生「成立」,⛔ **永不**计入任何「假设已验证」的计数。
> ⇒ **两级各司其职**:run-validity 层**放行**(该 run 本身合法:预登记过 ⇒ `not-collected` 合法);
> hypothesis-verdict 层**拦截**(该判据未被测量 ⇒ 不继承 pass)。
> ⇒ **两表不矛盾** —— 它们是 SU#1 §④ 两级 verdict 的**各层**,不是同一层的两条规则。

**gloss 的必然性**:若不加此 gloss,「结构性保证」的字面读法会**禁止 H3 型组合**本身
—— 而「判据引用一个未测量的 metric 并如实标『未测量』」**正是本裁决要鼓励的行为**
(诚实标注优于沉默宣称);禁止它 ⇒ 逼出「把未测量写成 0」的旧失败 ⇒ **与 SU#1 的目标相反**。

### ④ 「H4 / H8 ⏳ 待登记」的**禁产出**口径(承 oracle)

- **H4 / H8 在 `objective §9.1` ② 表中登记为 `⏳ 待登记`** ⇒ 其 `preregistration.json` 行
  `status` = **`pending`**(⛔ **不是缺行** —— 缺行 = 「未登记」,pending = 「已登记但禁产出」,两者语义不同)
- ⇒ **预登记必须先于产出**:H4 的运行期装置未建、H8 的 A/B 数据未产 ⇒ 在各自预登记行落地前,
  ⛔ **不得**产出对应 run
- ⇒ 由门禁 ③ 保证(emit whitelist 无 `h4`/`h8` 入口 + tools 对 `pending` 判红)

### ⑤ Verification(前瞻判据 · 可勾选)

- [x] 载体形态已裁(**真源 = `objective §9.1` ② 表不动 / 机器投影 = `experimental/preregistration.json`**)
- [x] **候选「run 自报 out-of-scope」已被判死**,依据 = `simulation_config.json` 与 `metrics.json` **同一 emit 进程**写出(实测)⇒ 同属被试者自证面家族
- [x] **候选「tools 读 objective markdown」已被判死**,依据 = 破分层 + 最坏后果是**校验静默失效**
- [x] 三道配套门禁已定(① 入冻结集 · ② drift 校验 · ③ emit whitelist 闭合 + pending 判红)
- [x] SU#1 `:296` 的 **gloss 已给**,且明确了「两级各司其职」的分工
- [x] `pending` vs 「缺行」的语义区分已定
- [x] 门禁 ③ 的能力边界已**坦承**(validity 不是 existence)
- [x] `experimental/preregistration.json` 已建 + 已入 `FROZEN_GLOBS`(builder 域 + 冻结面授权)—— 2026-11-05,per 台账 **Entry #218** §⑤-2
- [x] drift 校验 script + ctest 已落地(逐假设集合相等 + **4** 条反向 fixture:改投影 / 改真源 / `pending`→`registered` / emit 白名单越界)
- [x] tools 判据④ 已实现(读投影 + ⊆ 校验 + pending/无行判红 + **反向断言**:预登记 out-of-scope 者写 0 ⇒ 判红)
- [x] `objective §9.1` ② 表已加「机器投影位置 + drift 关系」指针行(planner 域)—— 2026-11-04,per 台账 **Entry #218**

**⑥ 关联**: 本文件 **SU#1**(§④ 的两级 verdict 与预登记闸)· `07 号` Status Update 2026-11-04(#2) ·
台账 **Entry #217** ⑥ 动作 #1 / #2 · `objective §9.1`(判据-指标需求表 · **预登记真源**)·
`scripts/test/cep_freeze_check.sh`(`FROZEN_GLOBS`)· `test/test_cep_experiment_emit.cc`(emit 侧实测)·
`include/tlm/cep/fabric.json`(**派生刷新型**冻结成员先例)· `test/CMakeLists.txt`(drift 校验的 ctest 注册先例)
