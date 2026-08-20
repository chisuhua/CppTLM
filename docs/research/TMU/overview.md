# 任务管理单元（TMU）研究综述

> 领域：GPU架构 | 子领域：任务管理与前端调度 | 文献数量：8篇专利 | 更新日期：2026-08-19

---

## 一、领域概述

Task Management Unit（TMU，任务管理单元）是 NVIDIA GPU（专利中称 PPU，Parallel Processing Unit）前端任务调度子系统的核心硬件，承担从主机接口接收任务、按优先级组织任务、管理任务间依赖、向处理集群分发任务（grid/CTA）的职责。NVIDIA 白皮书与公开文档极少披露 TMU 细节，而专利文献是理解这一"宏观调度器"（macro-scheduler）最系统的一手资料。

本目录汇集 8 件涉及 TMU 的 NVIDIA 专利（2012–2022 申请），覆盖任务前端的六大主题：**依赖任务管理**（自动启动与描述符预取）、**依赖解耦与高效启动**（调度/数据依赖解耦、pre-exit 触发，即 CUDA PDL 的硬件机制）、**执行抢占**（指令级/CTA级/软件辅助）、**嵌套并行**（设备端子 grid 发射，即 CUDA Dynamic Parallelism 的硬件基础）、**嵌套流事件**（无 CPU 介入的跨 stream 依赖强制）、**调试与性能剖析**（非抢占式调试、warp 级 trace 采集）。8 份 PDF 均存放于本目录，逐篇解析见对应的 `_解析.md` 文档。

## 二、TMU 在架构中的三种形态

综合各专利附图，TMU 以三种形态出现，均位于 Host Interface 与 SM 阵列之间的任务分发路径上：

| 形态 | 出处 | 组成与职责 |
|------|------|-----------|
| Task/Work Unit 207 = TMU 300 + Work Distribution Unit 340 | US20130198760A1、US8572573B2、US10552202B2、US20210349763A1、US9928109B2 | TMU 300 维护 Scheduler Table 321（按优先级的 TMD 分组链表）、TMD Cache 350；WDU 340 维护 Task Table 345（CTA 槽位，支持高优先级驱逐） |
| 独立 TMU 单元（215 / 234） | US9535815B2、US11182207B2 | 组织 pending/active grid 池，管理依赖计数、描述符预取；US9535815B2 明确称其为 "macro-scheduler"，与 SM 内 scheduler unit（"micro-scheduler"）构成两级调度 |
| Work Scheduler/Distribution Unit 230（WSDU，调度/分配合并） | US20230236878A1（2022 申请，Hopper 世代） | TMU 与 WDU 合并为单一单元，内含 Scheduling Dependency Logic 232 与片上 Task Dependency Table 240（依赖锁存器表）；实现调度/数据依赖解耦与提前启动，并接收 SM 端直接提交的消费者任务命令 |

关键术语：TMD（Task Metadata，任务元数据/描述符）是 TMU 调度的基本对象，编码 grid 参数、优先级、依赖字段等。

## 三、逐篇文献要点

### 3.1 US 2013/0198760 A1 — Automatic Dependent Task Launch（2012 申请，奠基之作）

首次系统披露 TMU（单元 300）结构：按优先级分层的 Scheduler Table、接收/调度速率解耦、TMD Cache。核心创新是把依赖任务的使能标志（Dependent TMD Enable）、描述符指针（Dependent TMD Pointer）、字段拷贝使能编码进前驱任务的 TMD，任务完成时 TMU 自动启动依赖任务，取代信号量同步。共 21 项权利要求。详见 [US20130198760A1_依赖任务自动启动_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20130198760A1_依赖任务自动启动_解析.md)

### 3.2 US 11,182,207 B2 — Pre-fetching Task Descriptors of Dependent Tasks（2021 授权，延迟优化）

TMU 234 内置 Dependency/Prefetch Unit 410：依据生产者 TMD 中的 Consumer Flags（462/464/466）在生产者执行期间并行预取消费者的描述符/指令/常量，存入 Consumer Task Descriptor Cache 480；flush 完成后递减消费者 Current Count 444，归零进入 Active List 420；Self-Reset Flag 446 支持循环依赖免软件复位。文本称可将依赖解析延迟削减一半以上。18 项权利要求。详见 [US11182207B2_依赖任务描述符预取_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US11182207B2_依赖任务描述符预取_解析.md)

### 3.3 US 9,535,815 B2 — Collecting Execution Statistics for GPU Workloads（2017 授权，TMU 术语与定位）

正文明确 "macro-scheduler (i.e., task management unit)" 与 "micro-scheduler (i.e., scheduler unit)" 的两级调度命名，附图给出 TMU 215 在芯片中的位置（host interface unit 210 与 work distribution unit 220 之间，组织 pending grid 池）。技术本体是 SM partition 内 trace cell 420 周期性快照全部 warp 的 PC + stall vector（16 条 FIFO），支持关联回源码行。实施例参数与 Kepler GK110 世代吻合。详见 [US9535815B2_GPU负载执行统计收集_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US9535815B2_GPU负载执行统计收集_解析.md)

### 3.4 US 10,552,202 B2 — Software-assisted Instruction Level Execution Preemption（2020 授权，抢占）

front end 212 控制的五阶段抢占框架（停止→保存→复位→加载→重启），三个实施例：硬件指令级抢占（SM 停在指令边界，trap handler 保存状态）、CTA 级抢占（排空 GPC，定时器到期回退为指令级）、软件辅助指令级抢占（借 CTA 级通道触发 preemption-save kernel，把 preemption TMD 插入最高优先级链表头，恢复由 TMU 优先调度）。TMU 300 是抢占停发的第一级闸门与任务级状态存点。是母案 US 9,652,282 的 continuation。详见 [US10552202B2_指令级执行抢占_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US10552202B2_指令级执行抢占_解析.md)

### 3.5 US 2021/0349763 A1 — Technique for Computational Nested Parallelism（2021 公开，CDP 硬件基础）

延续案链条可追溯至 2012-05-02（原始授权 US 9,513,975），与 CUDA 5.0 Dynamic Parallelism 同期。父线程经 `A<<<B>>>C` 发射子 grid；`cudaThreadSynchronize()` 时父 CTA 状态存入 continuation state buffer 并释放 SM；子 grid 经 MMU 的 PCAS（posted compare-and-swap）入队；子任务由 processing cluster array **直接提交给 TMU，绕过 pushbuffer/front end/CPU**，且子任务优先级链表高于停在屏障的父任务，避免死锁。详见 [US20210349763A1_嵌套并行计算_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20210349763A1_嵌套并行计算_解析.md)

### 3.6 US 9,928,109 B2 — Method and System for Processing Nested Stream Events（2018 授权，流调度）

stream 映射为 GPU 预分配的 TMDQ；跨流依赖经图变换为 wait event/signal event 对，原子递减 dependency count、归零即推进流，全程无锁、无 CPU 介入；OOM 时退化为单流串行化保正确性；事件逻辑由任务完成后 AtExit 调度的 scheduler kernel 执行。正文明确 "task/work unit 207 includes task management unit 300 and work distribution unit 340"，TMU 负责 grid/TMD 级硬件调度，scheduler kernel 负责 stream 级事件依赖。详见 [US9928109B2_嵌套流事件处理_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US9928109B2_嵌套流事件处理_解析.md)

### 3.7 US 8,572,573 B2 — Interactive Debugging on a Non-preemptible Graphics Processor（2013 授权，调试）

非抢占式 GPU 上的 re-launch loop 调试：trap 发生时保存 in-progress 线程状态（special registers、local/shared memory）到系统内存，杀线程腾出 GPU 维持屏幕刷新；收到 resume 后重发射 workload，按线程三态清单（已完成→杀、在途→恢复、未启动→从头跑）实现幂等单步。该专利同时完整披露了 Kepler 任务前端细节（TMU 300 的链表结构、round-robin/priority/partitioned priority 调度、WDU 340 的驱逐机制）。详见 [US8572573B2_非抢占式GPU交互调试_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US8572573B2_非抢占式GPU交互调试_解析.md)

### 3.8 US 2023/0236878 A1 — Efficiently Launching Tasks on a Processor（2022 申请，依赖解耦与 PDL 硬件机制）

新一代前端形态：TMU 与 WDU 合并为 Work Scheduler/Distribution Unit 230（WSDU），内含 Scheduling Dependency Logic 232 与片上 RAM 的 Task Dependency Table 240（依赖锁存器表，Wait On / Arrive At Latch 列）。核心是把**调度依赖与数据依赖解耦**：生产者程序中的 PREEXIT 指令 330（或默认判据）在其**执行完成前**即解除调度依赖，消费者任务随即开始启动；消费者按"数据无关部分 372 → ACQBULK 390 → 数据依赖部分 374"三段执行，在 ACQBULK 处等待数据依赖（flush 可见性）解除。配合最后一个生产者线程块启动时广播的抢先式 memory flush 320 与指令/常量预取 352/354，消费者的启动开销被折叠进生产者执行尾部。硬件保证生产者最后一个线程块先于消费者第一个线程块启动。机制与 CUDA 11.8 的 PDL（Programmatic Dependent Launch）一一对应（PREEXIT ↔ cudaTriggerProgrammaticLaunchOrdering，ACQBULK ↔ cudaGridDependencySynchronize），可视为 PDL 的硬件侧披露。20 项权利要求。详见 [US20230236878A1_高效任务启动_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20230236878A1_高效任务启动_解析.md)

## 四、技术演进脉络

| 时间 | 专利 | 主题 | TMU 视角的演进 |
|------|------|------|----------------|
| 2012-01 申请 | US20130198760A1 | 依赖任务自动启动 | 确立 TMU 结构 + TMD 依赖字段（硬件替代信号量） |
| 2012-03 申请 | US8572573B2 | 交互式调试 | 披露 TMU 调度策略与驱逐机制；软件重放弥补不可抢占 |
| 2012-05 申请 | US9928109B2 / US20210349763A1（延续链） | 嵌套流事件 / 嵌套并行 | TMU 接受 GPU 端直接提交任务，CPU 退出调度路径 |
| 2014-06 申请 | US9535815B2 | 执行统计收集 | 确立 macro/micro 两级调度命名与 TMU 芯片位置 |
| 2017-05 申请 | US10552202B2 | 指令级抢占 | TMU 成为抢占停发闸门与恢复调度入口 |
| 2019-06 申请 | US11182207B2 | 描述符预取 | TMU 内置预取引擎与硬件依赖计数器 |
| 2022-01 申请 | US20230236878A1 | 依赖解耦与高效启动（PDL） | TMU/WDU 合并为 WSDU；pre-exit 触发 + 片上依赖锁存器表，消费者在生产者完成前启动 |

主线一：**依赖表达硬件化**——从信号量（软件）→ TMD 内嵌依赖字段（2012）→ 预取 + 依赖计数 + 循环自复位（2021）→ 调度/数据依赖解耦与 pre-exit 提前启动（2022，PDL 硬件基础），这条线索与 CUDA Graphs 时代的任务图执行优化相呼应。

主线二：**CPU 退出关键路径**——依赖启动、嵌套并行、跨流依赖逐步由 TMU/scheduler kernel 在 GPU 内闭环完成，降低 host 往返延迟。

主线三：**可中断性演进**——从不可抢占（2013 用软件重放调试）到指令级/CTA 级多粒度抢占（2020），TMU 始终掌握任务级停发与恢复。

## 五、文件清单

| 专利 | PDF | 解析文档 |
|------|-----|----------|
| US20130198760A1 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20130198760A1.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20130198760A1_依赖任务自动启动_解析.md) |
| US11182207B2 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US11182207B2.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US11182207B2_依赖任务描述符预取_解析.md) |
| US9535815B2 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US9535815B2.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US9535815B2_GPU负载执行统计收集_解析.md) |
| US10552202B2 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US10552202B2.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US10552202B2_指令级执行抢占_解析.md) |
| US20210349763A1 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20210349763A1.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20210349763A1_嵌套并行计算_解析.md) |
| US9928109B2 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US9928109B2.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US9928109B2_嵌套流事件处理_解析.md) |
| US8572573B2 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US8572573B2.pdf) | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US8572573B2_非抢占式GPU交互调试_解析.md) |
| US20230236878A1 | [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20230236878A1.pdf)（扫描件） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/US20230236878A1_高效任务启动_解析.md) |

## 六、建议阅读路径

**路径 A：TMU 结构与职责（入门）** — 先读 US20130198760A1（TMU 基本结构）→ US9535815B2（两级调度命名与芯片位置）→ US8572573B2（调度策略细节）。

**路径 B：依赖与任务图（调度主线）** — US20130198760A1 → US11182207B2 → US20230236878A1（依赖解耦与 PDL）→ US9928109B2（流级依赖）。

**路径 C：执行控制（抢占/嵌套/调试）** — US10552202B2（抢占）→ US20210349763A1（嵌套并行）→ US8572573B2（调试）。

## 七、专利族补充（2026-08-19 内网 PatentsDB 检）

经内网 PatentsDB 后端 API 逐案核查（token AND 匹配 title/summary/inventor/assignee），为本目录 8 件专利补充以下同族/关联成员：

| 本目录专利 | 新补充的同族/关联成员 | 备注 |
|------|------|------|
| US20130198760A1 | CN-103226481-A（辉达公司） | 中国同族；内网库**未收录**该案的美国公开号 |
| US11182207B2 | US-2020401444-A1（预先公开）、CN-112130969-A | 美国预先公开 + 中国同族 |
| US10552202B2 | US-10552201-B2、US-9652282-B2（母案）、US-2017249152-A1、US-2017249151-A1、US-9710874-B2（关联） | 抢占延续案族 |
| US20210349763A1 | US-10915364-B2、US-2020151016-A1、US-2017083373-A1、US-9513975-B2（原始授权） | 嵌套并行延续案族 |
| US9928109B2 | DE-102013208554-B4 | 德国同族（nested execution streams） |
| US8572573B2 | DE-102013202495-A1 | 德国同族 |
| US20230236878A1 | US-2023236878-A1 | 内网库登记号 |
| 相关（不在本目录） | DE-102012222558-B4（动态生成任务的信令/排序/执行，2023-12-14 授权）、US-9286119-B2（任务间依赖管理）、DE-102012216568-B4（优先级调度）、DE-102013201178-B4（工作分发控制）、US-11954518-B2（用户定义计量优先级队列） | 同属任务前端调度脉络 |

检索备注：内网库以 "TMU" 字面检索仅命中 2 条无关记录（正文不出现缩写），须按发明人/主题逐案核查；详细检索方法与完整结果见 [14_Patents/NVIDIA_TMU专利专题整理.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/NVIDIA_TMU专利专题整理.md) 第 4 节。

## 八、关联资源

| 资源 | 位置 | 关联点 |
|------|------|--------|
| NVIDIA TMU 专利专题整理 | [14_Patents/NVIDIA_TMU专利专题整理.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/NVIDIA_TMU专利专题整理.md) | 本目录的检索来源与索引 |
| RTAS'23 GPU Compute Partitioning（libsmctrl） | `~/Documents/tech_docs/CudaStramCP/WD/rtas23.pdf` | 实验逆向 TMU 并发限制，与专利相互印证 |
| 02_NVIDIA_Architecture 研究综述 | [研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/02_NVIDIA_Architecture/研究综述.md) | Volta/Ampere 微基准中的前端观测 |
| 14_Patents 专利库 | [研究综述.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/研究综述.md) | US9575892（内存重放）等 TMU 协同专利 |

---

*本综述基于 8 份专利 PDF 全文提取与逐篇深度解析生成（7 份经 pypdf 文本提取；US20230236878A1 为扫描件，经 PyMuPDF 逐页渲染后视觉阅读提取）。专利族信息经内网 PatentsDB 补充核查（2026-08-19）。附图标记与引用关系以专利原文为准；PDF 提取乱码处已在各解析文档中标注。最后更新：2026-08-19*

