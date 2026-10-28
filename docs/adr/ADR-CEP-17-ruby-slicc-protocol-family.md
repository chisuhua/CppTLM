# ADR-CEP-17: Ruby-SLICC 协议族接入与协议族抽象语义

> **状态**: 🟡 起草中(v0.3)
> **日期**: 2026-10-20
> **作者**: CppTLM build agent(architect mode)
> **关联调研**: [`docs/research/cep-coherence-protocol-space-survey.md`](../research/cep-coherence-protocol-space-survey.md) **v0.3**(2026-10-20,5 类 21 份源;**含 3 篇论文 PDF 原文 + 178 页博士论文 PDF 原文**;36 条引文已逐条回溯 PDF)
> **关联真源 1**: [`docs/architecture/07-coherence-exploration-platform.md`](../architecture/07-coherence-exploration-platform.md) v0.1(§5.1 协议族映射表,含 §5.1.4 学术协议族)
> **关联真源 3**:
> - [`ADR-CEP-00`](../adr/ADR-CEP-00-platform-charter-and-governance.md) §5.1(gem5 Ruby 框架级类比)
> - [`ADR-CEP-02`](../adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md)(canonical 10 流)
> - [`ADR-CEP-04`](../adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md)(Sharer Tracker caps)
> - [`ADR-CEP-05`](../adr/ADR-CEP-05-stub-cache-core-interface.md)(Stub Cache 接口)
> - [`ADR-CEP-06`](../adr/ADR-CEP-06-experiment-protocol.md)(实验输出)
> - `ADR-CEP-12`(能力映射表,待起草 — per `ADR-CEP-NUMBER-MAP §2 Decision 1` 预分配)

---

## 1. 背景

### 1.1 问题

`ADR-CEP-00 §5.1` 引用 **gem5 Ruby 类比**作为 CEP 框架级定性的支撑锚点 — 但该类比只论证了"**框架级归口**",没有回答"**CEP 能否实现 gem5 Ruby 能探索的协议空间**"。

调研(per `docs/research/cep-coherence-protocol-space-survey.md` §3)发现:

| gem5 Ruby 协议 | CEP 现状 |
|---------------|---------|
| MESI_Two_Level / MESI_Three_Level / MOESI_CMP_directory | 抽象可容纳,未列入 §5.1 清单 |
| MOESI_hammer / MOESI_AMD_Base / GPU_VIPER | 抽象可容纳,未列入 §5.1 清单 |
| **MOESI_CMP_token** | 抽象**无对应物**(token 由副本携带权限,不依赖中心 sharer set) |
| **MESI_Three_Level_HTM** | 超出范围(事务内存边界) |

同时暴露 3 处扩展需求 + 2 个状态缺口(per 调研 §1.4 / §2.5 / §2.7 / §2.8)。

### 1.2 与 gem5 Ruby 类比的关系(不矛盾,互补)

- **§5.1 类比**回答的是"**CEP 属于哪一层**(框架级,类比 gem5 Ruby 之于 gem5)"
- **本 ADR** 回答的是"**CEP 能覆盖 Ruby 协议空间到什么程度**"

两者是**同一定性的两个面**:既然 CEP 被定性为"框架级一致性探索能力",那它**应该**能覆盖 gem5 Ruby 能探索的协议空间 — 否则类比只在归口层成立,在能力层落空。

---

## 2. 决策

### 决策 1:§5.1 表格改为"协议族 → 协议实例列表"语义

**问题**(per 调研 §2.7):当前 §5.1 是"5 协议族 → 6 adapter",暗示"一族一 adapter"。但 gem5 Ruby 是 **8 个独立协议**(各自独立 `.slicc`,per `gem5-coherence-protocols-survey.md:16-23`),不是一族一 adapter。

**决策**:
- §5.1 表格语义明确为 **"协议族 → 该族的协议实例列表"**
- adapter 数量 = **协议实例数**(不是族数)
- 表述改为"**5 协议族 / 8+ 协议实例 → N 个 ProtocolEngine adapter**"

**理由**:文档语义必须与实现粒度一致,否则 builder 会误以为"一族一个 adapter 足够"。

### 决策 2:Ruby-SLICC 协议族纳入 CEP 协议空间

**新增协议族行**(per 调研 §2.4 逐协议判定):

| gem5 Ruby 协议 | 判定 | Sharer Tracker 选型 | 备注 |
|---------------|------|-------------------|------|
| MESI_Two_Level | ✅ 直接支持 | `FullMapDirectory` | 标准 2 级 directory-based |
| MESI_Three_Level | ✅ 直接支持 | `FullMapDirectory` | 3 级;多层状态机在插件内 |
| MOESI_CMP_directory | ✅ 直接支持 | `FullMapDirectory` | `dir.sm` = 中心目录 |
| MOESI_hammer | ✅ 直接支持 | `SparseSnoopFilter` | `probeFilter` 保守过滤 = caps `CONSERVATIVE` 语义 |
| MOESI_AMD_Base | ✅ 直接支持 | `SparseSnoopFilter` | 含 `probeFilter` / `Region*` / `CorePair` |
| GPU_VIPER | ✅ 直接支持 | 复用 MOESI_AMD_Base | GPU 侧 coherence 在插件内 |
| **MOESI_CMP_token** | ⚠️ 需扩展 1 | **`BroadcastNull` 或 tracker 不参与** | token 由副本携带,见决策 3 |
| **MESI_Three_Level_HTM** | ❌ **明确排除** | — | 事务内存边界,见决策 5 |

**关键映射证据**:gem5 Ruby 消息模型 → CEP canonical 10 流 **100% 覆盖**(per 调研 §2.3),**零新增流**。

### 决策 3:token 类协议的 Sharer Tracker 归属(扩展 1)

**问题**:`MOESI_CMP_token` 的 token 由数据副本携带权限,不依赖中心 sharer set — 与 Sharer Tracker 5 实现全部正交。

**决策**:
- **token 逻辑住在 Protocol Engine 插件内部**(协议自己的状态)
- Sharer Tracker 选 `BroadcastNull` 或**不参与**(协议不声明 sharer tracking 需求)
- **新增 invariant**:`INV-CEP-04-IC7` — "token 类协议下,Sharer Tracker **不跟踪 sharer set**(token 由副本携带);实现上等价 `BroadcastNull` 或 tracker 不参与"

**与决策 4 同源**:协议私有状态(token / 瞬态态)一律住插件内部。

### 决策 4:协议私有状态边界(扩展 2 — 跨 ADR 契约)

**问题**:调研 §1.4 暴露 ArchForge 09 MOESI/GPU 6 状态里有两个 CEP Stub Cache 未覆盖的状态:
- `O`(Owned):稳定但 MOESI 特有(脏且共享)
- `T`(Temporary):**瞬态状态**(协议状态机多跳事务中途)

调研 §3.1 进一步证实:Oswald **ProtoGen** 的整个方法论建立在"**从 SSP(稳定状态协议)生成非原子协议**"之上 — 瞬态状态分配是 ProtoGen 算法的**第 2 步**(per thesis §核心贡献 1)。

**决策**(跨 ADR 契约,需同步到 `ADR-CEP-05`):

| 状态类别 | 例子 | 归属 |
|---------|------|------|
| **稳定状态(SSP)** | `I / S / E / M`(CEP Stub Cache 暴露);`O`(Owned) | **Stub Cache Core** 的 `perm` 枚举 |
| **瞬态状态** | `T`(Temporary);ProtoGen 生成的 transient states | **Protocol Engine 插件内部** |
| **协议私有状态** | token;HTM 的 read-set / write-set | **Protocol Engine 插件内部** |

**新增 invariant**:`INV-CEP-05-IC1` — "Stub Cache Core 只暴露**稳定状态**;协议**瞬态状态**与**协议私有状态**(token 等)一律住在 Protocol Engine 插件内部"

**`O`(Owned)的处理**:
- **选项 A**:`perm` 扩为 `NONE / READ / UNIQUE / WRITEBACK / DIRTY_SHARED`(5 值)
- **选项 B**:`O` = `M` + sharer set 非空 组合表达
- **选项 C**:`O` 归 Protocol Engine 内部(Stub 暴露 `S` + dirty 标志)

**决策**:**选 C** — 与"协议私有状态住插件内部"边界一致,且 `perm` 枚举保持 4 值(ADR-CEP-02 已锁 `NONE/READ/UNIQUE/WRITEBACK`)。`O` 状态由插件内的协议状态机持有,Stub 侧只暴露"读权限 + dirty 标志"。

**代价**:Stub 侧无法直接区分 `S`(干净共享)与 `O`(脏共享)→ 需 dirty 标志位。**已记录为 trade-off**。

### 决策 5:HTM 明确排除(扩展 3)

**问题**:`MESI_Three_Level_HTM` = 硬件事务内存 + 缓存一致性混合,需事务边界(read-set / write-set / 事务中止)语义。CEP canonical 10 流是**纯缓存一致性**模型。

**决策**:
- **Out-of-Scope**,在 `07 号 §5.1` 显式标注 + 理由
- 若未来需要,应作为**新的协议族维度**(事务内存)引入,**不**塞进 canonical 10 流(守 `ADR-CEP-02` 零流数变更)

### 决策 6:atomic 语义归属(扩展 4 — `perm` 枚举**不扩展**)

**问题**:gem5 Ruby `RubyRequestType` 含 atomic;HeteroGen 支持的 TSO-CC / RCC 也依赖 atomic / release 语义。CEP 10 流无独立 atomic 流。

**决策**:**选 C — atomic 表达为 Protocol Engine 插件内部语义**。

**理由**:
- 与决策 3/4 边界一致(协议私有语义 → 插件内)
- 守 `ADR-CEP-02` 零流数变更承诺
- atomic 的**可观测性**(atomic 次数 / 冲突率)走 `ADR-CEP-06` 的可选 metric,不占 canonical 流

**v0.2 一手证据(本决策闭合)**:

> "Second, ProtoGen cannot generate new protocol actions not explicitly specified in the SSP. For example, **it cannot infer how atomic read-modify-writes must be implemented without it being specified in the SSP**."
> —— `ProtoGen_ISCA2018-draft.md:746-748`(ProtoGen 局限性 L2)

推论链:atomic RMW 是**协议动作** → 协议动作必须由协议规格显式给出,不能由框架推导 → 在 CEP 中 atomic 的**语义**归 Protocol Engine 插件 → atomic 的**意图**只需从流 1 `request` 传到插件。

**实现约束(v0.2 闭合)**:
- atomic 请求仍走流 1 `request`
- **`perm` 枚举维持 4 值锁**(`NONE / READ / UNIQUE / WRITEBACK`,per `ADR-CEP-02`)—— **不新增 "ATOMIC" perm 值**
- atomic 意图作为 `request` 流的**独立属性**(与 `perm` 正交)承载,由插件按其一致性标签解释
- → `ADR-CEP-02` v0.2 需同步锁定该口径(本 ADR 不单方面改动 `ADR-CEP-02` 的锁)

**同源佐证(协议/互连分离)**:ProtoGen 局限性 L4 —— "ProtoGen does not specify how protocol actions must interact with the interconnect; it requires the user to manually define virtual channels and assign messages to channels"(`:755-757`)。→ **VC / 消息路由归 Fabric & Transport Plane,协议语义归 Protocol Engine** —— 这是 CEP 分层的**第二个独立一手来源**(第一个是 ArchForge 09 §4.4 "永不复制 slicc DSL",针对语言载体;本条针对接口边界,两者互补不重叠)。

### 决策 7:学术 / 一致性模型定向协议族纳入 + 一致性标签维度(v0.2 新增)

**问题**:v0.1 的 §5.1 覆盖"工业硬件协议族"(按物理链路分)+ "Ruby-SLICC 族"(按 gem5 实现分)。但 *Gen 算法族*的协议分类维度是**一致性模型**(SC / TSO / RC / PLO),与此二者都不同 —— 且这类协议**无法**在缺"一致性标签"的情况下表达。

**一手证据**(per `HeteroGen_HPCA2022-draft.md:1266-1276` Table I + `:276-299` 标签定义):

| 协议实例 | 一致性模型 | 关键特征 |
|---------|-----------|---------|
| MSI / MESI | **SC** | SWMR + writer-initiated invalidation |
| TSO-CC | **TSO** | 放弃 sharer tracking 以利用 TSO 松弛 |
| RCC / RCC-O | **RC** | 写缓冲 + release 写回 + acquire 自失效 |
| **GPU**(源自 Spandex) | **RC** | **write-through 直写** —— 与 ArchForge 09 §4.1 的 GPU write-through 策略**直接对应** |
| PLO-CC | **PLO** | 只强制 W→W 与 R→W 序 |

**决策**:
1. **纳入**这一族为 `07 号 §5.1.4`。⚠️ **v0.3 复审 R8 澄清**:本项**不是**与 §5.1.1 / §5.1.2 并列的"第三个族",而是**协议语义的索引视图** —— "族"= **来源集合**,"一致性模型"= **实例的语义属性**。**MSI / MESI 与 §5.1.2 的 gem5 Ruby 族交叉**(同一语义的不同实现),**不重复计 adapter**
2. **硬前置条件**:`ADR-CEP-12` 的能力映射表 schema **必须**包含 **`consistency_labels`** 字段:
   ```
   consistency_labels:
     read:  SC | TSO | RC-acquire | RC-data | PLO
     write: SC | TSO | RC-release | RC-data | PLO
   ```
   否则**无法表达"同一个 `barrier` 在 SC / TSO / RC 下效果不同"**(SC 无效 / TSO 全屏障 / RC release|acquire)
3. **不新增 canonical 流**:`barrier`(流 9)的 requester-scoped 语义**不变**;一致性效果归协议标签解释
4. **规模与验证参考入库**:`ADR-CEP-06` 后续须记录 —— Murϕ(Safety + Deadlock Freedom)+ **111 个异构 litmus test** + gem5 64 核对比 HCC(平均性能相当,网络流量差异 < 5%)

**范围纪律(v0.2 补)**:**不承诺**自动协议兼容性判定 —— HeteroGen 论文明确将此列为超出其工作范围("We believe this is a challenging problem that is beyond the scope of this work",`hetergen:1219-1224`)。CEP 同此立场:兼容性由**人工声明 + 实验验证**,不提供自动判定器。

**v0.3 细化(F6 + 博士论文 §7.6.6)**:`consistency_labels` 必须支持**请求级标签**,不能只到"读/写"两级。依据:在 RCC 协议中,**普通 read 与 acquire 都发同一种 `GetV` 请求**,目录**无法区分**二者;论文的解法是把请求**改名**为 `GetV_R` / `GetV_A`。→ 若 CEP 的 `consistency_labels` 只标到"read/write"两级,则**同一 wire 请求在不同标签下语义不同**这一情形无法表达。

### 决策 8:每个协议实例必须声明"排序假设 + 一致性标签"(v0.3 新增)

**问题**:v0.2 的 8 项决策回答了"**协议空间包含什么**",但没有回答"**一个协议实例如何声明自己的前提假设**"。而 *Gen 族的经验表明,这两类假设恰恰是组合失败的根源**:

| 前提假设 | 组合失败的具体表现 | 一手依据 |
|---------|------------------|---------|
| **排序假设**(点对点序 / 无序 / 逐 VC 序) | ProtoGen 明确"**不能**从有序网络的 SSP 推出无序网络的协议" | ProtoGen 局限 L3 |
| **一致性标签**(SC / TSO / RC / PLO) | 目录**无法区分** read 与 acquire(同为 `GetV`),必须靠请求改名 | 博士论文 §7.6.6 |

**决策**:`ADR-CEP-12` 的能力映射表 schema **必须**为每个协议实例承载两项声明:

```yaml
ordering_assumption:   # 逐网络(per-network)映射,非逐协议单值
  # ⚠️ key 必须能解析到唯一 vc_id(per oracle 全面架构审查 N8)
  <vc_id | named_network>: p2p_ordered | unordered
  # named_network 必须在 fabric config 中解析到唯一 vc_id,否则配置加载 fail
  # 理由:排序是 VC 的静态属性,NoCFlitBundle 不带 ordering 字段(flit 只有 vc_id 等)
consistency_labels:
  read:  SC | TSO | RC-acquire | RC-data | PLO
  write: SC | TSO | RC-release | RC-data | PLO
  request_level: <协议可声明的请求级标签映射>
```

**v0.3 源码级依据(F25)**:*Gen 族的协议描述语言(`*.pcc`)把排序假设建为**语言内建结构**:
```
Network { Ordered fwd;    //FwdGetS, FwdGetM, Inv, PutAck
          Unordered resp; // Data, InvAck
          Unordered req;   //GetS, GetM, PutM
          };
```
→ 排序假设是**协议规格的一部分**,且粒度是**逐网络**而非逐协议单值。→ 本决策的 `ordering_assumption` 采用**逐网络映射**。

**理由**:
1. 缺排序假设 → 组合时无法判断"有序网络 SSP 推出的协议"能否用于无序网络(ProtoGen L3 的直接教训)
2. 缺一致性标签 → 无法表达"同一 barrier 在 SC / TSO / RC 下效果不同"(F7),也无法做跨域组合(F8/F9)
3. 缺请求级标签 → 目录无法区分同型不同义的请求(§7.6.6 的 `GetV_R` / `GetV_A` 陷阱)

**与决策 7 的关系**:决策 7 说"学术协议族纳入需 `consistency_labels` 支撑";本决策把它从"§5.1.4 族的前置"提升为"**所有**协议实例的统一契约"。

**新增:VC 分配的职责边界(v0.3 依据博士论文 §9.3.1)**

> "ProtoGen does not automatically assign messages to virtual networks yet **for ensuring deadlock freedom**."

→ **VC 分配的目的不是 QoS 调优,而是保证死锁自由**。因此 CEP 的 Fabric & Transport Plane 承担 VC 分配时,必须与协议的 `ordering_assumption` **联合论证死锁自由性**。本决策**不**直接锁定 VC 分配规则(属 `ADR-CEP-09` 范围),但**记录该职责归属**,并作为 `ADR-CEP-08` / `ADR-CEP-09` 的输入。

### 决策 9:分层 + 异构组合 —— **可做,但不承诺自动化**(v0.3 新增,v0.3 复审修正措辞)

**问题**:本 ADR v0.2 之前隐含了一个乐观假设:CEP 可以同时支持**多层目录组合**(`ADR-CEP-08`)与**跨一致性域组合**(`ADR-CEP-13`),且**该组合的可行性未经验证**。

**一手依据**(博士论文 §9.2 Critical Analysis,作者自陈):
> "The HieraGen and HeteroGen tool are publicly available as two stand alone tools with **limited compatibility**. Automatically generating a heterogeneous and hierarchical cache coherence protocol from given input SSPs is **not possible with these tools without manual intervention**."

Table 1.3 佐证:已发布的 3 个工具**各缺一段算法阶段**(HeteroGen 无 HieraGen 阶段;HieraGen 无 HeteroGen 阶段)。

> **⚠️ v0.3 复审修正(措辞)**:原文限定词是 "**without manual intervention**" —— 即"**不能无人工介入地自动完成**",**不是"不能组合"**。算法层是**可组合**的:
> - 论文 §1.3:"the lower-level directory controller SSP input of HieraGen **can be** the atomic heterogeneous directory-controller output of the HeteroGen stage"
> - 源码预留点:`HeteroGen/Algorithms/ControllerGeneration/NetworkxGeneral/TraceNetworkx.py:77` —— "New sub trees can be generated by other Algorithms like for example ProtoGen or **HieraGen**"
>
> → 该限制是 **工具工程限制**(两个独立工具、limited compatibility),**不是算法不可能**。原 v0.3 草稿表述"不可同时自动生成 / CEP 不承诺"**过强**,已修正。

**决策**(措辞 per oracle 全面架构审查 Q4 精化 —— 原表述把"工具集成负担"与"语义组合负担"混为一层):

**CEP 承诺三件更窄、更可交付的事**:
1. **判据可复用** —— 把学术侧已形式化的 5 条组合判据(唯一序列化点 / E 态静默升级 / 复合一致性公理 `ppo_com` / 全局可见写静态判据 / proxy-cache 模板)转写成 CEP 的**结构化声明项**(进 `ADR-CEP-13` 的 declaration schema),使"人工声明"从口头承诺变成**有固定字段、可机器检查**的声明
2. **逐组合实验证伪** —— 正确性由**随机 / litmus 实验在声明的 `verification_scope` 内尝试证伪**(**不提供无违例保证**,per `ADR-CEP-06 §3` 的 `verification_mode` 契约)
3. **接口预留组合接缝** —— `ADR-CEP-07` 的插件接口从第一版为组合维度留扩展点(per F20)

**明确不承诺**:自动化组合生成(CEP 不做生成器,per 决策 10);自动兼容性判定(per 决策 7 / F10);任意配置穷尽验证(per F22 / F27)。

**为何这样分两层**:
- **工具集成负担** —— *Gen 族的两个独立工具 IR 不统一(ProtoGen 代码质量显著下降、HeteroGen 从零重写)。CEP 不做自动合成 ⇒ **不继承这一层**
- **语义组合负担** —— *Gen 团队用整篇论文才把"从两个独立协议语义导出组合协议语义"形式化,而 §9.2 结尾承认**他们自己也没做出自动化版本**。CEP 走手写路线 ⇒ **完全继承这一层**:每个组合都要人把这套判据手工套一遍,而判据本身**尚未被自动化验证过**

**原决策条目(保留编号连续性)**:
1. **CEP 承诺**"分层 + 异构"组合**可做** —— 但**走人工声明 + 实验证伪路线**(与决策 7 的兼容性不承诺同源),**不承诺自动化**
2. `ADR-CEP-13` 必须显式声明:该组合的**正确性由实验验证支撑,而非由工具自动保证**
3. `ADR-CEP-07` 的 ProtocolEngine 插件接口**必须为组合预留接缝**(per 调研 F20:ProtoGen 适配 HieraGen 时代码质量显著下降,HeteroGen 从零重写)—— 否则组合阶段将面临接口重写
4. **不**把"分层 + 异构**自动化**"写进任何 Stage 的 DoD

**理由**:
- *Gen 族走"自动生成"路线,其限制来自工具工程;**CEP 走手写插件路线**,该限制**不直接适用**
- 但学术侧给出的**组合判据**(唯一序列化点 / E 态静默升级检查 / 复合一致性公理 / 全局可见写判据)必须**人工套用**到 CEP 的插件上 —— 这就是"人工声明 + 实验验证"的含义
- 本决策**既不放弃该能力,也不虚假承诺自动化**;它把"能力可达"与"保证方式"分开表述

### 决策 10:竞合判断 —— CEP 与 *Gen 族是互补而非替代(v0.3 新增)

**事实**(博士论文 §9.2):作者正在开发 **ProtoGen+**,整合 3 个算法 + "a variety of **Backends like Rumur and SLICC**"。

**决策**:CEP 的定位**不是**"生成协议",而是"**在统一实验床上运行 / 对比协议**"。二者可共存。

**理由**:
- 若 *Gen 族能产出 SLICC → gem5 Ruby 的协议实现空间被扩张 → **CEP 需要探索的协议空间变大**,而非变小
- gem5 已有先例:Ruby(SLICC 手写协议)+ 可插拔机制并存
- 竞合压力在**成本**侧:若协议可自动生成,"每协议一个手写 adapter"的优势缩小 → 记入 `ADR-CEP-18` 的 trade-off 表

**范围**:`ADR-CEP-18` 负责完整的 DSL vs 插件路线 trade-off(含量级:ProtoGen 8,849 / HieraGen 15,771 / HeteroGen 30,413 / ProtoGen+ 45,000+ 行 Python,per 论文 Table 9.1)。本决策只锁**定位边界**。

---

## 3. 关键约束

### 3.1 不变量增量(本 ADR 新增 2 条;v0.3 未新增)

| ID | 规则 | 守门 ADR |
|----|------|----------|
| **INV-CEP-04-IC7** | token 类协议下 Sharer Tracker 不跟踪 sharer set(token 由副本携带) | ADR-CEP-04 §11 |
| **INV-CEP-05-IC1** | Stub Cache Core 只暴露稳定状态;瞬态状态 + 协议私有状态住 Protocol Engine 插件内部 | ADR-CEP-05 §4.3 |

**守门 invariant 总数(本 ADR 发起时)**:34 → **36**(per `ADR-CEP-04 §11` + `ADR-CEP-05 §4.3`)。
**⚠️ 该数字已过时**:2026-10-21 architect 阶段两批新 ADR 共增 **+31** 条(编号位 36 → **67**,实际 35 → **66**,15 组)。**当前计数单一真源 = `07 号 §4`**;本 ADR 不再维护计数。per `INV-CEP-NUMBER-MAP-IC1`(编号表是唯一真源)与 `ADR-CEP-16-IC2`(Stage 须能指出承接条目)。

### 3.2 协议族 vs 协议实例的表述纪律(全局)

- "协议族"(protocol family)= 一组共享语义基础的协议(gem5 Ruby 族 / ARM CHI 族 / CXL 族)
- "协议实例"(protocol instance)= 一个具体可实现协议(1 个 `.slicc` / 1 个 adapter)
- **adapter 数量 = 协议实例数**,不是族数
- 引用 gem5 Ruby 协议清单时**必须标注"8 个独立协议"**,不写"Ruby 族 1 个"

### 3.3 抓取点

1. **`07 号 §5.1` 表格语义**须含 Ruby-SLICC 族 + 8 个协议实例 + HTM 排除标注 + **学术协议族 5 实例(§5.1.4,per 决策 7)**
2. **`perm` 枚举边界**:Stub Cache 4 值锁(决策 4 选 C);**atomic 意图作为 `request` 流独立属性,不扩 `perm`**(决策 6 v0.2 闭合)
3. **协议实例数与 adapter 数一致性**:`07 号 §5.1` 表格内 adapter 数 == 协议实例数
4. **守门 invariant 36 条**在 4 份文档一致
5. **gem5 Ruby 协议清单**引用处标注"8 独立协议" + HTM 排除
6. **`ADR-CEP-12` schema 必含 `consistency_labels`**(per 决策 7 硬前置条件),否则 §5.1.4 族不可落地
7. **Stub Cache 定位措辞**:`ADR-CEP-05` 须写明"Stub Cache 是客户端侧协议状态投影,**不是缓存性能模型**"(per 调研 §5.4 F12;参考量级:ProtoGen 生成的 non-stalling MSI 为 18-20 状态)
8. **协议实例前提声明**(per 决策 8):`ADR-CEP-12` schema 必含 `ordering_assumption` + `consistency_labels`(含请求级标签)
9. **组合能力二维化**(per 调研 F16):`ADR-CEP-12` 必含 `combination_capabilities`(concurrent / hierarchical / heterogeneous)
10. **VC 分配职责**(per 决策 8 附注):`ADR-CEP-08` / `ADR-CEP-09` 须承载"VC 分配须论证死锁自由性"的要求(依据 博士论文 §9.3.1)
11. **组合承诺边界**(per 决策 9):`ADR-CEP-13` 须显式声明"分层 + 异构组合**可做**但走人工声明 + 实验验证、**不承诺自动化**";`ADR-CEP-07` 须为组合预留接缝
12. **插件接口可组合性**(per 调研 F20):`ADR-CEP-07` 须要求插件接口从第一版为组合维度留扩展点

### 3.4 schema 草案 → 已由 `ADR-CEP-12` 承接

> **状态**:schema 草案已于 2026-10-20 先行落地,**`ADR-CEP-12` 已于 2026-10-21 正式起草并采纳该草案**。
> → **schema 的单一真源现为 [`ADR-CEP-12 §2 决策 1`](ADR-CEP-12-protocol-engine-capability-map.md)**;
> 本节**不再作为引用来源**(草案原文见 git 历史)。
>
> **新增 5 个维度的理由**(原草案缺):`id` / `status` / `protocol_version` —— 否则「16 adapter」随 §5.1.4 候选语义落地即失真;
> `vc_binding` —— F18 原文主语是「assign messages to virtual networks」,只声明序假设不声明绑定则**死锁自由论证拿不到输入**;
> `sharer_tracker` —— §5.1.1 / §5.1.2 / §5.1.4 三张表全在做逐实例选型,schema 却漏了;
> `flow_support` —— `ADR-CEP-07` IC2 要求 `NullProtocolEngine` 有 10 流覆盖矩阵;
> `atomic` —— 决策 6 定了「atomic 意图作为 `request` 流独立属性」,schema 里原本没有该字段。
### 3.5 `ADR-CEP-09` 两段式建议 → 已由 `ADR-CEP-09` 承接

> **状态**:两段式建议(Decision A 装配期无环论证 / Decision B 运行时仲裁)**已由
> [`ADR-CEP-09`](ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md) 正式承接**,本节不再作为引用来源。
> 三方联合契约(① 排序假设 ② 消息→VC 绑定 ③ 仲裁一致性)见 `ADR-CEP-09` 决策 3。
## 4. 关联 ADR / 后续

| 项 | 状态 |
|----|------|
| `ADR-CEP-02`(canonical 10 流 + `perm` 枚举)| 🟡 v0.2 需锁"atomic 意图作为 `request` 流属性 + **不扩 `perm` 枚举**"(per 决策 6 v0.2) |
| `ADR-CEP-04`(Sharer Tracker)| 🟡 需补 `INV-CEP-04-IC7` |
| `ADR-CEP-05`(Stub Cache)| 🟡 需补 `INV-CEP-05-IC1` + **Stub Cache 定位澄清**(per 抓取点 7) |
| `ADR-CEP-12`(能力映射表)| 🟡 待起草 — 需**二维化**(per 调研 F16):`协议族/实例` × `组合能力(concurrent/hierarchical/heterogeneous)`;**必含 `consistency_labels`(含请求级标签)+ `ordering_assumption`**(per 决策 7/8) |
| `ADR-CEP-13`(一致性域归属)| 🟡 待起草 — 需采纳**复合一致性公理**(`ppo_com` 形式)+ **"全局可见写"静态判据** + 明写"**不提供自动兼容性判定器**" + **显式声明"分层 + 异构组合可做但走人工声明 + 实验验证,不承诺自动化"**(per 决策 7 范围纪律 / 决策 9;调研 F8/F9/F10/F19) |
| `ADR-CEP-08`(Ordering Point 可迁移性)| 🟡 待起草 — 需采纳 **唯一序列化点不变量** + **排序假设面** + **E 态静默升级兼容性检查** + **Ordering Point 不绑定物理实体(目录 OR 总线)**(per 调研 F3/F4/F5/F17) |
| `ADR-CEP-09`(多 VC 仲裁)| 🟡 待起草 — 需承载 **"VC 分配须论证死锁自由性"**(per 决策 8 附注;博士论文 §9.3.1) |
| `ADR-CEP-06`(实验输出协议)| 🟡 后续 — 需记录**实验规模上限**约束 + `memory_violations` 对齐 **SWMR + data-value** 双不变量 + **显式选择"探索平台 vs 验证平台"**(per 调研 F22;§7.2 P1) |
| `ADR-CEP-18-protocol-generation-scope.md`| 🟡 **编号已预留**(`ADR-CEP-NUMBER-MAP`)— DSL(SSP 描述 + 代码生成)vs C++ 插件路线的 trade-off(含 Table 9.1 量级)+ **竞合分析**(ProtoGen+ / SLICC 后端,per 决策 10;调研 F15/F21/F23) |
| `ADR-CEP-07`(仿真语义 + seam)| 🟡 后续 — 需增 **插件接口可组合性**要求(per 调研 F20:ProtoGen 适配 HieraGen 时代码质量显著下降,HeteroGen 从零重写) |
| `ADR-CEP-17A-ruby-slicc-adapter-families`(可选)| 🟡 后续 — 若 8 + 5 = 13 个 adapter 工作量过大,考虑"协议族模板"选项 B |

---

## 5. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| §5.1 表格语义 | 协议族 → 协议实例列表;adapter 数 = 实例数 | ✅ 已拍板 |
| Ruby-SLICC 族纳入 | 6 协议直接支持 + 1 需扩展 + 1 排除 | ✅ 已拍板 |
| token 归属 | Protocol Engine 内部;SharerTracker 不跟踪 | ✅ 已拍板 |
| 协议私有状态边界 | 瞬态状态 + token 住插件内;Stub 只暴露稳定状态 | ✅ 已拍板 |
| `O`(Owned)处理 | 选 C — 插件内持有,Stub 暴露 S + dirty 标志 | ✅ 已拍板 |
| HTM | 明确 Out-of-Scope | ✅ 已拍板 |
| **atomic 语义** | 选 C — 插件内部;**`perm` 枚举维持 4 值锁,atomic 意图作为 `request` 流独立属性** | ✅ **已闭合**(v0.2,ProtoGen 局限 L2 依据) |
| **学术 / 一致性模型定向协议族** | 纳入为 **§5.1.4 协议语义索引**(5 语义:SC / TSO / RC×3 / PLO);`ADR-CEP-12` 必含 `consistency_labels`。⚠️ v0.3 复审 R8:是**索引视图**非第三个族,与 §5.1.2 交叉不重复计数 | ✅ **已拍板**(v0.2;⚠️ v0.3 复审修正表述) |
| **协议/互连分离** | 协议语义归 Protocol Engine,VC / 消息路由归 Fabric & Transport Plane(ProtoGen 局限 L4 独立佐证) | ✅ **已拍板**(v0.2) |
| **兼容性自动判定** | **不承诺** —— 人工声明 + 实验验证 | ✅ **已拍板**(v0.2) |
| **协议实例前提声明** | 每个协议实例必声明 `ordering_assumption` + `consistency_labels`(含请求级标签) | ✅ **已拍板**(v0.3) |
| **VC 分配职责** | VC 分配须与排序假设**联合论证死锁自由性**(非 QoS 调优) | ✅ **已拍板**(v0.3,记入 ADR-CEP-08/09) |
| **组合承诺边界** | **分层 + 异构组合可做,但走人工声明 + 实验验证,不承诺自动化**;接口须预留组合接缝 | ✅ **已拍板**(v0.3;⚠️ v0.3 复审修正措辞 —— 原表述过强) |
| **竞合定位** | CEP 定位 = "统一实验床运行/对比协议",**非**"生成协议";与 *Gen 族互补 | ✅ **已拍板**(v0.3) |
| 守门 invariant | 34 → 36(新增 IC7 + INV-CEP-05-IC1) | ✅ 已拍板(v0.2/v0.3 均未新增) |

---

## 6. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 8. Status Update(2026-10-22):`ordering_assumption` / `vc_binding` 的 key 形态随 per-network 化修订

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)——
> §3 决策 8 的 schema 草图(:185-195)、§4 的模板(:318-325)、§6 的 invariant 建议(:344)
> 的**正文不改**,**自本段起以本段文本为准**。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` Entry #9(`adr-amendment`)

### 8.1 取代范围

| 项 | 位置 | 处置 |
|----|------|------|
| **决策 8 的 schema 草图** | §3 `:185-195` | **取代** —— 见 §8.2 |
| **§4 的 schema 模板** | §4 `:318-325` | **取代** —— 见 §8.2 |
| **§6 的 invariant 建议** | §6 `:344` | **取代** —— 见 §8.2 |

### 8.2 取代后文本(规范)

**schema 草图(取代后)**:

```
ordering_assumption:   # 逐网络 × 逐 vc_id(per ADR-CEP-12 决策 5 修订)
  <network_id>:                  # key = network(须在 fabric 网络命名空间登记)
    <vc_id>: p2p_ordered | unordered   # 其下每个 key 是一个**显式**的 vc_id
consistency_labels:
  read:  SC | TSO | RC-acquire | RC-data | PLO
  write: SC | TSO | RC-release | RC-data | PLO
  request_level: <协议可声明的请求级标签映射>
```

**schema 模板(取代后)**:

```
  ordering_assumption:                  # 决策 8,逐网络 × 逐 vc_id
    <network_id>:
      <vc_id>: p2p_ordered|unordered

  vc_binding:                           # 逐网络:消息类 → vc_id(F18 主语)
    <network_id>:
      <flow_or_message_class>: <vc_id>   # 消息类词表 = 6 类(per ADR-CEP-12 IC2 ③)
```

**invariant 建议(取代后)**:

> **必落 invariant(建议)**:schema 必含上述 **7 个字段组(13 个顶层属性)**
> (per `ADR-CEP-12` IC1 口径修订,2026-10-21);`ordering_assumption` / `vc_binding` 均为
> **逐网络 × 逐 `vc_id`** 形态 —— network 须在 fabric 网络命名空间登记、
> 其下每个 `vc_id` 须落在该网络的槽位上界(`vc_id < vc_count`)内、
> 且须出现在**同网络** `vc_binding` 的值域中;`vc_binding` 的消息类须 ∈ 6 类规范词表
> (per `ADR-CEP-12` §8.3 `INV-CEP-12-IC2` 的 ①②③);`status` 变更时 adapter 计数口径同步。

### 8.3 为何必须改(不只是措辞)

旧文「`named_network` 必须**解析到唯一** `vc_id`」这一**谓词在 per-network 嵌套形态下整体消失**:
network 本身就是 key,其下每个 `vc_id` 是**显式**给出的 —— 不存在「解析到多个 vc_id」的多义情形。
不修订则冻结集内 `ADR-CEP-17`(旧)与 `ADR-CEP-12 §8`(新)会对同一字段给出**两个不同的 fail 判据**。

**机器校验**: `scripts/test/cep_decl_alignment_check.sh` 第 ②/③/④ 项。

---

**版本**: v0.3 草案 + Status Update 2026-10-22
**最后更新**: 2026-10-22
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)