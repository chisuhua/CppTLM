# TileLink-C(TileLink Tree Manager)—— 公开来源锚点摘要(Task 0 普查产物)

> **日期**:2026-11-01 · **状态**:🔵 **普查完成(硬门通过)** —— **载体形态已钉死**(3 态 × 4 事件 = 12 格,
> **零 `Unknown`**);**版本锁定缺口(R-T1)已闭合**(2026-11-01,per 台账 Entry #135 + `ADR-CEP-11` 的
> `## Status Update(2026-11-01,per 台账 Entry #135)` ⇒ 锁定版本 = **`tilelink-c-1.7.1`**,SiFive);
> ✅ **3 项 architect 裁决已全部闭合**(2026-11-01,per 台账 Entry #138 + 3 份 ADR 的
> `## Status Update(2026-11-01,per 台账 Entry #138)`):**R-T6**(非对称定义形态 → `ADR-CEP-17`)·
> **R-T2**(通道 5 → VC 4 → `ADR-CEP-09`)· **R-T3**(6 类消息类词表 → `ADR-CEP-12`)
> ⇒ ⇒ **Task 0 硬门通过** ⇒ 队列 #11 的 **change 目录可建**(规约层先行)。
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

## §1 稳定态集合(**决定载体形态**)

**结论**:稳定态 = **{ `Nothing` , `Branch` , `Tip` }**(3 值)⇒ 转移载体 = **3×3 = 9 格**(+ `Cap` 的 3 条「源不定」转移)⇒ **零 `Unknown`**。

### 1.1 两个正交维度

- **维度 A —— 副本权限**(3 值):`None` / `Read` / `Read+Write`
  「The fundamental permissions it is possible for a particular agent's copy of a block to have are
  **None, Read, or Read+Write**」(D1 §8.1 / D2 §8.1)
- **维度 B —— 树中位置**(4 类):`Nothing` / `Trunk` / `Tip` / `Branch`
  「Every node in the graph falls into one of **four** categories describing its position on the tree」(D1 §8.1)

### 1.2 ⚠️ `Trunk` **折叠**为 `Branch`,不进稳定态集合(**决策口径**,**非**规范断言)

> ⚠️ **2026-11-01 自审更正**:本节原名「`Trunk` **是过渡态**」—— 但 D1 §8.1 把 `Trunk`
> **明列为 4 类持久节点之一**(«Every node in the graph falls into one of **four**
> categories describing its position on the tree»),**不是**过渡态。
> ⇒ 本节**结论不变**(仍不进稳定态集合),但**依据改为决策口径**(见下),并**登记语义损失**(§1.2bis)。

| 断言 | 强度 | 来源 |
|---|---|---|
| 转移记号的全集只含 **N / B / T**(= `Nothing` / `Branch` / `Tip`):`Cap`=`toT,toB,toN` · `Grow`=`NtoB,NtoT,BtoT` · `Prune`=`TtoB,TtoN,BtoN` · `Report`=`TtoT,BtoB,NtoN` ⇒ **无任何 `Trunk` 参与的转移** | **direct** | D1 §8.1.4 Table 8.3 |
| `Acquire`「**grow** the tree, either by **extending the trunk** or by **adding a new branch** from an existing branch or the tip」⇒ `Trunk` 是**生长过程**中的路径节点 | **direct** | D1 §8.1.1 |
| `Trunk` 的描述(D1)=「Has **Read** permissions on its copy, **which may contain dirty data**」;`Branch`=只读副本;`Tip`=「the **point of memory access serialization**」+ Read/Write 权限 + **may contain dirty data** | **direct** | D1 §8.1 |
| `Nothing` = 无副本、既无读权限也无写权限 | **direct** | D1 §8.1 |
| **`Trunk` 与 `Branch` 的访问权限完全相同**(均**仅** `Get`);只有 `Tip` 是 `Get, PutPartial, PutFull, Logical, Arithmetic` ⇒ 在「**权限**转移」模型里 `Trunk` 可**折叠**为 `Branch` | **direct** | D1 §8.1 **Table 8.1** |
| **权限转移表有规范的穷尽性逐字背书**:「Table 8.3 shows **all the permissions transitions any coherence policy based on TileLink could want to express**」⇒ `Grow`(源=`Tip`)与 `Prune`(源=`Nothing`) 的**空集是规范明确**的,而非「规范未覆盖」 | **direct** | D1 §8.1.4 **末段** |

⇒ ⇒ **决策口径(三步推导,全部 `direct`)**:

1. **Table 8.1**:`Trunk` 与 `Branch` 的**访问权限完全相同**(均仅 `Get`)⇒ 在权限层面二者**不可区分**
2. **Table 8.3** 的转移记号只有 **N / B / T**,且 `Grow`/`Prune` 按规范释义必须是**单调**的升级/降级
   ⇒ `T` 的**唯一自洽读法是 `Tip`** —— 若 `T` = `Trunk`,则 `BtoT` 两边都只有 `Get`(**不构成升级**),
   且该表将**无法表达任何进入 `Tip` 的转移** ⇒ 无法表达「取得写权限」
3. ⇒ **稳定态 = {`Nothing`, `Branch`, `Tip`}**;`Trunk` 在**权限转移模型**中**折叠**为 `Branch`

⇒ ⇒ ⚠️ **这是决策,不是规范断言** —— `Trunk` **确实存在**(D1 §8.1 的 4 类持久节点之一),我们只是
**决定不在权限转移模型中单独建模它**。依据是 Table 8.1 的访问权限等价 + Table 8.3 的单调性要求。

### 1.2bis ⚠️ **已登记的语义损失**(折叠的代价 —— 必须随载体一起登记)

| 面 | 损失 |
|---|---|
| **副本存在性** | `Trunk` **有**副本 ⇒ 因此**只能**折叠为 `Branch`,**不得**折叠为 `Nothing` |
| **脏数据** | `Trunk` 的 *may contain dirty data* **在 4 值 `perm` 中无值可表达**:`NONE` 丢「有副本」· `READ` 丢脏性 · `WRITEBACK` **越权**(`Trunk` 只有 `Get`)· `UNIQUE` 要求**干净** |
| **树位置** | `Trunk` 是「**Tip → Root 路径上**」的**位置**属性;`perm` 不建模位置 ⇒ 位置信息**全部丢失** |

⇒ ⇒ 载体 **MUST** 在**偏离清单**登记上述三条;**MUST NOT** 声称「`Trunk` 不存在」或「`Trunk` 是过渡态」。

### 1.3 ⚠️ 版本差异(D2 = 1.9.3)

| 断言 | 强度 | 来源 |
|---|---|---|
| D2 把节点类别改为 **5** 类:`Nothing` / `Trunk` / `Tip (with no Branches)` / `Tip (with Branches)` / `Branch` | **direct** | D2 §8.1 |
| D2 的 `Trunk` 表述变为「Has **neither read nor write** permissions on the copy. The copy **may be out of date** with respect to writes occurring at the Tip」(**与 D1 表述不同**) | **direct** | D2 §8.1 |
| D2 的 `Table 29` 给 `Trunk` 一项 `Get` 访问 —— **与 D2 自身正文矛盾**(正文说无读/写权限)⇒ 见 R-T5 | **direct**(矛盾事实本身) | D2 §8.1 Table 29 |

⇒ ⇒ **版本差异是实质的**(态数 3→4/5、事件词表 10→12)⇒ **钉版必须精确到版本**(R-T1)。

---

## §2 事件词表(**决定编译期断言**)

### 2.1 三个 transfer operations(D1 §8.1.1)

| 操作 | 方向 | 语义 |
|---|---|---|
| **`Acquire`** | master → slave | 创建副本 / 升级权限 |
| **`Release`** | master → slave | 自愿放弃副本 / 权限 |
| **`Probe`** | slave → master | 强制移除副本 / 权限 |

### 2.2 四个权限转移类别(D1 §8.1.4 Table 8.3)—— **事件分类的天然载体**

| 类别 | 转移 | 语义(D1 原文要点) |
|---|---|---|
| **`Cap`** | `toT` `toB` `toN` | 「权限变化**不指定原权限**,只指定**应变成什么**」 |
| **`Grow`** | `NtoB` `NtoT` `BtoT` | 「权限升级,**长树**,并记录原权限与新权限」 |
| **`Prune`** | `TtoB` `TtoN` `BtoN` | 「权限降级,**缩树**,并记录原权限与新权限」 |
| **`Report`** | `TtoT` `BtoB` `NtoN` | 「**空操作**,权限不变,但**报告当前权限**」 |

⇒ ⇒ **事件词表 = 4 类**(`Cap` / `Grow` / `Prune` / `Report`)——
⚠️ 这是**比 CXL 的 17 事件、MESIF 的 6 事件都更紧**的分类,且**每类有规范枚举的转移**。

### 2.3 逐格覆盖 ⇒ **零 `Unknown`**

- `Grow`(3) + `Prune`(3) + `Report`(3) = **9 条**,恰好覆盖 {N,B,T} 上的**全部 9 个有序对**;
- `Cap`(3) = 「到 T / 到 B / 到 N」,**源态不定** ⇒ 3 条**通配**转移。

⇒ ⇒ **3 态 × 4 事件 = 12 格,每格都有规范枚举的转移支撑** ⇒
⇒ **载体可产出 `pending=false` 格**(对比 CXL:68 格中 42 格 `Unknown`)。

### 2.4 消息表(**版本差异显著**)

| 版本 | TL-C 消息数 | 消息 |
|---|---|---|
| **D1(1.7.1)** | **10** | `Acquire`(A) · `Grant`(D) · `GrantData`(D) · `GrantAck`(E) · `Probe`(B) · `ProbeAck`(C) · `ProbeAckData`(C) · `Release`(C) · `ReleaseData`(C) · `ReleaseAck`(D) |
| **D2(1.9.3)** | **12** | D1 的 10 条 **+ `AcquireBlock` / `AcquirePerm`(代 `Acquire`)** **+ `ProbeBlock` / `ProbePerm`(代 `Probe`)** ⇒ **净增 2 条** |

> ⚠️ D2 修订记录明写:「**1.8.0 — Added TL-C Opcodes and Permissions**」
> ⇒ ⇒ **`AcquireBlock`/`AcquirePerm` 与 `ProbeBlock`/`ProbePerm` 的拆分是 1.8.0 才引入的**,
> **1.7.1 没有这层区分** ⇒ **事件词表随版本变化** ⇒ R-T1 的实质影响。

| 断言 | 强度 | 来源 |
|---|---|---|
| 「Across the **five** channels, TL-C specifies **ten** messages comprising **three** operations」 | **direct** | D1 §8.1.3 |
| 「The prioritization of messages across channels is **A < B < C < D < E**, in order of increasing priority」 | **direct** | D1 §4.2 · D2 §2 |
| 「…the message flow through all channels between all agents remains a **DAG**. This is a **necessary property** for TileLink to remain **deadlock free**」 | **direct** | D1 §4.2 · D2 §2 |
| 早期版本(0.3.1)用 **Acquire / Probe / Release / Grant / Finish** 模型,**无** Block/Perm 区分 | **direct** | D3 §1 |
| TL-C 客户端**收 B 通道探针、发 C 通道 release、发 E 通道 grant ack** | **direct** | D5(`pin=weak`) |

### 2.5 五个通道的**语义**(canonical 10 流映射的**直接依据**,D1 §8.1.2)

| 通道 | 方向 | D1 原文语义要点 |
|---|---|---|
| **A** | master → slave | 「A master **initiates acquiring permission** to read or write a copy of a cache block」 |
| **B** | slave → master | 「A slave **queries or modifies** a master's permissions on a cached data block, or forwards a memory access」 |
| **C** | master → slave | 「A master **acknowledges a Channel B message**, potentially releasing permissions along with any dirty data. **Also used to voluntarily write back dirtied cache data**」 |
| **D** | slave → master | 「A slave **provides data or permissions** to the original requestor, granting access. **Also used to acknowledge voluntary writebacks** of dirty data」 |
| **E** | master → slave | 「A master provides **final acknowledgment of transaction completion**, used by the slave for **transaction serialization**」 |

---

## §3 `perm` 投影(**3→3**,含 ⚠️ `UNIQUE` 不可达)

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

| 稳定态 | 副本权限 | 独占? | 可能脏? | 投影(**保守**) | 强度 |
|---|---|---|---|---|---|
| `Nothing` | None | — | — | **`NONE`** | direct |
| `Branch` | Read | 否 | 否(只读副本,非写者) | **`READ`** | direct |
| `Tip` | Read+Write | **是**(唯一写者) | **是** | **`WRITEBACK`** | direct(独占)+ pending(干净性) |

⇒ ⇒ **投影 = 3→3**,实际用到 `{NONE, READ, WRITEBACK}`。

### ⇒ 三条**结构性**结论

1. **`UNIQUE` 永不出现** ⇒ ⇒ **TileLink-C 实例不应声明 `UNIQUE → WRITEBACK` 静默升级**
   ⇒ ⇒ 也**不触发** `INV-CEP-08-IC2`(`perm` 含 `UNIQUE` **且**声明静默升级 ⇒ 才要求 hierarchical 组合能力)
   ⇒ ⚠️ 与 `chi_rnf` / `cxl_2_0`(均声明了静默升级)**形态不同** ⇒ 属**合法差异**,非不一致。
2. **`perm` 字段可只列 3 值**:schema 的 `perm` 是 `array` + `minItems: 1` + 枚举项
   ⇒ **无**「必须列全 4 值」的闸 ⇒ `["NONE","READ","WRITEBACK"]` 合法
   (⚠️ `mesif` / `chi_rnf` / `moesi_gpu` / `cxl_2_0` 当前**都列了 4 值** —— 这是**它们的合法选择**
   (各自都暴露 `UNIQUE`),**不是**对本协议族的约束;口径已于 **R-T4**(2026-11-02)裁决为**值域锁**)。
3. **「`perm` 4 值锁」是**值域**锁,不是「每实例用满 4 值」**
   ⇒ ✅ **已裁决**(2026-11-02,per 台账 **Entry #141** + `ADR-CEP-02` 的 `## Status Update(2026-11-02, per 台账 Entry #141)`)⇒ **R-T4 已闭合**。

---

## §4 canonical 10 流映射(per `ADR-CEP-02` §2)

**结论**:TileLink-C **无独立流**的项有 **5** 条 ⇒ 该实例是**目前最窄**的。

| # | canonical 流 | TileLink 载体(D1 §8.1.2 通道语义) | `flow_support` |
|---|---|---|---|
| 1 | `request` | **A**:`Acquire` | **true** |
| 2 | `completion` | **D**:`Grant` / `GrantData` | **true** |
| 3 | `snoop` | **B**:`Probe` | **true** |
| 4 | `snoop_resp` | **C**:`ProbeAck` / `ProbeAckData` | **true** |
| 5 | `writeback` | **C**:`Release` / `ReleaseData` —— D1 原文「**Also used to voluntarily write back dirtied cache data**」 | **true** |
| 6 | `forward_data` | **无独立流** —— 数据恒附着于 `GrantData` / `ProbeAckData` / `ReleaseData` | **false** |
| 7 | `back_invalidate` | **无独立流** —— 强制撤销由 **B**:`Probe` 承载(属 `snoop`) | **false** |
| 8 | `address_resolve` | **无** —— 路由静态(D2 §6.3:「every valid address must route over a single path to exactly one slave」) | **false** |
| 9 | `barrier` | **无** —— 规范无 barrier 消息 | **false** |
| 10 | `snoop_completion_ack` | **无独立流** —— `ProbeAck` 本身即 ack(配对已在 `snoop_resp` 内);**E**:`GrantAck` 配对的是 **completion** 而非 `snoop_resp` ⇒ CEP canonical 10 流**无**该位置 | **false** |

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
   `vc_id` 的 schema 上限是 **3** ⇒ **最多 4 个 VC**(`protocol_instance.schema.json:218`)。
3. TileLink 5 通道 → 6 类消息类的自然映射:

   | 通道 | 消息 | CEP 消息类 | 方向 |
   |---|---|---|---|
   | A | `Acquire` | `request` | master → slave |
   | B | `Probe` | `snoop` | slave → master |
   | C | `ProbeAck` / `ProbeAckData` | `snoop_resp` | master → slave |
   | C | `Release` / `ReleaseData` | ⚠️ **类归属歧义**(见 R-T3) | master → slave |
   | D | `Grant` / `GrantData` | ⚠️ **类归属歧义**(CEP 6 类里是 `response`,但 canonical 10 流里叫 `completion`) | slave → master |
   | E | `GrantAck` | **无对应类** | master → slave |

⇒ ⇒ **两个独立问题**:
- **(P-1) 类词表缺口**:CEP 6 类词表**无**「对 completion 的 ack」类 ⇒ `GrantAck` 无处可放;
  `Release` / `ReleaseData` 的类归属也需裁决(⚠️ `writeback` 是 **10 流名**,**不是**消息类名 ⇒ 两套词表被混用)。
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

**TileLink 的事实**(D1 §8.1.2 的 5 通道语义):

| 方向 | 消息 | 类别 |
|---|---|---|
| master → slave | `Acquire`(A) · `Release`/`ReleaseData`(C) · `GrantAck`(E) | **请求获得** / **自愿放弃** / **确认** |
| slave → master | `Probe`(B) · `Grant`/`GrantData`(D) | **强制撤销** / **授予** |

⇒ ⇒ **request 类消息(A 与 B)**双向都有 ⇒ **CXL 式定义不适用**。
⇒ ⇒ **真正单向的是「权限的授予」与「权限的强制撤销」** —— 二者**都只由 slave 发出**。

⇒ **可判定义(3 维度 ⇒ 维度 ①② 采用)** ✅ **已裁决**(2026-11-01,per 台账 **Entry #138** + `ADR-CEP-17` 的 `## Status Update`;R-T6 已闭合):
| 维度 | TileLink 表述 | 可判? |
|---|---|---|
| ① 权限**授予**方向 | **只有 slave 能授予**(`Grant` 仅出现在 D 通道) | ✅ 可判(逐消息查通道) |
| ② 权限**强制撤销**发起方 | **只有 slave 能发起**(`Probe` 仅出现在 B 通道) | ✅ 可判 |
| ③ request 类消息方向 | **双向**(A 与 B)⇒ **不可**用 CXL 式定义 | ❌ |

⇒ ⇒ **不能把队列 #10 的「非对称」断言形态直接套到 TileLink-C** ⇒ 需 architect 裁决 TileLink 的定义。

---

## §7 8 项残余风险(**不得推理填空** —— 截至 2026-11-02:**8 项全部已闭合**[R-T1 / R-T2 / R-T3 / R-T4 / R-T5 / R-T6 / R-T7 / R-T8])

| # | 残余风险 | 等级 | 处置 |
|---|---|---|---|
| **R-T1** | ~~**版本锁定缺口**~~ ⇒ ✅ **已闭合**(2026-11-01,per 台账 **Entry #135** + `ADR-CEP-11` 的 `## Status Update(2026-11-01,per 台账 Entry #135)`)—— `ADR-CEP-11 §3` 锁表现为 **`tilelink-c-1.7.1`**(SiFive;PDF sha256 `30c09b5f…a2dd`)。**历史事实保留**:原锁 `tilelink-c-1.3` 的正文**至今取不到**(≥6 次尝试:riscv.org 404 / RISC-V adoc 不存在 / `openspecification/tilespec` 仓库 **404** / GitHub 搜索无命中)⇒ **该版本永久不可确证** ⇒ 本裁决是**改锁**,**不是**「已取得 1.3」 | **✅ 已闭合** | 台账 Entry #135(授权记录)+ `ADR-CEP-11` SU |
| **R-T2** | ~~**通道 5 → VC 4 冲突**~~ ⇒ ✅ **已闭合**(2026-11-02,per 台账 **Entry #141** + `ADR-CEP-09` 的 `## Status Update(2026-11-02, per 台账 Entry #141)`)—— 裁决 = **合并 D∪E 到同一 VC**,**结论不变**,但**论证已换**:① **不再依赖**「VC 内有序(FIFO)」(该前提**假**:`D` slave→master 与 `E` master→slave **方向相反** ⇒ **无跨方向 FIFO**)⇒ 改用**因果创建关系**(`GrantAck` **只能在 `Grant` 到达后被创建** ⇒ 「E 等 D」是**生成顺序**,与队列是否 FIFO **无关**);② 依赖集 **3 条 → 4 条**:`vc0→vc3` · `vc1→vc2` · `vc1→vc3` · **`vc2→vc3`**(`Release`/`ReleaseData` → `ReleaseAck`,per D1 `Table 5.2`,原**漏**);③ ⚠️ **点名**:D1 §4.2.1「response always has higher priority than its initiating request」⇒ `E` > `D` 的**严格优先级被合并丢弃** ⇒ 登记为**第 4 条降维代价**(实现层须由 vc3 内仲裁补偿);④ ⚠️ **登记**:装配闸的 `compute_noc` 边表 `[[0,1],[0,2],[0,3],[1,3],[3,2]]` 与本 SU 的边表**不同**(有 `0→1`/`0→2`/`3→2`,缺 `1→2`/`2→3`)⇒ 「闸通过」**不构成** SU 论证正确的证据 | ✅ **已闭合** | 台账 Entry #141 |
| **R-T3** | ~~**6 类消息类词表缺口**~~ ⇒ ✅ **已闭合**(2026-11-01,per 台账 **Entry #138** + `ADR-CEP-12` 的同 Entry `## Status Update`)—— 裁决 = **6 类词表零改动** + 逐消息归属表:`Acquire`→`request` · `Probe`→`snoop` · `ProbeAck`/`ProbeAckData`/`Release`/`ReleaseData`→`snoop_resp` · `Grant`/`GrantData`/`ReleaseAck`/`GrantAck`→`response`;⚠️ **2 处降维已登记**(`Release` 归 `snoop_resp` / `GrantAck` 归 `response`);⚠️ **两套词表混用**的处置 = `writeback` **只**是 10 流名、**不得**写进 `vc_binding` | **✅ 已闭合** | 台账 Entry #138 |
| **R-T4** | ~~**`perm` 退化为 3 值**~~ ⇒ ✅ **已闭合**(2026-11-02,per 台账 **Entry #141** + `ADR-CEP-02` 的 `## Status Update(2026-11-02, per 台账 Entry #141)`)—— **裁决 = 「`perm` 4 值锁」是值域锁**(只锁**允许出现的字面值集合**,**不要求**每实例用满 4 值)⇒ 依据 3 条:① `CEN-Ib11` 的措辞是「**不新增** "ATOMIC" perm 值」⇒ 论域是词汇表;② schema 的 `perm` = `array` + `minItems:1` + 枚举 ⇒ **无**「必须列全」的闸;③ 若解释为「用满」,则不暴露 `UNIQUE` 的协议族**无法合法声明** ⇒ 与「协议中立」命题冲突。**推论**:TileLink-C 的实例 `perm` SHALL 填 **3 值** `["NONE","READ","WRITEBACK"]`。⚠️ **反面**:填 4 值 ⇒ `exposes_unique_perm=true` ⇒ IC2 走 `hierarchical` ⇒ **PASS 但 detail 写「UNIQUE(E 可静默升 M)」而 TileLink 无 E 态** ⇒ **语义为假的绿灯** | ✅ **已闭合** | 台账 Entry #141 |
| **R-T5** | ~~**两版各有内部表述不一致**~~ ⇒ ✅ **已闭合**(2026-11-01,自审复核)—— ① D1 §8.1.4 正文「**six** subsets」vs 表列 **4** 类(下一段又说 "four")⇒ **规范内部笔误**,以 **4 类**为准;② D1 `Table 8.3` 表头 `Permissions: None, Branch, **Trunk**` 而转移记号用 **T = Tip** ⇒ **图例行笔误**(`Table 8.1` 证明 `Trunk` 与 `Branch` 权限相同 ⇒ 图例应为 `None, Branch, Tip` 或直接给字母);③ D2 正文说 `Trunk` 无读/写权限而 `Table 29` 给 `Trunk` 一项 `Get` ⇒ **D2 内部矛盾**(D2 仅作版本对照,不进载体);④ ⚠️ **关键**:D1 §8.1.4 **末段有穷尽性逐字背书** —— 「Table 8.3 shows **all the permissions transitions any coherence policy based on TileLink could want to express**」⇒ `Known-empty` 2 格(`Grow` 源=`Tip` / `Prune` 源=`Nothing`)可判 **`direct`**,**不依赖**「以记号为准」的推断 | ✅ **已闭合** | 矛盾**显式保留登记**(供规范修订时复核);载体逐格 `basis` 可全 `direct` |
| **R-T6** | ~~**「非对称」定义**~~ ⇒ ✅ **已闭合**(2026-11-01,per 台账 **Entry #138** + `ADR-CEP-17` 的同 Entry `## Status Update`)—— 裁决 = TileLink-C 的非对称维度 = 「**权限的授予与强制撤销只由 manager 发起**」(`Grant`/`GrantData` **只**在 D 通道、`Probe` **只**在 B 通道);⚠️ **与 CXL 形态的差异已显式登记**(CXL = request 方向 / TileLink = 权限变更发起方)⇒ **两者不可互相套用** ⇒ 队列 #10 的断言形态**不得**直接套用 | **✅ 已闭合** | 台账 Entry #138 |
| **R-T7** | ~~**`Trunk` 的建模归属**~~ ⇒ ✅ **已闭合**(2026-11-01,自审复核)—— **决策口径**(非规范断言):`Table 8.1` 中 `Trunk` 与 `Branch` 的访问权限**完全相同**(均仅 `Get`) ⇒ 权限层面折叠为 `Branch`;`Table 8.3` 的 `Grow`/`Prune` 单调性要求 ⇒ `T` 唯一自洽读法 = `Tip`。⚠️ **`Trunk` 本身确实存在**(D1 §8.1 的 4 类持久节点之一),**不得**表述为「过渡态」;⚠️ **代价已登记**(§1.2bis 三条语义损失:副本存在性 / 脏数据 / 树位置) | ✅ **已闭合** | 载体按 3 态实现 + 偏离清单登记 §1.2bis 三条 |
| **R-T8** | ~~**1.3 与 1.7.1/1.9.3 的谱系关系**~~ ⇒ ✅ **已闭合**(2026-11-01,per 台账 **Entry #135** §⑤ + `ADR-CEP-11` 的 `## Status Update(2026-11-01,per 台账 Entry #135)`)—— 新锁表值写作 **`tilelink-c-1.7.1`** ⇒ 格式为 `<spec_name>-<spec_version>` ⇒ **版本线歧义不再存在**(值本身唯一指向 SiFive 的 1.7.1)⇒ 且 §3 表「锁定依据」列已**写明发布方 + PDF sha256** ⇒ 同类歧义**不再复发**。**历史事实保留**:原锁 `tilelink-c-1.3` 的正文**至今取不到** ⇒ 该版本**永久不可确证** | **✅ 已闭合** | 台账 Entry #135 §⑤ |

---

## §8 普查结论(硬门判定)

**判定**:🔵 **普查完成,但载体形态未钉死** —— ⇒ **Task 0 硬门 = 有条件通过**(照队列 #10 的同款形态)。

| 项 | 判定 |
|---|---|
| 公开来源可得性 | ✅ **良** —— TileLink 规范**开源可取**(3 份 `pin=strong` + sha256)⇒ **远优于**队列 #10(CXL 全程禁读) |
| 决定载体形态的 5 项 | ✅ **已钉死** —— **R-T1 已闭合**(锁版本 = `tilelink-c-1.7.1`)⇒ **内容可推导 + 版本已定** ⇒ 可产出 `pending=false` 格 |
| 态集合 | ✅ **可推导** —— **3 值** `{Nothing, Branch, Tip}`(`Trunk` **折叠**为 `Branch`;决策口径见 §1.2,R-T7) |
| 事件词表 | ✅ **可推导** —— **4 类** `{Cap, Grow, Prune, Report}` + **10 条**消息(D1)/ 12 条(D2) |
| 逐格覆盖 | ✅ **零 `Unknown`** —— 9 个有序对全被 `Grow`/`Prune`/`Report` 覆盖 + `Cap` 3 条通配 ⇒ **12 格全 Known** |
| `perm` 投影 | ✅ **3→3** —— `Nothing→NONE` · `Branch→READ` · `Tip→WRITEBACK`;⚠️ `UNIQUE` 不可达 ⇒ ✅ **已裁决**(R-T4 = **值域锁** ⇒ 实例填 3 值;2026-11-02 Entry #141) |
| canonical 10 流 | ✅ **可推导** —— 5 条 `true` / 5 条 `false`(§4) |
| 非对称定义 | ✅ **已裁决**(R-T6;2026-11-01 Entry #138)= 「权限的授予与强制撤销只由 slave 发起」 |
| 通道 → VC 冲突 | ✅ **已闭合**(R-T2;2026-11-02 Entry #141)= 合并 D∪E 到 vc3 + **因果依赖**论证 + 依赖集 4 条 |
| 硬门效力 | ✅ **通过** ⇒ **change 目录可建**(`cpptlm-cep-tilelink-tree-manager`;规约层先行,照 #10 两段式先例) |
| ⇒ 前置动作 | ✅ **R-T1 / R-T6 / R-T2 / R-T3 全部闭合**(Entry #135 + Entry #138)⇒ **版本不再是阻塞点**;**非对称定义形态**已裁决(§6 + `ADR-CEP-17` SU);**通道 → VC 映射**与**6 类归属**已裁决(§5 + `ADR-CEP-09` / `ADR-CEP-12` SU)<br>⇒ **下一步**:**可建 change 目录** ⇒ 起队列 #11 的**第 0 个 change(规约层)** |

### ⇒ 版本建议(**供 architect 裁决参考**)

| 候选 | 事件词表 | 态集合 | 内部一致性 | 生态 | 评价 |
|---|---|---|---|---|---|
| **`tilelink-c-1.7.1`**(D1) | **4 类转移** + **10 条**消息 | **3 稳定态**(`Trunk` **折叠**为 `Branch`,§1.2) | 2 处内部不一致(**均已闭合**,R-T5:转移记号为准 + 穷尽性句背书) | OpenTitan TL-UL 基线 | **✅ 推荐** —— 转移分类表**显式且完备** ⇒ 零 `Unknown`;事件词表最紧 |
| `tilelink-c-1.9.3`(D2) | 4 类转移 + **12 条**消息 | **4~5 稳定态**(Tip 拆两类) | 1 处内部不一致(**影响** Trunk 投影) | 最新(2023-02) | 备选 —— 最新但态数膨胀 + `Trunk` 矛盾落在投影上 |
| `tilelink-c-1.3` | **不可取** | **不可确证** | — | RISC-V 谱系未证实 | ❌ **不可行**(R-T1/R-T8) |

---

## §9 来源清单(钉版信息)

| # | 来源 | 版本 / 日期 | 取字节 | sha256 | 钉版 | 许可 |
|---|---|---|---|---|---|---|
| **D1** | SiFive TileLink Specification —— `https://sifive-china.oss-cn-zhangjiakou.aliyuncs.com/TileLink/TileLink-TileLink%20Spec%201.7.1.pdf` | **1.7.1** / 2018-12-03 | ✅ 735 145 B | `30c09b5fc16a4222b57b8ce1afe8a3dc56b7a66a33b4b2fa48a1864640f5a2dd` | **strong** | © 2018 SiFive Inc.(**非** CC-BY) |
| **D2** | SiFive TileLink Specification —— `https://sifive.cdn.prismic.io/sifive/928d6a82-77a9-4291-8b60-5e815429b1ab_tilelink_spec_1.9.3.pdf` | **1.9.3** / 2023-02-09 | ✅ 7 369 907 B | `0d28c6f735bb806e3ba823af18aaad08d3dfa6c51f16a320e05570dfa321e37a` | **strong** | © 2023 SiFive Inc. |
| **D3** | TileLink Specification **0.3.1**(早期 Acquire/Probe/Grant/Release/Finish 模型)—— `https://git.tiband.de/riscv/rocket-chip/raw/commit/d774afaf736c7a90a53ecf0c8e24f99f54c70538/uncore/doc/TileLink0.3.1Specification.pdf` | **0.3.1** | ✅ 172 795 B | `8caab6cc0082bc9c6373f5f7dba4729cecc58dfedf1a5749d72b9d2d3df474fc` | **strong** | © SiFive Inc. |
| **D4** | Synopsys 博客(通道优先级 `A<<B<<C<<D<<E`) | 2024 | ❌ | — | `weak` | 二手 |
| **D5** | Chipyard 文档(TL-C 客户端的 B/C/E 通道行为) | — | ❌ | — | `weak` | 二手 |
| **D6** | OpenTitan `TL-UL` 文档(明写「based on TileLink specification version **1.7.1**」) | — | ❌ | — | `weak` | 二手 |

> **取字节说明**:D1 / D2 / D3 均由本环境 `curl` 实取并算得 sha256(2026-11-01)⇒ `pin=strong`。
> ⚠️ **未取到**:`tilelink-c-1.3` 对应的规范正文 ⇒ **本摘要不得声称代表 1.3**。
> ✅ **锁定版本 = `tilelink-c-1.7.1` = D1**(2026-11-01,per 台账 Entry #135 + `ADR-CEP-11` SU)
> ⇒ **载体 basis 的唯一规范源 = D1**;D2(1.9.3)/ D3(0.3.1)仅作**版本对照**用,不得当载体依据。

---

## §10 关联

- **上位裁决**:台账 `obj-cep-platform.md` **Entry #134**(Task 0 硬门结论 + 版本裁决升级)
  · **Entry #135**(**R-T1 闭合的授权记录**)+ `ADR-CEP-11` 的 `## Status Update(2026-11-01,per 台账 Entry #135)`
  · **Entry #138**(**R-T2 / R-T3 / R-T6 闭合的授权记录**)+ `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17`
  的 `## Status Update(2026-11-01,per 台账 Entry #138)`
- **版本锁真源**:`docs/adr/ADR-CEP-11-protocol-version-locking.md` §3 锁表(**冻结面** · 变更须两步授权)
- **载体形态先例**:队列 #10 的 `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md`
  (CXL **全程禁读规范正文** ⇒ basis 全来自公开非规范来源;**TileLink 相反** ⇒ basis **直接来自规范**)
- **ADR**:`ADR-CEP-04`(SharerTracker caps · `TreeManager` = `SUBTREE`/`EXACT`/`LOCAL`/`TREE_PARENT` + hazard `CHAIN`)
  · `ADR-CEP-09`(多 VC 仲裁 + 死锁自由 · Decision A/B 两段式)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)
  · `ADR-CEP-12`(实例声明 schema · 6 类消息类词表)· `ADR-CEP-17`(协议族接入)
- **载体落点**(计划):`include/tlm/cep/tilelink_state_spec.hh`(规约载体)· `include/tlm/cep/tilelink_cache_adapter.hh`(实现层 adapter)
  · `include/tlm/cep/sharer_tracker.hh` 的 `TreeManager` 行为实现(第 **4** 种 tracker)
