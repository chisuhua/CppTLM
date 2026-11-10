# Protocol Engine 与协议适配(第 3 层)

> **本文件是** [`07-coherence-exploration-platform.md`](../07-coherence-exploration-platform.md) **的子文档**(per 台账 Entry #333 架构文档治理 · 批 3 按层拆分)。
> **总纲是唯一入口** —— 子文档只承载本层实体;⛔ 不另立节号体系(**节号以总纲为准**)。
> 协议族 → 协议实例 → adapter 映射、目录无关的运行时引擎形态,以及「转移表真源」判定式。
> **真源**:总纲「07 号」的对应编号节(`§N` / `§N.M`);本文件是其**实体下沉**,⛔ 不构成第二真源。

---



## 5.1 协议族 → 协议实例 → adapter 映射

> **表述纪律**(per `ADR-CEP-17 §3.2`):"协议族" = **协议实例的来源集合**;"协议实例" = 一个具体可实现协议(1 个 `.slicc` / 1 个 adapter)。**adapter 数量 = 协议实例数**,不是族数。
>
> **当前口径**:**2 个来源族(工业 6 族 / Ruby 1 族)= 16 个 adapter 实例** + §5.1.4 的 **5 个一致性模型语义索引**(交叉视图,不重复计 adapter)。

### 5.1.1 工业硬件协议族(6 族 / 8 实例)

| 协议族 | 协议实例 | adapter | 通道模型 | 关键差异 |
|--------|---------|---------|----------|---------|
| **CHI** | CHI RN-F | `ChiRnfAdapter` | RN-F 6 通道(TXREQ/RXRSP/RXDAT/RXSNP/TXRSP/TXDAT)| RN-F 只发 REQ 只收 SNP;Credit 流控;TxnID 管理;CompAck;**UC→UD 静默升级**(轴 2)与 `moesi_gpu` 的 `E→M` **同型** —— 4 值 `perm` 无法表达 ⇒ 静默升级声明归 `perm_upgrades.json`(per `ADR-CEP-12 §11`) |
| **CXL 2.0 非对称** | CXL 2.0 | `CxlCacheAdapter` | D2H/H2D 各 3 通道(Req/Rsp/Data)| Host 侧 Snoop Filter;设备不感知其他设备 |
| **CXL 3.0 对称** | CXL 3.0 对称 | `CxlDcohAdapter`(独立载体;**非** `CxlCacheAdapter` 的模式)| + CXL.mem 增 **BISnp(S2M)/ BIRsp(M2S)** 两通道 | **DCOH = Device Coherence(agent / engine)**(设备内的一致性代理,**非** "Ownership History");设备可对 **host** 发起反向 snoop(BI;**不**对 peer device);归属基于 **HPA**(设备内部翻译到 media address,**非** DPA 归属) |
| **TileLink-C** | TileLink-C | `TileLinkCAdapter` + `TileLinkProtocolEngine` | A/B/C/D/E 通道 | Tree Tip 权限模型;Probe/Acquire/Grant 家族,无传统 snoop 广播;lookup 可能 ESCALATE_UP |
| **NVLink** | NVLink | `NvlinkAdapter` | NVLink TL Read/Write/Atomic/Probe | NVSwitch 内嵌 SHARP/atomic/barrier;排序点可落在 Switch/SPU |
| **UALink** | UALink | `UalinkAdapter` | Station/Port/Lane;TL Load/Store/Atomic | In-Network Compute(ReadReduce/WriteMulticast/Broadcast/AllReduce) |
| **MOESI/GPU** | `moesi_gpu` | `MoesiGpuAdapter` | 6 态 × 6 事件;snoop 型(SoC 内) | ArchForge 贡献(per `32 号` §6.1);**E→M 静默升级**是 `ADR-CEP-13` 域可见写判据 (a)(b) 的**争点所在**(**该争点已裁定 → 状态见 `ADR-CEP-05 §13.1`**) |
| **MESIF** | `mesif` | `MesifAdapter` | 分级目录型(跨卡);目录查询 / 定向 snoop;**SharerTracker 选型 = `HierarchicalDirectory`**(`SYSTEM/EXACT/GLOBAL/HIERARCHICAL` —— 多级聚合) | ArchForge 贡献(per `32 号` §6.2);与 `moesi_gpu` **互不承认**(per `32 号` C1);⚠ 声明权威已迁本仓 ⇒ 与 `32 号 §6.2` **分叉**(跨仓追认债) |

> **协议数 6 族 / 8 实例**(✅ **2026-11-06 一致性修复** —— per 台账 Entry #239:新增 `ualink_4` 实例 ⇒ 本计数与 `protocol_instances.json` 的 8 实例**首次相符**;此前本处写 8 而 JSON 为 7);CXL 2.0 与 3.0 对称在**实现阶段**是两个 change;⚠️ **2026-11-02 订正**(per 台账 Entry #167)—— adapter **不共用**:2.0 = `CxlCacheAdapter` · 3.0 对称 = `CxlDcohAdapter`(**独立载体**;per 本文件 `Status Update(2026-11-02)` ③ + 台账 Entry #154)⇒ 原「同一 adapter 的两种模式」的表述**已失效**)
>
> **族计数的口径**(2026-10-22 补记,per 台账 **Entry #12**):`moesi_gpu` / `mesif` 计为**同一来源族**
> (ArchForge 声明族,含 2 套互不承认的一致性架构,per `32 号` C1)→ **+1 族 / +2 实例**,
> 镜像 CXL「1 族 / 2 实例」的先例。两者 `family` 字段均为 `industrial`(per `32 号` 声明)。
> **状态 = 按队列实况(2026-11-02 更新,per 台账 Entry #148 授权)** —— `moesi_gpu` = 队列 #6 **已落地归档**(2026-10-27;首个 `status=implemented` 实例);`mesif` = 队列 #9 **已落地归档**(2026-10-30;**第 3 个** `status=implemented` 实例;R-1 裁决 Entry #96 已落地为 `HierarchicalDirectory` 行为实现;2 项缺口(状态机规约 / 5→4 投影表)**已闭**(Entry #101 归档 + Entry #102 追认)⇒ **已就绪**);`chi_rnf` = 队列 #8 **已落地归档**(2026-10-28;**第 2 个** `status=implemented` 实例);`cxl_2_0` = 队列 #10 **已落地归档**(2026-11-01;**第 4 个** `status=implemented` 实例;⚠️ **两段式** —— 规约层 + 实现层;`sharer_tracker` = 「不参与」);`tilelink_c` = 队列 #11 **已落地归档**(2026-11-02;**第 5 个** `status=implemented` 实例;⚠️ **两段式** —— 规约层 + 实现层;`sharer_tracker` = `TreeManager`);`ualink_4` = **2026-11-06 声明落地**(per 台账 Entry #239 —— 路径 A;13 键取值**全部裁决** + `ADR-CEP-12` 订正 #1)⇒ **`status=implemented` 实例 7 → 8**;⚠️ 但 `sharer_tracker` = 「**不参与**」(无硬件一致性 ⇒ 无 sharer-set 知识需求)。**状态值以 `obj §4.0` 排队总表为唯一权威**(本表**不复制**排期,per 防双真源立场)。
>
> **2026-10-22 补记**(per 台账 **Entry #13** [adr-amendment]):`moesi_gpu` 行的「**争点所在**」表述
> **已失效** —— ArchForge 已裁定判据 (b) **成立**(`4ff2fa6`,Q-3a = 是)→ 该静默 `E→M` **是域可见写**。
> **状态值以 `ADR-CEP-05 §13.1` 为唯一权威**(本表**不复制**状态值,per 防双真源立场);
> 本行的「已落地 / 已排期」状态**不受影响**(排期 ≠ 就绪)。

### 5.1.2 Ruby-SLICC 协议族(1 族 / 8 实例,per `ADR-CEP-17 §2 Decision 2`)

> **来源**: gem5 Ruby `src/mem/ruby/protocol/*.slicc`(per `docs/research/gem5-coherence-protocols-survey.md:16-23` 实证)
> **定位**: **学术 / 教学协议族**,与 §5.1.1 的工业硬件协议族**互补而非重叠**

| 协议实例 | 判定 | Sharer Tracker 选型 | 关键说明 |
|---------|------|-------------------|---------|
| MESI_Two_Level | ✅ 直接支持 | `FullMapDirectory` | 标准 2 级 directory-based |
| MESI_Three_Level | ✅ 直接支持 | `FullMapDirectory` | 3 级;多层状态机在插件内 |
| MOESI_CMP_directory | ✅ 直接支持 | `FullMapDirectory` | `dir.sm` = 中心目录 = `FullMapDirectory` |
| MOESI_hammer | ✅ 直接支持 | `SparseSnoopFilter` | `probeFilter` 保守过滤 = caps `CONSERVATIVE` 语义 ✅ |
| MOESI_AMD_Base | ✅ 直接支持 | `SparseSnoopFilter` | 含 `probeFilter` / `Region*` / `CorePair` |
| GPU_VIPER | ✅ 直接支持 | 复用 MOESI_AMD_Base | GPU 侧 coherence 在插件内 |
| **MOESI_CMP_token** | ⚠️ **需扩展 1** | **`BroadcastNull` 或不参与** | token 由副本携带,不依赖中心 sharer set(per `ADR-CEP-17 §2 Decision 3`)|
| **MESI_Three_Level_HTM** | ❌ **明确排除** | — | 事务内存边界(HTM 需 read-set / write-set / 事务中止语义),超出纯缓存一致性模型(per `ADR-CEP-17 §2 Decision 5`)|

**关键映射证据**:gem5 Ruby 消息模型(RubyRequest / RubyResponse / RubySnoopResponse)→ CEP canonical 10 流 **100% 覆盖,零新增流**(per `docs/research/cep-coherence-protocol-space-survey.md §3.3`)。

> **注**:调研文档 `gem5-coherence-protocols-survey.md:27` 指出本快照中 `MESI_CMP_directory` 协议**不存在**(历史 gem5 曾有,现已删除,仅 MOESI 版存活);上表按调研文档原文照录。

### 5.1.3 协议私有状态边界(per `ADR-CEP-17 §2 Decision 4`)

| 状态类别 | 例子 | 归属 |
|---------|------|------|
| **稳定状态(SSP)** | `I / S / E / M`(CEP Stub Cache 暴露) | Stub Cache Core `perm` 枚举 |
| **瞬态状态** | `T`(Temporary,per ArchForge 09 MOESI/GPU 6×6);ProtoGen 生成的 transient states | **Protocol Engine 插件内部** |
| **协议私有状态** | token(per MOESI_CMP_token);HTM 的 read-set / write-set | **Protocol Engine 插件内部** |

**依据**:Oswald **ProtoGen** 方法论建立在"从 SSP(稳定状态协议)生成非原子协议"之上 — 瞬态状态分配是其算法第 2 步(per `docs/research/cep-coherence-protocol-space-survey.md §4.1.1` / §4.1.2)。CEP 的分层与之一致:Stub Cache = SSP 层,Protocol Engine = 非原子事务层。

**守门 invariant**:`INV-CEP-05-IC1`(Stub 只暴露稳定状态)+ `INV-CEP-04-IC7`(token 类协议 tracker 不跟踪 sharer set)。

> **v0.2 证据补强**(per `docs/research/cep-coherence-protocol-space-survey.md` v0.2 §4.1.2 / §4.1.5):ProtoGen 论文实测 **MSI 协议含 18 个瞬态状态**,non-stalling MSI/MESI/MOSI 为 **18-20 状态 / 46-60 转换**(`ProtoGen_ISCA2018-draft.md:89-90`, `:1282-1285`)。→ **CEP Stub Cache 的 4 个稳定状态不是缓存模型,而是客户端侧协议状态投影**;真实协议的瞬态复杂度全部归 Protocol Engine 插件。

### 5.1.4 协议语义实例(按一致性模型索引)(5 语义,per `ADR-CEP-17 §2 Decision 7`)

> **来源**:Oswald *Gen 算法族*(ProtoGen ISCA 2018 / HieraGen ISCA 2020 / HeteroGen HPCA 2022)论文 case study 集(per `HeteroGen_HPCA2022-draft.md:1266-1276` Table I)

> **⚠️ 分类维度澄清(v0.3 复审 R8)**:本小节**不是**与 §5.1.1 / §5.1.2 并列的"第三个族",而是**协议语义的索引视图**:
> - **"族"= 协议实例的来源集合** —— §5.1.1(工业硬件标准,6 族 / 8 实例)+ §5.1.2(gem5 Ruby 实现,1 族 / 8 实例)
> - **"一致性模型"= 协议实例的语义属性**,用于**索引语义**,**不是**来源分类
> - ⚠️ **两者交叉**:**MSI / MESI** 既是 §5.1.2 gem5 Ruby 族的实例,又是本表的 SC 语义 —— **同一协议语义的不同实现**,adapter 按 §5.1.1/§5.1.2 计,**不重复计数**
> - **落地口径**:本表列出的是**协议语义**;其 adapter 是否实现、归哪一族,由 §5.1.1 / §5.1.2 决定

| 协议语义 | 一致性模型 | 判定 | Sharer Tracker 选型 | adapter 归属 | 关键说明 |
|---------|-----------|------|-------------------|-------------|---------|
| MSI / MESI | **SC** | ✅ 直接支持 | `FullMapDirectory` | **§5.1.2**(gem5 Ruby MESI_Two_Level / Three_Level)| SWMR + writer-initiated invalidation;CEP 基线协议 |
| TSO-CC | **TSO** | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| **主动放弃 sharer tracking** 以利用 TSO 松弛(per HeteroGen §VI-B)→ 配 snoop filter 自相矛盾 |
| RCC / RCC-O | **RC** | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| 写缓冲 + release 时写回 + acquire 时自失效(不做中心目录)|
| **GPU**(源自 Spandex) | **RC** | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| **write-through 直写** + 局部缓冲 —— **与 ArchForge 09 §4.1 的 GPU write-through 策略直接对应**;Spandex 协议不做 sharer tracking 式中心目录(per 调研 §3.5)|
| PLO-CC | **PLO**(partial-load-order) | ✅ 直接支持 | **`BroadcastNull` 或不参与** | 新增(若选为实现目标)| 只强制 W→W 与 R→W 序;属自失效类协议,不做 sharer tracking |

**Sharer Tracker 选型修正说明(per oracle 全面架构审查 N5)**:`SparseSnoopFilter` **本身即一种 sharer tracker**,与「主动放弃 sharer tracking」的协议特征**自相矛盾**。上述语义(TSO-CC / RCC / RCC-O / GPU / PLO-CC)均属自失效 / write-through 类,统一取 `BroadcastNull` 或「不参与」 —— 与 `ADR-CEP-17` 决策 3(token 类协议同原则)一致。

**前置条件(硬)**:**这组协议语义必须**依赖**一致性标签维度**(`ADR-CEP-12` schema 的 `consistency_labels` 字段),否则无法表达"同一 `barrier` 在 SC / TSO / RC 下效果不同"。per `ADR-CEP-17 §2 Decision 7` 与调研 §5.3 F6。

**诚实边界(必读,per `ADR-CEP-17 §2 Decision 9` + 调研 F19)**:
*Gen 族作者自陈其**两个独立工具**"无法在**无人工介入**下自动完成"分层 + 异构组合("...is **not possible with these tools without manual intervention**",博士论文 §9.2)。⚠️ 注意:这是**工具工程限制**,**不是算法不可能** —— 论文 §1.3 明示 HieraGen 阶段**可以**消费 HeteroGen 阶段的输出。
→ **CEP 的对应结论**:多层目录组合 + 跨一致性域组合**可做**,但走**人工声明 + 实验证伪**路线 —— 正确性由**随机 / litmus 实验在声明的 `verification_scope` 内尝试证伪**;**平台不提供无违例保证**(per `ADR-CEP-06 §3` 的 `verification_mode` 契约)。**不承诺自动化**;且 `ADR-CEP-07` 的插件接口须为组合预留接缝。

> **口径澄清(per oracle 全面架构审查 Q4)**:*Gen 族的限制分两层 —— **工具集成负担**(CEP **不继承**,因其不做自动合成)与**语义组合负担**(CEP **完全继承**,因其须人工套用全部组合判据)。准确表述是「**工具集成负担不继承;语义组合负担完全继承**」,而非「该限制不直接适用」。

**规模与验证参考**:
- HeteroGen 在 **gem5** 上以 64 核(60 tiny + 4 big)对比手工异构协议 **HCC**,平均性能相当、网络流量差异 < 5%(per `hetergen:1382-1400`)→ **gem5 已是该研究线的标准评测床**
- 验证手段为 Murϕ(Safety + Deadlock Freedom)+ **111 个异构 litmus test**(per `hetergen:1277-1316`)→ 对应 CEP `ADR-CEP-06` 的 `deadlock_violations` / `memory_violations` 闸


### 5.1.5 「转移表真源」判定式(T1–T4)与来源效力

> **本节解决什么**:§5.1.1 的「补运行时引擎的触发条件」用了「**转移表真源**」一词 ——
> 本节给出它的**判定式**、**载体形态分类**、**来源效力分档**与**普查三要素**。
> ⛔ 本节是**判据**,不是状态登记(逐族达成度见 `objective-cep-platform`)。

**T1–T4 四要件(⛔ **合取** —— 任一子集成立**不构成**触发、**不构成**达成度)**：

| 要件 | 内容 | 机器可判标志 |
|---|---|---|
| **T1 存在** | 仓内规约载体持有**有限封闭**的转移结构(态集 × 事件集 ⇒ 目标集),非散文、非示例 | 载体有编译期矩阵 + 计数常量 |
| **T2 可判定** | 每格有 `basis.kind` + 条款级 `ref`;`Unknown` 比例可机器统计 | **决定性变异**:删一格 ⇒ 判据必红 |
| **T3 可消费** | 存在「平台消息/意图面 → 该表索引轴」的映射,**映射每一行同样有条款级 `ref`**;无锚行显式 `pending` 且 **fail-closed**(引擎 SHALL NOT 消费) | 映射表进规约载体(编译期锁)+ 毒化实测 |
| **T4 完备** | `Unknown` 格占比 = 0(或空集语义被规范明确) | 计数交叉核对(编译期 `static_assert`) |

**载体形态分类(⛔ 否则 T1/T2 的判定尺度不统一)**：

| 形态 | 适用族 | T1 / T2 的等价要求 |
|---|---|---|
| **状态矩阵型** | TileLink-C · CXL 2.0 / 3.0 | 态集 × 事件集矩阵 + 逐格 `basis`/`ref` + 删格必红 |
| **opcode / 事件表型** | CHI RN-F | 表级 `ref` + 表完备性 `static_assert`(⛔ **不要求**逐格 basis —— 其表是 opcode→channel/flow,**不是**态 × 事件矩阵) |

**T3 普查三要素(定义 —— 防下一个未定义术语)**：

| 要素 | 内容 |
|---|---|
| **行宇宙** | 「平台消息/意图面 → Event 轴」的**全部行**(哪些消息/意图 × 各事件)—— **逐行列出,不是抽样** |
| **锚标准** | 映射行的合法锚 = **规范条款级定位**;若某行只靠仓内构造(如 `ADR-CEP-12` 的 6 类消息类)⇒ 须标 `self-derived` 并**显式登记**,⛔ 不得当规范锚 |
| **交付物与登记位置** | 映射表进**规约载体**(编译期锁)+ 逐行 `basis` + `ref`;⛔ 悬空判据禁令 ⇒ 载体不存在前该判据**不可执行** |

> ⚠️ **两轴不同 artifact**:adapter 内「逐消息表」的映射目标是**消息类**(`ADR-CEP-12`),
> 与 T3 的 **Event 轴**不同 ⇒ ⛔ 不得据「adapter 已有逐消息表」宣称 T3 已满足。

**来源效力分档(适用新族是否计入,⛔ 不追溯既有格)**：

| 档 | 语义 | 可否计入「转移表真源」 |
|---|---|---|
| **规范条款级(`Direct`)** | 规范正文的 §/Table 级定位 | ✅ 可计入 |
| **人类成员条款摘要** | 人类按条款摘录 + 签署,含条款定位 | ✅ 可计入(**前提**:AI 不接触规范正文) |
| **公开非规范来源** | 博客 / 内核文档 / 厂商补丁 / webinar / 论文摘要 | ❌ **不构成** `Direct`;可作**探索载体**(弱档 + `pending=true` + fail-closed)但**不计入** |
| **仓内自构造(`self-derived`)** | 仅由本仓决策定义(如 6 类消息类) | ❌ 不计入 ⇒ 须显式登记 |

> **域边界(B3a vs B3b)**:同为非规范材料,**声明域**可被机器消费(实例声明经装配闸消费),
> **矩阵域**必须 fail-closed —— 理由是**错误代价不对称**:声明错 ⇒ 能力声明失准(可实验证伪、可回滚);
> 矩阵格错 ⇒ **引擎静默实现错误状态转移**且看起来权威。
> **carve-out(⛔ 不追溯)**:既有载体的 `basis` 枚举**注释语义不变**,其效力由各自 change 链授权。

**判据纪律(硬句)**:
- T1–T4 **合取**;任一子集 ✅ **不构成**触发、**不构成**达成度、**不得**作为立项 / 补引擎依据;
- 达成度**只许一个状态 token**(`✅ 达成 / ⛔ 冻结 / ⏭ 受限`),⛔ 不得出现「T1✅」字样;
- 逐要件 grid 只许留在附录,表头须写「**非判定粒度**」。
