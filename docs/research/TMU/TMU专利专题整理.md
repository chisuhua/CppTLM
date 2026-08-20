# NVIDIA TMU（Task Management Unit）专利专题整理

> 生成日期：2026-08-18 | 更新日期：2026-08-19（补入内网 PatentsDB 检索结果与 US20230236878A1/PDL）
> 目录路径：`/Users/chisuhua/source/myresearch/research/14_Patents`
> 检索路径：本机文件全文检索 → 浏览器历史核查 → Google Patents 网络检索 → 内网 PatentsDB 核查（2026-08-19，一次性例外授权）

---

## 1. 背景：TMU 在 NVIDIA GPU 架构中的位置

Task Management Unit（TMU任务管理单元）是 NVIDIA PPU（并行处理单元）前端任务调度子系统的核心部件。在不同代际的专利附图中它以三种形式出现：

- **Task/Work Unit 207 = Task Management Unit 300 + Work Distribution Unit 340**（US20130198760A1，Kepler 时代架构）：TMU 按执行优先级组织待调度任务，为每个优先级维护调度表（Scheduler Table 321），向 WDU 分发任务。
- **Task Management Unit 234**（US11182207，较新架构）：位于 Host Interface Unit 232 与 Work Distribution Unit 236 之间，接收来自主机接口的处理任务，管理任务描述符（Task Descriptor 430）、活跃任务列表（Active List 420）、依赖计数（Dependency Count 442 / Current Count 444）等元数据。
- **Work Scheduler/Distribution Unit 230（WSDU）**（US20230236878A1，2022 申请，Hopper 世代）：TMU 与 WDU 合并为单一单元，内含 Scheduling Dependency Logic 232 与片上 Task Dependency Table 240（依赖锁存器表），实现调度依赖/数据依赖解耦与消费者任务的提前启动——即 CUDA 11.8 PDL（Programmatic Dependent Launch）特性的硬件基础。

TMU 的核心职责可归纳为：任务接收与排队、基于优级的调度组织、任务间依赖管理、任务描述符的存取与预取。学术界对 TMU 也有实验性逆向分析（见第 7 节关联文献）。

---

## 2. 本机已有专利（TMU 核心，含全文）

| # | 专利号 | 标题 | 申请人/受让人 | 日期 | 正文 TMU 提及次数 | 本地文件 |
|---|--------|------|---------------|------|------|------|
| 1 | US 11,182,207 B2 | Pre-fetching Task Descriptors of Dependent Tasks | NVIDIA Corporation | 2021-11-23 授权 | 32 | [US11182207.pdf](file:///Users/chisuhua/source/myresearch/research/14_Patents/US11182207.pdf) |
| 2 | US 2013/0198760 A1 | Automatic Dependent Task Launch | NVIDIA Corporation | 2012-01-27 申请，2013-08-01 公开 | 29 | [US20130198760A1.pdf](file:///Users/chisuhua/source/myresearch/research/14_Patents/US20130198760A1.pdf) |

### 2.1 US 11,182,207 B2 — 依赖任务描述符预取

- **发明人**：Gentaro Hirota, Brian Pharris, Jeff Tuckey, Robert Overman, Stephen Jones
- **受让人**：NVIDIA CORPORATION, Santa Clara, CA
- **问题**：生产者任务完成时，启动消费者任务所需的信息（任务描述符）尚不用，造成流水线空转。
- **方案**：TMU（单元 234）在生产者任务被启动的同时，**并行地**从内存发起对消费者任务描述符的取回，用预取隐藏启动延迟。
- **关键结构**：Active List 420、Task Descriptor 430、Dependency Count 442、Current Count 444、Self-Reset Flag 446。
- **与 TMU 的关系**：本专利是对 TMU 描述符管理职责的直接增强，TMU 是方案的执行主体。
- **同族补充（内网 PatentsDB）**：预先公开 US-2020401444-A1；中国同族 CN-112130969-A。

### 2.2 US 2013/0198760 A1 — 依赖任务自动启动

- **发明人**：Philip Alexander Cuadra, Lacky V. Shah, Timothy John Purcell, Gerald F. Luiz, Jerome F. Duluk, Jr.
- **受让人**：NVIDIA Corporation（经 Google Patents 核实）
- **问题**：前后相继任务之间依赖信号量（semaphore）同步，任务切换延迟高。
- **方案**：把依赖任务的使能标志（Dependent TMD Enable 451）、描述符指针（Dependent TMD Pointer 452）、字段拷贝使能（453）等**编码进前驱任务的元数据（TMD）**；任务完成时 TMU（单元 300）收到通知，直接读取标志并自动启动依赖任务，无需软件介入。
- **关键结构**：Task Management Unit 300 + Work Distribution Unit 340 共同构成 Task/Work Unit 207；Scheduler Table 321 按优先级组织任务。
- **与 TMU 的关系**：给出了 TMU 在任务/work 分发子系统中的组织方式和依赖触发机制，是理解 TMU 职责的最基础文献。
- **同族补充（内网 PatentsDB）**：中国同族 CN-103226481-A（辉达公司；公开日 2013-07-31，申请日 2013-01-28，优先权日 2012-01-27）；内网库未收录本案的美国公开号记录。

> 注：这两份 PDF 在 `~/Documents/tech_docs/CudaStramCP/` 及 `CudaStramCP/WD/` 下另有副本。

---

## 3. 网络补充检索：其他涉及 TMU 的 NVIDIA 专利

检索条件：Google Patents，`q="task management unit" & assignee=NVIDIA`（2026-08-18 检索，共 10 条命中，前两条已在第 2 节列出）。

| 专利号 | 标题 | 公开/授权日 | 与 TMU 的关系 | 链接 |
|--------|------|------------|---------------|------|
| US 9,535,815 B2 | Collecting execution statistics for GPU workloads | 2017-01-03 | 正文明确写出 "task management unit (TMU)"；做 GPU 负载 trace 采集与源码匹配，附图中 TMU 位置清晰 | [Google Patents](https://patents.google.com/patent/US9535815B2/en) |
| US 10,552,202 B2 | Software-assisted instruction level execution preemption | 2020-02-04 | 指令级执行抢占，涉及任务/工作分发前端 | [Google Patents](https://patents.google.com/patent/US10552202B2/en) |
| US 2021/0349763 A1 | Technique for computational nested parallelism | 2021-11-11 | 计算嵌套并行，任务管理前端参与嵌套任务组织 | [Google Patents](https://patents.google.com/patent/US20210349763A1/en) |
| US 9,928,109 B2 | Method and system for processing nested stream events | 2018-03-27 | CUDA stream 嵌套事件处理，与 TMU 任务队列管理相关 | [Google Patents](https://patents.google.com/patent/US9928109B2/en) |
| US 8,572,573 B2 | Interactive debugging on a non-preemptible graphics processor | 2013-10-29 | 非抢占式 GPU 上的交互式调试 | [Google Patents](https://patents.google.com/patent/US8572573B2/en) |
| DE 102013114072 B4 | System and method for hardware scheduling of indexed barriers | 2024-08-01 | indexed barrier 硬件调度（德国同族） | [Google Patents](https://patents.google.com/patent/DE102013114072B4/en) |
| DE 102013017509 B4 | Efficient memory virtualization in multi-threaded processing units | 2024-09-26 | 多线程处理单元内存虚拟化（德国同族） | [Google Patents](https://patents.google.com/patent/DE102013017509B4/en) |
| DE 102012220029 B4 | Methods and systems for speculative execution and resetting | 2024-12-05 | 推测执行与重置（德国同族） | [Google Patents](https://patents.google.com/patent/DE102012220029B4/en) |
| DE 102013016871 B4 | （德国同族；机翻标题或有偏差，建议以原文为准） | 2024-09-12 | 待深入分析 | [Google Patents](https://patents.google.com/patent/DE102013016871B4/en) |
| US 2023/0236878 A1 | Efficiently launching tasks on a processor | 2023-07-27 公开（2022-01-25 申请） | **PDL 硬件专利**：TMU/WDU 合并为 WSDU，调度/数据依赖解耦、PREEXIT/ACQBULK、片上依赖锁存器表；对应 CUDA 11.8 PDL。经内网 PatentsDB 发现（US-2023236878-A1），PDF 与解析已入 `01_GPU_Architecture/12_Task_Management_Unit/` | [Google Patents](https://patents.google.com/patent/US20230236878A1/en) |

> 说明：DE 系列为同族专利的德国授权文本，公开日较晚属正常审查周期现象；如需美国对应文本可在 Google Patents 中通过 "Also published as" 字段追溯。US 2023/0236878 A1 为本地第 8 件核心专利，逐篇解析见 `01_GPU_Architecture/12_Task_Management_Unit/US20230236878A1_高效任务启动_解析.md`（含 PDL 对应关系专节）。

---

## 4. 时间线

| 时间 | 专利 | 里程碑意义 |
|------|------|-----------|
| 2012-01 申请 / 2013-08 公开 | US 2013/0198760 A1 | 首次系统披露 TMU（单元 300）职责与依赖任务硬件自动启动 |
| 2014-06 申请 / 2017-01 授权 | US 9,535,815 B2 | 明确使用 "task management unit (TMU)" 术语；GPU 负载统计 |
| 2018-03 授权 | US 9,928,109 B2 | 嵌套 stream 事件处理 |
| 2020-02 授权 | US 10,552,202 B2 | 指令级抢占 |
| 2021-11 授权 | US 11,182,207 B2 | TMU 描述符预取，进一步压缩依赖任务启动延迟 |
| 2021-11 公开 | US 2021/0349763 A1 | 嵌套并行 |
| 2022-01 申请 / 2023-07 公开 | US 2023/0236878 A1 | 依赖解耦与高效启动（WSDU、PREEXIT/ACQBULK）——CUDA PDL 的硬件披露 |
| 2024 陆续授权 | DE 同族 4 件 | 欧洲/德国布局 |

---

## 5. 内网 PatentsDB 补充检索（2026-08-19）

经用户一次性例外授权，通过浏览器访问内网 PatentsDB（`http://30.21.200.104:9527/PatentsDB/index.html`），绕过前端界面直接调用后端 API（`/api/patents?q=<tokens>&offset=&limit=`，对 title/summary/inventor/assignee 做 token AND 匹配），逐案核查本地 TMU 专利的专利族并扩展检索。

### 5.1 专利族补充

| 本地专利 | 内网库新发现的同族/关联成员 | 备注 |
|------|------|------|
| US20130198760A1 | CN-103226481-A辉达公司） | 中国同族；内网库**未收录**该案美国公开号 |
| US11182207B2 | US-2020401444-A1、CN-112130969-A | 美国预先公开 + 中国同族 |
| US10552202B2 | US-10552201-B2、US-9652282-B2（母案）、US-2017249152-A1、US-2017249151-A1、US-9710874-B2（关联） | 抢占延续案族 |
| US20210349763A1 | US-10915364-B2、US-2020151016-A1、US-2017083373-A1、US-9513975-B2（原始授权） | 嵌套并行延续案族 |
| US9928109B2 | DE-102013208554-B4 | 德国同族（nested execution streams，Durant） |
| US8572573B2 | DE-102013202495-A1 | 德国同族 |
| US20230236878A1 | US-2023236878-A1 | PDL 硬件专利（本次新分析对象） |

### 5.2 新发现的 TMU 相关专利（不在本地语料中）

| 专利号 | 主题 | 说明 |
|------|------|------|
| DE-102012222558-B4 | Signaling, ordering and execution of dynamically generated tasks | Purcell/Shah/Duluk/Treichler/Abdalla/Cuadra/Pharris；优先权日 2011-12-16，2023-12-14 授权；Kepler 时代动态任务的信令/排序/执行 |
| US-9286119-B2 | Management of dependency between tasks | 2013 年，任务间依赖管理（US11182207B2 首页申请人自引文献） |
| DE-102012216568-B4 | 优先级调度 | Purcell/Shah/Duluk |
| DE-102013201178-B4 | Controlling work distribution | Shah/Abdalla/Treichler |
| US-11954518-B2 | User-defined metered priority queues | 用户定义计量优先级队列（较新授权） |

### 5.3 检索注意事项

- 以 "TMU" 字面检索仅命中 2 条**无关**记录（DE-102018124298-A1、US-8570324-B2）：专利正文使用全称 "task management unit"，且该库为 token AND 匹配，须按发明人（Cuadra/Shah/Purcell/Hirota 等）、主题词（task、dependency、scheduling、nested）与受让人 NVIDIA 逐案组合检索；
- 内网库覆盖不完整：US20130198760A1 仅有 CN 同族记录，无美国公开号记录；
- API 端点：`/api/patents`（检索）、`/api/patents/trends`、`/api/categories`；返回字段含 id/title/summary/category_l2_primary/publication_date/priority_date/grant_date/filing_date/assignee/inventor/result_link。

---

## 6. 与本目录既有研究综述的关系

《研究综述.md》第 2.4 节「依赖任务自动调度与预取」已覆盖 US20130198760A1 与 US11182207 构成的专利群；本文档从 **TMU 视角** 做专题扩展，新增内容为：

1. TMU 在 PPU 前端中的结构定位（三代架构图对照，含 WSDU）；
2. 第 3 节网络补充的 10 件 TMU 相关专利（其中 US9535815 是正文直接使用 "TMU" 术语的关键文献，US20230236878A1 是经内网库发现的 PDL 硬件披露）；
3. 第 5 节内网 PatentsDB 补充的专利族成员与新发现专利；
4. 实验侧交叉验证文献（见第 7 节）。

---

## 7. 关联文献（非专利）

| 文献 | 位置 | 关联点 |
|------|------|--------|
| RTAS'23 "GPU Compute Partitioning with libsmctrl"（Hardware Compute Partitioning on NVIDIA GPUs） | `~/Documents/tech_docs/CudaStramCP/WD/rtas23.pdf` | 通过实验逆向分析 NVIDIA GPU 的 TMU 行为（任务并发限制），可与专利描述相互印证 |
| 2873053.pdf "A Closer Look at GPGPU" | `~/Documents/tech_docs/CudaStramCP/2873053.pdf` | GPGPU 架构白盒分析综述 |

---

## 8. 建议阅读路径

1. **US 2013/0198760 A1** — 理解 TMU 的基本职责、Task/Work Unit 组成与依赖触发机制（入门，附图清晰）；
2. **US 11,182,207 B2** — 理解 TMU 的依赖计数管理与描述符预取优化（进阶）；
3. **US 9,535,815 B2** — 查看 TMU 在整芯片架构图中的位置，以及 trace 采集如何经过前端（架构对照）；
4. **US 2023/0236878 A1** — 理解新一代 WSDU 的调度/数据依赖解耦与提前启动，及其与 CUDA PDL 的对应关系（最新进展，见 12_Task_Management_Unit 解析）；
5. **rtas23 论文** — 从实验角度验证 TMU 的实际行为与限制（交叉验证）。

---

## 9. 检索复现方法

- Google Patents 检索式：`"task management unit"` + assignee=`NVIDIA`
- XHR 接口（可程序化复现）：`https://patents.google.com/xhr/query?url=q%3D%22task%2Bmanagement%2Bunit%22%26assignee%3DNVIDIA`
- 本机全文检索命令：`mdfind '"task management unit"'`（Spotlight 会索引 PDF 内容）
- 内网 PatentsDB（需授权）：页面上下文 `fetch('/api/patents?q=<tokens>&offset=0&limit=50')`；勿用 "TMU" 字面检索，按发明人/主题词/受让人组合（详见第 5.3 节）

---

*本文档基于本地专利全文提取（pypdf）、Google Patents 网络检索与内网 PatentsDB 核查整理。US20230236878A1 为扫描件，经 PyMuPDF 渲染后视觉提取。专利的法律状态与权利要求范围以 USPTO/EPO 官方文本为准。最后更新：2026-08-19*

