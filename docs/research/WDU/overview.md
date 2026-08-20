# Work Distribution Unit（WDU）与 Work Distribution Crossbar 研究综述

> 领域：GPU架构 | 子领域：任务分发与集群互连 | 文献数量：16 篇本地专利（含 4 篇逐篇解析）+ 14 件网络/内网候选 | 更新日期：2026-08-19

---

## 一、领域概述

Work Distribution Unit（WDU，工作分发单元）是 NVIDIA PPU 任务前端的后半段：TMU（宏观调度器）负责任务的就绪判定与优先级组织，WDU 则把就绪任务实际"铺"到硬件上——踪已接收的已调度任务数量，按各 GPC 内可用 SM 的资源把任务（grid/CTA）分发到各 General Processing Cluster（GPC）执行。Work Distribution Crossbar 是与之配套的两级互连结构的统称，专利文本中实际存在**两个层级**，须区分：

- **GPC 内 Work Distribution Crossbar（330，亦记作 WDX）**：位于 GPC 内部，把任务分发到 GPC 内各 SM，并作为 memory flush 时 L1 数据写往 L2 的通道（US11182207B2、US10552202B2）；
- **芯片级 XBar（270 / 370）**：连接全部 GPC、Memory Partition Unit、Hub/NVLink 的通用互连，GPC 经它读写显存与跨芯片通信（US10067768B2、US10915445B2）。

WDU 是理解"任务如何从描述符变成 SM 上运行的线程块"的关键环节，也是抢占、限流、负载均衡等机制的物理执行点。

## 二、WDU 在三代架构中的形态

| 形态 | 出处 | 组成与职责 |
|------|------|-----------|
| WDU 340（Kepler） | US20130198760A1、US8572573B2、US9928109B2 | Task/Work Unit 207 = TMU 300 + WDU 340；WDU 维护 Task Table 345（CTA 槽位表），支持高优先级任务驱逐低优先级 CTA |
| Scheduler Unit + WDU（Volta/Ampere） | US10067768B2（WDU 225）、US10915445B2（WDU 325）、US11182207B2（WDU 236） | TMU 改称 Scheduler Unit（220/320），WDU 独立成块；WDU 跟踪从 scheduler unit 接收的已调度任务数，把任务分发到各 GPC；经芯片级 XBar（270/370）连 GPC 与 Memory Partition Unit |
| WSDU 230（Hopper） | US20230236878A1 | TMU 与 WDU 合并为 Work Scheduler/Distribution Unit 230，下游经 Intermediate Units/Crossbars 250（MPC）直达 SM |

## 三、本地专利：WDU / Crossbar 提及统计与要点

统计基于 pypdf 全文提取（US20230236878A1 为扫描件，数字为 0 不代表未提及，其内容经视觉阅读确认）。

| 专利 | WDU 全称次数 | WD Crossbar 次数 | crossbar 总次数 | WDU/Crossbar 视角的要点 |
|------|:---:|:---:|:---:|------|
| US10552202B2 | 34 | 2 | 13 | WDU 提及最多：CTA 级抢占时"排空 GPC"、WDU 停发闸门；附图含 Work Distribution Crossbar 330 的 GPC 框图 |
| US10067768B2 | 9 | 1 | 3 | Volta PPU 框图：WDU 225 跟踪 scheduler unit 220 的已调度任务并分发到 GPC 250，芯片级 XBar 270；主题为 convergence barrier |
| US11182207B2 | 9 | 4 | 15 | TMU 234 向 WDU 236 传 TMD 指针；flush 时各 SM 经 Work Distribution Crossbar 330 把 L1 数据写入 L2 |
| US8572573B2 | 9 | 3 | 14 | WDU 340 的 Task Table 345 槽位与高优先级驱逐；调试 relaunch 经 WDU 重入队 |
| US20130198760A1 | 7 | 3 | 13 | WDU 340 首次系统披露：接收 TMU 300 任务、Task Table、槽位分配 |
| US9535815B2 | 7 | 0 | 2 | WDU 220 芯片位置（TMU 215 下游）；trace 数据经前端汇总 |
| US9928109B2 | 6 | 1 | 14 | Task/Work Unit 207 = TMU 300 + WDU 340 的明确定义出处 |
| US20210349763A1 | 4 | 3 | 11 | CDP 子 grid 经 MMU/PCAS 入队后经分发路径进 GPC |
| US10915445B2 | 8 | 0 | 39 | Volta 框图 WDU 325 + 芯片级 XBar 370（GPC↔Memory Partition↔NVLink Hub）；GPC 内含 WDX 480；主题为高带宽扩展的一致性缓存 |
| US20230236878A1（扫描） | — | — | — | WSDU 230 合并形态；下游 Intermediate Units/Crossbars 250（MPC）→ SM 260 |

## 四、本次新发现的 WDU 主题专利

以下 4 件以 WDU/任务分发为直接主题，此前未被本库收录（摘要来自内网 PatentsDB 与 Google Patents）。**2026-08-19 补记：4 全文 PDF 均已下载入库，逐篇解析文档已完成**（见各小节链接与第九节文件清单）：

### 4.1 US 9,594,599 B1 — Distributing Work Batches to Processing Units Based on a Number of Enabled SMs（WDU 专题最早一篇）

2009-10-14 申请（Fermi 世代），2017-03-14 授权；发明人 Philip Browning Johnson、Dale L. Kirkland、Karim M. Abdalla。摘要明示：**WDU 按每个 GPC 所含（使能的）SM 数量向各 GPC 分发 work batch**，使各 GPC 获得与其算力成比例的工作量——WDU 的"按容量比例分发"职责的直接出处，也是目前所见最早的 WDU 主题专利。全文核心：每 GPC 一个 counter（按使能 SM 数累加、按 max SM count 扣减）+ load signal 准入门槛，给出改进轮询（FIG. 7，三掩码 AND 并行）、失速均衡（FIG. 8）、封顶不失速（FIG. 9）三种可动态切换的策略；全文无 task table 术语，可见 WDU 从"批次+计数器"到"任务表+槽位"的演进起点。解析：[US9594599B1_按SM数比例分发工作批次_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US9594599B1_按SM数比例分发工作批次_解析.md)

### 4.2 US 9,710,306 B2 — Auto-throttling Encapsulated Compute Tasks（2012，Kepler TMU 发明人团队）

2012-04-09 申请，2017-07-18 授权；发明人 Jerome F. Duluk Jr.、Jesse David Hall、Philip Alexander Cuadra、Karim M. Abdalla（与 US20130198760A1 同一团队）。驱动为并行处理器配置多个离散节流模式，非节流模式向全部处理单元分配内存、节流模式只向子集分配——任务分发面的限流/分区机制，与 WDU 的分发目标集合直接相关。全文核心：驱动按每 warp shader local memory 需求与阈值比较，在 TMD 的 execution parameters 中置 TMD.Throttled 标志；**WDU 340 对 task table 345 中所有 TMD 的标志位做 OR 聚合**，触发整颗 PPU 的活跃 SM 数切换（如 16→4，每 SM 的 SLM 从 16 MB 增至 64 MB），切换前先令全部 SM idle 保存状态；多级节流用多比特标志取最大值；驱动可为各档预分配、切换零重分配。另披露 task/work unit 207 按 TMD 内存参数限制并发 CTA 数的 CTA 级节流（throttle enable flag）。解析：[US9710306B2_封装计算任务自动节流_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US9710306B2_封装计算任务自动节流_解析.md)

### 4.3 US 10,332,310 B2 — Distributed Index Fetch, Primitive Assembly, and Primitive Batching（2015，图形工作切片）

2015-12-22 申请，2019-06-25 授权；发明人 Niket Agrawal、Amit Jain、Dale Kirkland、Karim Abdalla、Ziyad Hakura 等；预先公开 US-2017178401-A1。primitive distribution system 把 draw command 拆为多个 work slice 并行做 index fetch/图元装配/批处理——图形管线侧的工作分发，与计算侧 WDU 平行。全文核心：central PD 410 **不访问 index** 即按 index 数均分为带重叠区的 work slice；各 GPC 的 local PD 440 并行 fetch+scan，识别 restart index 位置、winding order、fan anchor、primitive/instance/vertex id 等跨片依赖特征，经 feedback packet 上报 GPC synchronization processor 420 再 publish 给相关 GPC；triple-buffered fetch 使 scan 开销与 batching/rendering 重叠。落位关键：central PD 410 与 synchronization processor 420 位于 task/work unit 207 内（与通用 WDU 同室），distribution crossbar 430 内嵌于 crossbar unit 210——前端图元分发是 task/work unit 内的第二条分发链。解析：[US10332310B2_分布式图元批处理_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US10332310B2_分布式图元批处理_解析.md)

### 4.4 US 8,941,653 B2 — Order-preserving Distributed Rasterizer（2013，有序分发）

2013-11-18 申请，2015-01-27 授权；发明人 Steven E. Molnar、Emmett M. Kilgariff、Timothy John Purcell、Sean J. Treichler、Ziyad S. Hakura 等。把光栅化工作分布到多个 GPC 同时**保序**——"分发 + 顺序约束"组合，与 US20230236878A1 的启动次序保证在思想上呼应（发明人 Purcell/Treichler 亦见于 TMU 谱系）。全文核心：本案为 2009-10-15 提交的母案 12/580,017（现 US 8,587,581）的 continuation；object-space 侧 primitive distribution unit 200 把 index 分为 batch（32 顶点/30 图元）轮转分给各 TPC，screen-space 侧按 16×16 screen tile 集合静态归属 GPC；**work distribution crossbar interface 330 内部结构首次完整披露**——每 TPC 一个 WWDX 340（bounding box×tile set 求交生成 GPC mask）+ 可选 aggregation unit 345（以 end-of-batch flag 为界重排）+ fabric 334（credit/debit 流控，可单周期广播）+ 接收侧 GPC reorder buffer 344（多线程 FIFO）与 SWDX 346（按 pipeline manager 305 提供的 GPC 顺序拉回 API 序）；总吞吐达 C triangles/clock，且"图元过 primitive distribution unit 后无任何单点汇聚"。解析：[US8941653B2_保序分布式光栅化_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US8941653B2_保序分布式光栅化_解析.md)

相关外围：US 9,501,847 B1（Parallel Line Stipple Computation，Purcell/Hakura，2009 申请，平行线 stipple 的分布计算）。

## 五、芯片级 XBar 与一致性：US 10,915,445 B2 族

US10915445B2（Coherent Caching of Data for High Bandwidth Scaling，Gandhi/Mandal/Manyam/Rao，2018-09-18 申请，2021-02-09 授权）虽以一致性缓存为主题，但其附图是 **Volta 世代最完整的芯片级互连披露之一**：Front End Unit 315 → Scheduler Unit 320 → Work Distribution Unit 325 → GPC 350(X)，XBar 370 连接全部 GPC、Memory Partition Unit 380(U) 与 NVLink Hub 330；GPC 内部含 Pipeline Manager 410、MPC 430、SM 440、WDX 480、MMU 490（FIG. 3/4A）。该专利解决高带宽内存扩展下跨 partition 缓存一致性，XBar 370 是一致性流量的承载体。

专利族（内网 PatentsDB 核实）：预先公开 US-2020089611-A1；中国同族 CN-110908929-B（2023-06-09 授权）。同主题前驱：US 9,727,463 B2 / US-2016342513-A1（Asymmetric Coherent Caching for Heterogeneous Computing，John Danskin，2015 申请）。

## 六、公网检索中的"模板化提及"（非核心文献）

Google Patents 短语检索（`"work distribution unit"` / `"work distribution crossbar"` + assignee=NVIDIA，2026-08-19）分别命中约 4300/3100 条，其中绝大多数仅在"标准 PPU 架构描述"段落模板化提及 WDU/crossbar，技术主题与任务分发无关，不列为核心文献，代表性样本：US 9,483,270 B2 / US 10,032,243 B2 / TW-201432609-A（Distributed Tiled Caching 族，Hakura 等，2013 申请）、US 9,779,533 B2（Hierarchical Tiled Caching）、US 10,535,114 B2（Multi-pass Rendering in Cache Tiling，Bolz）、US 10,438,400 B2（foveated rendering）、TWI592902B（sample mask）、US 10,733,794 B2（adaptive shading）、US 11,995,023 B2（硬件间数据传输，2024）、US 12,020,353 B2 系（数据多播，2024）、US 10,909,738 B2（ML 辅助 GPU 调优）、US 11,409,597 B2（流水线错误检测）、US 12,443,449 B2（可执行图修改，2025）。其中 tiled caching 族与"按 cache tile 切分工作并经 WDU 分发"的策略相关，可作图形侧延伸阅读。

## 七、内网 PatentsDB 检索记录（2026-08-19，一次性例外授权）

- `"work distribution unit"`：8 条，含 3 件新目标（US-9594599-B1、US-9710306-B2、US-10332310-B2）及数据中心散热类噪声（cooling/liquid distribution unit，同名歧义）；
- `"work distribution crossbar"`：0 条（摘要不含该短语）；
- `convergence barrier`：US-10067768-B2 + 预先公开 US-2016019066-A1；
- `coherent caching`：US-10915445-B2、CN-110908929-B、US-2020089611-A1、US-9727463-B2、US-2016342513-A1；
- `tiled caching`：US-10535114-B2、US-10032243-B2、US-9779533-B2、US-2017053375-A1、US-9483270-B2、TW-201432609-A；
- `distributed rasterizer`：US-8941653-B2、US-9501847-B1；
- `WDU+preemption`（Google Patents 组合查询）另命中 DE-102013016871-B4（Kepler 时代 DE 同族，标题为机翻"多线处理设备效率提升"，待核对原文）。

## 八、实验侧交叉验证

RTAS'23 / ECRTS'25（libsmctrl，Bakita & Anderson，UNC）通过实验逆向 NVIDIA GPU 任务前端的并发限制，正文 18 处使用 WDU 缩写、6 处全称，其观测到的"每优先级并发任务上限"等行为可与 US20130198760A1 的 Scheduler Table/Task Table 结构相互印证。文献位置：`~/Documents/tech_docs/CudaStramCP/WD/rtas23.pdf`、`ecrts25.pdf`。

## 九、文件清单

| 文件 | 类型 | 备注 |
|------|-----|------|
| [US10067768B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US10067768B2.pdf) | PDF | convergence barrier；Volta WDU 225/XBar 270 框图 |
| [US10915445B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US10915445B2.pdf) | PDF | 高带宽扩展一致性缓存；Volta WDU 325/XBar 370/WDX 480 框图 |
| [US9594599B1.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US9594599B1.pdf) | PDF | 2026-08-19 补全文（Google Patents 下载）；25 页 |
| [US9594599B1_按SM数比例分发工作批次_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US9594599B1_按SM数比例分发工作批次_解析.md) | 解析 | counter/load signal 三策略；WDU 专利谱系起点 |
| [US9710306B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US9710306B2.pdf) | PDF | 2026-08-19 补全文；24 页 |
| [US9710306B2_封装计算任务自动节流_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US9710306B2_封装计算任务自动节流_解析.md) | 解析 | TMD.Throttled + task table OR 聚合；活跃 SM 数离散切换 |
| [US10332310B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US10332310B2.pdf) | PDF | 2026-08-19 补全文；21 页 |
| [US10332310B2_分布式图元批处理_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US10332310B2_分布式图元批处理_解析.md) | 解析 | work slice + feedback packet；task/work unit 内第二条分发链 |
| [US8941653B2.pdf](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US8941653B2.pdf) | PDF | 2026-08-19 补全文；20 页 |
| [US8941653B2_保序分布式光栅化_解析.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/US8941653B2_保序分布式光栅化_解析.md) | 解析 | WWDX/SWDX/reorder buffer；WDX 330 内部结构的权威定义 |

WDU 核心行为的其他一手文献（Kepler task table、抢占等）在 [12_Task_Management_Unit](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) 目录（8 篇 TMU 专利 PDF 及逐篇解析）。

## 十、建议阅读路径

1. **US20130198760A1**（WDU 340 结构与 Task Table）→ **US8572573B2**（槽位驱逐）——Kepler WDU 基础；
2. **US10552202B2**——WDU 在抢占中的角色（提及最多）；
3. **US10067768B2 / US10915445B2**——Volta 世代 Scheduler Unit + WDU + 芯片级 XBar 框图；
4. **US20230236878A1**——WSDU 合并形态（Hopper）；
5. **US9594599B1**（按 SM 容量比例分发）、**US9710306B2**（自动节流）、**US10332310B2**（图元 work slice）、**US8941653B2**（有序分发）——分发策略四种形态，均有逐篇解析；
6. **rtas23**——实验侧验证。

## 十一、关联资源

| 资源 | 位置 | 关联点 |
|------|------|--------|
| TMU 研究综述 | [12_Task_Management_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) | WDU 的上游 TMU，8 篇核心专利与解析 |
| NVIDIA TMU 专利专题整理 | [14_Patents/NVIDIA_TMU专利专题整理.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/NVIDIA_TMU专利专题整理.md) | 检索方法（Google Patents XHR、内网 PatentsDB API）与专利族记录 |

---

*本综述基于本地专利全文提取（pypdf）、Google Patents XHR 短语检索与内网 PatentsDB 核查（2026-08-19，一次性例外授权）整理。US20230236878A1 为扫描件，相关结论来自视觉阅读。附图标记以专利原文为准。最后更新：2026-08-19*

