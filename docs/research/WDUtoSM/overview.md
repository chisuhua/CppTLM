# 专题 16：WDU→SM 分发段 与 AMD SPI/SQ 波前分发 — 研究综述

> 建立日期：2026-08-19
> 专题目录：`01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/`
> 上游专题：[12_Task_Management_Unit](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md)（TMU）、[14_Front_End_and_Task_Graphs](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/研究综述.md)（前端/任务图）、[15_AMD_Front_End_CP_Dispatch](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/研究综述.md)（AMD CP 前端）
> 姊妹专题：[13_Work_Distribution_Unit](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/研究综述.md)（WDU 本体与两级 crossbar 的既有收集）

## 一、专题定位与范围

本专题覆盖两条链路的**最后一段分发路径**——工作单元离开集中式调度器之后，如何穿过互连网络、被放到具体的执行单元上并完成资源准入：

- **NVIDIA**：WDU 236 → **Work Distribution Crossbar 330 → GPC 208（Pipeline Manager 305）→ SM 310**，即 CTA 的跨 GPC 路由、目的选择、SM 侧接收与资源分配，以及 Hopper 时代新增的 CGA（Cooperative Group Array / thread block cluster）跨 SM 协同分发；
- **AMD**：CP 之后 → **SPI（Shader Processor Input）→ SQ（Sequencer）→ CU**，即 workgroup/wavefront 向 SE/CU/SIMD 的分发目的地选择、资源准入仲裁、波前调度与多内核并发管理。

与既有专题的分工：13 号专题收集 WDU 本体专利（任务选取、描述符处理、两级 crossbar 的 WDU 侧视角）；15 号专题覆盖 AMD CP 前端（doorbell/HQD/IB/PM4/用户态分发）。本专题是两者的**下游接续**：NVIDIA 侧从 crossbar 出口讲到 SM 接收，AMD 侧从 CP 出口讲到 CU 上的波前调度。

## 二、链路概览

```
NVIDIA:  CPU → pushbuffer → Host Interface → Front End → TMU 234 → WDU 236
         → [Work Distribution Crossbar 330 → GPC 208 / Pipeline Manager 305 → SM 310]  ← 本专题
AMD:     CPU → ring buffer / AQL queue → doorbell → CP (ME/PFP/MEC/HWS) → IB/PM4
         → [SPI (shader input block) → SQ → CU / SIMD wavefront]                        ← 本专题
```

本专题 12 件核心专利的机制落点：

| 链路段 | NVIDIA | AMD |
| --- | --- | --- |
| 分发目的地动态选择 | US20240356866A1（crossbar 逐周期动态目的选择）、US8917271B2（GPC 间再平衡） | US20230206382A1（SE 级闭环动态分发） |
| 分发单元层次结构 | US12333311B2（GPU2GPC/GPC2SM 两级 work distributor + speculative launch） | EP3785113B1（workgroup 跨 CU 拆分分发 + 反馈引导） |
| 基本工作单元定义 | US8112614B2（CTA 奠基：thread ID、共享内存、barrier、单核完成契约） | workgroup→wavefront 粒度转换（EP3785113B1 拆分、US9135077B2 重组） |
| 接收侧资源准入/策略 | US20240036952A1（pending/active pool + spread/balance 策略 API） | US10579388B2（SPI 两级仲裁策略）、US20180108106A1（资源画像缓存） |
| 执行侧线程组织与调度 | US9921847B2（TDT 树形分歧线程管理） | EP3803583B1（多内核波前两级调度 + 迟滞节流） |

## 三、NVIDIA 侧核心专利（6 件）

| 专利 | 标题 | 优先权/申请 | 状态 | 机制要点 |
| --- | --- | --- | --- | --- |
| [US8112614B2](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US8112614B2_协作线程阵列并行数据处理_解析.md) | Parallel Data Processing Using Cooperative Thread Arrays | 母案优先权 2005-12-15；本件申请 2010-12-17 | 授权 2012-02-07，17 claims | CTA（thread block 原型）硬件定义：launch 硬件自动分配多维 thread ID、以 thread ID 计算共享内存地址、barrier 同步、"CTA 在单个核内完成"封闭契约；FIG. 12 两种分发机制为 Work Distribution Crossbar 的概念前身 |
| [US8917271B2](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US8917271B2_几何图元再分配负载均衡_解析.md) | Redistribution of Generated Geometric Primitives | 优先权 2009-10-05（61/248,834） | 授权 2014-12 | tessellation/geometry shader 生成的图元经 TGA 切分为资源受限小任务，由 TDU + work distribution crossbar 跨 GPC 再分配给 SPM 均衡处理，再重排回 API 顺序——"分发后动态再平衡"的早期范式 |
| [US9921847B2](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US9921847B2_树形线程管理_解析.md) | Tree-based Thread Management | 申请 2014-01-21 | 授权 2018-03-20，22 claims | 每 CTA 一棵 TDT 节点树（PC + WIA 索引 + warp 掩码）取代 CRS 隐式栈，分歧路径成为可并行调度的叶节点，消除分歧串行化引起的 barrier/spinlock 死锁——SM 侧线程组织层 |
| [US20240356866A1](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US20240356866A1_交叉开关动态目的选择_解析.md) | Crossbar with At-Dispatch Dynamic Destination Selection | 申请 2023-04-20 | 公开 2024-10-24，20 claims | DDS crossbar：等价目的端口组成 super destination group，mapper 展开 → SCD + DCS/WFA 二维仲裁 → remapper 归并，按实时目的信用与端口可用性**逐周期动态选定具体目的 SM/GPC**——直接命中 Work Distribution Crossbar 330 |
| [US12333311B2](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US12333311B2_协作群组阵列_解析.md) | Cooperative Group Arrays | 申请 2022-03-10（17/691,621） | 授权 2025-06-17，20 claims | Hopper thread block cluster 硬件专利：Grid 与 CTA 之间新增 CGA 层级；两级 work distributor（GPU2GPC WD 420a → GPC2SM WD 420b）speculative launch，按 GPC/µGPU/GPU 域亲和协同共驻，all-or-none 并发保证；配套 DSMEM（GXBAR SM-to-SM 网络）与 CGA 线性共享内存池 |
| [US20240036952A1](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US20240036952A1_线程块调度策略_解析.md) | Scheduling Policy for Blocks of Threads | 申请 2023-04-28（18/034,225）；优先权 IN 202241043444（2022-07-29） | 公开 2024-02-01，36 claims（4 独立） | block cluster 调度策略的 **API 暴露层**：spread/balance 放置 hint、cluster 维度 set/get、硬件并发上限查询；披露硬件链路 front-end 5210 → scheduler 5212 → WDU 5214（每 GPC pending 32 / active 4 槽）→ XBar 5220 → GPC 5218（MPC 5310/DPC 5306）→ SM 5314；与 12 件同日申请（Attorney Docket 0112912-497US0–USC）构成 block cluster API 专利网 |

## 四、AMD 侧核心专利（6 件）

| 专利 | 标题 | 优先权/申请 | 状态 | 机制要点 |
| --- | --- | --- | --- | --- |
| [US10579388B2](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US10579388B2_着色器核心资源分配策略_解析.md) | Policies for Shader Resource Allocation in a Shader Core | 优先权 2011-12-14；本件申请 2018-07-19（延续案） | 授权 2020-03-03 | 多 compute pipeline 共享 shader core 的两级仲裁：HQD 内队列级（4 bit 优先级 + quantum + 六种上下文切换条件 + MQD 保存/恢复）与 HQD 间 pipe 级（CS_HIGH/MEDIUM/LOW + HP3D/GFX 五级优先级 + totem pole 公平电路）；SPI 202 决定哪条流水线的 wavefront 可提交 shader core，抢占点在"shader 资源分配之前" |
| [US20180108106A1](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US20180108106A1_GPU着色器动态资源分配_解析.md) | Dynamically Allocating Resources Among GPU Shaders | 申请 2016-10-19 | 公开 2018-04-19（后放弃） | 首次遇到某工作负载时用性能监视器测量各 shader 负载、生成逐 shader 电压/时钟/内存分配画像，与工作负载 ID 存档复用——SPI/SQ 资源准入的"策略缓存"层 |
| [US9135077B2](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US9135077B2_波前重组计算优化_解析.md) | GPU Compute Optimization via Wavefront Reforming | 申请 2012-03-16 | 授权 2015-09-15，27 claims | work-item 分发到 CU 之后，用每处理器队列 + 阈值判定 + fill/steal/share 原子重分配，把多个部分填充的 wavefront 重组为满负荷 wavefront，回收 SIMD 尾部空闲算力——SQ 波后调度与 CU 利用率层 |
| [EP3785113B1](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/EP3785113B1_反馈引导工作组拆分分发_解析.md) | Feedback Guided Split Workgroup Dispatch for GPUs | 优先权 2018-04-27（US 15/965,231）；EP 申请 2019-02-26 | EP 授权 2024-04-03，11 claims；DOE PathForward 资助 | 打破"workgroup 必须整体装入单个 CU"约束：资源不足时把 workgroup 拆成单个 wavefront 分发到多个 CU，用性能计数器反馈计算的 load-rating 引导 CU 选择，集中式 split workgroup scoreboard（CU 掩码 + barrier 计数）完成跨 CU barrier 同步 |
| [EP3803583B1](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/EP3803583B1_多内核波前调度器_解析.md) | Multi-kernel Wavefront Scheduler | 优先权 2018-05-30（US 15/993,061）；EP 申请 2019-03-27 | EP 授权 2024-10 | CU 内"按 kernel 优先级分组的两级波前调度器 + 去调度队列"：多内核 wavefront 以调度组为粒度仲裁发射，按停顿周期等资源竞争度量做双阈值迟滞节流，实现 CU 上多内核并发、干扰抑制与前向进展保障 |
| [US20230206382A1](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/US20230206382A1_动态工作组分发_解析.md) | Dynamic Dispatch for Workgroup Distribution | 申请 2021-12-29 | 公开 2023-06-2920 claims | CP 内 dispatch controller 闭环动态分发：依据各 SE 物理参数（active CU 数，反映制造 harvesting 不对称）+ SPI 经 compute dispatch bus 上报的进度/槽位状态，把 workgroup 负载均衡到各 SE——SPI 分发层的 SE 级策略 |

## 五、NVIDIA ↔ AMD 术语与机制对照（本专题段）

| 功能 | NVIDIA 术语 | AMD 术语 |
| --- | --- | --- |
| 集中式分发单元出口 | WDU 236 | CP 内 dispatch controller / SPI（shader input block 202） |
| 分发互连网络 | Work Distribution Crossbar 330（GPC 内另有 WD Crossbar） | compute dispatch bus（SPI↔CU）、SE 间分发路径 |
| 目的执行单元 | GPC 208 / SM 310 | SE / CU / SIMD |
| 基本工作单元 | CTA（thread block）；Hopper 起 CGA（thread block cluster） | workgroup；执行粒度 wavefront |
| 接收侧资源准入 | SM 资源池（寄存器/共享内存/warp 槽）；WDU pending/active pool（每 GPC 32/4 槽，US20240036952A1 披露） | SPI 两级仲裁（队列级 + pipe 级，US10579388B2）；wave 槽/VGPR/LDS 分配 |
| 单元内线程组织 | warp；TDT 树形分歧管理（US9921847B2） | wavefront；按 kernel 分组的两级调度（EP3803583B1）；wavefront 重组（US9135077B2） |
| 跨单元协同 | CGA + DSMEM/GXBAR（US12333311B2） | split workgroup + scoreboard（EP3785113B1） |
| 软件可见调度控制 | spread/balance hint、cluster 维度、上限查询 API（US20240036952A1） | 未见对等 API 专利（负结果） |

## 六、架构对比要点

1. **两级分发是两家共识，但层次定义不同**：NVIDIA 是空间两级——GPU2GPC WD 420a 与 GPC2SM WD 420b（US12333311B2），目的选择由 crossbar 仲裁硬件逐周期完成（US20240356866A1）；AMD 是"SE 级 + CU 级"两级——CP dispatch controller 选 SE（US20230206382A1），SPI 在 CU 间做资源准入仲裁（US10579388B2），且把制造 harvesting 不对称（active CU 数）显式写入分发决策，这一点 NVIDIA 专利未见对应披露。
2. **工作单元完整性：两家走了相反的路**。NVIDIA 自 US8112614B2（2005）起坚守"CTA 在单个 SM 内完成"的封闭契约，跨 SM 协同通过在其上叠加新层级（CGA，all-or-none 共驻保证 + DSMEM）实现；AMD 的 EP3785113B1（2018）反而**放松** workgroup 完整性，允许拆成 wavefront 跨 CU 执行并用 scoreboard 补同步。前者加层保粒度，后者放粒度保并行度，是同一矛盾（分发粒度 vs 执行粒度）的两种对偶解法。
3. **目的选择从静态轮询走向闭环反馈**：NVIDIA 2009 年的 US8917271B2 已做 GPC 间负载再平衡（图形图元语境），2023 年的 US20240356866A1 把目的选择细化到 crossbar 逐周期信用仲裁；AMD 2021 年的 US20230206382A1 用 SPI 实时上报 + SE 物理参数构成闭环。两家都在把"分发决策"从编译期/启动期静态假设迁移到运行期硬件反馈。
4. **资源准入策略是 AMD 专利化的重点，NVIDIA 则藏在占用率模型里**：AMD 有显式的两级仲裁 + totem pole 公平电路（US10579388B2）、双阈值迟滞节流（EP3803583B1）与资源画像缓存（US20180108106A1）；NVIDIA 侧本专题仅经 US20240036952A1 的 pending/active pool 与上限查询 API 间接披露，SM 准入逻辑本身未见独立专利（负结果）。
5. **执行单元的公平与进展保证**：NVIDIA US9921847B2 用 TDT 树把 warp 分歧路径并行化，解决的是单 CTA 内分歧串行化导致的 barrier 死锁；AMD EP3803583B1 用优先级分组 + 去调度队列解决多内核共享 CU 的干扰与饥饿。两者分别在"线程分歧"与"内核竞争"两个维度提供前向进展保证，共同指向同一工程问题：共享执行资源上的多租户调度。

## 七、检索记录

### 7.1 Google Patents XHR 检索（patents.google.com/xhr/query，受让人过滤 + 客户端精确短语筛选）

第 1 轮（12 组查询）：

| 查询键 | 短语 | 受让人 | 命中总数 |
| --- | --- | --- | --- |
| nv_pm | "pipeline manager" | NVIDIA | 4550 |
| nv_wdc | "work distribution crossbar" | NVIDIA | 2457 |
| nv_wdu | "work distribution unit" | NVIDIA | 3835 |
| nv_td | "task descriptor" | NVIDIA | 49 |
| nv_cta | "cooperative thread array" | NVIDIA | 789 |
| nv_tb | "thread block" | NVIDIA | 2809 |
| amd_spi | "shader processor input" | AMD/ATI | 16 |
| amd_wgd | "workgroup dispatch" | AMD/ATI | 3 |
| amd_wfd | wavefront/workload 分发短语 | AMD/ATI | 7 |
| amd_seq | "sequencer" | AMD/ATI | 331 |
| amd_sra | "shader resource allocation" | AMD/ATI | 10 |
| amd_cud | compute unit + dispatch 短语 | AMD/ATI | 194 |

第 2 轮（8 组查询）：nv_gpc "graphics processing cluster"（5866）、nv_pm2 "pipeline manager"+"processing cluster"、nv_wdpm "work distribution"+"pipeline manager"、nv_smwd "streaming multiprocessor"+"work distribution"、nv_wl "workload distribution"、nv_tbsm "thread block"+"streaming multiprocessor"（过滤后 0 条）、amd_wfseq "wavefront"+"sequencer"（过滤后 0 条）、amd_wfdisp "wavefront"+"dispatch"（过滤后 12 条）。

第 3 轮（标题定向）：`"application programming interface" "schedule thread blocks"` → 命中 US20240036952A1；`"wave throttling"` → 仅 KR20240004302A。

### 7.2 内网 PatentsDB（一次性例外授权，仅 NVIDIA 收录）

8 组查询：pipeline manager / work distribution / work distribution unit / task distribution / processing cluster / streaming multiprocessor / thread block / crossbar。关键命中：

- **US-2024356866-A1** "Crossbar with at-dispatch dynamic destination selection"（2023-04-20）→ 本专题 US20240356866A1；
- **thread block API 族**：US-12554534-B2 及 US-2024036951–57-A1 系列（优先权 2022-07-29，"API to indicate/schedule thread blocks、share memory between groups of blocks" 等）→ 导向本专题 US20240036952A1；
- **CGA 族**：US-12333311-B2、US-2025272107-A1 "Cooperative Group Arrays"、US-2025173152-A1 "Distributed Shared Memory"（2022-03-10）；
- 外围候选：US-20260169795-A1 "API to schedule thread blocks"（2026 公开，无 PDF）、US-20260186832-A1、US-2025200859-A1（divergent branch prioritization）、US-2025245061-A1（reservation policies）等。

## 八、负结果与剔除记录

1. **NVIDIA "pipeline manager" 无专属专利**：精确短语过滤后数千命中几乎全是偶然提及（渲染/AI 专利顺带描述 GPC 结构）；GPC Pipeline Manager 305 仅作为 FIG. 3 背景部件出现在 US11182207B2 等专利中，未检索到以其为权利要求主体的专利。
2. **AMD "wavefront"+"sequencer" 组合为 0**：SQ 在专利文本中极少以 "sequencer" 字面出现，其机制散布于 dispatch/scheduling/resource allocation 主题，须按机制检索而非按部件名检索。
3. **KR20240004302A** "Wave throttling based on parameter buffers"（优先权 2021-03-30）仅韩国公开、无美国同族，韩文文本，未纳入逐篇解析，留作候选。
4. **US12217331B2** "Optimizing Grid-based Compute Graphs"：标题易误判为 CTA grid 调度，视觉阅读核实内容为**图数据的网格化位流压缩**（图算法负载表示层），与 WDU→SM 分发无关，已移出本专题。
5. **US12554534B2 / US20260147612A1 等 2025–2026 公开/授权文本**在 Google Patents 无 PDF，改以同族早期公开文本或相关件替代。
6. Google Patents 对 NVIDIA 部件名（crossbar、work distribution unit）的短语检索噪声极大（数千条），有效信号主要来自 PatentsDB 的 thread block/crossbar 定向查询与机制短语（cooperative thread array、tree-based thread management）——与 15 号专题"AMD 须双受让人检索"同为本次检索的方法论要点。

## 九、候选清单（未逐篇解析）

| 专利 | 主题 | 备注 |
| --- | --- | --- |
| KR20240004302A | Wave throttling based on parameter buffers | 仅 KR 公开，SPI 参数缓冲节流 |
| US20200380761A1 | Command processor based multi dispatch scheduler | CP 侧多分发调度，偏 15 号专题范围 |
| CN112204523B | Multi-core Wavefront Scheduler | EP3803583B1 的中国同族（授权 2025-01） |
| US2025272107A1 | Cooperative Group Arrays | US12333311B2 同族后续公开 |
| US2025173152A1 | Distributed Shared Memory | CGA 的 DSMEM 配套申请（2022-03-10） |
| US11367160B2 | Simultaneous compute and graphics scheduling | 计算/图形混跑调度，层级偏上 |
| US20240036951A1 等 12 件 | block cluster API 族（0112912-497US0–USC） | US20240036952A1 的同日申请兄弟件 |
| DE102019101853B4 | Dynamic partitioning of execution resources | 执行资源动态分区（德国公开） |

## 十、文件清单

本目录 12 件专利 PDF + 12 篇逐篇解析 + 1 篇综述 = **25 个文件**（另有 `_txt/` 文本提取与 `_png/` 扫描件渲染工作目录，不计入统计）。扫描件 5 件：US20240356866A1、US20230206382A1、US12333311B2、US20240036952A1（视觉阅读提取，已读页码范围见各解析文末）。

## 十一、关联资源

| 资源 | 位置 | 关联点 |
| --- | --- | --- |
| WDU 研究综述（本专题上游） | [13_Work_Distribution_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/研究综述.md) | WDU 本体与两级 crossbar 专利；本专题接续其下游（crossbar 出口→SM） |
| TMU 研究综述 | [12_Task_Management_Unit/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) | TMU→WDU 上游链路；US11182207B2 的 FIG. 2/3 是本专题链路的出处 |
| 前端与任务图研究综述 | [14_Front_End_and_Task_Graphs/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/研究综述.md) | US10489056B2 SM 队列管理器（SM 侧接收的另一视角） |
| AMD 前端专题（15 号） | [15_AMD_Front_End_CP_Dispatch/研究综述.md](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/研究综述.md) | AMD CP 前端；本专题接续其下游（SPI/SQ→CU） |
| NVIDIA TMU 专利专题整理 | [14_Patents/NVIDIA_TMU专利专题整理.md](file:///Users/chisuhua/source/myresearch/research/14_Patents/NVIDIA_TMU专利专题整理.md) | 检索方法与内网 PatentsDB 使用记录 |

---

*本综述基于 12 份专利全文提取与逐篇解析生成（7 份经 pypdf 文本提取，5 份扫描件经 PyMuPDF 渲染后视觉阅读提取）。检索经 Google Patents XHR 与内网 PatentsDB（2026-08-19，一次性例外授权）。US20240036952A1 解析稿中的延续链/同族信息经 Google Patents 页面二次核实更正（原始优先权为印度临时申请 202241043444）。附图标记与引用关系以专利原文为准。最后更新：2026-08-19*

