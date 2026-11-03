# CEP · NVLink SHARP 公开来源锚点摘要（队列 #13 Task 0）

> **状态**: ✅ 已产出(2026-11-03) · **队列**: #13 `cpptlm-cep-nvlink-sharp`(Stage 2)· **achitect 裁决**: `ADR-CEP-01` / `02` / `03` 已签发 v1.0(Entry #169 / #170)· **VC**: Decision D(Entry #171)
> **用途**: 队列 #13 **规约层**的逐格 `basis` 来源。本文件**只记"来源定位 + 断言摘要"**,不复制任何规范 / NDA 正文。
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

| id | 来源 | 类型 | URL | bytes | sha256 |
|---|---|---|---|---|---|
| **S1** | *Demystifying NCCL: An In-depth Analysis of GPU Communication Protocols and Algorithms*(arXiv:2507.04786**v3**) | 公开预印本(学术) | `https://arxiv.org/html/2507.04786v3` | 235202 | `c7932a3ae3cfebd97ca62addf8623bf1511f7f9317d13f8014151d4e9708a66d` |
| **S2** | *Advancing Performance with NVIDIA SHARP In-Network Computing*(NVIDIA Developer Blog,2024-10-25,Scot Schultz) | 官方技术博客 | `https://developer.nvidia.com/blog/advancing-performance-with-nvidia-sharp-in-network-computing/` | 225120 | `68cda3daed0cff19309e529160b416ffa5c452c8138671125d23c90fa64fd29b` |
| **S3** | *NVIDIA Scalable Hierarchical Aggregation and Reduction Protocol (SHARP)* Rev **3.16.0** — Overview | 官方产品文档(公开) | `https://networking-docs.nvidia.com/sharpum/3.16.0` | 46015 | `bb415a6b57e0d14246f6ce742b4a5991e007176e35b45d17376c8dc4411e7ec3` |

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

⚠️ **已知脆弱点**:S2 / S3 是**动态网页**(含导航 / 版本选择器)⇒ 站点改版会让 hash 漂移但**语义未变**。⇒ 复核时若 hash 变化,**先**比对 §2 的断言是否仍成立;**断言不变可只更新 hash 并记 `reverified`**,断言变化 ⇒ 走完整重跑。
