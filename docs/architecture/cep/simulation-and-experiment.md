# 仿真语义与实验输出

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> simulation_mode 单一维度、两种推进模式差异、Null 兜底,以及实验 4 件套 / 13 必纳 metric / 验证深度 / 「全绿」两级判定。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 6.1 simulation_mode 单一维度(per `ADR-CEP-07 §2 Decision 1`)

| 模式 | 含义 | 延迟 | 何时用 |
|------|------|------|--------|
| `functional` | 业务/网络逻辑正确性验证 | **0** | 前期性能建模期 |
| `timing` | 周期精确仿真 | > 0 | 后期协议特化时 |

**真源**:`06 v0.4 §5.1` + `01-hybrid §4.7`(per oracle v0.4 B3-5 实证 — 06 §4.7 不存在)。

**不引入 `network.mode`**:协议特化是**工程**(改模块代码),不是 JSON 字段切换(per `06 §5.2` gem5 SLICC 经验)。

## 6.2 两种模式的推进差异

| 仿真语义 | coherence 流推进 | group-scoped completion 推进 |
|---------|------------------|-------------------------------|
| `functional` | 同步直调(per `SimplePort::send`);事务完成 = 单次 tick 即达 | rendezvous 等待 = 占位(0 cycle),全组 ack 后即时 emit |
| `timing` | cycle-accurate;跨 cycle 累加 | rendezvous 等待 = 累计 cycle;未到齐不推进仿真周期 |

**实现**:`ProtocolEngine::advance_cycle()` 虚函数(per `chstream_module.hh:73` 默认空实现)按 mode 分发 — functional: no-op;timing: cycle accumulation。

## 6.4 Null 实现(解耦测试兜底)

- **`NullProtocolEngine`**:10 流覆盖矩阵 — 流 1/3/4/5/6/7/10 no-op;流 2 immediate-complete;流 8 immediate-resolve;流 9 immediate-complete
- **`NullCollectivePlane`**:所有原语返回"不支持" — CHI / CXL 配置下的默认绑定,是"启停干净"的可测试载体

---

## 7.1 4 件套(per `ADR-CEP-06 §2`)

```
experimental/<run_id>/
  ├── simulation_config.json      # JSON 配置快照(输入)
  ├── trace.h5                   # HDF5 trace(per cycle per txn 事件)
  ├── metrics.json               # 关键 metric 数值
  └── report.html                # A/B 对比报表
```

`run_id` 格式:`{protocol}-{topology}-{directory}-{timestamp}`

## 7.2 13 个必纳 metric

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
| `directory_misses` | int | count | directory lookup 失败次数 |
| `deadlock_violations` | int | count | 死锁违例次数(应 0,否则 fail)|
| `memory_violations` | int | count | 一致性违例次数(应 0,否则 fail)|

**4 个可选 metric**:`link_utilization_per_link` / `vc_stall_per_vc` / `snoop_fanout_per_request` / `directory_size_distribution`

**兜底契约**:不可观测 metric 标 `not-collected` + `_not_collected_reason`,**禁止**省略 / 写 0 / 写 -1。

## 7.2.1 实验验证深度契约(per `ADR-CEP-06 §3`)

- 每次实验**必须**声明 `verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} 与 `verification_scope`(规模上限描述)
- `bounded_random` / `sampled` 的结果**不得**表述为「已证明无死锁 / 无违例」—— `deadlock_violations == 0` 只代表「在声明的 `verification_scope` 内未观测到」
- 平台**不承诺**「任意配置都能穷尽验证」—— 这是规模事实,不是实现缺陷

> 完整契约见 [`ADR-CEP-06 §3`](../adr/ADR-CEP-06-experiment-protocol.md)(含一手规模上限依据:极端配置需 1TB RAM + 数小时)。**本节不复制具体数字,避免第二真源。**


### 7.2.2 「全绿」的两级判定(run-validity / hypothesis-verdict)

> **本节解决什么**:「全绿」曾是**单一词**:既被读成「13 键全部有值」,也被读成「假设成立」
> ⇒ 两者达成度天差地别。本节把它**拆成两级,各自独立可判**。

| 级 | 判据 | ⛔ 不得混用 |
|---|---|---|
| **run-validity**(运行有效性) | ① 13 键**齐备**;② 每值**要么真实数值、要么带 reason 的 `not-collected`**;③ `not-collected ⊆` **预登记** out-of-scope 集;④ **违例类**若 `not-collected` ⇒ IC3 判「**⚪ 不适用**」 | ⛔ **不是**「13 键全部有值」 |
| **hypothesis-verdict**(假设结论) | §9 判据逐条标「**测量了 / 未测量 / fail**」;未测量**不继承** pass | ⛔ 未测量 ⛔ 不得算 pass |

**预登记闸是本判定的承重结构**:若 emit 端可**事后自由**标 `not-collected`,
则「结构零冒充实测」只是被替换成「`not-collected` 冒充预声明」—— **自欺升级**而非消除。
⇒ 预登记集合的载体在 **objective / plan 域(评审过的设计面)**,⛔ **不在** `metrics.json`(被试者自证的报表面)。

**违例类允许 `not-collected`(⛔ 禁止会把未测量写成 0)**:若禁止,emit 的唯一合规出路就是
**把未测量写成 0**。正确规则 = **允许 + 连带证据降级**(IC3 判「不适用」+ 不得作一致性安全断言的证据 + 报告层 badge)。

## 7.3 sweep / A-B 工具

| 工具 | 功能 |
|------|------|
| `tools/run_sweep.py` | 维度组合(protocol × topology × directory)→ N 个 4 件套 + `sweep_summary.csv` |
| `tools/ab_compare.py` | baseline vs target → `delta_metrics.json` + CDF overlay + `report.html` |
| `tools/render_report.py` | `metrics.json` + `trace.h5` → `report.html`(summary 表 + latency CDF + violation badges) |
| `tools/aggregate.py` / `plot.py` / `recommend.py` | 聚合 / 排名图 / 阈值建议 |

---
