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

---

## Status Update 2026-11-05(#3)—— 违例类 metric 的「**设计预期违例**」载体裁决 + IC3 报告层**三态**规范（SU#1 §④ 的落地配套）

**主题**:SU#1 §④ 允许违例类为 `"not-collected"` 并要求「**报告层须渲染对应 badge**」,但落地时暴露 **2 个未裁决点** —— 本则裁之。

**性质**:architect 域的架构级裁决(per 台账 **Entry #224**)。
**关联**: 本文件 **SU#1**(§④ 的违例类规则 + B4 读取约束)· **SU#2**(预登记载体 + 三道门禁 + 自证面家族)·
`07 号` Status Update 2026-11-04(#1)(同一规则的真源 1 同步)· 台账 **Entry #222** / **#223** ·
`docs/validation/2026-11-05-cep-h4-completion-domain-experiment.md` · `docs/validation/2026-11-05-cep-h1-h5-conclusion-registration.md`

⛔ **本则不改本 ADR 正文**(ADR 不可变纪律),只**追加** SU。

---

### ① 问题(本则的根因 —— 两个未裁决点)

**问题 1:报告层 badge 只有两态 ⇒ 对未测量 metric 作出**被禁止的肯定断言****

SU#1 §④ 明文要求:
> not-collected ⇒ IC3 判定「不适用」+ 该 run 不得作一致性安全断言的证据 + **报告层须渲染对应 badge**

但 `tools/render_report.py` 现有 violation badge **只有两态**:
- `:307` `<div class="bad">❌ 一致性违例(实验 fail)`
- `:310` `<div class="ok">✓ 一致性闸门通过(deadlock_violations == 0 且 memory_violations == 0)`

⇒ **实测(2026-11-05)**:违例类为 `"not-collected"` 的 run(h4 与 h6)经 `consistency_violations()` 得 `violations = []`
⇒ 报告渲染 **「✓ 一致性闸门通过」** ⇒ 对**未测量**的 metric 作出**肯定的一致性安全断言**。

⚠️ **这不是「badge 未实现」的实现欠账**,而是**方向相反的断言**:SU#1 §④ 禁止的正是这条断言
⇒ 属「与已签批 ADR 冲突且未走取代流程」(per 台账 **Entry #224** 的 BLOCKER 认定)。

**问题 2:「设计预期违例」的载体未裁决**

H4 的**核心证据**是**反事实负控**(coherence 被错标 group-scoped ⇒ 死锁)——
per `ADR-CEP-01 §6.2` 代价 3「分类错误 = 静默死锁」,这是 H4 假设的**因果支**。
但「违例类槽位」的语义尚未裁决 ⇒ 两个坏选项:
- **进槽位** ⇒ `deadlock_violations = 1` ⇒ tools 的 IC3 闸把 h4 判成「实验 fail」⇒ **掩盖正控结论**;
- **丢弃** ⇒ H4 只剩正控 ⇒ 退化为「两类流同实验没死锁」= **H6 换名** ⇒ **判据的必要性支消失**。

---

### ② 裁决

**裁决 A —— 报告层三态(规范本体)**

| 态 | 触发条件 | 渲染文本(⛔ 不得含「通过」字样,除非确为通过) |
|---|---|---|
| **✓ 通过** | 两个违例类(`deadlock_violations` / `memory_violations`)**均实测**为 `0` | `✓ 一致性闸门通过(deadlock_violations == 0 且 memory_violations == 0)` |
| **❌ 违例 fail** | 任一违例类**实测** `> 0` | `❌ 一致性违例(实验 fail)` + 明细 |
| **⚪ 不适用** | 任一违例类 = `"not-collected"` | `⚪ IC3 不适用:违例类未采集 ⇒ 本 run 不作一致性安全断言的证据` |

⛔ **三态互斥且**「不适用」**优先于「通过」**:任一违例类未采集 ⇒ **禁止**渲染「通过」字样
⇒ 落实 SU#1 §④ 的「不得作一致性安全断言的证据」在**报告层**的可判定形式。
📌 实现方式 = **扩展**既有 violation badge 机制(SU#1 §④ 明示「**不新建**」)⇒ 三态复用同一渲染点。

**裁决 B —— 违例类槽位语义 + 「设计预期违例」的载体**

- **槽位语义**:违例类槽位 = **本 run 实测违例数**。⛔ **设计预期违例不进槽位**。
- **锚定双件套**(设计预期违例的唯一合法证据形态):
  1. **(i) 同 PR 机器测试** —— 负控断言 + **反向 fixture**;并加**产出层硬约束**:
     「**负控红不了 ⇒ 装置不合格 ⇒ ⛔ 不产出 run、不登记结论**」;
  2. **(ii) `verification_scope` 散文指针** —— ⚠️ **显式承认:散文属 SU#2 §① 的「被试者自证面」家族**
     ⇒ **仅作指针,⛔ 不得单独作证据成立**。
- ⇒ **散文不可校验不构成缺陷**(因 (i) 提供独立机器锚)⇒ 但**若 (i) 缺失 ⇒ 本裁决的载体不成立**。

**裁决 C —— 本则与既有裁决的关系**

⛔ 本则**不改** SU#1 / SU#2 的任何既有裁决;只**追加** ②A / ②B 的载体规范 + ③ 的代价 + ⑤ 的验证判据。

---

### ③ 选项对比表(≥ 3 行,每行含结论 + 理由)

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| 1 | **报告层** 保持两态 + 依赖 `problems` 列表为空来渲染「通过」 | ❌ **否决** | 对未测量 metric 渲染肯定断言,**直接违反 SU#1 §④**;实测 h4/h6 命中 ⇒ 属方向性错误,非措辞问题 |
| 2 | **报告层** 三态(✓ 通过 / ❌ 违例 / ⚪ 不适用),「不适用」优先 | ✅ **采纳** | 落实 SU#1 §④ 的两个分句(IC3 不适用 + 报告层 badge);**扩展**既有 badge 机制(SU#1 明示不新建);三态互斥可判定 |
| 3 | **设计预期违例** 进 `deadlock_violations` 槽位 | ❌ **否决** | IC3 闸把 h4 判成「实验 fail」⇒ **掩盖正控结论**;且「预期违例」与「实测违例」同槽 ⇒ 槽位语义歧义 |
| 4 | **设计预期违例** 不进槽位,锚定 = 同 PR 机器测试 + 散文指针 | ✅ **采纳** | 槽位语义单一(= 本 run 实测);机器锚独立于散文 ⇒ 证据不落在自证面;⛔ 散文仅指针 |
| 5 | **设计预期违例** 丢弃(只做正控) | ❌ **否决** | H4 退化为 **H6 换名** ⇒ 必要性支消失 ⇒ 装置不合格(per Entry #222 ③ 的「负控是装置合格的前提」) |
| 6 | 新增伴生字段(如 `_designed_violations` map)承载设计预期违例 | ⛔ **不采纳(留待触发)** | 需 schema + 3 件 tools + main spec 三处同步 ⇒ 新增契约面字段;当前 (i) 已提供独立机器锚 ⇒ **无必要性**;若将来出现「单 run 多预期违例」需求 ⇒ 再立 ADR 追加 |

---

### ④ 后果 / 代价

**正面**:
- SU#1 §④ 的两个分句(IC3「不适用」+「报告层 badge」)在**报告层**有了可判定形式 ⇒ 报告不再对未测量 metric 作肯定断言;
- 违例类槽位语义单一(= 本 run 实测)⇒ 「实测违例」与「设计预期违例」不再混槽;
- 「设计预期违例」有了**明确载体 + 产出层硬约束** ⇒ 未来 emitter / tools 读者不会误把负控死锁写进槽位。

**负面 / 代价**:
- `tools/render_report.py` 属 **M1 冻结面** ⇒ 改动须 `--update --authorized-by`(per 台账 Entry #224 的授权链);
- 「三态」比「两态」多一个分支 ⇒ 报告文案与断言路径须同步改 ⇒ **实现面 2 处**(badge 渲染 + `consistency_violations()` 的提前返回结构);
- ⚠️ **连带洞**:现 `consistency_violations()` 在 `deadlock_violations == "not-collected"` 时**提前 return** ⇒ 跳过 `memory_violations` 检查 ⇒ 构造 `dv=not-collected ∧ mv=5` 的 run 会**漏报真实违例** ⇒ 改三态时须一并把提前 return 改为**不提前 return**(各分支独立判定)。
- ⛔ **本则不新增契约面字段**(选项 6 留待触发)⇒ 不扩大 M1 冻结面。

**风险**:
- 「不适用」态可能被下游误读为「通过」⇒ 缓解 = 渲染文本**必须**含「不作一致性安全断言的证据」这一分句,且**禁止**出现「通过」字样。

---

### ⑤ Verification(可勾选验收判据 —— 前瞻方法)

- [x] 报告层三态已实现:违例类任一 = `"not-collected"` ⇒ `report.html` 渲染 **⚪ 不适用** 且**全文不含**「一致性闸门通过」*(实测:官方 4 组 h2/h3/h4/h6 的 `report.html` 全部 ⚪ 且 grep「一致性闸门通过」零命中)*
- [x] 违例类均实测 `0` ⇒ 渲染 **✓ 通过** — ⚠️ **本项措辞已被本文件 SU#3 勘误 §③ 订正**:「既有 h3 / h6 行为不回归」**有误** —— 两违例类至少一个未采集 ⇒ h2/h3/h4/h6 此前渲染「通过」本身是错的,修复后全渲染「不适用」是**纠正非回归**;「✓ 通过」态当前**无真实 run 实例** ⇒ 由合成 fixture **F1** 覆盖(防死代码,实测通过)
- [x] 违例类任一实测 `> 0` ⇒ 渲染 **❌ 违例 fail***(实测:F3 fixture + H8 的 g1b/g2b 真实 run 均渲染「❌ 一致性违例(实验 fail)」)*
- [x] 「不适用」态**优先于**「通过」(`dv=not-collected ∧ mv=0` ⇒ ⚪ 而非 ✓)*(命中 fixture F2)*
- [x] 连带洞已修:`dv="not-collected" ∧ mv=5` 的 fixture ⇒ **报告出** memory 违例(不得漏报)*(命中 fixture F4;check 路径不再短路)*
- [x] `consistency_violations()` 的提前 `return` 已移除(各违例类独立判定)*(代码实测;F4 双路径同红)*
- [x] emit 产出层硬约束已落地:**负控红不了 ⇒ ⛔ 不产出 run、不登记结论**(机器断言)*(`EmitResult.fatal` + `device_qualified()`;`[cep][h4]` 含 `REQUIRE_FALSE(r.fatal)` 前置)*
- [x] `verification_scope` 的**自证面**性质已在 validation 文档与本则中**显式承认***(本则 §②B(i) + `docs/validation/2026-11-05-cep-h4-completion-domain-experiment.md` §7 注)*
- [x] `M1` 冻结面 `--update --authorized-by` 完成 + hash / provenance 双绿*(实测:内容 commit `ae11e0f1` → `--update --authorized-by "Entry #224"` → manifest commit `83f11700`;41 产物 hash + provenance 双绿)*

---

### ⑥ 关联

本文件 **SU#1**(§④ 违例类规则 + B4 读取约束 + §⑥ 勘误指针表)· **SU#2**(§① 自证面家族 + ② 三道门禁)·
`07 号` Status Update 2026-11-04(#1)(同一规则的真源 1 同步)· `ADR-CEP-01` **§3.2** / **§6.2** 代价 3 ·
台账 **Entry #224**(本则的授权与落地台账)· **#223**(H4 装置落地 + 报告面暴露本问题)· **#222**(H4 形态裁决 + 负控是装置合格的前提)·
`tools/render_report.py`(:52-54 提前 return · :286 死变量 · :307-310 两态 badge)· `test/cep_experiment_harness.hh`(`well_formed()`)·
`test/test_cep_experiment_emit.cc`(`run_h4` · emit whitelist)· `experimental/preregistration.json`(预登记投影)·
`docs/validation/2026-11-05-cep-h4-completion-domain-experiment.md` · `docs/validation/2026-11-05-cep-h1-h5-conclusion-registration.md`

---

## Status Update 2026-11-05(#3 勘误)—— 三态**优先级**澄清 + §⑤ 判据措辞订正

**主题**:本则(SU#3)落地时的**合成 fixture** 暴露了 §②A 表格的一处**歧义**,且 §⑤ 的一条判据**措辞有误** ⇒ 本勘误订正之。
**性质**:architect 域的**勘误**(⛔ 不改 SU#3 的任何既有裁决文字,只**追加**澄清与订正)。
**关联**:本文件 **SU#3** · 台账 **Entry #224** · `scripts/test/cep_ic3_report_states_check.sh`

---

### ① 三态**优先级**澄清(§②A 表格的歧义订正)

§②A 的表格按「行」给出触发条件,但在 `dv = "not-collected"` ∧ `mv = 5` 这类**混合**取值下,
「❌ 违例 fail」(任一违例类**实测** > 0)与「⚪ 不适用」(任一违例类 = `"not-collected"`)
**两行同时命中** ⇒ 需明确优先级。

**裁定:优先级 = 「违例」> 「不适用」> 「通过」** —— 即:

| 序 | 判定 | 理由 |
|---|---|---|
| 1 | **违例 fail** | 若**任一**违例类存在**实测违例**,⛔ **必须报出来** —— 「不适用」**不得掩盖**真实违例 |
| 2 | **不适用** | 无实测违例,但任一违例类未采集 ⇒ 该 run 不作一致性安全断言的证据 |
| 3 | **通过** | 两违例类**均实测**为 0 且无非法/缺失 |

⚠️ **「不适用」只压「通过」,不压「违例」**。
⇒ **实测依据**(合成 fixture **F4**:`dv=not-collected ∧ mv=5`):修复前该 run 被渲染为「⚪ 不适用」
⇒ **真实违例 `mv=5` 被掩盖** ⇒ 修正优先级后渲染为「❌ 一致性违例(实验 fail)」并给出明细。

⚠️ **连带修复(同源)**:`tools/render_report.py` 的 **check 路径**此前以 `_ic3_na` **短路整个**一致性检查
⇒ `dv=not-collected ∧ mv=5` 在 check 路径**不报**、在 render 路径**漏报** ⇒ 两处**同源**修复:
① `consistency_violations()` 移除提前 `return`(两违例类**各自独立判定**)· ② check 路径**不再短路**。

### ② §⑤ 判据措辞订正(「既有 h3 / h6 行为**不回归**」一条)

**原文**(§⑤ 第 2 条):
> - [ ] 违例类均实测 `0` ⇒ 渲染 **✓ 通过**(**既有 h3 / h6 行为不回归**)

⚠️ **该措辞有误** —— 订正为:

> - [ ] 违例类均实测 `0` ⇒ 渲染 **✓ 通过**
> - [ ] ⚠️ **h2 / h3 / h4 / h6 的渲染** **不得**停留在「✓ 一致性闸门通过」
>       (实测:h2 两违例类均未采集 · h3 `deadlock_violations` 未采集 · h4/h6 `memory_violations` 未采集
>        ⇒ 四者**均应**渲染 **⚪ 不适用**;此前渲染「通过」= 对**未测量** metric 的**肯定断言**
>        ⇒ 修复后的变化是**纠正**,⛔ **不是**回归)

⚠️ **推论**:当前 4 个真实 run **无一**落在「✓ 通过」态 ⇒ 该分支若无合成 fixture 即为**死代码**
⇒ 由 `scripts/test/cep_ic3_report_states_check.sh` 的 **F1**(dv=0 ∧ mv=0)覆盖。

### ③ Verification(可勾选验收判据 —— 前瞻方法)

- [x] 三态优先级 = **违例 > 不适用 > 通过**(F4 红转绿)*(F4 实测:修复前渲染「⚪ 不适用」掩盖 `mv=5`,修复后渲染「❌ 一致性违例(实验 fail)」)*
- [x] 合成 fixture **4 条**全绿(F1 通过 / F2 不适用 / F3 违例 / F4 连带洞)*(`bash scripts/test/cep_ic3_report_states_check.sh` rc=0)*
- [x] `cep_ic3_report_states_check` 已注册进 ctest*(`ctest -N` 实测 #134)*
- [x] h2 / h3 / h4 / h6 的 `report.html` **均**渲染 **⚪ 一致性闸门不适用**且**全文不含**「一致性闸门通过」*(官方 4 组重跑实测:四者全 ⚪、`grep '一致性闸门通过'` 零命中)*
- [x] `consistency_violations()` 的提前 `return` 已移除;check 路径**不再**因 IC3 不适用而短路*(代码 + F4 双路径)*
- [x] emit 产出层硬约束已落地(**负控未死锁 ⇒ ⛔ 不产出 run、不登记结论**)*(`EmitResult.fatal` + `[cep][h4]` 断言)*
- [x] `M1` 冻结面 `--update --authorized-by "Entry #224"` 完成 + hash / provenance 双绿*(实测:内容 commit `ae11e0f1` → `--update` → manifest commit `83f11700`;双绿)*

### ④ 关联

本文件 **SU#3**(§②A 报告层三态 + §②B 设计预期违例载体 + §③ 选项对比 + §④ 后果/代价)· **SU#1**(§④ 违例类规则 + B4 读取约束)· **SU#2**(§① 自证面家族)·
台账 **Entry #224** · `scripts/test/cep_ic3_report_states_check.sh`(合成 fixture 4 条)· `tools/render_report.py`(三态 + 连带洞 + 死变量清理)·
`test/CMakeLists.txt`(ctest 注册)· `07 号` Status Update 2026-11-04(#1)(三态未触发同步)

---

## Status Update 2026-11-05(#4)—— B4 达成证据的**载体裁决**:装置补全,不重裁验收口径

**状态**:Accepted(2026-11-05)· **日期**:2026-11-05 · **作者**:CppTLM build agent(architect mode)
**审稿报告**:`.rddf/reviews/ADR-CEP-06-SU4-b4-evidence-carrier.md`(oracle 2 轮:2 BLOCKER 全闭合;critic 1 轮:APPROVE_WITH_CONDITIONS,0 BLOCKER)
**作用域**:`objective §6.2 B4` 的**达成证据形态**(B4 本身的验收条款**不改**)
**关联 change**:`backlog: cep-b4-evidence-device`(待 planner 立项;本则 §② 的六条硬约束 + §⑤ 的验收判据作为该 change 的 AC,立项后回填指针)
**关联**:台账 **Entry #225 ⑧**(发现「B4 结构性不可达」+ 二选一路由)· **#217**(B4 两级口径)· 本文件 **SU#1 §④** / **SU#3**(三态 + 勘误)· `objective §9.1 ①` H3 **分支 2**

### ① 决策

**走装置补全:把 h3 实验装置升级为「一致性 × 完成域」复合装置,使**单 run 同时实测两类违例**;⛔ 不重裁 B4 的验收口径。**

- **载体 = h3 装置升级**(⛔ 不新增 `run_hN`,不新增假设行):B4 的缺口(两违例类必须实测)**与**
  `§9.1 ①` H3 分支 2(`deadlock_violations` 未测量)是**同一个缺口** ⇒ 一处装置升级收口两处;
- `memory_violations` 沿用既有 `SwmrDataValueDetector`(h3 域);`deadlock_violations` 必须由
  **真实在途窗口 + 阻塞机制**产生(⛔ 不得挂一个恒不触发的 watchdog 空转);
- **⚠️ 强制配套(缺此项 ⇒ 本决策不成立)**:升级后 h3 实测 `deadlock_violations` ⇒ H3 预登记行**必须收缩**
  (`deadlock_violations` 移出 H3 的 out-of-scope 集):落点 = `objective §9.1 ②` H3 行 **与**
  `experimental/preregistration.json` H3 行(**双源同改**,drift 闸两闸互为锁;JSON 入 M1 冻结集 ⇒
  `--update --authorized-by "<台账 Entry>"`)。**授权 entry 的 kind = `scope-change`**(预登记集合收缩;
  per `§11` kind 集合 —— ⚠️ 它与「planner 立项 entry」**不是同一授权**,后者只是取材料建 change 的动作记录),
  冻结 `--update` 授权与 `scope-change` **同行登记**。⛔ **授权落地前不得产出升级后的 run**(该顺序是**流程闸**,
  ⚠️ 机器闸拦不住它:`deadlock_violations` 一旦在未收缩的预登记下被**实测**,drift 闸的 `not-collected ⊆ out-of-scope`
  方向**恒过** ⇒ 没有机器兜底,只有落地顺序)
  (否则产出与已评审的设计声明互斥 —— 正是预登记闸要防的「设计声明 vs 实测面脱节」失败模式)。
  ⚠️ 该收缩的语义 = **修复 H3 的「判据-预登记脱节」**(H3 的 `§9.1 ①` 表**本就登记了两条分支**,先例:
  H6 的 2026-11-05 预登记补正,Entry #223 ④),⛔ 不是放宽。
- **B4 的达成证据形态(本则定死,不可再放宽)**:单个 run 同时满足
  (i) run-validity 绿(13 键齐备;非违例类 `not-collected ⊆ H3 预登记`);
  (ii) **两违例类均实测 0**(⛔ 不是 `not-collected`,⛔ 不是跨 run 继承);
  (iii) `render_report --check` 绿 **且** `report.html` 渲染 **✓ 一致性闸门通过**
  —— 这也是 IC3 三态中「✓ 通过」的**首个真实 run 实例**(闭合 SU#3 勘误 §② 承认的「该态无真实实例」缺口)。

### ② 非空洞性硬约束(本则的核心 —— 缺一即不成立)

| 序 | 约束 | 判定锚 |
|---|---|---|
| 1 | **在途窗口真实存在** | 装置单测断言:正控 run 的 `in_flight` 峰值 > 1 且 `max_idle_run > 0`(否则 watchdog 的观测面为空) |
| 2 | **检测力由负控证明(两个检测器各自红一次)** | 错标完成域 ⇒ 死锁=1;**另需 SWMR 注入 ⇒ `memory_violations > 0`**;emit 层硬约束(同 H4 的 `device_qualified` 模式):负控不红 ⇒ **拒绝产出** |
| 3 | **0 是实测 0** | 两违例类必须出现在 `_measured` 集,且 `_not_collected` 无该键(预登记闸 + metrics 双查) |
| 4 | **不得以空检查充数** | 若实现中发现阻塞机制**无法真实构造**(死锁面物理上不可能发生)⇒ **停手回报**,由 architect 重裁(转向路径 ② 并**显式登记降格**),⛔ 不得静默勾 B4 |
| 5 | **同一事务流(防「两件实验穿一件风衣」)** —— ⛔ 不得把「一致性段」与「完成域段」拆成两段互不相交的独立跑再拼进同一 run | **机器锚**:两检测器的观测键集合**交集非空**(同一地址/节点集合上的读写事务同时被 SWMR/data-value 与完成域阻塞覆盖);复合 run 的 trace 校验该交集,为空 ⇒ **拒绝产出** |
| 6 | **耦合探针(防「阻塞平凡化一致性测量」)** —— 若阻塞串行化事务,SWMR 观测窗会被收缩 ⇒ `memory_violations=0` 可能是阻塞的人造物 | **机器锚**:阻塞**开/关**两配置下(两份独立 `metrics.json`,非同一 run 改标)`memory_violations` 的计数差**无不解释漂移**(或漂移方向与**预测**一致 —— ⚠️ 预测须在 run 产出**前**写入 `verification_scope`,`after-the-fact` 补写不成立,时间戳可查);散文只能作**指针**,⛔ 不得单独成证据(对齐 SU#3 §②B 的载体等级纪律) |

### ③ 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **① 装置补全**(采纳) | ✅ **采纳** | 满足 B4 的**字面**条款(不重裁验收口径);B4 缺口与 H3 分支 2 缺口**同源** ⇒ 一处收口;产出首个真实「✓通过」实例;成本有界(装置住 `test/`,零契约面改动、零新假设、白名单正则不变) |
| ② 重裁 B4 口径(群集级覆盖:memory 由 h3 测 0、deadlock 由 h6/h4 测 0) | ⛔ **不采纳** | 把**已写死的验收条款**改成适配当前实现 = `07 号` Status Update 2026-11-04 ① 禁止的「**DoD 降格后仍宣称达成**」;且它**回答不了**「单个 run 能否同时承载两类违例实测」这个契约级问题 —— 该问题在 ① 中可低成本被回答 ⇒ 无降格必要 |
| ③ 维持现状(B4 永久登记「结构不可达」) | ⛔ **不采纳** | 把可修复的能力缺口永久化;H3 分支 2 的「未测量」会长期作为 `⛔ 不得继承 pass` 的脚注,每次引用都要携带一条永久缺陷说明 |
| **④ 新增伴生入口**(新假设行 + `run_h3c` 类入口;B4 证据绑新行,不碰 H3 行) | ⛔ **不采纳** | 它的真实收益 = **零语义漂移**(H3 行/装置全不动);真实代价 = 新预登记行 + 白名单正则变更(`run_h3c` 类入口**不匹配** `run_(h\d+)\s*\(` ⇒ 须扩正则,属 Entry #224 ⑤ 警示的**静默漏检**风险类)。**关键反驳答**:「原位升级 = 把复合实验语义并回 H3 = 漂移源」不成立 —— H3 的 `§9.1 ①` 表**本就登记了两条分支**(`memory_violations` 与 `deadlock_violations`),分支 2 未测量是**实现缺口**而非设计边界;原位升级**闭合的是 H3 已声明的判据**,且升级后的 v2 run 会**同时重证分支 1 的数值**(swmr=0/data_value=0,同 1200 事务)⇒ 不是「换掉 H3」而是「补齐 H3」;而 ④ 会把 H3 分支 2 永久留空 ⇒ 与 H3 已登记的判据自相矛盾 |

### ④ 后果 / 代价

**正面**
- B4 获得唯一合规证据形态;H3 结论完整性提升(分支 2 可实测,分支 1 由 v2 run **同步重证**);IC3「✓通过」态由合成 fixture 升级为有真实实例;
- **冻结面**的**机制**零变(`metrics.schema.json` / 13 键语义 / 预登记三道门禁 / drift 闸),唯一内容变更 = **H3 行收缩**(本条 §① 强制配套,须走冻结授权链)。

**负面**(必须承认)
- h3 装置复杂度上升(一致性不变量 × 完成域阻塞面合流),`verification_scope` 需同时声明两个维度、散文变长;
- 装置形态**换版**(v1 2026-11-04 → v2 复合)⇒ 旧 run 产物(`experimental/` 不入库)不可原样复现:
  ⇒ 缓解 = ① `verification_scope` 带**装置版本标签**;② `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` 的
  H3 分支 1 结论**只勘误不重跑**,注明「数值由 v2 同参数 run 重证」⇒ 可追溯性由**重证**承担,不靠归档旧产物。

**风险**
- **空洞达成**(最大风险):为过 B4 补一个永不触发的 watchdog ⇒ §② 的硬约束专堵;触发即**停手回报**;
- **语义耦合**:两机制共享同一事务流,阻塞可能变形 SWMR 测量窗 ⇒ 声明测量窗独立性(`verification_scope` 指针)**且**耦合探针机器锚通过(§② 约束 6;⛔ 散文声明不足);探针不过 ⇒ 停手回报。

### ⑤ Verification(可勾选验收判据 —— 前瞻方法)

> **回填(2026-11-05,per 台账 Entry #228 + AC-9 评审)**:12 条逐条证据 =
> `docs/validation/2026-11-05-cep-b4-evidence-device.md`(§3 实测 / §6 AC 映射)·
> `.rddf/reviews/ADR-CEP-06-SU4-ac9-implementation-review.md`(第 12 条 = AC-9 PASS)·
> `[cep][b4]` 6 cases / 49 assertions · ctest 的 `cep_b4_observation_keys_check` / `cep_b4_coupling_probe`。

- [x] 升级后 run 的 `deadlock_violations` 与 `memory_violations` **均在 `_measured`** 且实测 `0`
- [x] 负控:错标完成域 ⇒ 死锁=1;SWMR 注入 ⇒ `memory_violations > 0`(**两检测器各自红过一次**);负控不红 ⇒ emit 拒绝产出
- [x] 正控 `in_flight` 峰值 > 1 且 `max_idle_run > 0`(watchdog 观测面非空,⛔ 非恒假)
- [x] `render_report --check` 绿 **且** `report.html` 含「✓ 一致性闸门通过」(首个真实实例)
- [x] 升级后 run 的 `not-collected ⊆ H3 预登记`(`cep_prereg_drift_check` rc=0)
- [x] **同一事务流**:两检测器的观测键集合**实际落 trace**(前置:不落盘则交集校验无从落地)+ 交集非空;交集为空/拆分两段 ⇒ emit **拒绝产出**(含负向 fixture)
- [x] **耦合探针前置**:「阻塞开/关」= 两份独立 `metrics.json`(⛔ 非同一 run 改标);预测句落盘时间戳 **早于** run 产出时间戳
- [x] **耦合探针**:阻塞开/关两配置下 `memory_violations` 无**未解释**漂移(§② 约束 6)
- [x] **预登记收缩先行**:H3 行(真源 ② + JSON 双源同改,含冻结授权)于升级 run 产出**之前**完成;drift 闸 rc=0
- [x] **装置版本标签**:v2 run 的 `verification_scope` 含形态/版本标签;`2026-11-04` 验证文档的 H3 分支 1 只勘误不重跑(注明由 v2 重证)
- [x] H3 ① 表分支 2 由 planner 更新为实测结论;`§6.2 B4` 由 planner 回填 ✅(**本则不预先宣告**)
- [x] 实现评审(architect):装置含真实阻塞机制,**非**「watchdog 恒假 + 默认 0」;否则触发 §② 约束 4 回退;
      回退的**唯一授权链** = architect 经 `objective §11` 的 `adr-amendment` entry 签发(planner 联签),实现侧⛔ 无自行降格出口

### ⑥ 关联

本文件 **SU#1 §④**(两级 verdict + B4 读取约束)· **SU#3** + 勘误(报告层三态;「✓通过」无真实实例)· `objective §6.2 B4` / `§9.1 ①` H3 分支 2 ·
台账 **Entry #225 ⑧**(发现与路由)· **#223**(H4 装置的负控/`device_qualified` 先例)· **#222**(负控是装置合格的前提)·
`test/cep_experiment_harness.hh`(`ProgressWatchdog` / `SwmrDataValueDetector`)· `07 号` Status Update 2026-11-04 ①(禁止 DoD 降格)

---

## Status Update 2026-11-05(#5)—— 读法约束的**传导范围**裁决:「判据」与「扩展观测」的可判定边界(H2 `directory_misses` 归位)

**状态**:**Accepted**(2026-11-05)· **日期**:2026-11-05 · **作者**:CppTLM build agent(architect mode)
**审稿**:`oracle` 1 轮(4 节建议,高影响项全采纳)+ `critic` 1 轮(**APPROVE_WITH_CONDITIONS**,`mergeable: yes` / 0 BLOCKER / 2 SHOULD_FIX 已处置 / 3 NIT)
**作用域**:`SU#1 §④` 的 **hypothesis-verdict 读法**中「⚠️ 任一**判据** ⏭ ⇒ **假设整体**不计」的**传导范围**;
⛔ **不改** B4 的验收口径(run-validity / 违例类规则 / 预登记闸**逐字不动**),⛔ **不推翻** `SU#2 §③`
**审稿报告**:`.rddf/reviews/ADR-CEP-06-SU5-judgement-vs-extension.md`
**关联change**:台账 **Entry #229**(**待 planner 落** —— 本则签发即为其 `adr-amendment` 授权对象;⛔ 冻结 `--update` **须在 Entry #229 实际存在之后**)
**关联**:台账 **Entry #228 ⑤**(冲突登记)· `objective §9.1 ①` H2 行 / **`§9.1 ②` H2 行** / **`§9.1 ③` 读法约束** / `§6.2 B7` · 本文件 **SU#1 §④** / **SU#2 §③** / **SU#4** · 台账 **Entry #218**(事实 #2 的偏好排序)

### ① 问题:未决的是**传导范围**,不是表结构合法性

**先澄清一项既有裁决(⛔ 本则不改动它)**:`SU#2 §③` 已签发 gloss —— `SU#1 §④` 的「结构性保证」作用于 **verdict 层**,
**不是** registration / run-validity 时的禁令;并明确「同一实验**可以**既预登记某 metric 为 out-of-scope、又在假设判据里引用它,
但**该判据**的 verdict **永远** = 『未测量』,⛔ 永不产生『成立』,⛔ 永不计入任何『假设已验证』的计数」。

⇒ **H2 现状的表结构合法**:`directory_misses` 同时出现在「判据」列与 H2 的 out-of-scope 集,**按 SU#2 §③ 是允许的组合**;
且其判据 verdict 如实标 ⏭ 已履行全部义务。⛔ 本则**不**把该组合定性为「缺陷」。

**未决问题(本则真正要裁的末环)**:`objective §9.1 ③` 的读法约束把 ⏭ **从判据层无差别传导到假设层**:

> 「一个假设只要有**任一判据**为 ⏭ 未测量 ⇒ 该假设**整体**不得计为『有结论』」

这产生一个**不可收口**的后果,且与 SU#2 §③ 的粒度**不一致**(SU#2 只说「该判据不计入」,未说「整假设失格」):

| 事实 | 值 |
|---|---|
| H2 的命题(「3 策略 latency 差异可量化」) | **已实测 ✅**(4 / 14 / 18 cycles ⇒ 250% / 28.6% / 350%) |
| `directory_misses` 在该实验中的定位 | **设计上刻意不采** —— Entry #218 事实 #2 的处置是「⛔ 不为它补造采集链路(**判据收窄优先于补实现**)」。⚠️ **两种强度须钉死**:该 ⛔ 的**适用前提**是「`directory_misses` 仍是 H2 的判据」;本则归位后该禁令的**适用对象已不存在**(它不再是判据)⇒ 本则是在**执行**该偏好(判据收窄),⛔ 不是绕过禁令。(绝对版措辞见 `§9.1 ②` 的落地行,同样以「它是判据」为前提) |
| 该判据能否转 ✅ | **不能**(不补产出者) |
| ⇒ 严格传导 | **B7 = 7/8,且不可收口**(除非推翻 Entry #218 的偏好或补产出者,见 ③) |

⚠️ **这不是「H2 没有结论」**:H2 的命题已实测成立;争议点是**一条该实验主动不采的观测是否连带否定整个假设**。

### ② 决策:引入**可判定**的「扩展观测」类别(仅对「判据」之外的观测生效)

**定义(两条**同时**满足才算「扩展观测」;任一不满足 ⇒ 一律按「判据」处理并受传导约束)**

| 序 | 条件 | **操作定义**(消除二义) |
|---|---|---|
| 1 | 不属命题的真值条件 | **「命题陈述」= `§9.1 ①` 表的「假设」列语句**;「判据(逐条)」「成功判据」两列的条目**不属**命题陈述,**除非** architect 个案判定其被命题语义**蕴含**(判定理由须随归位登记) |
| 2 | 不被其他验收条款独立要求实测 | 「独立要求」= `objective §6.x` 明文规定该 metric **必须实测**(现行唯一此类 = **违例类**,per SU#4)。仅「被 13 必纳键集包含」**不算**独立要求(run-validity 允许 `not-collected`) |

**本则适用结论**:H2 的 `directory_misses` 两条均满足 ⇒ 归位为**扩展观测** ⇒ 从「判据」语义下移出
(⛔ **不移出表**、⛔ **不删除该行**),其 ⏭ **未测量登记保留** ⇒ **B7 计数 = 8/8**,
表述须为:**「8/8,其中 H2 的 `directory_misses` 为登记的扩展观测(未测量),不构成 H2 结论判据」**
—— **限定嵌入计数行本身**,⛔ 不得写成「8/8 + 另附说明」。

**四条硬约束(防滑坡)**

| # | 约束 |
|---|---|
| 1 | **禁止降级「实现缺失」类**:若该 metric 的设计定位是判据(已登记为判据 **或** 被其他验收条款独立要求),则**不得**因「当前未测量」而改为扩展观测 ⇒ 唯一出路是补装置。**反例对照 = `SU#4`**:H3 的 `deadlock_violations` 同为 ⏭,但它**绑 B4**(违例类必须实测)⇒ 走**装置补全**,不是降级 |
| 2 | **归位必须留痕且可复核**:须在台账登记 + 表内保留该行(原措辞 + 归位理由 + **命中的两条条件**)⇒ ⛔ 不得删除、⛔ 不得把 ⏭ 写成 0。⚠️ **列头张力**:归位行仍位于「判据(逐条)」列 ⇒ **按该列机械统计「判据数」的工具会被误导** ⇒ 表内须加一行表注声明该行语义非判据 |
| 3 | **对外表述须带限定**:B7 的**每一次**引用(终态表 / roadmap / KPI 报告)须携「H2 的 `directory_misses` 属扩展观测,未测量」⇒ ⛔ 不得表述为「H2 全部观测已测量」。⚠️ **已实测的违例**:`Entry #228 ④` 的表落行写「8/8 已登记」**未携限定** ⇒ 本则落地动作**必须含该行回填** |
| 4 | **禁止「先扩 out-of-scope 再归位」的两步绕道**:凡对**已在判据列登记**的 metric 新增 out-of-scope 包含(即条件 2 的**自我制造**),自动触发**降级嫌疑审查**——举证责任在申请方:须以该行**自拆表起**的表文本证明其为扩展观测,否则走 `SU#4` 式装置补全。⚠️ **为什么授权链堵不住它**:冻结授权链校验的是「**有没有**授权记录」,不校验「该 metric 该不该被预登记」⇒ 它是**流程闸,不是判据闸**(先例:SU#4 对 `deadlock_violations` 做的是预登记**集合收缩**,同样只需一条授权记录;⚠️ 两则改动的**对象不同** —— SU#4 改集合、本则**零 `scope-change`**,故不可类比为「反向操作」) |

### ③ 选项对比

| 选项 | 结论 | 理由 |
|---|---|---|
| **A 归位为「扩展观测」**(采纳) | ✅ **采纳** | **最符合 Entry #218 的原始偏好**(「判据收窄**优先于**补实现」)—— 本则正是执行该偏好,不是绕过它;H2 的命题已实测成立;判定标准可客观判定(两条 + 操作定义);四条硬约束防滑坡 |
| a 从 `§9.1 ①` **彻底移出**该行(不新建语义类别) | ⛔ 不采纳 | 表面更小(§9.1 ③ 一字不改),但**删除表内未测量登记** ⇒ 与 SU#1 决策 4「禁止省略」的表内纪律冲突,且失去可追溯性 ⇒ 退化为「静默删除未测量项」(本系列反复堵的失败模式) |
| b 重开 Entry #218 + **补产出者** + H2 重跑 | ⛔ 不采纳(**记录在案**) | 闭合度最高,但成本 = 推翻已评审偏好 + planner 表改 + 冻结 `scope-change` + 装置实现 + 重跑;而 `directory_misses` **无下游消费者**(B4 只约束违例类)⇒ 在「H2 命题已实测成立」下**成本不成比例**。⛔ **不是**「为目标造证据」(该指控会误伤 SU#4 的装置补全)——真正的问题是**违背已评审的设计承诺** |
| c 维持严格传导(B7 = 7/8) | ⛔ 不采纳 | 该判据在 Entry #218 的**政策**下不可达 ⇒ 7/8 是**政策选择的永久化**,而非物理不可达;且需推翻 `Entry #225` 已过双审的 8/8 登记 ⇒ 程序成本高于本则。⚠️ 若本则不采纳,**7/8 + 显式待办**是可接受的保守路径(不是「不可行」) |
| D 删掉该行(不留痕) | ⛔ 不采纳 | 违反硬约束 2 ⇒ 静默删除 |

**回滚路径(显式 —— 本则签发并落地后若被后续裁决推翻)**

| 步 | 动作 | 域 |
|---|---|---|
| 1 | 追加新 SU 恢复严格传导(「扩展观测」类别废除或收窄) | architect |
| 2 | `objective §9.1 ①` H2 行恢复为判据;`§9.1 ③` 撤回 gloss;`§6.2 B7` 改回 **7/8 + 显式待办** | planner |
| 3 | 处理 `Entry #225` 的 8/8 已登记与 `Entry #229` 的归位登记(**append-only**,追加说明而非改写) | planner |
| 4 | 冻结面重算(`--update --authorized-by <回滚 entry>`) | builder |

⇒ 本则**未落地前**的拒签 = 零成本回滚(表未改、冻结链未执行)。

### ④ 后果 / 代价

**正面**
- 读法约束的**粒度**与 SU#2 §③ 对齐(判据层拦截,**不**无条件传导到假设层);
- B7 从「政策导致的不可收口」变为可达成,**且不牵连** B4 口径;
- 「判据 vs 扩展观测」从**隐式惯例**变为**可判定标准**(两条 + 操作定义 + 反例对照)。

**负面**(必须承认)
- **H2 的判据面窄化 1 项**(命题证据未变,但证据面变薄);
- 判定标准引入**人工判断**环节(条件 1 的「命题蕴含」需 architect 个案判定)⇒ 无纯机器闸;缓解 = 台账留痕 + 硬约束 1 的反例对照 + 硬约束 4 的举证责任倒置;
- **硬约束 3 的引用稀释已有实测先例**(Entry #228 ④ 未携限定)⇒ 本则落地含回填动作。

**风险**
- **最大风险 = 滑坡**(把真未测的判据都改叫扩展观测)⇒ 硬约束 1 / 4 专堵,举证责任在申请方;
- 本则⛔ **不得批量套用**:每次扩用须重新命中两条条件 + 个案判定 + 登记。

### ⑤ Verification(可勾选验收判据 —— 前瞻方法)

> **回填(2026-11-05,per 台账 Entry #229)**:7 条逐条证据 =
> `objective §9.1 ①` H2 行(扩展观测标注 + 原措辞 + 归位理由 + 命中两条条件)· `§9.1 ①` 表注 ·
> `§9.1 ②` H2 行(out-of-scope 未动)· `§9.1 ③` 读法约束补句 · `§6.2 B7` 内嵌限定版 ·
> `Entry #228 ④` 行内订正注记 · `cep_prereg_drift_check` rc=0 · 本文件 **SU#4**(反例对照)。

- [x] `objective §9.1 ①` H2 的 `directory_misses` 行:⛔ **仍在表内**,标注为「扩展观测(未测量)」+ 归位理由 + **命中的两条条件**
- [x] `objective §9.1 ②` H2 的 out-of-scope 集**仍含** `directory_misses`(⛔ 未因归位而从预登记删除)
- [x] `objective §9.1 ③` 补一句:读法约束「任一**判据** ⏭ ⇒ 假设整体不计」**只对判据生效**;扩展观测的 ⏭ 不使假设失格,但须保留登记
- [x] `objective §6.2` B7 = **「8/8,其中 H2 的 `directory_misses` 为登记的扩展观测(未测量),不构成 H2 结论判据」**(限定**嵌入计数行**,非另附)
- [x] **`Entry #228 ④` 的 B7 行回填**(硬约束 3 的实测违例闭合)
- [x] `§9.1 ②` ↔ `experimental/preregistration.json` 的 drift 闸 **rc=0**(本则**不动**预登记集合 ⇒ 零 `scope-change`)
- [x] 硬约束 1 的反例对照可寻址:本文件 **SU#4**(H3 分支 2 走**装置补全**而非降级)

### ⑥ 关联

本文件 **SU#1 §④**(两级 verdict + 结构性保证)· **SU#2 §③**(结构性保证的 gloss —— 本则不推翻)· **SU#4**(装置补全 = 硬约束 1 的反例对照)·
`objective §9.1 ①` H2 行 / `§9.1 ②` H2 行 / `§9.1 ③` / `§6.2 B4` / `§6.2 B7` · 台账 **Entry #218**(事实 #2「判据收窄优先于补实现」)· **#225**(8/8 登记)· **#228 ⑤**(冲突登记)· **#229**(本则授权 + 表修复)


---

## Status Update(2026-11-06 · 四,per 台账 Entry #250)—— `backlog:` 占位**回填**(⛔ 不改任何决策内容)

> **性质**:纯**指针回填**(append-only)。
> **原占位**:`backlog: cep-b4-evidence-device`(待 planner 立项)
> **回填**:该条目**已实施**且**不占 OpenSpec slot**:装置 = `test/test_cep_b4_evidence_device.cc`(「一致性 × 完成域」复合装置),过 **AC-9** 实现评审 **PASS**(报告 `.rddf/reviews/ADR-CEP-06-SU4-ac9-implementation-review.md`,per 台账 **Entry #228**)⇒ improvement `.rddf/improvements/cep-b4-evidence-device.md` 状态已回填为「已实施」。

---

## Status Update(2026-11-08,per 台账 Entry #293)—— **台账元数据**的最小字段集(F-12:含 `metrics.json` 的 sha256 + 13 键明细 + 规模上限声明)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文 §1 ~ §5 与**任何**既有 SU(尤其 **SU 2026-11-04(#1)** 的两级 verdict 裁决 · **SU 2026-11-04(#2)** 的预登记载体裁决 · **SU 2026-11-05(#3)** 的三态裁决 —— **逐字保留不回改**);⛔ **不新增**任何 `INV-CEP-06-*`(per 本 ADR 追加段的既有惯例:追加段只登记台账侧投影,**不动** run 侧契约);⛔ 不改 4 件套的构成与 `experimental/` 的目录约定。
> **触发**:台账 **Entry #291** ⑤ 决策行的 **F-12**(D-9 只登记元数据 ⇒ 台账的「13 键结论」无据可核)· 用户 2026-11-08 口令「把 F-12 一并落成 SU」· `07 号` **SU · 十三** ② 的字段表。
> **审查**:`oracle`(字段集是否与本 ADR 既有契约冲突 / 是否越权改判据集合)→ `critic`(本段 + `07 号` SU · 十三 + `ADR-CEP-19` SU · 三 的架构审稿),顺序不可颠倒。
> **关联**:`07 号` **SU · 十三** · `ADR-CEP-19` SU 2026-11-08 · 三 · 台账 **Entry #291 / #292 / #293**

### ① 问题(为什么需要这一段)

- `07 号` SU · 十一 ① 的 **D-9** 规定:产物**不入库** ⇒ 入台账的是**元数据**。⚠️ 但 D-9 原文列的字段只有「run id · config 快照 sha256 · **13 键结论** · IC3 verdict · 拓扑标记」⇒ ⇒ 落库后会出现两个洞:
  1. **无据可核** —— 「13 键**结论**」只记判定,**不记取值** ⇒ 复核者无法据台账条目核对任何一项数值;
  2. **无法对账** —— 未记 `metrics.json` 本身 ⇒ 台账条目与「产出该结论的那一次 run」之间**没有可验证的绑定**;
- 且有一处**越界宣称风险**:本 ADR §3 规模上限契约第 1 条**已要求**实验元数据声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} 与 `verification_scope`,第 2 条**禁止**把 `bounded_random` / `sampled` 的结果表述为「已证明无死锁 / 无违例」⇒ ⇒ 而台账条目只记「IC3 verdict」⇒ ⇒ **台账会把「在声明的 scope 内未观测到」压扁成「绿」**⇒ ⇒ 台账须携带规模上限声明。

### ② 裁定的**最小字段集**(台账条目的内容)

| # | 字段 | 内容 | 载体 / 来源 |
|---|---|---|---|
| 1 | `run_id` | run 标识 | run 目录名(per 决策 1 的 `run_id` 格式) |
| 2 | `config_sha256` | `simulation_config.json` 的 sha256 | 4 件套(决策 1) |
| 3 | **`metrics_sha256`** | **`metrics.json` 的 sha256** | 4 件套(决策 1)⇒ **F-12 新增** |
| 4 | **`metrics_13`** | **13 个必纳字段的**值 **或** `not-collected` 标记**,逐键**;⚠️ `not-collected` 的键 SHALL **同时带 reason**(per `INV-CEP-06-IC4` / `_not_collected` 表)—— 只带标记不带 reason ⇒ 产物一旦丢失,降格理由不可考 | `metrics.json`(per `INV-CEP-06-IC2` / 决策 4 的 `not-collected` + reason 口径)⇒ **F-12 精确化**(原「13 键结论」为判定摘要) |
| 5 | **`verification_mode` + `verification_scope`** | 规模上限声明;**载体 = `simulation_config.json`**(per `tools/run_sweep.py` 的机器校验点)⇒ ⚠️ `metrics.json` 若有同名字段且两处漂移 ⇒ **以 config 为准** | §3 规模上限契约第 1 条(既有要求)⇒ **F-12 带入台账** |
| 6 | `ic3_verdict` + `hypothesis_id` | IC3 判定 + 预登记假设行标识;⚠️ 判定 SHALL 用 **SU 2026-11-05(#3) 三态词表**(✓ 通过 / ❌ 违例 fail / ⚪ 不适用)⇒ ⛔ **不得**记二值「绿 / 红」——「⚪不适用」被压成「绿」= 本段警告的**压扁风险在台账侧复发** | `INV-CEP-06-IC3` + `metrics.json` 的 `hypothesis_id`(per §3 规模上限契约第 3 条「须在报告中一并呈现」的同族要求) |
| 7 | `topology_tag` | 拓扑标记(A / B) | 本框架的拓扑标记(per `07 号` SU · 十三 ②) |

⇒ ⚠️ **字段 4 与字段 6 的分工**:字段 4 记**取值**(事实),字段 6 记**判定**(结论)⇒ ⇒ 二者**不得互相替代**(⛔ 只记判定 = 本段的原始洞;⛔ 只记取值 = 丢失 IC3 门禁的判定位)。

### ③ 后果 / 代价 + 边界

| 面 | 后果 |
|---|---|
| **可审计性** | 台账条目从「结论摘要」升级为「**实验元数据的完整投影**」⇒ ⇒ 复核者可据字段 4 逐键核对、可据字段 3 与产出 run 对账 |
| **代价 ①** | 台账条目**变长**(13 键逐键 + 2 个 hash + scope 描述)⇒ ⇒ 台账体积增长 ⇒ ⛔ 但**不引入**新文件形态(仍是**一段散文登记**) |
| **代价 ②** | 产物**仍不入库** ⇒ ⇒ **可复算性未获得**(见边界) |
| **代价 ③** | 字段 5 把「规模上限声明」带进台账 ⇒ ⇒ 台账读者**必须**读 scope 才能读懂 verdict ⇒ ⇒ ⛔ 这是**有意的摩擦**(per §3 规模上限契约第 2 条) |

> **边界(逐字)**:**登记 ≠ 可复算** —— 因产物**不入库**,台账条目只提供**对账**能力(该条目 ↔ 那一次 run 的 `metrics.json`,由 `metrics_sha256` 对齐)⇒ ⛔ **不得**表述为「实验可复算 / 可重放」;⚠️ `metrics_sha256` **不**保证结论正确,它只保证「台账记的是哪一份产物」;⛔ **不得**据台账条目声称「已证明无死锁 / 无违例」—— 该表述受 `verification_mode` / `verification_scope` 约束(per `ADR-CEP-06` §3 规模上限契约第 2 条);⚠️ **对账能力的前提是「那一份产物仍可取得」** —— run 目录不入库 ⇒ 可核性随 **retention** 衰减 ⇒ ⛔ 本段**不**裁决 retention(属另一条独立决策)。

⇒ ⇒ **与既有不变量的关系**:`INV-CEP-06-IC1`(4 件套)· `IC2`(13 必纳字段)· `IC3`(违例类判定)· `IC4`(`not-collected` + reason)· `IC5`(SWMR + data-value)**零改动** ⇒ 本段**只**规定**台账侧**投影的内容,**不动** run 侧契约 ⇒ ⇒ **新增 invariant 数 = 0**。

### ④ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且本 ADR 正文与**全部**既有 SU **逐字未变**;**新增 invariant 数 = 0**
- [ ] ① 的两个洞(无据可核 / 无法对账)+ 规模上限契约的压扁风险已登记
- [ ] ② 的 7 字段集已逐项登记,且字段 4 / 字段 6 的**分工**(取值 vs 判定)已登记
- [ ] ③ 的「登记 ≠ 可复算」边界句已**逐字**登记;3 条代价已登记;与 `INV-CEP-06-IC1`~`IC5` 零改动的说明已登记
- [ ] `07 号` SU · 十三 ② 的字段表与本段 ② **逐条一致**(⛔ 两处不得漂移)
- [ ] ⛔ `experimental/preregistration.json` **零 diff**;`experimental/metrics.schema.json` **零 diff**;⛔ 本段**未**裁决把产物入库;**`B3b-2` 维持 0/4**
- [ ] 冻结面两步授权已执行:台账 **Entry #293** 授权 → **commit 本段 + `07 号` SU · 十三 + `ADR-CEP-19` SU · 三** → `cep_freeze_check.sh --update --authorized-by "Entry #293"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #293**(授权)· **#292**(双审闭环)· **#291**(落地义务清单)· `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md`(F-12)· `07 号` **SU · 十一** ① 的 D-9 行 · `07 号` **SU · 十三** ② · `ADR-CEP-19` SU 2026-11-08 · 三 · `docs/architecture/07-coherence-exploration-platform.md` §3 的 canonical 10 流 · `experimental/metrics.schema.json` · `experimental/preregistration.json` · `tools/run_sweep.py`(4 件套的实际产出:输入 3 件 + `report.html`)


---

## Status Update(2026-11-08,per 台账 Entry #295)—— **实验产物留存裁定 = 丙**(只入库 `metrics.json` + `simulation_config.json`;**范围 = 有台账条目的 run**)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文 §1 ~ §5 与**任何**既有 SU(尤其 **SU 2026-11-04(#1)** 的两级 verdict · **(#2)** 的预登记载体 · **(#3)** 的三态词表 · **Entry #293 SU** 的台账元数据 7 字段 —— **逐字保留不回改**);⛔ **不新增**任何 `INV-CEP-06-*`;⛔ 不改**决策 1** 的 4 件套构成(仍在 `experimental/<run_id>/` 下产出 4 件)。
> **触发**:`07 号` **SU · 十三** ③ 明写「⛔ 本段**不**裁决 retention(属另一条独立决策)」⇒ 本条即该项;· 台账 **Entry #293** 裁定 2(F-12)的**对账能力**无载体可依(实测 `experimental/` 仅 4 个 tracked 文件 ⇒ `metrics_sha256` 绑定的是「已不存在的东西」)。
> **审查**:`oracle`(留存量级 / 「可对账 ≠ 可复算」边界 / IC5 的明细载体)→ `critic`(本段 + `07 号` SU · 十四 的架构审稿),顺序不可颠倒。
> **关联**:`07 号` **SU · 十三** / **SU · 十四** · 台账 **Entry #293 / #295** · `.gitignore` · `experimental/metrics.schema.json` · `tools/run_sweep.py` · `tools/render_report.py`

### ① 问题(为什么需要独立裁定)

- **实测现状**:`.gitignore` 有**明文政策**(`experimental/*` 忽略 + 仅 3 条例外:`.gitkeep` / `*.schema.json` / `preregistration.json`)⇒ ⇒ **全部 run 产物不入库**;且 **无任何外部归档**(`.github/workflows/ci.yml` 唯一 `upload-artifact` 上传 `build/Testing/`,**retention-days: 7**)。
- ⇒ ⇒ 由 **Entry #293 裁定 2**:台账条目记 `metrics_sha256` 以提供**对账** ⇒ ⇒ **但被绑定的产物不在仓内、也无归档** ⇒ ⇒「对账能力」**当下落空**;`07 号` SU · 十三 ③ 已明写该前提(「以那一份产物仍可取得为前提」)⇒ ⇒ 本条负责**把该前提变成可满足的**。
- **规模实测**(2026-11-08):`experimental/` **11 MB** / **127** 个 run 目录 / **206** 份 `metrics.json`;单件均值 = `metrics.json` **12.5 KB** · `simulation_config.json` **0.6 KB** · `trace.h5` **18.6 KB** · `report.html` ~12.6 KB。

### ② 选项对比表(≥ 3 行)

| 选项 | 内容 | 结论 | 理由 |
|---|---|---|---|
| **甲** | 维持现状(**全不入库** + 无归档) | ⛔ **不取** | 台账的 `metrics_sha256` **无可对账对象** ⇒ 与 Entry #293 裁定 2 的立意**直接矛盾**;条目不(可)核,且**静默衰减** |
| **乙** | **全量入库 4 件套** | ⛔ **不取** | 体积 **11 MB**(`trace.h5` 随仿真长度**线性增长**);且 `report.html` 的 **summary / violation badges 面可由 `metrics.json` 再生**(⚠️ 但 **latency CDF 需 `trace.h5`** ⇒ 非全部可再生,见 决策 3)⇒ 入库 = 存大部分可再生产物 |
| **丙** | **只入库 `metrics.json` + `simulation_config.json`**;范围 = **有台账条目的 run** | ✅ **取** | 体积 **2.6 MB**(存量上限)/ ~**13 KB** per run;直接支撑台账 7 字段的**对账** + 13 键**逐键复核** + 「同 config 同种子**可复跑**」;`trace.h5` 不入库**不影响**任何 IC 的**判定面**(见 ③ 的 IC 分层) |
| **丁** | 不入库 + **外部归档**(扩展 CI 上传 `experimental/`) | ⛔ **不取** | ① **无现成归档**(CI 只传 `build/Testing/`,7 天);② 只覆盖 **CI 产出的** run ⇒ **本地 run 无归档**;③ 归档**会过期** ⇒ 对账仍衰减 |

### ③ 裁定 + 边界(**逐字**)

> **裁定 = 丙**:`metrics.json` + `simulation_config.json` **作为证据载体入库**;**范围 SHALL 限于「有对应台账条目的 run」**(⚠️ 首批 = **h3 升级 run 及以后**;⚠️ 历史 **127** 个 run **grandfather**:⛔ 不回填、⛔ 不得以其为对账依据);`trace.h5` / `report.html` **不入库**;⛔ 二者入库**不等于**升格为**契约面**(⛔ 不进 `FROZEN_GLOBS`、⛔ 不产生任何 `INV-*` / 判据、⛔ 不改 `experimental/` 的任何 schema)。

> **能力边界(逐字)**:入库的两件只提供 ① **对账**(台账 `metrics_sha256` ↔ 仓内 `metrics.json`) ② **13 键逐键复核** ③ **可复跑**(同 `simulation_config.json` + 同种子 + **同代码版本(git rev)** + 同工具链)⇒ ⛔ **不**提供 **bit-identical 复现**(`RANDOM` 模式的 `uniform_int_distribution` **跨 stdlib 非位精确**,per `07 号` SU · 十一 ① 的 D-7 漂移登记);⛔ **不**提供 **trace 级分析的可复核性**(`trace.h5` 不入库);⚠️ **schema 漂移不影响对账**(`metrics_sha256` 锚的是**字节**,不是 schema 合法性)⇒ 老 `metrics.json` 可能不满足未来的 `metrics.schema.json`,但对账仍成立。

- **IC 分层(逐条 · ⛔ 不得压成一句 blanket)**:
  - **IC1 / IC2 / IC4** ⇒ 判据面 = `metrics.json`(4 件套的存在性与字段齐备 / 13 必纳字段 / `not-collected` + reason)⇒ ⇒ **不依赖** `trace.h5`;
  - **IC3** ⇒ 判据面 = `metrics.json` 的 `deadlock_violations` / `memory_violations` ⇒ **不依赖** `trace.h5`;
  - **IC5** ⇒ 「区分 SWMR 违例 / data-value 违例」的明细载体 = **`metrics.json` 的 `memory_violation_kinds`**(实测:`experimental/metrics.schema.json` 定义该字段 · `test/cep_experiment_harness.hh` 写出该字段)⇒ ⇒ **不依赖** `trace.h5` ⇒ ⇒ 丙口径下 **IC1~IC5 的判定与复核面全部保留**。
  - ⚠️ `trace.h5` 的独有价值仅在**过程分析**(latency CDF / 逐周期事件)⇒ 该面**不入库** ⇒ ⛔ 不得声称「trace 级分析可复核」。

### ④ 后果 / 代价

| 面 | 后果 |
|---|---|
| **正面** | 台账 7 字段的 `metrics_sha256` / `metrics_13` **首次有仓内对账对象** ⇒ 「登记 ≠ 可复算」升级为「**登记可对账**」;IC1~IC5 **判定面完整保留** |
| **负面 ①** | 仓库 **+2.6 MB 上限**(受「仅有台账条目的 run」约束 ⇒ 实际增量取决于 run 数) |
| **负面 ②** | `.gitignore` 需增例外(`run-*/metrics.json` / `run-*/simulation_config.json`)⇒ **政策载体变更**,须与注释同步(⛔ 不得只改规则不改注释;⚠️ 注释的 provenance 须改写为 Entry #295 的取代链) |
| **风险 ①** | `metrics.json` 可能含**机器名 / 路径**等非契约字段 ⇒ 入库前 SHALL 过白名单校验(见 ⑤) |
| **风险 ②** | 「入库」会被读成「升格为契约」⇒ ⛔ 三处声明不可省(③ 裁定句 / `.gitignore` 注释 / 台账条目) |

- **回滚路径**:撤销 `.gitignore` 的两条例外 + `git rm` 已入库的 run 目录 ⇒ 恢复现状;⚠️ **台账已记条目保留为历史记录、⛔ 不回删**。

### ⑤ 机器可判点(实施期落地,归 builder)

1. **每个被提交的 run 目录 SHALL 只含** `metrics.json` + `simulation_config.json`(⛔ `trace.h5` / `report.html` / 其他);
2. **双向闸**:仓内**每份**已提交的 `metrics.json` SHALL 对应**一个台账条目**(⛔ orphan 证据);反向:台账条目 SHALL 能定位到仓内 `metrics.json`(sha256 相等);
3. `.gitignore` 的**例外规则与注释逐条对应**;
4. ⛔ 两件文件**不得**进入 `FROZEN_GLOBS`。

### ⑥ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且本 ADR 正文与**全部**既有 SU **逐字未变**;**新增 invariant 数 = 0**;决策 1 的 4 件套构成**未改**
- [ ] ② 的选项对比表 ≥ 3 行且每行含**结论 + 理由**(甲/乙/丙/丁);乙行已含「CDF 需 trace」的限定
- [ ] ③ 的裁定句(**含「范围 = 有台账条目的 run」+ grandfather**)、能力边界(四句)、**IC 分层 5 条**均已逐字登记
- [ ] ④ 的正面 / 负面 / 风险已登记(含 `.gitignore` 注释须同步)
- [ ] ⑤ 的 4 条机器可判点已登记(**含双向闸**)
- [ ] `07 号` SU · 十四 ② 与本段**逐条一致**(裁定句 + 边界句逐字)
- [ ] ⛔ `experimental/preregistration.json` / `metrics.schema.json` **零 diff**;⛔ 本段**不改** `FROZEN_GLOBS`;**`B3b-2` 维持 0/4**
- [ ] 冻结面两步授权已执行:台账 **Entry #295** 授权 → **commit 本段 + `07 号` SU · 十四** → `cep_freeze_check.sh --update --authorized-by "Entry #295"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #295**(授权)· **#293**(裁定 2:台账元数据 7 字段 —— 本段为其提供对账载体)· **#292**(D-9 元数据字段的原始裁决)· `07 号` **SU · 十三** ③(「不裁决 retention」的指针)/ **SU · 十四** ② · `.gitignore` · `experimental/metrics.schema.json`(含 `memory_violation_kinds`)· `test/cep_experiment_harness.hh`(写出该字段)· `tools/run_sweep.py` · `tools/render_report.py`(summary/badges 可再生;CDF 需 trace)


## Status Update(2026-11-09,per 台账 Entry #304)—— **三键测量归属**的落地口径 + 「**框架验证 run**」的**证据边界**(⛔ 非实验证据 run)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文与既有 SU(尤其决策 1 ~ 5 与 SU 2026-11-08 · 二 的 retention 裁定 —— **逐字保留不回改**);⛔ **零** schema 改动(`experimental/metrics.schema.json` 与 `experimental/preregistration.json` **零 diff**);⛔ 新增 invariant 数 = **0**。
> **触发**:change **`cpptlm-cep-stub-cache-chstream-surface`** 的实施闭环(台账 Entry #297 ~ #303)+ 用户 2026-11-09 口令「切 architect 做 A9.2」。
> **审查**:`oracle`(三键归属口径与证据边界)—— 单审 oracle(「只同步实施态」档),⛔ 不启 critic 双审。

### ① 三键的**测量归属**(per `07 号` SU · 十一 ① 的 D-8 ②「待定」项落地)

| 键 | 本装置的**产出者** | 口径 | 落法 |
|---|---|---|---|
| `coherence_stalls` | ✅ **有** —— `StubCacheEndpointTLM` 的 in-flight 观测 | 「**in-flight 非零的周期数**」(因一致性请求未完成而停顿) | **实测值** |
| `collective_stalls` | ❌ **无** —— 本拓扑**不绑定** collective plane | — | `not-collected` + reason「本装置无 collective plane 绑定 ⇒ 无产出者」 |
| `directory_misses` | ❌ **无** —— 本拓扑**不挂** directory / sharer tracker | — | `not-collected` + reason「本装置无 directory / sharer tracker ⇒ 无产出者」 |

⚠️ **「真实 0」口径的落法**(逐字):「**实测 0**」与「**未测量**」**不得同形** ⇒ 每键必须落成二者之一 —— ① **实测值**(可为 0,但**必须**登记进 `measured`)+ 产出者口径;② `not-collected` + **非空 reason**;⛔ 不得为凑「13 键全有值」给未测键填 0。
⇒ **机器可判点**:`cep_exp::metrics_13_covered()`(逐键:实测 **或** `not-collected` + 非空 reason,否则判红并返回首个缺结论的键名)。

### ② 「**框架验证 run**」的证据边界(⛔ **非**实验证据 run)

- **run class** = `framework-verification`(`simulation_config.json` 的 `run_class` + `not_an_experiment_run: true`);`hypothesis_id` = **`framework-e2e`** ⇒ ⛔ **不在** `experimental/preregistration.json` 的 `registered` 行内 ⇒ ⛔ **不绑定**任何预登记假设;
- ⇒ ⛔ **不得**作 **B4** 达成证据 · ⛔ **不得**改任何 hypothesis 结论 · ⛔ **不得**作任何一致性安全断言(IC3 = **⚪ 不适用**)⇒ 用途**仅** = **可对账**;
- ⚠️ **为何不绑 `H3`**:H3 的预登记 out-of-scope = {`coherence_stalls`, `collective_stalls`, `directory_misses`} ⇒ **延迟族 5 键必须实测**;而本装置**无已完成往返**(框架层 dual-port 入向路由恒落 PE 侧 ⇒ 回程不可达)⇒ 延迟族**无样本** ⇒ 绑 H3 会违反「未列入 out-of-scope 者**必须实测**」;⛔ 且**不得**事后扩 `preregistration.json`(明令禁止)⇒ ⇒ 只能落为**框架验证 run**(详见 `07 号` SU · 十五 ② ②);
- ⚠️ **IC3 三态**的产出者 = `cep_exp::ic3_verdict()`(词表**逐字** = `✓ 通过` / `❌ 违例 fail` / `⚪ 不适用`;⛔ 违例类任一 `not-collected` ⇒ **⚪ 不适用**,**优先于**「通过」)⇒ 本 run 落 **⚪ 不适用**;
- ⚠️ **第 5 句边界(补)**:本 run **不在受监管的实验 emit 路径上** —— 专用标签 `[cep][e2e][framework-emit]`(⛔ 不共用 `[cep][exp][emit]`,那个过滤器属于 h2/h3/h4/h6 的预登记 run)+ `CEP_EXP_ID` 守卫(非 `framework-e2e` ⇒ 不产出)⇒ ⇒ **不会覆盖**预登记 run 的产物(该防护由台账 Entry #303 ⑤ 的实测缺陷促成);
- **retention 丙 的两件入库**(per SU 2026-11-08 · 二)维持:仅 `metrics.json` + `simulation_config.json` 入库;`trace.h5` / `report.html` **不入库**;⛔ 「**登记 ≠ 可复算**」——入库件只提供 ① **对账**(`metrics_sha256` ↔ 仓内文件)② **13 键逐键复核** ③ **可复跑**(同 config 快照 + 同种子 + 同 git rev + 同工具链)⇒ ⛔ **不**提供 bit-identical 复现(跨 stdlib 非位精确)· ⛔ **不**提供 trace 级可复核性;
- ⚠️ **首批入库 run 的字面追认**:SU 2026-11-08 · 二 ③ 的「首批 = **h3 升级 run** 及以后」**字面**与本轮实际(首批 = **框架验证 run** `cep-e2e-2026-11-09-a`)不符 ⇒ 本段**追认**该字面为「首批 = **有台账条目的 run**(首批 = 框架验证 run)」⇒ ⛔ 不回改 SU 2026-11-08 · 二 原文。

### ③ 台账 7 字段与**双向闸**的落地形态

- **7 字段**:`run_id` · `config_sha256` · `metrics_sha256` · `metrics_13` · `verification_mode` + `verification_scope` · `ic3_verdict` + `hypothesis_id` · `topology_tag`;载体 = **两处并存**(台账散文段 + `test/cep_e2e_runs.jsonl`);
- ⚠️ **`config_sha256` 的口径 = 拓扑 config 文件的指纹**(⛔ **不含** provenance ⇒ 否则「配置指纹」退化为「环境指纹」,对账语义被稀释);⚠️ **追认 F-12 的字面**:F-12 SU 的 `config_sha256` 原文 = 「`simulation_config.json` 的 sha256」⇒ 本段以**取代**方式追认该口径(run 侧另列 `simulation_config_sha256` 字段锚定该文件)⇒ ⛔ **不回改** F-12 原文;⚠️ **闸的覆盖限制**:双向闸只校验 `metrics_sha256` 与文件存在性,**不**校验 `config_sha256` 指向哪个文件 ⇒ 该口径由**本段文本**承担(⛔ 不得读成机器已判);
- **双向闸** = `scripts/test/cep_e2e_reconcile_check.sh`:① 零 orphan(仓内**已提交** `experimental/run-*/metrics.json` ⇒ 必有台账条目)② 反向零 orphan(台账条目 ⇒ 文件存在且被 git 跟踪)③ `metrics_sha256` 相等 ④ 7 字段齐备 ⑤ verdict 三态词表;**grandfather**:历史 **127** run **不回填** ⇒ 只覆盖**首批入库**的 run。

### ④ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且本 ADR 正文与**全部**既有 SU **逐字未变**;**新增 invariant 数 = 0**
- [ ] ① 的三键**产出者 / 口径 / 落法**逐条登记;「真实 0 ≠ 未测量」的落法**逐字**登记
- [ ] ② 的证据边界**五句**齐备(⛔ 非 B4 证据 · ⛔ 非一致性安全断言 · ⛔ 不改 hypothesis 结论 · 用途仅 = 可对账 · **不在受监管 emit 路径上**)+ 不绑 `H3` 的理由齐备;⛔ `preregistration.json` **零 diff**
- [ ] ③ 的 7 字段 + `config_sha256` 口径(**含 F-12 字面的取代追认句**)+ 双向闸 5 项 + grandfather 边界齐备;⛔ 未把「闸不校验 `config_sha256` 指向」写成机器已判
- [ ] ⛔ `experimental/metrics.schema.json` / `experimental/preregistration.json` **零 diff**;⛔ `FROZEN_GLOBS` 未变;**`B3b-2` 维持 0/4**
- [ ] 冻结面两步授权已执行:台账 **Entry #304** 授权 → **commit 本段 + `07 号` SU · 十五 + `ADR-CEP-19` SU** → `cep_freeze_check.sh --update --authorized-by "Entry #304"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #304**(授权)· **#303**(首批 run 入库 + 双向闸 + 7 字段台账)· **#302**(A7 三键归属 + IC3 三态产出者)· **#295**(retention 丙)· **#293**(台账 7 字段裁定)· `07 号` **SU · 十五** · `ADR-CEP-19` SU 2026-11-09 · 一 · `test/cep_e2e_runs.jsonl` · `scripts/test/cep_e2e_reconcile_check.sh` · `test/cep_experiment_harness.hh`
