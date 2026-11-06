# CEP · UALink INC 公开来源锚点摘要（队列 #14 Task 0）

> **状态**: ✅ 已产出(2026-11-04) · **队列**: #14 `cpptlm-cep-ualink-inc`(Stage 3)· **开工前置**: `ADR-CEP-01` / `04` / `09` 的 2026-11-03/04 三则 SU(台账 **Entry #193**)
> **用途**: 队列 #14 **规约层**的逐格 `basis` 来源。本文件**只记"来源定位 + 断言摘要"**,不复制任何规范正文。
> **两类条目**: `A-n` = **公开来源断言**(§2)· `N-n` = **pending 白名单**(§3)· **`C-n` = 契约锚点**(§2bis,指向已签 ADR)
> ⇒ 三者合起来构成本 change **唯一的人工审计面**
> **产出者**: builder(Task 0 普查)· **复核周期**: 来源改版即失效(见 §5)

---

## 0. 合规边界(本轮的硬约束,写在最前以免误用)

| # | 约束 | 依据 | 本轮执行 |
|---|---|---|---|
| 1 | ⛔ **不得读取 / 引用 UALink Common 2.0 的规范正文**(PDF 技术正文) | **实测**:该规范的下载门是 **EVALUATION COPY AGREEMENT**(2025-04-07 生效,落地页法律正文)⇒ 与本仓 **CXL ECA §3**「USE OF ARTIFICIAL INTELLIGENCE SYSTEMS」**同型**(per 台账 `:7810` / `:9632` 的 CXL 禁令先例)。⚠️ **用户 2026-11-03 的裁决「UALink Common 2.0 是公开规范,可投喂」**系基于「公开可下载」;**实测授权条款为 Evaluation Copy** ⇒ 本轮按**保守侧**执行(照 NVLink SHARP 同款处理,per `:10477` / `:10829`) | §1 的 4 个来源**全部**是**零注册可访问**的公开页面;**未**下载、**未**打开、**未**引用任何规范 PDF |
| 2 | **不得摄取标注 NDA / CONFIDENTIAL 的材料** | 同上 | §4 列出**已排除**的来源(含 2 个 403 的新闻站) |
| 3 | 只记**断言摘要 + 条款级定位**,不抄录长篇正文 | 版权 + 本仓「附属文档引用真源、不重发明」 | §2 的每条 `A-n` ≤ 2 句 |
| 4 | 公开来源**不能**支撑的语义 ⇒ **必须标 `pending`**,不得自推 | `ADR-CEP-03 §4` 的 S1~S4 + 本文件 §3 | §3 的 **9 项** pending |

### 0.1 ⚠️ 一处**用户裁决与实测的偏差**(登记,不改裁决)

| 面 | 用户裁决(2026-11-03) | 实测(2026-11-04) |
|---|---|---|
| UALink Common 2.0 的合规边界 | 「是**公开规范**,**可投喂**」 | 落地页载明 **EVALUATION COPY AGREEMENT**(2025-04-07)+「ALL RIGHTS NOT SPECIFICALLY GRANTED HEREIN ARE RESERVED BY UALink」+ 下载需签署 EULA(姓名/公司/邮箱)⇒ **与 CXL ECA 同型** |

⇒ **对本 change 的实质影响 = 零**:§2 的公开来源**本就不含** INC 的技术语义(见 §2bis)⇒ 无论合规边界如何,**9 项细粒度语义都是 `pending`**。
⚠️ **2026-11-06 附注**:`N-8` / `N-9` 已由**非规范公开来源**闭合(per §2bis **`C-10`**)⇒ **不依赖**规范正文 ⇒ **不违反**本节禁令;
但 ⛔ 该闭合是「**一步推断**」⇒ 载体以 `SelfDerived` + ref `C-10` 表达(**不**发明第三种 basis 值)⇒ 后续读本文件者**不得**把它读成「有规范依据」。
⇒ 差别只在**未来**能否升级:若用户后续**自行**取得 Evaluation Copy 并**明确**授权投喂,`N-n` 才有转正可能。
⇒ **本轮不代用户改裁决**;偏差登记于此 + 台账。

---

## 1. 钉版清单(`sha256` = 抓取字节的整文件哈希)

抓取时间:**2026-11-04**(UTC 日)· 抓取方式:`curl -sSL`(零注册 · 零 Cookie)

| id | 来源 | 类型 | URL | bytes | sha256 | 许可(人工确认) |
|---|---|---|---|---|---|---|
| **S1** | UALink Consortium · **Specifications** 总页 | 联盟官网(公开) | `https://ualinkconsortium.org/specification/` | 158850 | `1db98a15da47ee72bb11886804ad28017e8548501f21838d041feb2053afe553` | 公开页面 · **可引**(不含规范正文) |
| **S2** | UALink Consortium · **UALink Common 2.0 Specification** 落地页(含 ECA 法律正文 + 公告 + 下载门) | 联盟官网(公开) | `https://ualinkconsortium.org/specification/ualink-common-2-0-specification/` | 187854 | `76f2196465a32725f949d8af8d7ca61b010bde15b948ddd96fe558ff9027b2ae` | 公开页面 · **可引**;⚠️ 其**技术正文**(PDF)**不在**本轮范围(§0 #1) |
| **S3** | UALink Consortium · **UALink 200G 1.0 Specification** 落地页 | 联盟官网(公开) | `https://ualinkconsortium.org/specification/ualink-200g-1-0-specification/` | 188483 | `a49a48ba00b35744a0f703642d12b97bc8b285171b3607692dcad26b5e6d2a59` | 同上 |
| **S4** | UALink Consortium 博客 · *Exploring In-Network Compute: How UALink Is Redefining AI Scale-Up Architecture* | 联盟官网(公开) | `https://ualinkconsortium.org/blog/exploring-in-network-compute-how-ualink-is-redefining-ai-scale-up-architecture-1509/` | 160827 | `dff4d84bdc92034484ad77c6cd7cf6069b6121dc5d001233a4a7a13aca46b337` | 同上 |
| **S5** | Data Center Digest · *UALink Consortium Publishes Four Specifications…*(新闻稿转载) | 第三方媒体(公开) | `https://datacentredigest.com/ualink-consortium-publishes-four-specifications-to-support-deployment-in-multi-workload-environments/` | 972456 | `26afb1cbf22c973b6803c0c2eadb9d0cd19660b5a7d7d4b7a1829f931f0f12cd` | 第三方通稿 · **可引** |

**权威级别声明**:**S2 / S3**(联盟官网 · 规范自身的落地页 + 法律正文)> **S4**(联盟官方博客 · 描述性)> **S5**(第三方通稿 · 描述性)。
⇒ 五者**均非规范正文** ⇒ **不能**支撑任何细粒度协议语义 ⇒ §3 的 9 项 `pending` 由此而来。

---

## 2. 公开来源断言(`A-n`)

| id | 断言摘要(≤2 句) | 来源 | 定位方式 |
|---|---|---|---|
| **A-1** | UALink **Common 2.0** 规范**已发布**,其相对 1.0 的核心新增是 **In-Network Compute(INC)** —— 目的是让计算与通信在加速器之间**融合**,以**降延迟、省带宽、提扩展效率** | S1 + S2 + S5 | 落地页公告块 + 通稿条目 |
| **A-2** | INC 的**适用操作族**被联盟博客描述为:归约与聚合(reductions and aggregations)· 同步功能(synchronization functions)· 集合通信原语(collective communication primitives)· 数据变换(data transformations)· 调度优化(scheduling optimizations) | S4 | 博客正文「Some examples of operations that can benefit from In-Network Compute include…」 |
| **A-3** | Common 2.0 的**规范正文获取**受 **EVALUATION COPY AGREEMENT**(2025-04-07 生效)约束;下载需接受 EULA(姓名/公司/邮箱/同意条款);协议明文「ALL RIGHTS NOT SPECIFICALLY GRANTED HEREIN ARE RESERVED BY UALink」,且除 §2.1 Evaluation License 的有限用途外**不授予**任何实施或再利用许可 | S2 | 落地页法律正文块 |
| **A-4** | **DL/PL(数据链路/物理层)已从 Common 规范中拆出**为独立规范(128G DL/PL **1.0** 与 200G DL/PL **2.0**),以免物理层演进拖慢 Common 规范;Common 2.0 因此**不含**物理层定义 | S1 + S5 | 落地页条目 + 通稿条目 |
| **A-5** | 本轮共发布 **4 份**规范:UALink **Common 2.0** + 128G DL/PL 1.0 + 200G DL/PL 2.0 + Chiplet 1.01(另有 Manageability 1.0 在同批) | S1 + S5 | 落地页条目枚举 + 通稿条目枚举 |

> ⚠️ **A-1 ~ A-5 的边界**:它们只支撑「**有什么**、**为什么**」,**不支撑**「**怎么做**」(原语定义 / 语义 / 排序 / 原子性 / 部分失败)。

---

## 2bis. 契约锚点(`C-n` —— 指向**已签**的仓内决策;非公开来源)

| id | 锚点 | 已签依据 |
|---|---|---|
| **C-1** | **VC 路径 = M-2**(集合流量**共享** coherence 已占用的 VC + **四条可机器判定的证书** C-1′/C-2/C-3/C-4) | `ADR-CEP-01` SU 2026-11-03(#4)(台账 Entry #193) |
| **C-2** | UALink 是**全集合族**(对比 NVLink = SHARP op 集) | `ADR-CEP-03:70` |
| **C-3** | **原语最小集 = 恰 4**;`AllReduce` = REDUCE + BROADCAST 的**复合**,**不是**第 5 原语 | `ADR-CEP-01:94` / `ADR-CEP-03:75`(判据 `CP-PRIM-SET`) |
| **C-4** | 集合 VC 预留为**空** + `vc_reservation_declaration` **四字段**齐全;`status` 取值域 = 2 值 | `ADR-CEP-01` SU#4 的 E.6(取代 D.4 的 `V6-a` / `V6-c`) |
| **C-5** | 版本锁 = **`ualink-2.0`** / UALink Common 2.0 | `ADR-CEP-11 §3` |
| **C-6** | adapter = `UalinkAdapter`;INC 四原语 = `ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce`;通道 = Station / Port / Lane;TL Load / Store / Atomic | 07 号 §5.1.1 UALink 行(⚠️ **仓内**来源,非公开来源) |
| **C-7** | tracker 选型 = **候选集(7 值)+ 逐实例理由登记**;⛔ **不存在** `flow_support` 派生规则 | `ADR-CEP-04` SU 2026-11-03 |
| **C-8** | `ualink_fabric` 的 4 个 VC 语义 = VC0=Fault Request / VC1=Atomic / VC2=Remote Query / VC3=Response(**已锁定**) | ArchForge `22-nsa` §0.5.4.1 表 `:147`(经 `fabric.json` 的 `derived_from` 指针) |
| **C-9** | `moesi_gpu` / `mesif` 在 `ualink_fabric` 上的 `vc_binding` **逐类相等** ⇒ C-1′ 的「继承良定义」成立 | `protocol_instances.json`(实测) |
| **C-10** | `ualink_4` 的 **`flow_support.snoop` / `snoop_resp` = `false`**(链路上**不承载** snoop 类流量)与 **`perm` = `["NONE"]`**(零可锚定非 `NONE` 值)两项取值的**裁决本体** —— ⚠️ 支撑来源 = **4 个互相独立的公开非规范来源**:① Linux 内核 `docs.kernel.org/gpu/amdgpu/ualink.html`(7.4-rc1)——「direct loads/stores over the fabric」,**无 snoop 事务类型**(其 remote TLB shootdown 是 **TLB 失效**非 cache snoop)② AMD amd-gfx **95 个补丁**系列(2026-08-31)——「no copy involved; direct loads/stores over the fabric」,无 coherence 引擎 ③ UALink Consortium 官方博客(2025-05-15)——「**Coherency is maintained through software**」④ UnifabriX webinar(联盟官网托管,2025-12-10)——「software coherency」,并把 `Coherent (via CXL.mem/CXL.cache/CHI C2C)` 明确归给 **host 侧**。⇒ ⛔ **本项是「非规范来源支撑的建模声明」,不是「有规范依据」**;公开可钉版文本中**没有字面 "no snoop"**(该字面句只在**禁读**的规范正文与**不可钉版**的 YouTube 转录里)⇒ 「software coherency ⇒ `snoop=false`」是**一步推断**;⛔ 本仓**未**读取任何 UALink 规范 PDF 技术正文 | `ADR-CEP-04` **SU 2026-11-06**(per 台账 **Entry #238**)——**已签**的仓内决策 ⇒ 载体 `ualink_inc_state_spec.hh` 的 `Uis08` / `Uis09` 以 `SelfDerived` + ref `C-10` 表达(⛔ **不发明第三种 `BasisKind` 值**) |

---

## 3. pending 白名单(`N-n` —— 公开来源**不能**支撑 ⇒ **不得自推**)

| id | 待定语义 | 为什么公开来源撑不住 | 若要转正需要 |
|---|---|---|---|
| **N-1** | INC **四原语的规范定义与语义**(`ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce` 各自的确切行为) | 公开来源**连原语名都没有**(A-2 只给「操作族」);四原语名来自**仓内** C-6 ⇒ **无**公开条款级依据 | 规范正文条款级定位(Evaluation Copy) |
| **N-2** | `WriteMulticast` 的**部分失败**语义(哪些成员失败时其余成员观察到什么) | 同 N-1;公开来源零涉及 | 同上 |
| **N-3** | `ReadReduce` 与 **coherence 可见性**的关系(归约结果对缓存副本的可见性点) | 同 N-1;且跨协议可见性属本仓抽象层,非 UALink 规范 | 同上 + `ADR-CEP-03` 的跨平面契约细化 |
| **N-4** | INC 原语与 coherence 流的**排序 / 原子性**要求 | 同 N-1 | 同上 |
| **N-5** | INC 的**控制面**(group / epoch / 成员集合)是否由 UALink 定义,还是完全由**本仓**抽象 | 公开来源未涉及控制面 | 同上;若规范**不**定义 ⇒ 由 `ADR-CEP-03` 的既有抽象承担(则本仓**自推导**,须另行登记) |
| **N-6** | 本仓 `ReduceOp`(恰 6 值)与 UALink 归约算子的**对应关系** | 公开来源未给算子表 | 同上 |
| **N-7** | 集合流量在 `ualink_fabric` 上占用的 **VC 语义**(4 个 VC 均为硬件语义锁定的既有用途) | C-8 已锁定 VC 语义;INC 是否需要**新** VC 语义未知 ⇒ M-2 下**继承**既有映射(C-1′),但「继承是否足够」依赖 N-1~N-4 | 同上 |

⇒ **7 项 pending**(`N-1` ~ `N-7`)—— ⚠️ **2026-11-06**:`N-8` / `N-9` **已转正**(取值见 §2bis 的 **`C-10`**;
转正依据 = **4 个互相独立的公开非规范来源** + 显式「一步推断」限定,⛔ **不是**规范依据 ⇒ **不依赖**规范正文 ⇒ 不违反 §0 禁令)⇒
**白名单 9 项 → 7 项**(载体 `ualink_inc_state_spec.hh` 的 `kPendingCount` 同步 **9 → 7**)。
⇒ **本 change 的任何语义格都不得以公开来源为断言依据**
⇒ 逐格 `basis` 只能是 `kind = self-derived`(引 `C-n` 的仓内锚点)或 `kind = pending`(引 `N-n`)。

---

## 4. 已排除的来源(登记,防复用)

| 来源 | 排除理由 |
|---|---|
| UALink Common 2.0 / 200G 1.0 的**规范 PDF** | ⛔ **Evaluation Copy Agreement**(A-3)⇒ 技术正文**禁投喂**(§0 #1) |
| BusinessWire 通稿(`businesswire.com/news/home/20260407620696/...`) | `curl` **HTTP 403**(反爬)⇒ 无法钉版 ⇒ 排除(内容由 S5 转载覆盖) |
| HPCwire 通稿(`hpcwire.com/off-the-wire/ualink-consortium-publishes-4-specifications...`) | `curl` **HTTP 403** ⇒ 同上 |
| LinkedIn / YouTube 的联盟帖子与视频 | 非规范来源 + 不可稳定钉版 ⇒ 排除 |

---

## 5. 失效条件(来源改版即本文件失效)

| # | 条件 | 后果 |
|---|---|---|
| 1 | S1 ~ S5 任一页面的 `sha256` 不再匹配(改版 / 重排) | 本文件的 `A-n` 定位**全部**失效 ⇒ 须重抓并重新钉版 |
| 2 | UALink 发布 **Common 2.1+** 或 INC 的规范条款被公开 | `N-1` ~ `N-9` 的可转正性变化 ⇒ 须复核 `N-n` 状态 |
| 3 | 用户后续**明确**授权投喂 Evaluation Copy 正文 | `N-1` ~ `N-9` 可开始转正 ⇒ 须走**新的** Task 0 轮次(不得在本文就地扩写) |
| 4 | 本仓 `ADR-CEP-01` SU#4 的证书四条被再次裁决 | `C-1` / `C-4` 失效 ⇒ 本文件的契约锚点须同步 |

---

**关联**: 队列 #14 improvement(`.rddf/improvements/cpptlm-cep-ualink-inc.md`)· `ADR-CEP-01` SU 2026-11-03(#4)·
`ADR-CEP-03:70` / `:75` · `ADR-CEP-04` SU 2026-11-03 · `ADR-CEP-11 §3` · 07 号 §5.1.1 UALink 行 ·
`protocol_instances.json`(C-9 实测)· ArchForge `22-nsa` §0.5.4.1(C-8)· 台账 `obj-cep-platform.md` Entry #193 / #194
