# ADR-CEP-02: Coherence Interface 10 条抽象事务流清单 + barrier 流语义澄清

> **状态**: ✅ **已签发(v1.0)**(2026-11-03 定稿;per 台账 **Entry #169** —— 队列 #13 `cpptlm-cep-nvlink-sharp` 的**开工前置**)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)/ **2026-11-03 定稿**
> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 1:`06-inter-module-network.md` v0.4 §2.1 L1-L6 仿真层层模型(正交,非 CEP 6 层架构)
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3(锚点 per `ADR-CEP-00A §3.2`;**§11.5 非 P3-P6 演进路线**,§11.5 = "相关测试",全文 grep "barrier" 0 匹配 — per oracle 评审 B6)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
> - 配套:`ADR-CEP-05-stub-cache-core-interface.md`(折叠映射表引用本 ADR canonical 10 流)

---

## 1. 背景

### 1.1 Coherence Interface 抽象事务流 — CEP 首次定义

之前讨论中提及的"ADR-I01 / 06 §15.1 / 06-bis §11.5"中 10 条抽象事务流的描述,**经 oracle 评审实证**:
- `06-inter-module-network.md` §15.1 行 1170-1179 内容为引用 ArchForge 文档清单,无 10 条流清单
- `06-bis-coherence-as-network-extension.md` 全文 grep "barrier" 0 匹配
- "ADR-I01" 不存在,全仓 grep 仅 CEP ADR 自引用

**结论**:Coherence Interface 10 条抽象事务流清单 **由本 ADR 首次定义**(per oracle B1 修复方向 (b)),作为后续 ADR 与 I 实施契约的 canonical 锚点。

### 1.2 barrier 流语义歧义风险

CEP 抽象事务流必须避免歧义 — `barrier` 流存在混入两种 barrier 语义的风险:

1. **Ordering Point barrier**:requester-scoped,排空本请求者先前在途操作(per CHI DVM-Sync 类)
2. **Group rendezvous barrier**:group-scoped,全组成员到达后 release(per NVLink SHARP / UALink barrier)

如果不钉死 barrier 流语义,无论 ADR-CEP-01 选哪个 Fabric Collective 归属方案,"NVLink rendezvous barrier 走哪条"都会成为下一个争议点。

---

## 2. 决策:Coherence Interface Canonical 10 条抽象事务流清单

| # | 流名 | 方向 | 完成纪律 | 备注 |
|---|------|------|---------|------|
| 1 | `request` | node → center | requester-scoped | 缓存请求:read / unique / writeback / evict |
| 2 | `completion` | center → node | requester-scoped | 请求完成 + data(若 inline)+ perm + error |
| 3 | `snoop` | center → node | requester-scoped | snoop 查询 / invalidate / clean / forward |
| 4 | `snoop_resp` | node → center | requester-scoped | snoop 响应:hit + state + dirty + data + txn_id |
| 5 | `writeback` | node → center | requester-scoped | 驱逐/写回:addr + data + dirty |
| 6 | `forward_data` | node ↔ node(经中心调度)| requester-scoped | 数据转发:src_addr + dst_addr + data + perm(per `ADR-CEP-05` §2 折叠映射表 不可分辨性)|
| 7 | `back_invalidate` | **center → node**| requester-scoped | 反向失效:addr + type(invalidate/clean)+ txn_id(CXL 3.0 DCOH 的 BISnp 走这里;⚠️ **方向反转例外** = `node → center`(device→host,S2M),其响应 BIRsp = M2S —— 方向反转例外说明见 `ADR-CEP-02` §2 表下) |
| 8 | `address_resolve` | node ↔ center | requester-scoped | 地址归属解析:pa + node_id → home_node + coherent_flag |
| 9 | `barrier` | center → node ↔ center(双向)| **requester-scoped**(per Decision 3) | 单请求者排空 / 可见性点;**不携带 group_id / participant_count / epoch_id** |
| 10 | `snoop_completion_ack` | node → center | requester-scoped | snoop 完成后 ack(CHI CompAck 类);不与 `completion` 流重叠 |

**不变量集合**(CEN-Ib1 ~ CEN-Ib11,与本 ADR 一一对应):
- CEN-Ib1: `request` 必有配对 `completion`(per `ADR-CEP-05` §4.3 INV-SC4 配对性)
- CEN-Ib2: `completion` 不携带 unsolicited 数据(per INV-SC4)
- CEN-Ib3: `snoop` 必有 `snoop_resp`
- CEN-Ib4: `snoop_resp` 必须含 `txn_id` 用于 ack 配对
- CEN-Ib5: `writeback` 不与 `request` 竞态(per INV-SC2 evict 互斥)
- CEN-Ib6: `forward_data` 对 stub 透明(per `ADR-CEP-05` §2 折叠映射表 不可分辨性,契约测试 1)
- CEN-Ib7: `back_invalidate` 是 requester-scoped,**不**是 group rendezvous
- CEN-Ib8: `address_resolve` 在 ProtocolEngine 内终结(per `ADR-CEP-05` §6 折叠表)
- CEN-Ib9: `barrier` 流**不**携带 group_id / participant_count / epoch_id(per Decision 3)
- CEN-Ib10: `snoop_completion_ack` 配对 `snoop_resp` 不配对 `completion`
- **CEN-Ib11**(2026-10-20 新增):atomic 意图作为 `request` 流的**独立属性**承载;`perm` 枚举**维持 4 值锁**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`),**不新增 "ATOMIC" perm 值**

**跨 ADR 一致性要求**:
- `ADR-CEP-05` §2 必须逐字对齐本表 10 条流
- `ADR-CEP-03` / `ADR-CEP-04` / `ADR-CEP-01` 引用本表时使用本 ADR canonical 名

> **⚠️ 流 7 的方向反转例外(CXL 3.0 DCOH)** —— canonical **方向列** = **`center → node`**
> (center = 一致性权威侧 · node = 请求者侧)⇒ **CXL 3.0 DCOH 的 BISnp 是方向反转例外**:
> DCOH 里**设备**持一致性代理 ⇒ BISnp = **`node → center`**(device→host,S2M);其响应 BIRsp = M2S(center→node)
> ⇒ **实现须显式承载该反转**,不得按 canonical 方向静默处理。
> ⚠️ **订正史**:原写「host→device 方向」与方向列**自洽但事实错**(公开来源已反证,per `07 号` Entry #154 + 锚点摘要 §6 #2)
> ⇒ 2026-11-03 首次订正**只改备注** ⇒ 与方向列**互相排斥**(critic B1)
> ⇒ 本次按**最小修法**收敛:**方向列不动**(保 canonical 语义 + 4 处零 diff)⇒ 把 DCOH 的方向写成**显式例外**。

### 2.0 选项对比表(定稿补入 —— 「barrier 归谁」这一决策的可审计取舍)

| 方案 | 结论 | 理由 |
|---|---|---|
| **A** `barrier` 流承载 **rendezvous**(group-scoped) | ❌ **否决** | `barrier` 流的完成纪律是 **requester-scoped**;改判 group-scoped ⇒ 死锁分析需**同时**用两套纪律 ⇒ 违反本 ADR §3.2「两套纪律**不能混用同一分析工具**」;且 `group_id` / `participant_count` / `epoch_id` 进 Coherence Interface ⇒ 破坏**协议无关性** |
| **B** `barrier` 流**保持 requester-scoped**;rendezvous 走 `collective_barrier` | ✅ **采纳** | 与 `ADR-CEP-01` 完成域判别规则**单一规则**推导一致;**canonical 10 流零变更**;两平面各自保持单纯(「复合流」由 Core 层编排) |
| **C** 另加**第 11 条流** `collective_barrier` 进 Coherence Interface | ❌ **否决** | 同 **A** 的协议无关性问题 + 破坏 **canonical 10 流**不变量(per 决策 1) |
| **D** 把 `barrier` **从** Coherence Interface **移出** | ❌ **否决** | DVM-Sync / CXL Fence / GPU sys-scope atomic release 的语义**就是** requester-scoped 可见性点(per 决策 3)⇒ 移出即**丢语义** |

> ⚠️ **决策 1(零流数变更)是硬约束** ⇒ atomic / Fence / 未来需求**只能**塞进既有 10 流 ⇒ 每次扩展都要重新论证「哪一条流承载」(代价见 §6)。


### 决策 1(精简):零流数变更(守 canonical 10 条流清单)

- **不增加** Coherence Interface 抽象事务数量(10 → 11)
- **不修改** 任何现有流的字段语义
- 本 ADR 是首次定义(canonical),不修改既有(per oracle B1 修复方向 (b))

### 决策 2:CXL Fence 走 barrier 流

CXL Fence 的语义 = requester-scoped("我之前的写对 X 可见后再前进"),**留在 coherence `barrier` 流**。

**理由**:CXL 是 host 中心的内存一致性,无 group 概念,barrier 语义本质是 DVM-Sync 类的"我之前的写对 X 可见后再前进" → Ordering Point。

**未来扩展**:若未来出现多 CXL 设备会合需求,再按完成域规则判到 Fabric Collective Plane — 不需要现在预留。
### 决策 3:barrier 流钉死为 Ordering Point 语义

**barrier 流的语义 = requester-scoped**(requester-scoped 完成纪律):

- **barrier 类型**:quiescence point / visibility point(静默/可见性点)
- **完成条件**:本请求者先前所有在途 coherent 操作已获最终 ack(CompData / SnpResp_I 等),数据可见性已建立
- **典型场景**:
  - CHI DVM-Sync(`之前 DVM 操作全局可见后再前进`)
  - CXL Fence(`之前写对 X 可见后再前进`)
  - GPU sys-scope atomic release 语义(排空部分)
- **不承载**:rendezvous barrier(NVLink SHARP / UALink INC barrier)— 这类走 Fabric Collective Plane 的 `collective_barrier` 原语

**关键不变量**(per ADR-CEP-01 完成域判别规则):
```
requester-scoped completion → Coherence flow (含 `request` / `completion` / `snoop` / `snoop_resp` / `writeback` / `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`,10 条流 per §2 的 **canonical 10 流表** —— ⚠️ **2026-11-03 订正**(per 台账 Entry #170 ⑤-补 ⑫):原写「§2 Decision 0」**悬空**(该表无「决策 0」标题)⇒ 改指表本身;per oracle v0.3 S10 修订:初版漏列 `forward_data` 和 `barrier`)
group-scoped completion     → Fabric Collective Plane (含 collective_barrier)
```


---

> **⚠️ v0.3 补充(per `ADR-CEP-17 §2 Decision 6` + oracle 全面架构审查 Q7)**:新增 **CEN-Ib11** —— atomic 意图的承载方式。
>
> **一手依据**:ProtoGen ISCA 2018 p.7 局限 L2:"ProtoGen **cannot generate new protocol actions not explicitly specified in the SSP**. For example, **it cannot infer how atomic read-modify-writes must be implemented without it being specified in the SSP**" —— atomic RMW 是**协议动作**,须由协议规格显式给出,不能由框架推导。
>
> **结论**:atomic 的**语义**归 Protocol Engine 插件;atomic 的**意图**通过流 1 `request` 的**独立属性**(与 `perm` 正交)传递。**canonical 10 流不变**;**`perm` 枚举不变**。
>
> **配套**:一致性标签(SC / TSO / RC / PLO)对同一 `barrier` 的效果差异归 `ADR-CEP-12` 的 `consistency_labels`,**不改本 ADR 的流语义**(per `ADR-CEP-17` 决策 7 / F7)。

## 3. 关键约束

## 3.1 barrier 分类 litmus test(per ADR-CEP-01 §3.1)

```text
// 判断标准:
// 1. barrier 的 key 是地址还是参与者组?
//    地址 → Ordering (per-address 串行化)
//    组   → Collective (group rendezvous)
// 2. 完成条件数的是事务还是参与者?
//    数事务 (drain/flush) → Ordering
//    数参与者 (N 个成员到达) → Collective
```

**复合流**:`release-drain → rendezvous` = Ordering drain + Collective rendezvous 的复合,由 Core / Traffic Generator 层编排。

## 3.2 barrier 流的契约规范

```text
barrier 流接口信号(per ADR-CEP-02 锁定语义):
  barrier_valid     // 请求者发起 barrier
  barrier_ready     // 协议插件 ack
  barrier_domain    // 一致性域 ID(⚠️ **2026-11-03 订正**:原写「per `ADR-CEP-13` 待起草」**失实** —— `ADR-CEP-13` 已起草(v0.1),其决策 1 明确 `barrier_domain` 是**作用域标签** ≠ 域边界;per NUMBER-MAP §2 Decision 1 — per oracle B3-6 实证:ADR-CEP-11 不在预分配表)
  barrier_txn_id    // txn_id(沿用 ADR-X.1-transaction-id.md 命名,非独立定义)
  barrier_direction // 0: center→node, 1: node→center
```

**禁止**:
- barrier 流**不携带** `group_id` 字段(per ADR-CEP-01 完成域判别规则 + CEN-Ib9)
- barrier 流**不携带** `participant_count` 字段(同上)
- barrier 流**不携带** `epoch_id` 字段(同上,group lifecycle 属 Collective Plane)

## 3.3 关于 `barrier_direction` 字段说明

`barrier_direction` 字段(per 初版"06-bis v0.3 雏形"已 oracle B6 删 — 06-bis 全文无 barrier 章节):本字段由本 ADR **首次定义**,允许 barrier 在两个方向上进入(0: center→node, 1: node→center)。ProtocolEngine 适配器层按此区分(CHI 内部 barrier 走方向 0;CXL Fence 走方向 1)。

---

## 4. 与 P2.0-P5 演进路线对齐(per `06-inter-module-network.md` v0.4 §15.4,**无 P6**,per oracle v0.3 B3-7 实证)

| 06 §15.4 阶段 | barrier 流承担 | collective_barrier 承担 |
|--------------|---------------|----------------------|
| **P2.0** CoherenceProtocol C++ 实现 | CHI DVM-Sync 类 drain barrier 实现 | — |
| **P2.1** CoherenceBridge C++ 实施 | barrier 流跨域映射(per-domain barrier) | — |
| **P3** SnoopFilter directory-based | barrier 流 + Ordering Point 配合 | — |
| **P3.5** JSON 端口路径打通 | barrier 域 ID 解析 | — |
| **P4** CHI 完整协议 | CHI CompAck + barrier 完整语义 | — |
| **P5** IOMMU 集成 | CXL Fence 走 barrier 流(NVLink barrier 走 Collective) | NVLink SHARP / UALink INC barrier 走 collective_barrier |

---

## 5. 关联 ADR

- 本体:`06-inter-module-network.md` v0.4 §2.1 L1-L6 仿真层(正交,非 barrier 流真源)
- 配套:`ADR-CEP-01-fabric-collective-plane.md`(旁路平面决策 + 完成域判别规则)
- 配套:`ADR-CEP-03-fabric-collective-plane-internals.md`(collective_barrier 接口)
- 配套:`ADR-CEP-05-stub-cache-core-interface.md`(折叠映射表引用本 ADR canonical 10 流)
- 关联:`06-bis-coherence-as-network-extension.md` v0.3 §7.3-§7.7 阶段范围(P2.0-P5,**§7.8 非阶段范围**,per `ADR-CEP-00A §3.2` 实证)— 未来 supersede by CEP ADR-CEP-00
- 关联:`ADR-X.1-transaction-id.md`(txn_id 命名空间沿用)

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 10 条流清单 canonical | §2 的 **canonical 10 流表**(⚠️ 2026-11-03 订正:原写「§2 Decision 0」**悬空**) | ✅ 首次定义(per B1) |
| 跨 ADR 一致性 | 后续 ADR 引用本表 10 条流时使用 canonical 名 | ✅ 已拍板 |
| barrier 流语义 | requester-scoped (Ordering Point) | ✅ 已拍板 |
| rendezvous barrier 归属 | Fabric Collective Plane `collective_barrier` | ✅ 已拍板 |
| 流数变更 | 零变更(守 canonical 10 流) | ✅ 已拍板 |
| CXL Fence | 走 barrier 流(非 future) | ✅ 已拍板 |
| barrier_direction 字段 | 本 ADR 首次定义,允许两个方向 | ✅ 首次定义(per B6) |
| barrier_txn_id 命名 | 沿用 `ADR-X.1-transaction-id.md` snake_case 命名 | ✅ 已拍板(per S4) |

---

## 7. 后果 / 代价

### 7.1 正面

| # | 后果 |
|---|---|
| 1 | canonical **10 流一次定义**、跨 ADR 逐字对齐(`ADR-CEP-02 §2` ↔ `07 号 §3.1` ↔ schema `flow_support` ↔ `ADR-CEP-05` 折叠映射表)⇒ **单一真源**;`perm` **4 值锁** + atomic 意图走 `request` 的**独立属性** ⇒ 协议插件可表达 atomic 意图而**不**扩 perm 值域(不破坏 `CEN-Ib11`,也不动 `ADR-CEP-12` schema) |
| 3 | barrier 分类 litmus **入正文** ⇒ 「rendezvous 走错平面」这一类**静默死锁**有机械判据(而非口头约定,per `ADR-CEP-01 §3.3` 抓取点 5) |
| 4 | `CEN-Ib1 ~ CEN-Ib11` 是**跨 ADR 不变量集合** ⇒ 协议族的**共享假设**集中在 11 条 ⇒ 新协议族接入时**逐条核对**即可 |

### 7.2 负面 / 代价

| # | 代价 |
|---|---|
| 1 | **零流数变更**是硬约束 ⇒ atomic / Fence / 未来语义**只能**塞进既有 10 流 ⇒ 每次扩展都要**重新论证**承载流;承载不合适时**无处可去** ⇒ 只能改字段语义(⇒ 更强的评审压力) |
| 2 | `barrier_direction` **两个方向**可表达 ⇒ 适配器层需按方向分支 ⇒ 判据与测试成本翻倍 |
| 3 | `CEN-Ib1 ~ CEN-Ib11` 是**跨 ADR 契约** ⇒ 任一 ADR 改动都要**复核 11 条** ⇒ 变更成本集中化(优点是可审计) |
| 4 | atomic 作为 `request` 的**独立属性**(与 `perm` 正交)⇒ `perm` 4 值锁下**无法**从 perm 看出 atomic ⇒ 消费方**必须**读该属性 ⇒ 漏读即语义错 |

### 7.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| 1 | **口径漂移**:§2 表 / `07 号 §3.1` / schema `flow_support` / `ADR-CEP-05` 四处任一漂移 | 机器 diff(§8 **V1**)⇒ 漂移即红 |
| 2 | **方向类错误**(本 ADR 定稿前**已发生一次**:流 7 的 BISnp 方向) | 已订正 + 逐条复核公开来源(per 锚点摘要)⇒ **不再**只改 07 号而不同步 ADR |
| 3 | **行号锚点脆弱**:实施笔记用 `ADR-CEP-02:55` 形式引用 ⇒ 改行即失准(已登记,per 台账 Entry #169 ⑦) | 建议**改节号引用**(`ADR-CEP-02 §2`)⇒ 本 ADR 定稿后**该类行号锚点一律视为待修** |

---

## 8. Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)

| # | 判据 | 载体 | 反向 fixture(毒化 ⇒ 必红 + 归因 id) |
|---|---|---|---|
| **V1** | canonical **10 流清单零 diff**(§2 表 ↔ `07 号 §3.1` ↔ schema `flow_support` ↔ `ADR-CEP-05`) | 机器 diff(四方) | 改任一侧的流名 / 顺序 / 条数 ⇒ diff 必红(`CEN-10-DIFF`) |
| **V2** | `barrier` 流**不**携带 `group_id` / `participant_count` / `epoch_id` | 静态扫描 + 运行期 | **纯负向 ⇒ 须配正控**:同一扫描器**必须**确认 `barrier` 流**确实**携带 `txn_id` 与 `barrier_direction`(⇒ 扫描器本身有鉴别力);毒化:加进 `group_id` ⇒ 必红(`BARRIER-NO-GROUP-ID`) |
| **V3** | `perm` 枚举**恰 4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`) | 值域锁 | **正控**:某实例确实声明 4 值;毒化:加第 5 值 ⇒ 必红(`PERM-4-LOCK`) |
| **V4** | atomic 意图走 `request` 的**独立属性**,**不**占 `perm` | schema + 实例声明 | 毒化:以第 5 个 perm 值表达 atomic ⇒ 必红(`ATOMIC-NOT-PERM`);**正控**:某实例声明 `atomic.supported = true` 且 `perm` 仍 4 值 |
| **V5** | `barrier_direction` **两个方向**均可表达(0 = center→node / 1 = node→center) | 字段存在性 + 适配器分支 | **正控**:两个方向各一例通过;毒化:方向字段被删 / 恒为 0 ⇒ 必红(`BARRIER-DIRECTION`) |
| **V6** | 配对关系不重叠:`request`↔`completion` · `snoop`↔`snoop_resp` · `snoop_completion_ack` 配 `snoop_resp` **不**配 `completion` | 运行期断言 | **正控**:三类配对各一例;毒化:`snoop_completion_ack` 被改成配 `completion` ⇒ 必红(`PAIRING-OVERLAP`) |

---

## 9. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。

---

## Status Update(2026-11-02, per 台账 Entry #141)—— **R-T4 裁决**:`perm` 4 值锁 = **值域锁**

> **授权**:台账 `obj-cep-platform.md` **Entry #141** [scope-change] —— **冻结面授权记录**。
> **形态**:**纯追加** —— 本 ADR 正文(`CEN-Ib11` 条文)**原文保留不改**;本段只**解释**其口径。
> **议题**(R-T4):`CEN-Ib11` 的「`perm` 枚举维持 **4 值锁**」是
> **(a) 值域锁**(只允许这 4 个字面值)还是 **(b) 每实例必须用满 4 值**?

### ① 裁决 = **(a) 值域锁**

`CEN-Ib11` 的 4 值锁 **锁定的是「允许出现的字面值集合」**,**不要求**每个协议实例的 `perm` 数组列全 4 值。

### ② 依据(三条,均可复核)

| # | 依据 | 说明 |
|---|---|---|
| ① | **条文措辞** | `CEN-Ib11` 的原文是「`perm` 枚举**维持 4 值锁**……**不新增 "ATOMIC" perm 值**」⇒ 论域是**词汇表**(禁止新增第 5 值),**不是**「每实例覆盖度」 |
| ② | **schema 无对应用闸** | `include/tlm/cep/protocol_instance.schema.json` 的 `perm` = `{"type":"array","minItems":1,"uniqueItems":true,"items":{"enum":[4 值]}}` ⇒ **没有**任何「必须列全 4 值」的约束 ⇒ 「用满」解释**无载体支撑** |
| ③ | **与平台命题冲突** | 若解释为「必须用满」,则**任何不暴露 `UNIQUE` 的协议族**都**无法合法声明** ⇒ 与 `ADR-CEP-00` 的「协议中立 / 跨协议对比」命题**直接冲突** |

### ③ 推论(对协议实例的约束)

| 情形 | `perm` 数组 | `exposes_unique_perm` | `INV-CEP-08-IC2` 的结果 |
|---|---|---|---|
| 协议族**暴露** `UNIQUE`(有干净独占态) | 4 值(或含 `UNIQUE` 的子集) | `true` | 由 `hierarchical` 声明决定(PASS / FAIL)—— **语义真实** |
| 协议族**不暴露** `UNIQUE`(如 **TileLink-C**:投影 3→3) | **3 值** `["NONE","READ","WRITEBACK"]` | `false` | **vacuous PASS**,detail = 「perm 不含 UNIQUE ⇒ 无静默升级入口」—— **语义真实** |

⇒ ⇒ **不暴露 `UNIQUE` 的协议族 SHALL 填 3 值**。

### ④ ⚠️ 反面约束(**MUST NOT**)

| ❌ 禁止 | 后果 |
|---|---|
| 为了让数组「看起来完整」而**填 4 值**(含 `UNIQUE`) | `exposes_unique_perm` 被推为 `true` ⇒ `INV-CEP-08-IC2` 走 `hierarchical` 分支 ⇒ **PASS**,但 detail 写「UNIQUE(E 可静默升 M)」而该协议族**根本没有 `E` 态** ⇒ **语义为假的绿灯** ⇒ 平台在该维度**失去判别力** |

> 依据:`include/tlm/cep/ordering_point_checker.hh` 的 IC2 实现 ——
> `!exposes_unique_perm` ⇒ PASS + 「(vacuous PASS)」;`exposes_unique_perm && hierarchical` ⇒ PASS + 「UNIQUE(E 可静默升 M)」。
> ⇒ 后者的 PASS 文字**默认**协议族存在 `E` 态;对本仓 TileLink-C 与任何 3 值投影族**均不成立**。

### ⑤ 后果 / 代价

| 面 | 后果 |
|---|---|
| **`CEN-Ib11` 正文** | **零改动**(仅追加本段解释) |
| **`perm` 值域** | **零改动** —— 仍为 4 值,不新增第 5 值 |
| **`protocol_instance.schema.json`** | **零改动**(现有 `minItems:1` + 枚举已足以承载本裁决) |
| **对既有 4 实例的影响** | **零** —— `moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` 的 `perm` 均**已含 `UNIQUE`** ⇒ 本裁决不改变其判定 |
| **对 TileLink-C 的影响** | 实例 `perm` **填 3 值** ⇒ `no_upgrade_instances` 派生集**首次非空** ⇒ 既有断言 `no_upgrade_instances.empty()` **必红** ⇒ 第 1 个 change 须改为「恰含 `tilelink_c`」(**已在该 change 的 improvement 草稿登记**) |
| **残余** | ⚠️ 「值域锁」的措辞目前**只在本段**;若未来 continuum 需要更强的「用满」语义,须**另立 ADR**,不得回改本段 |

**关联**: 台账 `obj-cep-platform.md` **Entry #141**(授权记录)· #138(2026-11-01 三项裁决)
· `CEN-Ib11`(本条正文)· `ADR-CEP-05` §13.1(Q-3b:perm 形态演进)· `ADR-CEP-08`(IC2 定义)
· `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/ordering_point_checker.hh`
· `include/tlm/cep/perm_upgrades.json` · `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md` §7 R-T4

---

## 附录 · Verification 判据的**载体归属**与**归因 id 登记**(2026-11-03,per 台账 Entry #170)

**⚠️ 判据 id 体系(须与既有机器闸一致)** —— 本仓既有归因 id = **`CP-01`(零调用边)· `CP-02`(canonical 10 流零 diff)· `CP-03`(collective VC 预留)· `CP-04`(区域互斥)· `CP-05`(完成域判别)· `CP-06`(抓点可测性登记)**(见 `include/tlm/cep/collective_plane_contract.hh`)⇒ 本章判据**一律复用该体系**;**尚未纳入既有体系**者 ⇒ 归因 id **待队列 #13 建立**(登记于 #13 的 tasks)⇒ **禁**在本仓新造平行 id 体系。

**载体归属总表(S3 —— 每条判据落在**哪个队列的哪个载体**上)**

| # | 判据 | 载体归属 | 载体状态 |
|---|---|---|---|
| V1 | 10 流清单零 diff(四方) | 机器 diff(ADR 自解析 + collective 侧无泄漏 + schema 编译期锁) | ⚠️ **部分实现** ⇒ **07 号 §3.1 / `ADR-CEP-05` 侧 diff 未实现** ⇒ 由队列 #13 / #14 补齐(既有 id `CP-02`) |
| V2 | `barrier` 不带 `group_id` / `participant_count` / `epoch_id` | `openspec/specs/cep-core-contracts/spec.md` 的 CEN-Ib9 Scenario | ⚠️ **判据本体(测试)尚未存在** ⇒ 由队列 #13 / #14 交付 ⇒ 在此之前**不可执行**;归因 id **待 #13 建立** |
| V3 | `perm` 恰 4 值(**值域锁**,per Entry #141 R-T4) | 值域锁(`ADR-CEP-02 §2` CEN-Ib11 + `include/tlm/cep/perm_upgrades.json` + 实例声明判据) | ✅ **已存在**;⚠️ **不要求每实例列全 4 值**(`tilelink_c` 填 3 值**合法**) |
| V4 | atomic 走 `request` 独立属性 | `include/tlm/cep/protocol_instance.schema.json` 的 `atomic.supported` / `atomic.intent_field` | ✅ **已存在**;归因 id **待 #13 建立** |
| V5 | `barrier_direction` 两方向 | 字段 + 适配器分支 | ⚠️ **载体尚不存在**(`barrier_direction` 全仓零实现;`include/tlm/cep/protocol_engine_base.hh` 的 `on_barrier(domain, txn_id)` 无方向参数)⇒ 由队列 #13 / #14 交付 ⇒ 在此之前**不可执行**;归因 id **待 #13 建立** |
| V6 | 配对关系不重叠 | `openspec/specs/cep-core-contracts/spec.md` | ⚠️ **判据本体(测试)尚未存在** ⇒ 由队列 #13 / #14 交付 ⇒ 在此之前**不可执行**;归因 id **待 #13 建立** |

> ⚠️ **悬空判据禁令**(per Entry #66 对抓取点 2 / 7 的处置):**载体尚不存在**者 ⇒ 该判据在载体落地前**不可执行** ⇒ **禁**写成恒真 / 恒 SKIP 判据。

> ⚠️ **纪律**(per 队列 #12 沉淀的 4 铁律):① **纯负向断言须配正控** ② 每条判据须有**可执行反向 fixture 且因正确原因红**(须写明**载体**与**归因 id**)③ **编译期锁不得只以「编译红」充当 ctest 判据**。


---

## 定稿说明(2026-11-03,per 台账 **Entry #169** —— 队列 #13 `cpptlm-cep-nvlink-sharp` 的**开工前置**)

> **性质**:本段是**追加式**(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §8 **正文内容**除下列**两处订正**外**一律不改**。

**① 本次定稿的实质**

| # | 动作 | 说明 |
|---|---|---|
| 1 | **流 7 的 BISnp 方向订正** | 原写「CXL 3.0 BISnp 走这里,**host→device 方向**」⇒ **方向反了**;公开来源已反证(BISnp = **S2M device→host**;其响应 BIRsp 才是 M2S)⇒ 订正为 **device→host(S2M)**;⚠️ `07 号` 已于 **Entry #154** 订正同一口径 ⇒ 本 ADR 同步(per `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md` §6 #2) |
| 2 | **决策编号重排** | 原序 = 决策 3 → 决策 1(精简) → 决策 2(**乱序**);现序 = 决策 1 → 决策 2 → 决策 3。⚠️ **标签不变** ⇒ 外部引用(如 `ADR-CEP-03` 的「`ADR-CEP-02` Decision 3」)**不断** |
| 3 | 补 **选项对比表**(§2.0)/ **后果·代价**(§7)/ **Verification**(§8) | per 产出形态契约的 4 必备要素(元数据 ✓ / 选项对比表 / 后果·代价 / Verification)⇒ 本次补齐后三者 |

**② 关联 change**:`cpptlm-cep-stub-cache-core` · `cpptlm-cep-directory-ordering-plane` · `cpptlm-cep-moesi-gpu-adapter` ·
`cpptlm-cep-mesif-adapter` · `cpptlm-cep-cxl-nonsymmetric` · `cpptlm-cep-tilelink-tree-manager` ·
`cpptlm-cep-cxl-dcoh-symmetric`(均 ✅ 已归档)· `cpptlm-cep-nvlink-sharp`(队列 #13 · 📋 Planned)·
`cpptlm-cep-ualink-inc`(队列 #14 · 📋 Planned)

**③ ⚠️ canonical 10 流清单本身**零改动**(仅流 7 的**备注文字**订正)⇒ `CEN-Ib1 ~ CEN-Ib11` 不变量集合**逐条不变**
⇒ `ADR-CEP-05` 的折叠映射表 / `07 号 §3.1` / schema `flow_support` 的对齐关系**不受影响**。

**④ 行数口径**:**正文行数** = §1 ~ §8(**不含**修订记录段与 Status Update 追加段)⇒ 本 ADR 正文 = **248 行**(2026-11-03 定稿后实测;口径 = §1 ~ §8 + Verification 章,**不含**修订记录 / Status Update 追加段 / **附录**)≤ 250(per 产出形态契约)。

**⑤ ⚠️ 未随之处置者(已登记,不在本次范围)**

| 项 | 位置 | 处置 |
|---|---|---|
| **实施笔记的行号锚点** | `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md` 的 `ADR-CEP-02:55` / `:66` 形式引用 | ⚠️ 该文件属**实施笔记**(per `obj §4.7.1` append-only;**不在** architect 的 `arch_paths` 内)⇒ 本次**不改**;⇒ **建议**改用**节号**引用(`ADR-CEP-02 §2`)⇒ 登记为 backlog(per 台账 Entry #170 ⑤-补 / obj §10) |
| **一决策一文档的存量越界** | 本 ADR 含 **4** 个决策(决策 0 表 + 决策 1 / 2 / 3) | per 产出形态契约「**存量文档不追溯**」⇒ **不**在本次拆分 ⇒ 登记为 backlog(per 台账 Entry #169 ⑤) |
