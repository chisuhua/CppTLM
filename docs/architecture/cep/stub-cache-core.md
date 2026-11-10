# Stub Cache Core(第 1 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> Stub Cache 的 net 侧投影面与「缓存核心协议无关性」。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 3.2 Stub Cache Core 的 RN 侧投影(per `ADR-CEP-05 §2`)

**Stub 只感知 5 条流**(net-side),另 5 条由 ProtocolEngine 内部消化:

| Stub 接口 | 对应 canonical 流 | 折叠说明 |
|-----------|----------------|---------|
| `request(addr, perm)` | 流 1 | — |
| `completion(txn_id, data, perm, error)` | 流 2 | — |
| `snoop(addr, type, txn_id)` | 流 3 | — |
| `snoop_resp(addr, hit, state, dirty, data, txn_id)` | 流 4 | — |
| `writeback(addr, data, dirty)` | 流 5 | — |
| `evict_line(addr)` | **core-side 原语** | 非 CohIf 流投影;net-side 表现为流 5 + 流 2 |

**5 隐藏流 → stub 原语折叠映射表**(α 方案正确性的唯一审计点):

| 隐藏流 | stub 表现 | 关键不变量 |
|--------|----------|-----------|
| 流 6 `forward_data` | 不可显式为接口;stub 收到流 2 `completion` 时数据可用即视为"forward 或 memory 完成" | **CEN-Ib6 不可分辨性** |
| 流 7 `back_invalidate` | 表现为普通 invalidate snoop | stub 无协议特异行为 |
| 流 9 `barrier` | 对 stub 不可见 | barrier 穿插时 stub 的 MSHR 占用、snoop 响应、写回顺序均不变 |
| 流 8 `address_resolve` | 在 ProtocolEngine 内终结 | 不泄漏到 stub 任何接口 |
| 流 10 `snoop_completion_ack` | 由 ProtocolEngine 内部消化 | stub 不知道 snoop ack 单独流存在 |

**Stub Cache 7 条不变量**(per `ADR-CEP-05 §4.3`):

| ID | 规则 |
|----|------|
| INV-SC1 | 同一 addr 在 stub 内部任何时刻最多 1 个 outstanding request |
| INV-SC2 | evict 不允许选取 outstanding line |
| INV-SC3 | snoop 响应必须在固定延迟内返回(参数化)|
| INV-SC4 | completion 与 request 严格配对(无 unsolicited completion);`txn_id` 沿用 `ADR-X.1-transaction-id.md` 命名 |
| INV-SC5 | dirty writeback 必须带 data;clean writeback 不带 |
| INV-SC6 | hit 路径不产生网络流量(cpu_side 直接返回)|
| INV-CEP-05-IC1 | 只暴露**稳定状态**;瞬态态 + 协议私有态(token 等)住 Protocol Engine 插件内部(per `ADR-CEP-05 §4.3`)|

## 3.3 缓存核心协议无关性

- **缓存行粒度**:64B,地址对齐
- **权限状态**:`NONE / I / S / E / M`(MESI 抽象),**不暴露** CHI 的 UC/SC/UD/SD 七态
- **内部资源**:tag/data SRAM、MSHR、写回缓冲、替换策略、预取器
- **禁止**:缓存核心直接处理 REQ/SNP/DAT 通道,或直接处理 TxnID / HomeNID / CompAck

---
