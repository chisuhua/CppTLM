# CEP · NVLink SHARP 公开来源锚点摘要（队列 #13 Task 0）

> **状态**: ✅ 已产出(2026-11-03) · **队列**: #13 `cpptlm-cep-nvlink-sharp`(Stage 2)· **achitect 裁决**: `ADR-CEP-01` / `02` / `03` 已签发 v1.0(Entry #169 / #170)· **VC**: Decision D(Entry #171)
> **用途**: 队列 #13 **规约层**的逐格 `basis` 来源。本文件**只记"来源定位 + 断言摘要"**,不复制任何规范 / NDA 正文。
> **两类条目**: `A-n` = **公开来源断言**(§2)· `N-n` = **pending 白名单**(§3)· **`C-n` = 契约锚点**(§2bis,指向已签发本仓 ADR)
> ⇒ 三者合起来构成本 change **唯一的人工审计面**(per `tasks 3.1`)
> **产出者**: builder(Task 0 普查)· **复核周期**: 来源改版即失效(见 §6)

---

## 0. 合规边界(本轮的硬约束,写在最前以免误用)

| # | 约束 | 依据 | 本轮执行 |
|---|---|---|---|
| 1 | **不得读取 / 引用 NVLink SHARP 正式规范或产品 spec 正文** | 用户裁决 2026-11-03(照 CXL 的 Evaluation Copy Agreement + AI AUP 同款处理) | 未摄取任何规范正文;§1 的来源均为**公开可分发**材料 |
| 2 | **不得摄取标注 NDA / CONFIDENTIAL 的材料** | 同上 | §4 列出**已排除**的来源(含一份公开可下载但页面标注 CONFIDENTIAL 的讲稿) |
| 3 | 只记**断言摘要 + 条款级定位**,不抄录长篇正文 | 版权 + 本仓「附属文档引用真源、不重发明」 | §2 每条为**改写** + 定位(章节 / 表格编号) |
| 4 | 公开来源**不能**支撑的语义 ⇒ **必须标 `pending`**,不得自推 | `ADR-CEP-03 §4` 的 S1~S4 + 本文件 §3 | §3 明确列出「不得据此断言」的清单 |

---

## 1. 钉版清单(`sha256` = 抓取字节的整文件哈希)

抓取时间:**2026-11-03**(UTC 日)

| id | 来源 | 类型 | URL | bytes | sha256 | `pin` | 引用 / 再分发许可(**人工确认**) |
|---|---|---|---|---|---|---|---|
| **S1** | *Demystifying NCCL: An In-depth Analysis of GPU Communication Protocols and Algorithms*(arXiv:2507.04786**v3**) | 公开预印本(学术) | `https://arxiv.org/html/2507.04786v3` | 235202 | `c7932a3ae3cfebd97ca62addf8623bf1511f7f9317d13f8014151d4e9708a66d` | ✅ `pin=strong` | `引用 OK` / `再分发 待确认`(arXiv per-paper license 未逐一核验) |
| **S2** | *Advancing Performance with NVIDIA SHARP In-Network Computing*(NVIDIA Developer Blog,2024-10-25,Scot Schultz) | 官方技术博客 | `https://developer.nvidia.com/blog/advancing-performance-with-nvidia-sharp-in-network-computing/` | 225120 | `68cda3daed0cff19309e529160b416ffa5c452c8138671125d23c90fa64fd29b` | ✅ `pin=strong` | `引用 OK` / `再分发 未授权`(© NVIDIA) |
| **S3** | *NVIDIA Scalable Hierarchical Aggregation and Reduction Protocol (SHARP)* Rev **3.16.0** — Overview | 官方产品文档(公开) | `https://networking-docs.nvidia.com/sharpum/3.16.0` | 46015 | `bb415a6b57e0d14246f6ce742b4a5991e007176e35b45d17376c8dc4411e7ec3` | ✅ `pin=strong` | `引用 OK` / `再分发 未授权`(© NVIDIA) |

**两轴规则的判定面**(per `tasks 2.3` / `design §4`):`pin` = 钉版强度(有 `bytes` + `sha256` ⇒ `strong`);**许可列** = 「引用 / 再分发许可已**人工确认**」的结论。
⚠️ **三个来源的「再分发」许可均未确认** ⇒ 载体 `kSourceLicenseConfirmed[S1/S2/S3]` 一律取 **`false`**(保守)。
⇒ 因此本 change 的 12 格**没有任何一格以公开来源为断言依据**:全部判 `kind = self-derived`、`pending = false`、`source = AdrContract`(见 §2bis 口径)。

**权威级别声明**:S1(学术预印本,同行可追溯)> S2(官方博客,描述性)> S3(官方产品文档,描述性)。三者**均非规范**,冲突时**不得**用它们互裁;冲突 ⇒ 记 `conflict` 并标 `pending`。

---

## 2. 条款级定位 + 断言摘要

### A-1 **NVLink SHARP(NVLS)是"节点内"归约;跨节点走 CollNet**

| 项 | 内容 |
|---|---|
| **定位** | S1 §V-A *Overview of Algorithm and Protocol Support*(正文句 + Table II / Table III) |
| **断言摘要(改写)** | NCCL 的 **NVLS** 算法族使用 **NVLink SHARP 做节点内(intra-node)归约**;`NVLS` 与 `NVLS Tree` 的差别在**跨节点**处理:`NVLS` 继续经 **CollNet + 支持 SHARP 的交换机**做归约,`NVLS Tree` 改用树形 fan-out |
| **旁证(同节 Table II)** | 传输层分类表在**网页文本化**后的列序为 Intra-Node: `P2P` / `SHM` / `NVLS`,Inter-Node: `NET` / `COLLNET` —— 与正文句一致。⚠️ 与 Table III 同款**列对齐风险**(见 A-2),故仅作旁证、不作独立判据 |
| **对队列 #13 的约束** | `nvlink_4` 实例的 collective 能力边界 = **节点内**(NVSwitch 域)。**不得**把 NVLink SHARP 建模成跨 fabric 的通用归约;跨节点语义属 CollNet(= `ualink` 系 / 后续队列的面) |

### A-2 **NVLS 的能力面:以 AllReduce 为主,另支持 ReduceScatter / AllGather**

| 项 | 内容 |
|---|---|
| **定位** | S1 §V-A 正文句(紧邻 Table III 的说明段) |
| **断言摘要(改写)** | NVLS 与 CollNet 都是**主要为 AllReduce 优化**的专用算法;**NVLS 另借硬件能力支持 ReduceScatter 与 AllGather** |
| ⚠️ **不可用之处** | Table III 在**网页纯文本化后列对齐丢失**(✓/✗ 无法可靠对应到列名)⇒ **本文件不据此断言"哪个集合操作支持/不支持"的逐格矩阵** |
| **对队列 #13 的约束** | 规约层的 `flow_support` / reduce 算子族**只**能登记「AllReduce 为主 + ReduceScatter/AllGather 存在」这一**粒度**;逐操作布尔矩阵须标 `pending`(见 §3) |

### A-3 **归约发生在交换节点(in-network),而非端点**

| 项 | 内容 |
|---|---|
| **定位** | S2(§Creation of NVIDIA SHARP 段)· S3(Overview 段) |
| **断言摘要(改写)** | SHARP 把 all-reduce / reduce / broadcast 等集合操作**从 CPU/GPU 卸载到网络交换机**;**归约在 fabric 内完成**,随着到达聚合节点,网络中传输的数据量随之下降;S2 另称该迁移可把传输数据量**减少约一半**并降低抖动 |
| **对队列 #13 的约束** | 与 `ADR-CEP-01` 的「collective 是**旁路平面**、完成域为 group-scoped」一致;`ADR-CEP-03 §4` 的 **S1**(唯一分类点在 fabric ingress)得到该来源支持(归约点在网络侧,不在源端)。⚠️ **S4**(opcode 永不作分类器)**不**由本来源支撑 —— 它是 ADR 的**设计约束**,无需公开来源 |

### A-4 **第 3 代 SHARP:同一拓扑上可建多棵聚合树(多作业并发)**

| 项 | 内容 |
|---|---|
| **定位** | S3 Overview 段(「With the 3**rd** generation of SHARP…」) |
| **断言摘要(改写)** | 第 3 代 SHARP 允许在**同一拓扑**上构建**多棵聚合树**,使多个并行作业**共享**同一基础设施获得 in-network computing 收益 |
| **对队列 #13 的约束** | **`group_id` 不能是单例**:多 group 并存是**必须支持**的形态 ⇒ 规约层的 group 表 / epoch 表须按**多组**建模(单树假设 = 与来源冲突) |

### A-5 **NVLS 与 CollNet 是两条不同的算法通路(不是同一实现的两种模式)**

| 项 | 内容 |
|---|---|
| **定位** | S1 §V-A(Table II 的传输层分类 + 正文) |
| **断言摘要(改写)** | `NVLS` 与 `CollNet` 分属不同算法/传输实现(`nvls.cc` vs `coll_net.cc`);**NVLS Tree** 是 NVLS 的树形变体(节点内仍用 NVLink SHARP,跨节点改 fan-out) |
| **对队列 #13 的约束** | `NvlinkAdapter` 的语义位置 = **节点内 fabric 代理**,**不是** CollNet 的网络侧代理 ⇒ 不能复用 CollNet 的建模假设(与 `ADR-CEP-04` 的 adapter 分离纪律一致) |

---

## 2bis. 契约锚点(`C-n`)—— **self-derived 格的 `ref` 解析空间**

> **为什么需要这一节**:本 change 的 12 个格**全部**由**已签发的本仓 ADR** 定稿(而非公开来源)⇒
> 若 `ref` 只能指 `A-n` / `N-n`,则这些格**无处可指** ⇒ 与 `NSS-09`「每格 SHALL 带 `ref`」直接冲突。
> ⇒ 本节登记 **`C-n`** 命名空间:`C-n` 的真源是 **`docs/adr/` 的已签发条款**,不是公开来源。

### 2bis.1 逐条登记

| id | 契约内容(一句话) | **ADR 条款**(真源) | 覆盖的载体格 |
|---|---|---|---|
| **C-1** | R1/R2:**opcode 永不作分类器**;**唯一分类点 = fabric ingress** | `ADR-CEP-03 §3.1` R1 / R2 | `IngressDecision::DiverT` · `::Pass` |
| **C-2** | R3:注册三步原子(**登记 + drain + 建 epoch**);**drain 失败 ⇒ 注册失败且不建 epoch** | `ADR-CEP-03 §3.1` R3 | `RegisterResult::Ok` · `::DrainFailed` |
| **C-3** | R4:epoch **未 commit ⇒ 放行**;commit ⇒ divert;注销 ⇒ 失效化并**回归 coherence 域** | `ADR-CEP-03 §3.1` R4 | `EpochState::Uncommitted` · `::Committed` |
| **C-4** | ordering **A**:同址 coherent atomic → SHARP reduce ⇒ atomic 须先 completion | `ADR-CEP-03 §3.3` A | `OrderingRule::A` |
| **C-5** | ordering **B**:同址 SHARP reduce → coherent atomic ⇒ reduce 须先 commit | `ADR-CEP-03 §3.3` B | `OrderingRule::B` |
| **C-6** | ordering **C**:同 group 内 reduce 由 **code** 排序;⚠️ **code 相等 ⇒ 顺序未定义** | `ADR-CEP-03 §3.3` C | `OrderingRule::C` |
| **C-7** | ordering **D**:SHARP barrier → SHARP reduce ⇒ group-scoped rendezvous 须先完成 | `ADR-CEP-03 §3.3` D | `OrderingRule::D` |
| **C-8** | 集合 VC 预留 = **`unavailable`**(Decision D);**四字段权威在** `collective_planes.json` + `CP-03` | `ADR-CEP-01` SU 2026-11-03 **D.4** | `VcReservationState::Unavailable` |
| **C-9** | **能力域 = 节点内**(载体 SHALL NOT 声明跨 fabric 归约);⚠️ 只约束**能力域**,不约束**流量落点** | `ADR-CEP-01` SU 2026-11-03(#2) **G.1** + `ADR-CEP-03 §1` | `CollectiveScope::IntraNode`(佐证 `A-1` / `A-5`) |

⇒ **共 9 条**;12 个格**共享**这 9 条(允许**多格引同一条目**,照队列 #12 的 `Base` 先例)。

### 2bis.2 口径(三条,避免后人误读)

| # | 口径 |
|---|---|
| 1 | **`C-n` 是「契约锚点」,不是公开来源** ⇒ 以 `C-n` 为依据的格,其 `kind = self-derived`、`source = AdrContract`(**许可自持** = `true`) ⇒ `pending = false` **合法**(per 台账 Entry #182 的 **D-2**) |
| 2 | ⚠️ **`A-n` 在本 change 里只作「佐证指针」**:它说明契约**不凭空**,但**不是**断言的依据 ⇒ **不受**「许可已人工确认」门约束(该门只约束**以公开来源为依据**的格)。⇒ 若某格 `ref` 同时含 `C-n` 与 `A-n`,**门控看 `source`**(= `AdrContract`) |
| 3 | ⚠️ **`A-2` 与 `A-4` 当前无载体格** —— `A-2`(「AllReduce 为主 + ReduceScatter/AllGather 存在」的粒度)归**实现层**的 flow_support;`A-4`(多 group 并存)载体仅以 `GroupId` **形状**承载、**未断言**「多 group」⇒ 二者登记为**未表达**,⛔ **不得**声称已覆盖(per `tasks 2.8c`) |

> ⚠️ **不得据此扩 `A-n`**:`C-n` 的存在**不**豁免「公开来源不支持 ⇒ 必须 `pending`」这条规则 —— 该规则约束的是
> **以公开来源为依据**的情形;本 change 的 self-derived 格压根**不以公开来源为依据**。

---

## 3. ⚠️ **不得据此断言**的清单(negative space —— 规约层必须标 `pending`;**9 项**)

公开来源**没有**给出、或给出的粒度不足以下判断据的,一律 **`pending` + 注「无公开来源支撑」**:

> `N-1` ~ `N-7` 来自 Task 0 普查(Entry #173);`N-8` / `N-9` 来自裁决 ③ / ④(Entry #177)。

| # | 事项 | 为何不得断言 |
|---|---|---|
| N-1 | **reduce 函数族的穷举算子集**(sum / min / max / prod / bitwise… 的确切闭集) | 三个来源只给"summing, averaging"这类**举例**,非闭集 |
| N-2 | **逐集合操作的 ✓/✗ 支持矩阵** | S1 Table III 网页文本化后列对齐丢失(见 A-2) |
| N-3 | **group epoch 的状态机与提交时点** | 无公开来源;§2 无对应断言 |
| N-4 | **VC / 通道编号、每 VC 的用途分配** | 与官方材料无关;且本仓 VC 已由 Decision D 明确「无隔离 VC」 |
| N-5 | **聚合树的构建算法**(谁选根、树高、失败重建) | 无公开来源 |
| N-6 | **协议报文格式 / opcode 布局** | 属规范正文 ⇒ 合规边界禁止(§0 #1) |
| N-7 | **数值 / 精度语义**(浮点归约顺序、确定性) | 来源未涉及 |
| N-8 | **NVLink 实例的 `perm` 具体子集** —— 公开来源未描述 NVLink 的内存权限模型;⚠️ **不得**照抄 `tilelink_c` 的 3 值(那是 TileLink 的语义) | 来源未涉及(裁决 ③,per 台账 Entry #177) |
| N-9 | **NVLink atomic 的协议侧 completion 配对** —— 公开来源未描述 NVLink 的 atomic 语义;⚠️ improvement 原倾向「`request` + `completion`(RMW 语义)」**无公开来源支撑** ⇒ 不得作为载体断言 | 来源未涉及(裁决 ④,per 台账 Entry #177) |

> ⚠️ **判据要求**(`ADR-CEP-03 §4` 的 S1~S4 + 本仓 4 铁律):规约层每一格的 `basis` **必须**指向本文件 §2 的 A-n 或明确写 `pending`;**不得**出现「凭直觉填值 + basis 留空」。
> ⇒ 若某格 `pending` 却又被实现层当作定值消费 ⇒ 视为**过度声明**(`ADR-CEP-03 §4` 回退口径)。

---

## 4. 已排除的来源(合规记录 —— 说明"为什么没用",不是"没找到")

| 来源 | 可得性 | 排除理由 |
|---|---|---|
| Devendar Bureddy,《SHARP: In-Network Scalable Streaming Hierarchical Aggregation and Reduction Protocol》(MUG 2020 讲稿) | 公开可下载 | ⚠️ 页面/每页标注 **"NVIDIA CONFIDENTIAL. DO NOT DISTRIBUTE."** ⇒ **未摄取任何内容**(仅记录该排除决定) |
| NVLink SHARP / NVSwitch 的正式规范或产品 spec 正文 | 需授权 | §0 #1(用户裁决 + ECA/AI AUP 同款)⇒ 未摄取 |
| NCCL 源码(`nccl/src/device/*.cc` 等) | 公开 | 本轮**未**作为锚点:源码是**实现**,不是**语义声明**;若后续需要(例如确证 NVLS 的算法分支),须**单独立项**并说明为何论文不足 |

---

## 5. 对队列 #13 的直接影响(汇总)

| # | 结论 | 落在哪 |
|---|---|---|
| 1 | `nvlink_4` 的 collective 能力 = **节点内**(A-1 / A-5) | 实例声明 + 规约层能力面 |
| 2 | 多 group 并存是**必须**(A-4) | group 表 / epoch 表按多组建模 |
| 3 | 归约点在 **fabric ingress**(A-3)⇒ 支撑 `ADR-CEP-03` 的 **S1**;⚠️ **S4 属设计约束**,不引用公开来源 | 分类判据 |
| 4 | reduce 算子族 / 逐操作矩阵 / epoch / VC / 报文格式 ⇒ **`pending`**(§3) | 逐格 `basis` |

---

## 6. 复核方式(来源改版即失效,**不自动采信**)

```bash
# 逐条复核:重抓 → 比对 sha256;不一致 ⇒ 本文件失效 ⇒ 重跑 Task 0(不得只改 hash)
curl -sSL -A "Mozilla/5.0" https://arxiv.org/html/2507.04786v3 | sha256sum
curl -sSL -A "Mozilla/5.0" https://developer.nvidia.com/blog/advancing-performance-with-nvidia-sharp-in-network-computing/ | sha256sum
curl -sSL -A "Mozilla/5.0" https://networking-docs.nvidia.com/sharpum/3.16.0 | sha256sum
```

⚠️ **`C-n` / `N-n` 的复核方式不同**:二者**不依赖重抓**,而依赖**本仓 ADR / 普查结论是否变更** ⇒
对应 `ADR-CEP-01` / `ADR-CEP-03` 追加 **Status Update**、或 `N-n` 的 pending 结论被裁定时,
**本文件在对应格上即失效** ⇒ 须同步改 §2bis / §3 **并与载体 `basis` 表同批更新**(否则 `tasks 3.1` 的逐格核对必红)。

⚠️ **已知脆弱点**:S2 / S3 是**动态网页**(含导航 / 版本选择器)⇒ 站点改版会让 hash 漂移但**语义未变**。⇒ 复核时若 hash 变化,**先**比对 §2 的断言是否仍成立;**断言不变可只更新 hash 并记 `reverified`**,断言变化 ⇒ 走完整重跑。
