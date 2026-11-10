# Coherence Interface(第 2 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> canonical 10 条一致性流 + Bundle 载体约束。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---


## 3.1 清单(per `ADR-CEP-02 §2`,首次定义)

| # | 流名 | 方向 | 完成纪律 | 语义 |
|---|------|------|---------|------|
| 1 | `request` | node → center | requester-scoped | 缓存行请求(读 / 唯一化 / 写回 / 驱逐) |
| 2 | `completion` | center → node | requester-scoped | 请求完成 + data(若 inline)+ perm + error |
| 3 | `snoop` | center → node | requester-scoped | snoop 查询 / 失效 / clean / forward |
| 4 | `snoop_resp` | node → center | requester-scoped | snoop 响应:hit + state + dirty + data + txn_id |
| 5 | `writeback` | node → center | requester-scoped | 驱逐/写回:addr + data + dirty |
| 6 | `forward_data` | node ↔ node(经中心调度)| requester-scoped | 数据转发:src_addr + dst_addr + data + perm |
| 7 | `back_invalidate` | center → node | requester-scoped | 反向失效:addr + type(invalidate/clean)+ txn_id(CXL 3.0 DCOH 的 BISnp 走这里;⚠️ **方向反转例外** = `node → center`(device→host,S2M),其响应 BIRsp = M2S —— 方向反转例外说明见 `ADR-CEP-02` §2 表下)|
| 8 | `address_resolve` | node ↔ center | requester-scoped | 地址归属解析:pa + node_id → home_node + coherent_flag |
| 9 | `barrier` | center → node ↔ center(双向)| requester-scoped | 单请求者排空/可见性点;**不携带 group_id / participant_count / epoch_id** |
| 10 | `snoop_completion_ack` | node → center | requester-scoped | snoop 完成后 ack;配对 `snoop_resp` 而非 `completion` |

**不变量**(per `ADR-CEP-02 §2` CEN-Ib1~10):

| ID | 规则 |
|----|------|
| CEN-Ib1 | `request` 必有配对 `completion` |
| CEN-Ib2 | `completion` 不携带 unsolicited 数据 |
| CEN-Ib3 | `snoop` 必有 `snoop_resp` |
| CEN-Ib4 | `snoop_resp` 必须含 `txn_id` 用于 ack 配对 |
| CEN-Ib5 | `writeback` 不与 `request` 竞态 |
| CEN-Ib6 | `forward_data` 对 stub 透明(不可分辨性) |
| CEN-Ib7 | `back_invalidate` 是 requester-scoped,**不是** group rendezvous |
| CEN-Ib8 | `address_resolve` 在 ProtocolEngine 内终结 |
| CEN-Ib9 | `barrier` 流不携带 group_id / participant_count / epoch_id |
| CEN-Ib10 | `snoop_completion_ack` 配对 `snoop_resp` 不配对 `completion` |
| **CEN-Ib11** | **atomic 意图作为 `request` 流的独立属性承载;`perm` 枚举维持 4 值锁,不新增 "ATOMIC" perm 值**(per `ADR-CEP-02`,一手:ProtoGen p.7 局限 L2)|

## 5.4 Bundle 约束

- **协议 Bundle**(网络层,项目级):`NoCFlitBundle`(16 字段)/ `PcieTlpBundle`(frozen)/ `Axi4Bundle`(≥5 消费者)/ MSI-X / DLLP / TLP wire
- **业务 Bundle**(业务层,模块 owner):`CacheReqBundle` / `CacheRespBundle` / `GmmuPteBundle` / `SdmaCmdBundle` / `DoorbellBundle` / `CrBundle`(APB-lite LWC)/ `MsixBundle`
- **跨模块共享的业务 Bundle**(第三类:业务语义字段 + 项目级 owner,真源 1 `06 号 §3.3`):`CohIfBundle`(`include/bundles/cep_coherence_bundles_tlm.hh`)—— 承载 canonical 10 流中**跨 Stub↔PE 边界的 5 条 RN-side 流**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`;由**强类型** `flow_id` 区分,域外 5 条流**类型不可表达**);**不含网络层字段**;**取网络侧演进纪律**
- **编译期锁**:`BUNDLE_FIELD_WIDTH` static_assert + `is_trivially_copyable` 断言
- **不做**:协议专属字段泄漏进共享抽象(per `ADR-CEP-04 §3.2` 规范 intent 原则)

---
