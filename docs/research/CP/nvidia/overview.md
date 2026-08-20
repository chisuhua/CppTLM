# GPU 前端（命令处理）与任务图（Task Graphs）研究综述

> 领域：GPU架构 | 子领域：TMU 上游模块（pushbuffer/主机接口/CP/IB 对应物、计算图处理） | 文献数量：7 篇本地专利（含 5 篇逐篇解析）+ 20 余件网络/内网候选 | 更新日期：2026-08-19

---

## 一、领域概述

TMU（Task Management Unit）的上游是"命令如何从 CPU 进入 GPU 并变成任务"的整条前端链路。综合本地与族内专利的样板架构段落，NVIDIA PPU 的标准前端链路为：

```
CPU 102 ──写入──> pushbuffer（系统内存/PP 内存中的命令流缓冲，存指向命令数据结构的指针）
        ──PCIe──> I/O Unit 205
        ────────> Host Interface Unit 206/210/232
        ────────> Front End 212 / Front End Unit 215/315
        ────────> TMU 234 / Scheduler Unit 220/320（任务就绪判定、优先级组织）
        ────────> WDU 236/225/340 ──> GPC ──> SM
```

pushbuffer 是命令流的容器：软件应用经驱动把命令数据结构（含指向 TMD 的指针）的指针写入 pushbuffer，GPU 从 pushbuffer 读出命令流并异步于 CPU 执行；每个 pushbuffer 可由软件设定执行优先级以控制调度（US9489763B2、US8572573B2 等 Kepler 世代样板段落）。

本专题覆盖三类主题：

1. **pushbuffer/主机接口/前端**——命令流的提交、读取与前端处理（含 AMD 术语 CP/IB 的 NVIDIA 对应物考证）；
2. **队列与提交机制**——SM 级队列管理器（queue manager）、用户态直接工作提交（doorbell/put pointer 类机制）；
3. **计算图（CUDA Graphs）处理**——执行图的构建/实例化/修改/同步专利族，及其与硬件任务图（US11182207B2）的分工。

**术语对照**（详见第七节负结果考证）：AMD 的 Command Processor（CP）/Indirect Buffer（IB）在 NVIDIA 专利文本中的功能对应物是 **pushbuffer + Host Interface Unit + Front End**；"HyperQueue" 一词未见于任何 NVIDIA 专利；"work submission token（WST）" 亦无专利命中，其思想在 US20230153146A1 中以 put pointer + notifier 形式出现。

## 二、本地全文专利与逐篇解析（5 篇）

| 专利 | 主题 | 世代/年份 | 解析文档 |
|------|------|-----------|----------|
| US9489763B2 | Techniques for Setting Up and Executing Draw Calls（pushbuffer/draw call） | Kepler，2012 申请 / 2016 授权 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US9489763B2_draw_call建立与执行_解析.md) |
| US10489056B2 | Queue Manager for Streaming Multiprocessor Systems（SM 级队列管理器） | Volta 时代，2017 申请 / 2019 授权 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US10489056B2_SM系统队列管理器_解析.md) |
| US20230153146A1 | Direct User Mode Work Submission in Secure Computing Enabled Processors（用户态直接提交，扫描件视觉阅读） | Ampere+ 安全计算，2021 申请 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20230153146A1_用户态直接工作提交_解析.md) |
| US20210149719A1 | Techniques for Modifying Executable Graphs to Perform Different Workloads（可执行图就地修改） | CUDA Graphs，2019-12-31 申请 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20210149719A1_可执行图修改_解析.md) |
| US20210248115A1 | Compute Graph Optimization（graph code 参数化再执行免重实例化） | CUDA Graphs，2020-02-10 申请 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20210248115A1_计算图优化_解析.md) |

各篇要点：

- **US9489763B2**：本专题中唯一逐字描述 pushbuffer 链路的文本专利。完整披露 pushbuffer→host interface 206→front end 212→task/work unit 207（TMU 300 + WDU 340）→GPC 208 通路；发明主题为 draw call 的建立（驱动把 buffer 指针交还应用、draw command 自带指针免绑定）与执行（transform feedback→control buffer 504→计数器→GPU 自生成 draw command 的闭环），是 NV_bindless_multi_draw_indirect 扩展的专利侧奠基文献（第一发明人 Kubisch 即该扩展规范作者）。
- **US10489056B2**：queue manager（QM 718）不在前端而在 **GPC 内 TPC 层级**，与 L1/SM/MPC 通信；命令队列+数据队列成对环形结构，base/head/tail + xhead/xtail 四指针界定 queue body 与 push/pop pending zone；标量命令 2B 编码（THREAD/QUADTHREAD/WARP/CTA 类型），queue chomper 按目标原生宽度组装出站命令，keep bit 支持非破坏性弹出。单一发明人 John Erik Lindholm；继续案 US10983699B2（2019）。
- **US20230153146A1**（扫描件）：安全计算环境下的用户态直接工作提交。Secure Task Launch System 400：Notifier 410、Scheduler 432、Copy Engines 434；Compute Protected Region 440 内含 put pointers 422、pushbuffers 424、pushbuffer segments 426、runlist 442；FIG. 5 的 X 510→X' 512 两级 pushbuffer 结构；用户态经 notifier/put pointer 直接投递工作，绕开传统内核态路径——doorbell/WST 类机制在 NVIDIA 专利中的最早显式披露之一（原文未用 WST 一词）。
- **US20210149719A1**：CUDA Graphs 软件层生命周期：task graph 构建 3210→实例化 3220（含资源分配与全局优化）→启动 3230→就地修改节点参数 3240→成功判定/回退重建 3250。核心是"可修改参数白名单"（回调指针、memset/memcpy 参数、kernel 线程数/参数指针）与"不可改黑名单"（任务身份、起始地址、依赖边）。**未披露 front end/TMU 如何消费执行图**——纯 host/驱动层技术；与 US11182207B2 的硬件任务图互补覆盖命令流路径两端。姊妹申请 US20210149734A1 授权为 US12443449B2（2025-10-14）。
- **US20210248115A1**：graph code 实例化一次后以不同参数二次执行而免 reinstantiation：阈值 T0 判定、部分再优化（FIG. 3 region 310）、禁止处理单元重分配（FIG. 5）、禁止结构变更（FIG. 6）、仅 kernel 节点可改。第一发明人 Stephen Jones 与 US11182207B2 重合。样板段落中出现 1 次 doorbell 式前端投递描述（FIG. 20A）与 pushbuffer→front-end unit 2510→scheduler unit 2512→work distribution unit 2514 通路（FIG. 25）。

## 三、pushbuffer 与命令流：短语检索发现

Google Patents `pushbuffer` + assignee=NVIDIA 短语检索（2026-08-19）宽匹配约 804 条、摘要精确含短语 94 条。绝大多数仅在样板架构段落提及 pushbuffer（与第三节 US9489763B2 相同的模板文字），以 pushbuffer 机制为主题的代表性候选：

| 专利 | 标题 | 优先权年 | 关联点 |
|------|------|----------|--------|
| US9489763B2 | Setting Up and Executing Draw Calls | 2012 | 核心，已解析（第二节） |
| US9342857B2 | Techniques for Locally Modifying Draw Calls | 2013 | US9489763B2 姊妹主题：draw call 局部修改 |
| US8938598B2 | Facilitating Simultaneous Submission to a Multi-producer Queue | 2011 | 多生产者并发提交队列——pushbuffer 提交侧并发机制 |
| US9442755B2 | Hardware Scheduling of Indexed Barriers | 2013 | 命令流中索引屏障的硬件调度 |
| US8760460B1 | Hardware-managed Virtual Buffers Using Shared Memory for Load Distribution | 2009 | 硬件管理的虚拟缓冲（Fermi 世代） |
| US8766988B2 | Providing Pipeline State Through Constant Buffers | 2009 | 经常量缓冲传递管线状态（减少 pushbuffer 状态命令） |
| US8970608B2 | State Objects for Specifying Dynamic State | 2010 | 状态对象——命令流的状态组织 |
| US9324175B2 | Memory Coherency in Graphics Command Streams and Shaders | 2010 | 命令流内的内存一致性（内网 PatentsDB 亦命中） |

以上候选除 US9489763B2 外均未下载全文，如需可另行补入（检索链接见第八节）。

## 四、Queue Manager 族：SM 级队列的专利谱系

| 专利 | 申请日 | 状态 | 备注 |
|------|--------|------|------|
| US10489056B2 | 2017-11-09 | 授权 2019-11-26 | 核心，已解析；预先公开 US-2019138210-A1 |
| US10983699B2 | 2019-10-25 | 授权 2021-04-20 | 继续案（内网 PatentsDB 核实） |

两件均以 "Queue manager for streaming multiprocessor systems" 为题、发明人均为 John Erik Lindholm。queue manager 位于 TPC 内（而非前端），管理向 SM 提交的命令/数据环形队列——它补全了"WDU 把任务铺到 GPC"之后"任务如何进入单个 SM"的最后一级，与 libsmctrl（RTAS'23/ECRTS'25）实验逆向出的 per-SM 队列与并发上限行为可相互印证。

## 五、用户态直接工作提交：US20230153146A1 族

| 成员 | 公开/授权 | 备注 |
|------|-----------|------|
| US20230153146A1 | 2023-05-18 公开 | 核心，已解析（扫描件视觉阅读） |
| CN116127476A / CN116127476B | 2023-05-16 公开 / 2026-03-10 授权 | 中国同族 |
| DE102022129589A1 | 2023-05-17 公开 | 德国同族 |

该族把传统"内核态驱动写 pushbuffer + MMIO 通知"的提交路径改为用户态直接写 put pointer + notifier，服务于安全计算（Confidential Computing）下内核不可信的威胁模型。这是理解现代 NVIDIA GPU 用户态命令提交（与公开资料中的 work submission token/doorbell 机制对应）的关键专利；原文术语为 put pointers 422 / notifier 410，未使用 WST/doorbell 字样。

## 六、CUDA Graphs（计算图处理）专利族

以 2019-12-31 首次申请为起点，NVIDIA 围绕 CUDA Graphs 的构建、实例化、修改、同步、信号量集成与队列结构形成了密集的专利族。按主题归组（日期为优先权/申请日，来源：Google Patents 与内网 PatentsDB 交叉核实）：

**A. 执行图生命周期（修改/优化）**

| 专利 | 标题 | 日期 | 备注 |
|------|------|------|------|
| US20210149719A1 | Modifying Executable Graphs to Perform Different Workloads | 2019-12-31 | 已解析；EP3822785A1、CN112817739A 同族 |
| US20210149734A1 → **US12443449B2** | Modifying an Executable Graph … New Task Graph | 2019-12-31 | 姊妹申请，2025-10-14 授权；CN112817738B、US20260079755A1 同族 |
| US20210248115A1 | Compute Graph Optimization | 2020-02-10 | 已解析；CN113256475B（2025-10-03 授权） |
| US20230176933A1 | Techniques for Modifying Graph Code | 2021-12-13 | graph code 修改 API |

**B. 图资源建立、同步与节点控制**

| 专利 | 标题 | 日期 | 备注 |
|------|------|------|------|
| US20230093254A1 | API to Set Up Graph Resources | 2021-09-23 | 本地有 PDF（扫描件，未解析）；DE102022123627A1、CN115858021A 同族 |
| US20230084951A1 | Synchronizing Graph Execution | 2021-09-15 | 图执行同步 |
| US20230083345A1 | Multi-architecture Execution Graphs | 2021-09-15 | 跨架构执行图 |
| US12461800B2 | API to Control Execution of Graph Nodes | 授权 2025-11-04 | 节点级执行控制 |
| US20260169840A1 | API to Control Execution of Graph Nodes | 2025-10-17 公开 | 同主题新申请（内网 PatentsDB 命中） |
| US12536058B2 | API to Locate Incomplete Graph Code | 授权 2026-01-27 | 未完成图定位 |

**C. 信号量集成与图启动**

| 专利 | 标题 | 日期 | 备注 |
|------|------|------|------|
| WO2023114738A1 | API to Cause Graph Code to Update a Semaphore | 2021-12-13 | 图内信号量更新 |
| WO2023114747A1 | API to Cause Graph Code to Wait on a Semaphore | 2021-12-13 | 图内信号量等待 |
| US20240338257A1 | API to Cause Graph Code to Update a Semaphore | 2021-12-13 | 美国对应公开（本地有 PDF，扫描件，未解析） |
| US12619477B1 | （信号量族授权成员） | — | 已授权（B1） |
| US20240152413A1 | Launching Graphs Using GPUs | 2022-11 | 图启动路径 |
| CN119597442A | （kernel 启动依赖） | — | 中国申请，kernel boot dependencies |

**D. 队列结构（最接近硬件的一支）**

| 专利 | 标题 | 日期 | 备注 |
|------|------|------|------|
| US20260099483A1 / US20260099484A1 | **Work Graph Queue Structures** | 2025-04-24 申请 | 内网 PatentsDB 命中；明确以"work graph + 队列结构"为题，是图族中硬件化程度最高的信号 |

**族系观察**：

1. 2023 年之后的美国公开文本在 Google Patents 上均为扫描图 PDF（无文本层），本库已下载的 US20230093254A1、US20240338257A1 均如此，故未做逐篇解析；
2. 族内实质内容集中于驱动/API 层（图的生命周期管理），与硬件任务图机制（US11182207B2 的 TMU 依赖解析/预取）分工明确：软件图层负责"少重做"，硬件任务图负责"快执行"；
3. 发明人高度重合（Stephen Jones、David Anthony Fontaine 等与 TMU 专利团队交叉），且 2025 年出现的 Work Graph Queue Structures 表明图执行正在向硬件队列结构下沉——与 Hopper/Blackwell 世代 CUDA Graphs 硬件加速的公开方向一致。

## 七、HyperQueue、CP/IB、WST：术语考证（负结果记录）

| 术语 | Google Patents（assignee=NVIDIA） | 内网 PatentsDB | 结论 |
|------|----------------------------------|----------------|------|
| hyperqueue | 0（无 assignee 限定时仅 1 件无关 Quantel 1988 专利） | 0 | 专利文献不用该词；它出自 NVIDIA 早期架构资料，非专利术语 |
| indirect buffer / indirect pushbuffer | 0 / 0 | 3（均为 index buffer 类图形专利，非命令 IB） | IB 为 AMD 术语；NVIDIA 对应物为 pushbuffer |
| command processor（标题级） | 53（多为噪声，无 GPU 前端 CP 专利） | 36（噪声为主，另见 queue manager） | CP 为 AMD/Tegra 术语；NVIDIA 桌面 GPU 前端在专利中称 host interface + front end |
| work submission token / submission token | 0 / 0 | 0 | 无专利命中；思想见于 US20230153146A1（put pointer + notifier） |
| doorbell | 233（宽匹配，NVIDIA 名下无 GPU 前端专利） | 2（均为 RDMA NIC 专利） | GPU 前端 doorbell 未成为专利主题；US20230153146A1 的 notifier 为其对应物 |
| runlist | — | 0（"run list" 29 条多为噪声，另见 DE-102012222391-B4 Multichannel Time Slice Groups） | runlist（通道调度表）仅见于德文同族样板 |

结论：NVIDIA 在专利申请中刻意使用自洽术语（pushbuffer、host interface unit、front end、task management unit、queue manager、put pointer/notifier），业界熟知的 HyperQueue/CP/IB/WST/doorbell 等称谓几乎不进入权利要求与说明书。检索时须用 NVIDIA 术语或功能描述（"command stream"、"queue manager"、"user mode work submission"）。

## 八、Google Patents 检索记录（2026-08-19）

检索经 `patents.google.com/xhr/query` XHR 端点执行（限流规避：查询间隔 ≥8–12s）：

- `q="pushbuffer"&assignee=NVIDIA`：宽匹配 804 / 摘要精确 94，主题命中见第三节；
- `q="hyperqueue"`（无 assignee）：1（无关）；`q="hyper queue"&assignee=NVIDIA`：0；
- `q="indirect pushbuffer"&assignee=NVIDIA`：0；`q="submission token"&assignee=NVIDIA`：0；
- `q="command processor"&assignee=NVIDIA`：53（噪声为主；命中 US20210248115A1、US20230093254A1、DE102011081585B4 等）；
- `q="task graph"&assignee=NVIDIA`：20 → CUDA Graphs 族全量（第六节）；
- `q="host interface unit" / "command stream" / "doorbell" / "front end unit"&assignee=NVIDIA`：592 / 4103 / 233 / 3972，绝大多数为样板段落宽匹配，人工筛后无新增核心文献（command stream 筛出 US9324175B2、US9489763B2）。

## 九、内网 PatentsDB 检索记录（2026-08-19，一次性例外授权）

API `/api/patents?q=<tokens>&offset=&limit=`（token AND 匹配 title/summary/inventor/assignee）：

- `pushbuffer` 0；`hyperqueue` 0；`runlist` 0；
- `command processor` 36：噪声为主，有效命中 US-10983699-B2 / US-2020057560-A1 / US-10489056-B2 / US-2019138210-A1（queue manager 族）、US-9489245-B2 / US-9135081-B2（work-queue-based GPU work creation）；
- `host interface` 52：噪声为主，有效命中 CN-110858387-B（multiprocessor-coprocessor interface）、US-12511106-B1（API to indicate host dependencies）；
- `task graph` 74：CUDA Graphs 族命中 US-12443449-B2、CN-112817738-B、US-20260079755-A1；
- `command stream` 32：US-9324175-B2、US-9665920-B1（simultaneous compute and graphics）；
- `queue manager` 6：US-10983699-B2、US-10489056-B2 及其预先公开 + DE-102012222558-B4（动态生成任务的信号/顺序/执行，CDP 相关）；
- `work queue` 29：US-20260099484-A1 / US-20260099483-A1（**WORK GRAPH QUEUE STRUCTURES**）、US-9489245-B2、US-11367160-B2（simultaneous compute and graphics scheduling）；
- `work submission` 2：US-2023153146-A1（**用户态直接提交**）、US-2025077444-A1（post-send coalescing，NIC 向）；
- `doorbell` 2：均为 RDMA NIC 专利；`graph code` 187 / `execution graph` 356：宽匹配噪声为主，另命中 US-20260169840-A1、US-12555176-B1（storing kernel attributes）。

## 十、文件清单

| 文件 | 类型 | 备注 |
|------|------|------|
| [US9489763B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US9489763B2.pdf) | PDF（15 页） | draw call 建立与执行 |
| [US10489056B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US10489056B2.pdf) | PDF（35 页） | queue manager |
| [US20230153146A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20230153146A1.pdf) | PDF（21 页，扫描件） | 用户态直接提交 |
| [US20210149719A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20210149719A1.pdf) | PDF（78 页） | 可执行图修改 |
| [US20210248115A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20210248115A1.pdf) | PDF（84 页） | 计算图优化 |
| [US20230093254A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20230093254A1.pdf) | PDF（88 页，扫描件） | 图资源建立 API，未解析 |
| [US20240338257A1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/US20240338257A1.pdf) | PDF（105 页，扫描件） | 图代码更新信号量 API，未解析 |

另有 5 篇逐篇解析文档（第二节表）与本综述，共 13 个文件。

## 十一、建议阅读路径

1. **US9489763B2**——先建立 pushbuffer→host interface→front end→TMU 的标准链路印象；
2. **US20230153146A1**——看该链路在用户态直接提交下的变形（put pointer/notifier）；
3. **US10489056B2**——链路的另一端：任务进入 SM 前的最后一级队；
4. **US20210248115A1 → US20210149719A1**——CUDA Graphs 软件层生命周期，与 [12_Task_Management_Unit](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) 的 US11182207B2（硬件任务图）对读，理解"软件图层 + 硬件任务图"双层体系；
5. 需要提交侧并发/屏障机制时回看第三节外围候选（US8938598B2、US9442755B2）。

## 十二、关联资源

| 资源 | 位置 | 关联点 |
|------|------|--------|
| TMU 研究综述 | [12_Task_Management_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) | 下游 TMU，8 篇核心专利；US11182207B2 与本专题图族互补 |
| WDU 研究综述 | [13_Work_Distribution_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/研究综述.md) | 下游 WDU 与两级 crossbar |
| NVIDIA TMU 专利专题整理 | [14_Patents/NVIDIA_TMU专利专题整理.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/NVIDIA_TMU专利专题整理.md) | 检索方法与族谱记录 |
| AMD 前端与分发研究综述（AMD 对照篇） | [15_AMD_Front_End_CP_Dispatch/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/研究综述.md) | AMD CP/IB/PM4、doorbell/HQD、SPI/SQ 专利对照；含 NVIDIA↔AMD 前端术语对照表 |
| libsmctrl 实验论文 | `~/Documents/tech_docs/CudaStramCP/WD/rtas23.pdf`、`ecrts25.pdf` | per-SM 队列/并发上限实验，与 US10489056B2 互证 |

---

*本综述基于本地专利全文提取（pypdf/PyMuPDF）、Google Patents XHR 短语检索与内网 PatentsDB 核查（2026-08-19，一次性例外授权）整理。US20230153146A1 为扫描件，解析结论来自视觉阅读；US20230093254A1、US20240338257A1 为扫描件且未做逐篇解析。附图标记以专利原文为准。最后更新：2026-08-19*

