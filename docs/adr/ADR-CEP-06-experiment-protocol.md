# ADR-CEP-06: Coherence Exploration Platform 实验输出协议

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CEP 实验输出 / metric / sweep / A-B 对比报表**,解 metis G1 "探索平台没有探索"盲点
> **关联真源**:
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §15(演进 seam)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
> - 真源 3:`ADR-CEP-00A-real-source-anchor-map.md`(章节映射)
> - 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(仿真语义)
> - 真源 3:本系列 CEP ADR

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
- **不可观测项的兜底**(per oracle "未达成 = 标注 not-collected")

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

#**INV-CEP-06-IC5**(新增,2026-10-20):`memory_violations` 的判定**必须**覆盖 **SWMR + data-value 双不变量**;报告须能区分违例类型(SWMR 违例 / data-value 违例)。

## 4.1 sweep 模式(per `tools/run_sweep.py`)

- **输入**:sweep 维度(protocol ∈ {CHI, CXL, TileLink, NVLink, UALink} / topology ∈ {mesh, ring, crossbar} / directory ∈ {full-map, sparse, broadcast} ...)
- **执行**:对每个维度组合跑 1 个 run,产出 N 个 4 件套
- **聚合**:`tools/aggregate.py` 生成 `experimental/<sweep_id>/sweep_summary.csv`(每行 = 1 个 run,每列 = 13 必纳 metric)
- **可视化**:`tools/plot.py` 生成 `protocol_rank.png` / `topology_rank.png` / `directory_rank.png`

#**INV-CEP-06-IC5**(新增,2026-10-20):`memory_violations` 的判定**必须**覆盖 **SWMR + data-value 双不变量**;报告须能区分违例类型(SWMR 违例 / data-value 违例)。

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

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | metis G1 | 解战略盲点"探索平台没有探索" |
| §2.1 13 必纳 metric | oracle GEP 评审建议 + metis G4 | 涵盖吞吐 / 延迟 / stall / 违例 |
| §2.2 4 可选 metric | metis G1 sweep 需求 | link utilization / VC stall / snoop fanout / directory size |
| §2.4 兜底契约 | oracle 评审建议 | not-collected + 禁止省略/0/-1 |
| §3 4 条不变量 | 综合 | 与 §2.1-§2.4 一一对应 |
| §4 sweep / A-B | metis G1 | 工具链与产出格式 |

---

**版本**: v0.1 草案
**最后更新**: 2026-10-20(v0.5 修订)
**批准状态**: 🟡 待 Oracle v0.5 二次复审
**维护者**: CppTLM build agent(architect mode)