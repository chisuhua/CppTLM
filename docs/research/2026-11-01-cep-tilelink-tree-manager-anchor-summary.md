# TileLink-C(TileLink Tree Manager)—— 公开来源锚点摘要(Task 0 普查产物)

> **日期**:2026-11-01 · **状态**:🔵 **普查完成(有条件通过)** —— **载体形态未钉死**:
> **版本锁定缺口**(R-T1)未闭合 ⇒ 决定载体形态的 5 项**全部 `pending=true`**;
> 另发现 **1 项头号架构风险**(通道 5 → VC 4 的映射冲突,R-T2)+ **3 项待裁决设计点**。
> · **用途**:队列 #11 `cpptlm-cep-tilelink-tree-manager` 的 **basis 唯一人工审计面**
>   (per `ADR-CEP-11` Status Update ①-c / ④ 的同款定位)
> · **上位裁决**:台账 `obj-cep-platform.md` **Entry #134** · `docs/adr/ADR-CEP-11-protocol-version-locking.md` 的 `## Status Update(2026-11-01)`

---

## §0 范围与合规声明(⚠️ 必读)

| 项 | 内容 |
|---|---|
| **⛔ 合规** | **TileLink 无 CXL 类禁令** —— SiFive 明示 TileLink 是「a **free and open standard**」(D1/D2 §1)⇒ **规范正文可自由投喂 AI** ⇒ 本摘要**直接引用规范条款**(与队列 #10 的 CXL 情形**相反**:CXL 全程**未读**规范正文) |
| **⚠️ 但仍有 IP 边界** | 规范 © SiFive Inc.(D1: © 2018;D2: © 2023)**非** CC-BY ⇒ 本摘要仍**只记条款级定位 + 断言摘要**,**不整段复制**原文段落 |
| **强度三档** | `direct`(该来源**明确这样写**)· `analogical`(跨协议类比)· `inferred`(推理)⇒ **禁止升格** |
| **⚠️ 钉版强度分级** | `pin=strong`(本环境**取到字节**并算得 sha256)· `pin=weak`(仅 URL + 抓取日)· `pin=transcribed`(原文取不到,仅二手转述)⇒ 载体侧 `public-direct` **SHALL 只接受 `pin=strong`** |
| **上游无实现** | Linux 无 TileLink 协议栈;QEMU 无 TL-C;OpenTitan 只有 **TL-UL**(非缓存一致性层)⇒ **本仓无现成 TL-C 实现可抄** ⇒ 规约层必须自建载体 |
| **不整段复制** | 本摘要只记**条款级/文献级定位 + 断言摘要**,**不复制原文段落**(IP 风险) |

---

## §1 客户端可见「稳定态」集合(**决定载体形态** —— ⚠️ **未钉死**)

**结论**:TileLink-C 给的是**两个正交维度**,不是**一组**稳定态:

- **维度 A —— 副本权限**(3 值):`None` / `Read` / `Read+Write`
- **维度 B —— 树中位置**(节点类别):`Nothing` / `Trunk` / `Tip` / `Branch`

| 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|
| 「The fundamental permissions it is possible for a particular agent's copy of a block to have are **None, Read, or Read+Write**」 | **direct** | D1 §8.1 · D2 §8.1 | D1/D2 均 `pin=strong` |
| 「Every node in the graph falls into one of **four** categories describing its position on the tree」= `Nothing` / `Trunk` / `Tip` / `Branch` | **direct** | D1 §8.1 | D1 `pin=strong` |
| D2(1.9.3)把节点类别改为 **5** 类:`Nothing` / `Trunk` / `Tip (with no Branches)` / `Tip (with Branches)` / `Branch` | **direct** | D2 §8.1 | D2 `pin=strong` |
| `Nothing` = 无副本、既无读权限也无写权限 | **direct** | D1 §8.1 | D1 `pin=strong` |
| `Trunk`(D1 表述)= 「Has **Read** permissions on its copy, **which may contain dirty data**」 | **direct** | D1 §8.1 | D1 `pin=strong` |
| `Trunk`(D2 表述)= 「Has **neither read nor write** permissions on the copy. The copy **may be out of date** with respect to writes occurring at the Tip」 | **direct** | D2 §8.1 | D2 `pin=strong` |
| `Tip` = 「the **point of memory access serialization**」(1.9.3 细化为「point of **write** serialization」)· 有 Read/Write 权限 · **may contain dirty data** | **direct** | D1 §8.1 · D2 §8.1 | D1/D2 `pin=strong` |
| `Branch` = 只读副本 | **direct** | D1 §8.1 · D2 §8.1 | D1/D2 `pin=strong` |
| D2 的 `Table 29`(权限 ↔ 允许的访问)给出:`None`→无;`Branch`→Get;`Trunk`→Get;`Tip(w/ Branches)`→Get;`Tip(no Branches)`→Get/PutPartial/PutFull/Logical/Arithmetic | **direct** | D2 §8.1 Table 29 | D2 `pin=strong` |
| ⚠️ **D2 内部表述不一致**:D2 正文说 `Trunk`「neither read nor write permissions」,**同一份** `Table 29` 却给 `Trunk` 一项 `Get` 访问 | **direct**(矛盾事实本身) | D2 §8.1 | D2 `pin=strong` |

### ⇒ 载体形态的**两个候选**(均**未钉死**,见 R-T1)

| 候选 | 态集合 | 优点 | 缺点 |
|---|---|---|---|
| **(S-a) 树位置维度** | 4 类(1.7.1)/ 5 类(1.9.3) | 与 CXL 的 4 态**数量对齐**;直接对应 `TreeManager` 的 `SUBTREE` scope | 「位置」**不是** client 的协议状态 ⇒ 投影到 `perm` 需跨维度推理 |
| **(S-b) 副本权限维度** | 3 值 `{None, Read, Read+Write}` | 语义上**就是**「client 的副本能做什么」⇒ 与 `perm` 投影**同维度** | 丢失「是否独占」信息 ⇒ `UNIQUE` vs `WRITEBACK` **无法区分**(见 §3) |

⇒ ⚠️ **不得先选后补**:两个候选产出的 `perm` 投影**形状不同**(4→4 vs 3→4)⇒ 属**载体级**决策。
⇒ **须先闭合 R-T1(版本)** ⇒ 再由 architect 裁决 (S-a) / (S-b)。

---

## §2 事件词表与通道(**决定编译期断言**)

**结论**:TL-C 消息族 = **12 条**(不含 TL-UL/TL-UH 的 Get/Put/Atomic/Hint 族),分布在 **5 个通道**。

| 通道 | 方向 | 承载(TL-C 部分) | 语义 |
|---|---|---|---|
| **A** | client → manager | `AcquireBlock` · `AcquirePerm` | 请求**权限** |
| **B** | manager → client | `ProbeBlock` · `ProbePerm` | 查询 / 撤销副本权限 |
| **C** | client → manager | `ProbeAck` · `ProbeAckData` · `Release` · `ReleaseData` | 对 probe 的应答 / 释放权限(可带脏数据) |
| **D** | manager → client | `Grant` · `GrantData` · `ReleaseAck` | **授予**权限(可带数据) / 释放确认 |
| **E** | client → manager | `GrantAck` | cache block 传输的最终握手 |

| 断言 | 强度 | 来源 | 钉版 |
|---|---|---|---|
| 「The prioritization of messages across channels is **A < B < C < D < E**, in order of increasing priority」 | **direct** | D1 §4.2 · D2 §2 | D1/D2 `pin=strong` |
| 「Priorities ensure that messages flowing through the TileLink network **never enter a routing or hold-and-wait loop** … the message flow through all channels between all agents remains a **DAG**. This is a **necessary property** for TileLink to remain **deadlock free**」 | **direct** | D1 §4.2 · D2 §2 | D1/D2 `pin=strong` |
| `Table 13` 逐消息给出「通道 / opcode / Operation / Response」对应(TL-C 12 条) | **direct** | D2 §9.3 Table 13 | D2 `pin=strong` |
| `AcquireBlock` → `Grant` 或 `GrantData`;`AcquirePerm` → `Grant`;`ProbeBlock` → `ProbeAck` 或 `ProbeAckData`;`ProbePerm` → `ProbeAck`;`Release`/`ReleaseData` → `ReleaseAck`;`Grant`/`GrantData` → `GrantAck` | **direct** | D2 §9.3 Table 13 | D2 `pin=strong` |
| 早期版本(0.3.1)用 **Acquire / Probe / Release / Grant / Finish** 五消息模型,**无** Block/Perm 之分 | **direct** | D3 §1 | D3 `pin=strong` |
| TL-C 客户端**收 B 通道探针、发 C 通道 release、发 E 通道 grant ack** | **direct** | D5 | `pin=weak`(二手) |

### ⇒ 载体侧编译期结构断言(**V-3** 形态,per 队列 #10 先例)

- 态集合大小 == §1 选定候选的态数(4 / 5 / 3 **待 R-T1 + 裁决**定)
- 事件词表大小 == 12(TL-C 消息族;**不含** TL-UL/TL-UH 的 Get/Put/Atomic/Hint)
- **通道数(5) 与 VC 数(4) 不等** ⇒ 编译期须显式登记该失配(见 §5 / R-T2)

---

## §3 `perm` 投影(**形状待定** + ⚠️ **`UNIQUE` 不可达**的发现)

**CEP 的 `perm` 4 值语义**(per `ADR-CEP-02` CEN-Ib11 4 值锁 + `ADR-CEP-11` 的 CXL 投影行
`I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK` 反推):

| `perm` 值 | 语义 |
|---|---|
| `NONE` | 无副本 |
| `READ` | 有副本 · 可读 · **干净** |
| `UNIQUE` | 有副本 · **独占**可写 · **干净** |
| `WRITEBACK` | 有副本 · 可写 · **脏**(独占与否**不表达**) |

**TileLink 侧的事实**:规范**只说**副本「**may** contain dirty data」⇒ **不承诺**干净/脏
⇒ ⇒ **`UNIQUE`(干净独占)在 TileLink-C 投影中不可确证**。

| 候选投影(以 (S-b) 3 值权限维度为例) | 投影结果 | 强度 |
|---|---|---|
| `None` | `NONE` | direct(权限维度直接读出) |
| `Read`(Trunk / Branch) | `READ` **或** `WRITEBACK` —— 取决于副本是否脏 ⇒ **不可确证** | ⚠️ 需保守降级 |
| `Read+Write`(Tip) | `WRITEBACK`(独占但**可能脏** ⇒ 保守) | direct(独占性)+ pending(干净性) |

### ⇒ 三条**结构性**结论(**均须进载体设计**)

1. **保守降级方向唯一**:凡副本**可能脏** ⇒ 一律投影 `WRITEBACK` ⇒ **`UNIQUE` 永不出现**
   ⇒ ⇒ **TileLink-C 实例不应声明 `UNIQUE → WRITEBACK` 静默升级**
   ⇒ ⇒ 也**不触发** `INV-CEP-08-IC2`(`perm` 含 `UNIQUE` 且静默升级 ⇒ 才要求 hierarchical 组合能力)
   ⇒ ⚠️ 与 `chi_rnf` / `cxl_2_0`(均声明了静默升级)**形态不同** ⇒ 属**合法差异**,非不一致。
2. **`perm` 字段可只列 3 值**:schema 的 `perm` 是 `array` + `minItems: 1` + 枚举项
   ⇒ **无**「必须列全 4 值」的闸 ⇒ `["NONE","READ","WRITEBACK"]` 合法
   (但 `mesif` / `chi_rnf` / `moesi_gpu` / `cxl_2_0` 当前**都列了 4 值** ⇒ 需确认口径,见 R-T4)。
3. **`UNIQUE` 不可达 ⇒ CEP 的 `perm` 在 TileLink-C 上退化为 3 值** ⇒
   ⚠️ 这是**平台层**问题:「`perm` 4 值锁」是**值域**锁(4 值**上界**),不是「每实例必须用满 4 值」
   ⇒ 需确认该口径已被显式承认(见 R-T4)。

---

## §4 canonical 10 流映射(per `ADR-CEP-02` §2)

**结论**:TileLink-C **无独立流**的项有 **5** 条 ⇒ 该实例是**目前最窄**的。

| # | canonical 流 | TileLink 载体 | `flow_support` | 依据 |
|---|---|---|---|---|
| 1 | `request` | A:`AcquireBlock` / `AcquirePerm` | **true** | D2 Table 13 |
| 2 | `completion` | D:`Grant` / `GrantData` | **true** | D2 Table 13 |
| 3 | `snoop` | B:`ProbeBlock` / `ProbePerm` | **true** | D2 Table 13 |
| 4 | `snoop_resp` | C:`ProbeAck` / `ProbeAckData` | **true** | D2 Table 13 |
| 5 | `writeback` | C:`ReleaseData`(释放权限**并回传脏数据**) | **true**(承载为 `ReleaseData`) | D2 Table 13 + D1 §8.1「may contain dirty data」 |
| 6 | `forward_data` | **无独立流** —— 数据恒附着于 `GrantData` / `ProbeAckData` / `ReleaseData` | **false** | D2 Table 13(无独立消息) |
| 7 | `back_invalidate` | **无独立流** —— 权限撤销由 B:`ProbePerm` 承载(属 `snoop`) | **false** | D2 Table 13 |
| 8 | `address_resolve` | **无** —— TileLink 路由静态(「every valid address must route over a single path to exactly one slave」) | **false** | D2 §6.3 |
| 9 | `barrier` | **无** —— 规范无 barrier 消息 | **false** | D2 Table 13(TL-C 12 条内无) |
| 10 | `snoop_completion_ack` | **无独立流** —— `ProbeAck` **本身**即 ack(配对已在 `snoop_resp` 内) | **false** | D2 Table 13 |

⚠️ **对照 `chi_rnf`**:`chi_rnf` 的 `forward_data` / `snoop_completion_ack` 均为 `true`
⇒ ⇒ **两者取值不同是合法的**(协议族差异),但**须在 `protocol_instances.json` 的散文里说明**,
否则会被误读成「漏填」(per 队列 #10 的「计数口径澄清」先例)。

---

## §5 ⚠️ **通道 5 → VC 4** 的映射冲突(**头号残余风险 R-T2**)

**事实链**:
1. TileLink 有 **5** 个通道(A/B/C/D/E),**规范自带**全序 `A < B < C < D < E` ⇒ **DAG** ⇒ 死锁自由
   (per D1 §4.2 / D2 §2)—— 这是**规范级**的死锁自由论证。
2. CEP 的 `vc_binding` **消息类词表 = 6 类** `{request, response, snoop, snoop_resp, atomic, query}`
   (per `ADR-CEP-12` 决策 6 + ArchForge 32 号 §6.0.1),**每个消息类恰一个 `vc_id`**,且
   `vc_id` 的 schema 上限是 **3** ⇒ **最多 4 个 VC**(per `protocol_instance.schema.json:218`)。
3. TileLink 的 5 通道 → 6 类消息类的自然映射:

   | 通道 | 消息 | CEP 消息类 | 方向 |
   |---|---|---|---|
   | A | AcquireBlock / AcquirePerm | `request` | client → manager |
   | B | ProbeBlock / ProbePerm | `snoop` | manager → client |
   | C | ProbeAck / ProbeAckData | `snoop_resp` | client → manager |
   | C | Release / ReleaseData | `writeback` 系(`snoop_resp`?)⚠️ **类归属歧义** | client → manager |
   | D | Grant / GrantData | `response`(→ CEP 无 `response` 类,只有 `completion`)⚠️ **类归属歧义** | manager → client |
   | E | GrantAck | **无对应类** | client → manager |

⇒ ⇒ **两个独立问题**:
- **(P-1) 类词表缺口**:CEP 6 类词表**无**「对 completion 的 ack」类 ⇒ `GrantAck` 无处可放;
  `Release` / `ReleaseData` 的类归属也需裁决(`snoop_resp`? `writeback`? —— `writeback` **不在** 6 类词表里,
  它是 **10 流**的名字,不是**消息类**名 ⇒ ⚠️ 两套词表被混用,须分清)。
- **(P-2) VC 数不足**:5 通道 → ≤4 VC ⇒ **至少 2 个通道必须合并** ⇒
  **合并会破坏 `A<B<C<D<E` 的全序** ⇒ 依赖图上可能出现**真实存在的环** ⇒
  ⇒ **装配期闸(`INV-CEP-09-IC1`)可能误报**(过近似 ⇒ 保守拒绝,per `ADR-CEP-09` 决策 2 前提 2)。

⇒ ⚠️ **本项不是 builder 可自决的** ⇒ 需 **architect 裁决**:
① 扩 `vc_id` 上限到 4(=5 通道保真)—— 动 schema + `ADR-CEP-12` + `ADR-CEP-09`(**冻结面**);
② 合并通道 + 登记保守拒绝的代价;
③ 重新定义 6 类消息类词表(动 `ADR-CEP-12` —— **冻结面**)。

---

## §6 「非对称」的可判定义(**与 CXL 不同型** ⇒ 需独立裁决)

**CXL 的非对称**(队列 #10 已裁决):`H2D Req` **只含 snoop**、`D2H Req` **只含 local** ⇒
「**请求类消息只在一个方向出现**」。

**TileLink 的事实**:
- **A 通道**(client→manager)与 **B 通道**(manager→client)**都是 request 类**
  ⇒ ⇒ **TileLink 的 request 是双向的** ⇒ **CXL 式定义不适用**。
- **真正单向的是「权限的授予 / 撤销」**:
  - **授予**只由 manager 发(`Grant` / `GrantData`,D 通道)⇒ **D 通道单向**;
  - **撤销**只由 manager 发起(`ProbeBlock` / `ProbePerm`,B 通道)⇒ **B 通道单向**;
  - client 侧只**请求**(A 通道)与**应答 / 释放**(C/E 通道)。

⇒ **候选可判定义(3 维度,待裁决)**:
| 维度 | TileLink 表述 | 可判? |
|---|---|---|
| ① 权限授予方向 | **只有 manager 能授予**(Grant 仅出现在 D 通道) | ✅ 可判(逐消息查通道) |
| ② 权限撤销发起方 | **只有 manager 能发起撤销**(Probe 仅出现在 B 通道) | ✅ 可判 |
| ③ request 类消息方向 | **双向**(A 与 B)⇒ **不可**用 CXL 式定义 | ❌ |

⇒ ⇒ **不能把队列 #10 的「非对称」断言形态直接套到 TileLink-C** ⇒ 需 architect 裁决 TileLink 的定义。

---

## §7 8 项残余风险(**只能靠规范正文 / 人工裁决确定** ⇒ 不得推理填空)

| # | 残余风险 | 等级 | 处置 |
|---|---|---|---|
| **R-T1** | **版本锁定缺口** —— `ADR-CEP-11 §3` 锁 `tilelink-c-1.3`;本环境**取不到 1.3 正文**(≥6 次尝试全失败:riscv.org 404 / RISC-V adoc 不存在 / `openspecification/tilespec` 仓库 **404** / GitHub 搜索无命中);而可取的 **1.7.1** 与 **1.9.3** 属 **SiFive 版本线**、且**语义已漂移**(Trunk 权限表述变更 + 节点类别 4→5)⇒ **不得拿 1.7.1/1.9.3 冒充 1.3** ⇒ 决定载体形态的 5 项**全部 `pending=true`** | **🔴 阻塞** | **architect 裁决**(见 §10) |
| **R-T2** | **通道 5 → VC 4 冲突** —— 见 §5;合并通道会破坏规范自带的 `A<B<C<D<E` 无环论证 ⇒ 装配期闸可能误报 | **🔴 高** | **architect 裁决**(见 §5 ③ 个选项) |
| **R-T3** | **6 类消息类词表缺口** —— `GrantAck` 无对应类;`Release` / `ReleaseData` 的类归属歧义;`writeback` 是**流名**不是**消息类名**(两套词表混用) | 🟡 中 | 与 R-T2 合并裁决 |
| **R-T4** | **`perm` 退化为 3 值的口径** —— TileLink-C 投影**永不出 `UNIQUE`** ⇒ 该实例**不声明**静默升级 ⇒ 「`perm` 4 值锁」是**值域**锁还是「每实例用满 4 值」,需显式承认 | 🟡 中 | architect 口径确认 + 实例散文登记 |
| **R-T5** | **D2(1.9.3)内部表述不一致** —— 正文说 `Trunk` 无读/写权限,**同份** `Table 29` 却给 `Trunk` 一项 `Get` | 🟡 中 | 若钉版到 1.9.3 ⇒ 须**保守**取 `READ`(可读)⇒ 投影 `READ`;登记矛盾 |
| **R-T6** | **「非对称」定义** —— 见 §6;TileLink 的 request 双向 ⇒ CXL 式断言形态**不适用** | 🟡 中 | architect 裁决 |
| **R-T7** | **态集合维度选择** —— (S-a) 树位置 4/5 类 vs (S-b) 副本权限 3 值 ⇒ 投影形状不同(4→4 / 3→4) | 🟡 中 | 须在 R-T1 闭合**之后**裁决 |
| **R-T8** | **1.3 与 1.7.1/1.9.3 的谱系关系** —— RISC-V 的「TileLink 1.3」与 SiFive 的 1.7.1/1.9.3 是否同一版本线**未证实** ⇒ `tilelink-c-1.3` 这个**标识符本身**可能指错版本 | 🟡 中 | 与 R-T1 合并处置;若改锁表则须写明**版本线 + 发布方** |

---

## §8 普查结论(硬门判定)

**判定**:🔵 **普查完成,但载体形态未钉死** —— ⇒ **Task 0 硬门 = 有条件通过**(照队列 #10 的同款形态)。

| 项 | 判定 |
|---|---|
| 公开来源可得性 | ✅ **良** —— TileLink 规范**开源可取**(2 份 `pin=strong` + sha256)⇒ **远优于**队列 #10 的 CXL(全程禁读) |
| 决定载体形态的 5 项(态数 / 事件词表 / 投影形状 / 非对称定义 / 编译期断言) | ❌ **未钉死** —— **R-T1** 使其全部 `pending=true` |
| 事件词表 | ✅ **可钉**(TL-C 12 条 + 5 通道 + 优先级全序,D1/D2 `pin=strong`) |
| 通道 → VC 冲突 | 🔴 **新发现**(R-T2)⇒ **超出 #10 同类先例**,须 architect 裁决 |
| 硬门效力 | ⇒ **change 目录**(`cpptlm-cep-tilelink-tree-manager`)**暂不建** —— 与队列 #10 的硬门同款效力:「**公开来源普查未完成 ⇒ change 目录不建**」 |
| ⇒ 前置动作 | **须先闭合 R-T1(版本裁决)** —— 该裁决**属 architect 域**(`ADR-CEP-11 §3` 是**冻结面**) |

---

## §9 来源清单(钉版信息)

| # | 来源 | 版本 / 日期 | 取字节 | sha256 | 钉版 | 许可 |
|---|---|---|---|---|---|---|
| **D1** | SiFive TileLink Specification —— `https://sifive-china.oss-cn-zhangjiakou.aliyuncs.com/TileLink/TileLink-TileLink%20Spec%201.7.1.pdf` | **1.7.1** / 2018-12-03 | ✅ 735 145 B | `30c09b5fc16a4222b57b8ce1afe8a3dc56b7a66a33b4b2fa48a1864640f5a2dd` | **strong** | © 2018 SiFive Inc.(**非** CC-BY) |
| **D2** | SiFive TileLink Specification —— `https://sifive.cdn.prismic.io/sifive/928d6a82-77a9-4291-8b60-5e815429b1ab_tilelink_spec_1.9.3.pdf` | **1.9.3** / 2023-02-09 | ✅ 7 369 907 B | `0d28c6f735bb806e3ba823af18aaad08d3dfa6c51f16a320e05570dfa321e37a` | **strong** | © 2023 SiFive Inc. |
| **D3** | TileLink Specification **0.3.1**(早期 Acquire/Probe/Grant/Release/Finish 模型)—— `https://git.tiband.de/riscv/rocket-chip/raw/commit/d774afaf736c7a90a53ecf0c8e24f99f54c70538/uncore/doc/TileLink0.3.1Specification.pdf` | **0.3.1** | ✅ 172 795 B | `8caab6cc0082bc9c6373f5f7dba4729cecc58dfedf1a5749d72b9d2d3df474fc` | **strong** | © SiFive Inc. |
| **D4** | Synopsys 博客(通道优先级 `A<<B<<C<<D<<E`) | 2024 | ❌ | — | `weak` | 二手 |
| **D5** | Chipyard 文档(TL-C 客户端的 B/C/E 通道行为) | — | ❌ | — | `weak` | 二手 |
| **D6** | OpenTitan `TL-UL` 文档(明写「based on TileLink specification version 1.7.1」) | — | ❌ | — | `weak` | 二手 |

> **取字节说明**:D1 / D2 / D3 均由本环境 `curl` 实取并算得 sha256(2026-11-01)⇒ `pin=strong`。
> ⚠️ **未取到**:`tilelink-c-1.3` 对应的规范正文(见 R-T1)⇒ **本摘要不得声称代表 1.3**。

---

## §10 关联

- **上位裁决**:台账 `obj-cep-platform.md` **Entry #134**(Task 0 硬门结论 + 版本裁决升级)
- **版本锁真源**:`docs/adr/ADR-CEP-11-protocol-version-locking.md` §3 锁表(**冻结面** · 变更须两步授权)
- **载体形态先例**:队列 #10 的 `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`
  (CXL **全程禁读规范正文** ⇒ basis 全来自公开非规范来源;**TileLink 相反** ⇒ basis **直接来自规范**)
- **ADR**:`ADR-CEP-04`(SharerTracker caps · `TreeManager` = `SUBTREE`/`EXACT`/`LOCAL`/`TREE_PARENT` + hazard `CHAIN`)
  · `ADR-CEP-09`(多 VC 仲裁 + 死锁自由 · Decision A/B 两段式)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)
  · `ADR-CEP-12`(实例声明 schema · 6 类消息类词表)· `ADR-CEP-17`(协议族接入)
- **载体落点**(计划):`include/tlm/cep/tilelink_state_spec.hh`(规约载体)· `include/tlm/cep/tilelink_cache_adapter.hh`(实现层 adapter)
  · `include/tlm/cep/sharer_tracker.hh` 的 `TreeManager` 行为实现(第 **4** 种 tracker)
