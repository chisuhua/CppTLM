# NVIDIA Blackwell/Hopper SM 内部实现专利与论文 — 研究综述

> 专题目录：`01_GPU_Architecture/17_Blackwell_SM_Internals/`
> 创建日期：2026-08-20
> 材料构成：10 件 NVIDIA 专利（PDF + 逐篇解析）+ 4 篇微基准剖析论文与 1 份官方技术简报（均 PDF + 逐篇解析）

## 一、专题定位与范围

本专题聚焦 NVIDIA **SM（Streaming Multiprocessor）内部实现**的专利与公开文献，时间轴覆盖 Hopper（SM_90，2022）到 Blackwell（SM_100 数据中心 / SM_120 消费级，2024–2025）。之所以把 Hopper 专利作为主体，原因有二：

1. 现行 SM 内部的关键机制——TMA 异步张量搬运、wgmma warp 组矩阵乘加、mbarrier 事务屏障、DSMEM 分布式共享内存、thread block cluster——**全部奠基于 2022-03-10 的 Hopper 专利申请集群**（同一日提交、CROSS-REFERENCE 互引的十余件姊妹案），Blackwell 沿用并扩展了这套机制（tcgen05 即 wgmma 的 tensor memory 化演进）；
2. Blackwell 世代自身的 SM 内部专利（如 tensor memory 分配类申请）截至 2026-08 公开文本尚少，且部分无公开 PDF 可取（见第八节负结果）。

因此本专题的检索口径是：**"Blackwell 系列或更新架构的 SM 内部实现"** = 在 Blackwell 上仍然生效、被 Blackwell 新增机制直接继承或改造的 SM 内部专利族 + Blackwell 世代的补充申请 + 直接剖析 Blackwell/Hopper SM 微架构的论文。子系统覆盖：异步数据通路（TMA）、张量核心编程范式（wgmma/tcgen05）、同步原语（mbarrier）、分布式共享内存（DSMEM/cluster）、寄存器文件配置、线程组负载均衡。

## 二、专利群总览（10 件）

| # | 专利号 | 主题 | 申请日 | 状态/授权版 | 分组 | 逐篇解析 |
|---|--------|------|--------|-------------|------|----------|
| 1 | US20230289304A1 | TMA 多维数据异步访问（TMAU） | 2022-03-10 | 授权 US12141082B2 | A 异步搬运 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289304A1_TMA多维数据异步访问_解析.md) |
| 2 | US12020035B2 | TMA 可编程多播（LRC 跟踪） | 2022-03-10 | 已授权；族含 US12608212B2 | A 异步搬运 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US12020035B2_TMA可编程多播_解析.md) |
| 3 | US20240176663A1 | TMA 张量映射缓存（描述符缓存） | 2023-07-07（优先权 2022-11-28 IN） | 公开中 | A 异步搬运 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20240176663A1_TMA张量映射缓存_解析.md) |
| 4 | US20230289398A1 | wgmma warp 组矩阵乘加 | 2022-03-10 | 公开中（DE 同族） | B 张量核心 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289398A1_wgmma组矩阵乘加_解析.md) |
| 5 | US20230289242A1 | mbarrier 异步事务同步（SYNCS） | 2022-03-10 | 授权 US12536056B2 | C 同步 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289242A1_mbarrier异步事务同步_解析.md) |
| 6 | US20230315655A1 | 快速数据同步（store-and-arrive） | 2022-03-10 | 公开中（DE 同族） | C 同步 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230315655A1_快速数据同步_解析.md) |
| 7 | US20250173152A1 | 分布式共享内存 DSMEM | 2025-01-31（母案 17/691,690 授权 US12248788B2） | 公开中 | D cluster/DSMEM | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20250173152A1_分布式共享内存_解析.md) |
| 8 | US20240378089A1 | 基于性能指标的寄存器配置 | 2023-05-09 | 公开中（CN/DE 同族） | E 寄存器 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20240378089A1_基于性能指标的寄存器配置_解析.md) |
| 9 | US20230289211A1 | 线程组可扩展负载均衡（CWD/CGA 准入） | 2022-03-10 | 公开中（CN/DE 同族） | F 调度 | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289211A1_线程组可扩展负载均衡_解析.md) |
| 10 | US20240231830A1 | 工作负载分配（SpMV 线程级均衡） | 2023-01-09 | 公开中 | F 调度（软件，外围） | [解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20240231830A1_工作负载分配_解析.md) |

申请号速查：1 号 = 17/691,276；2 号 = 17/691,288；5 号 = 17/691,296；6 号 = 17/691,303；4 号 = 17/691,406；9 号 = 17/691,872；7 号 = 19/042,001；8 号 = 18/195,305。1/2/4/5/6/9 号同属 **2022-03-10 Hopper 集群**，CROSS-REFERENCE 互引，构成一个完整的"异步 SM"专利组合。

## 三、子系统纵览：专利视角

### 3.1 异步数据搬运体系（TMA 三件）

**US20230289304A1（TMAU 核心）**：每 SM 配一台紧耦合 Tensor Memory Access Unit，采用"tensor descriptor（张量映射，含维数/尺寸/步长/swizzle）+ access descriptor + 指令内坐标"三层参数模型，把多维坐标在硬件内翻译为多个内存地址，单条指令异步搬运 KB–MB 级块并拆分为不超 L2 line 的子请求；完成通知不落 TMAU——barrier 地址随每个子请求下发，由内存系统按字节数直接更新 mbarrier 事务计数。tile/im2col 双模式 + 128B swizzle 直接供数 wgmma。这是 PTX `cp.async.bulk.tensor` 与 driver API `cuTensorMapEncodeTiled/Im2Col` 的硬件原型。

**US12020035B2（多播）**：多播元数据在 L2 Request Coalescer（LRC）处剥离并存入跟踪表，L2 slice 按普通单播读一次，响应回程重新挂上最多 16 个 CTA 的接收方列表，沿 response crossbar 单包传输、仅在分叉点复制——"读一次、多 SM 共享内存同时落点"，对应 PTX multicast 变体的 `ctaMask`。GEMM 权重广播的带宽/能耗收益即出于此。

**US20240176663A1（描述符缓存）**：Hopper 发布后的生态补强。TMAU 内 descriptor cache 以描述符全局地址为标签，miss 时经 GCC 与当前请求**并行前瞻预取**；软件面配套 `prefetch.tensormap`（.const/.param）。点名 GH100，描述符 64B/128B 架构相关。与另两件构成"生成→预取→缓存→消费"闭环。

三件合起来回答了"Hopper/Blackwell GEMM 主循环里数据怎么到 SM"：描述符暖缓存 → 单指令坐标准备 → 硬件翻译多地址 → 多播分发 → 字节计数直达 mbarrier。

### 3.2 张量核心编程范式（wgmma → tcgen05）

**US20230289398A1**：4 warps×32 threads 的 warp group 跨数据通路共享寄存器文件——B 操作数读一次多播给 4 个 warp，A 可绕过寄存器文件直接来自共享内存描述符，FIG. 6A 标注 "H100 TC Instruction 64k MACs, 32 cycles"；异步执行由"worker thread"状态机驱动（WG.ARRIVE→屏障→GMMA 发射→读操作数/计算/写回流水→WG.DEPBAR），与公开 PTX `wgmma.mma_async / commit_group / wait_group` 语义一一对应。GMMA 描述符统一尺寸/格式/布局/swizzle，单指令驱动整块运算。

**Blackwell 演进（论文 + 技术简报佐证，见第四、五节）**：SM_100 转向 `tcgen05`——单线程发射、操作数改由共享内存与新增的 **Tensor Memory（TMEM）** 供给，warp 级同步消失；CTA pair（2CTA 共享操作数，映射到 TPC）成为新的协作粒度；新增 FP4/FP6/MX 格式与卷积 weight-stationary 数据流。消费级 Blackwell（SM_120，RTX 50 系）则不兼容 wgmma，统一走 tcgen05。wgmma 专利因此是理解"Blackwell 改了什么"的基准线。

### 3.3 同步基石（mbarrier 两件）

**US20230289242A1**：内存驻留事务屏障（Phase/Arrive Count/Lock/Transaction Count/Expected Arrive Count 字段）+ SM 内 SYNCS 加速单元（4×257-bit 全相联 write-back barrier cache、32 项 CAM try-wait buffer、coalescer）。"双计数器归零"（线程 arrive 计数 + 硬件事务字节计数）统一了线程同步与 TMA/DMA/multicast 异步事务的等待，即 PTX `mbarrier.expect_tx / arrive-on / test_wait` 的硬件本体。

**US20230315655A1**：与 mbarrier 案互为表里——把 **store-and-arrive**（数据写 + 屏障更新合成单消息，REDS_CGA.ARRIVE_TCNT）立为 ISA 一等公民，跨 SM producer-consumer 同步从 3–4 个 L2 往返降到 ~0.5 往返；再配 L2 中介队列把该能力扩展出 GPC/CGA 范围。两案合起来是"Hopper/Blackwell 异步流水同步体系的两半"：6 号管数据+通知如何过互连，5 号管屏障对象如何在 SM 内缓存/加速/等待。

### 3.4 DSMEM 与 thread block cluster

**US20250173152A1**（母案 17/691,690 已授权 US12248788B2）：GPC 内 CGA（cluster）各 SM 的本地共享内存经**专用 SM2SM 网络**（GPCARB/GXBAR，与 MXBAR/L2 路径物理隔离）互访；4GB/256×16MB CTAID 分段地址窗口、源侧两级路由表 + 目的侧 ShMemBase CAM、uTLB 地址 hash、phase 翻转 MEMBAR 与合并写确认、CGABAR.ARRIVE/WAIT 集群屏障与 CGA 退出/flush 协议。Blackwell 的 CTA pair/CGA 操作数共享即跑在这条网络上。

### 3.5 寄存器文件配置

**US20240378089A1**：编译器侧专利（Berson 独任）。以**预编译期估计的性能指标**统一驱动 IR 变换选择、pre-RA 预测性减压与可撤销回溯、以及"register cliff"（每线程等量寄存器块数阶梯）局部搜索——下崖换并发线程数、上崖换 spill 减少。权利要求锁定"指标须在寄存器分配前测得（=估计）"。它是 `__launch_bounds__`/maxrregcount/occupancy 决策背后机制的专利化，直接决定 SM 上 resident warp 数与延迟隐藏能力。

### 3.6 线程组负载均衡与工作分配

**US20230289211A1**：Hopper 集群的"调度层枢纽"。集中式 CWD 以 free-slots 查询 + mock-GPC 影子状态投机启动，对 CGA 内全部 CTA 做 **all-or-nothing 并发准入**与注水式负载均衡；两级 WD（GPU2GPC/GPC2SM）可递归扩展；"每 GPC 按使能 SM 数初始化计数器"与 13 号专题 US9,594,599B1 的比例分发原则一脉相承。其 CGA all-or-none 思想在 Blackwell 世代的延续即 16 号专题的 US12333311B2。

**US20240231830A1**（外围）：稀疏矩阵 SpMV 内核的线程级负载均衡算法（二分查找分块 + offset expansion 前缀和），与 SM 硬件内部相关度低—中，作为软件视角的 SM 资源使用样例收录。

## 四、Blackwell 世代的 SM 变化（论文与官方简报证据）

综合 4 篇微基准论文与官方技术简报，Blackwell 相对 Hopper 的 SM 内部变化可归纳为：

1. **张量核心第五代 + TMEM**：tcgen05 指令族单线程发射 MMA，操作数来自 SMEM 与新设 Tensor Memory（每 SM 专属，`tcgen05.ld/st/cp` 显式管理分配/搬运/释放）；warp 级同步取消，张量操作获得真正的每线程调度。
2. **低精度扩张**：原生 FP4/FP6（配合第二代 Transformer Engine 与 MX 格式），FP4 使参数带宽对 HBM 的需求翻倍。
3. **CTA pair 执行**：相邻 rank 的两个 CTA 共享操作数、映射到同一 TPC，走 TPC 内专用通信网络——cluster/DSMEM 机制在张量核心侧的直接运用。
4. **卷积数据流硬化**：collector buffer 缓存并复用权重（B 操作数），weight-stationary。
5. **解压缩引擎（DE）**（B200/GB100）：硬件解压数据库/压缩数据，卸载通用核心。
6. **双 die**（GB100/B200）：两芯经片内互连呈现为单一 GPU，SM 编排对软件透明。
7. **消费级 SM_120 差异**（RTX 5080/GB203 vs H100）：寄存器文件 256KB/SM 保持；可配置共享内存 ≈99KB/SM（H100 为 227KB 档），L1/SMEM 统一缓存容量减半，L2 更大（实测口径见论文 1 号）；GDDR7 替代 HBM；warp 调度器对分歧负载降低分发延迟；不兼容 wgmma，tcgen05 为唯一张量路径（且在 SM_120A 专属变体出现前消费卡仅能走 mma.sync 公共分母）。

## 五、论文逐篇点评（5 份）

1. **[Dissecting the NVIDIA Blackwell Architecture with Microbenchmarks](https://arxiv.org/abs/2507.10789)**（Jarmusch/Graddon/Chandrasekaran, Delaware, 2025；本地 [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2507.10789_Blackwell微基准剖析.pdf)｜[逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2507.10789_Blackwell微基准剖析_解析.md)）——RTX 5080（GB203）对 H100 PCIe 的逐项微基准：内存层级延迟/带宽、SM 执行管线、子核单元、第五代张量核心 FP4/FP6。价值：消费级 Blackwell SM_120 的一手实测（寄存器 256KB/SM、可配置共享内存 ≈99KB/SM、warp 调度差异），并实证 wgmma 在 GB203 上不可用、tcgen05 尚未落地的 ISA 断裂窗口。
2. **[Microbenchmarking NVIDIA's Blackwell Architecture: An in-depth Architectural Analysis](https://arxiv.org/abs/2512.02189)**（Jarmusch/Chandrasekaran, Delaware；本地 [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2512.02189_Blackwell微基准深入.pdf)｜[逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2512.02189_Blackwell微基准深入_解析.md)）——**B200（GB100 双 die）对 H200**：本组中唯一的数据中心 Blackwell 实测。首次系统刻画 **TMEM**（256KB/SM、512 列×128 lane、读带宽约 16TB/s、64×64 最优 tile）、tcgen05/wgmma/Volta-Ampere 三代张量指令流水对比（tcgen05.mma 延迟恒定 ≈11 cycles vs wgmma 随尺寸线性 32–128 cycles）、解压缩引擎吞吐、密集/稀疏 GEMM 与训练负载；结论如 B200 混合精度训练 ResNet-50 1.85×、GPT-1.3B 1.55×、能效 +32%。
3. **[Benchmarking and Dissecting the Nvidia Hopper GPU Architecture](https://arxiv.org/abs/2402.13499)**（Luo/Fan 等, HKUST, IPDPS 2024；实测 **H800 PCIe**；本地 [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2402.13499_Hopper微基准剖析.pdf)｜[逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2402.13499_Hopper微基准剖析_解析.md)）——Hopper 新特性首批系统实测：DPX 动态规划指令、分布式共享内存（SM-to-SM 延迟 180 clocks，低于 L2 32%）、FP8 张量核心（wgmma 完成延迟 128 clocks），跨 Hopper/Ada/Ampere 三代延迟吞吐对比；是 Hopper 特性（含 DSMEM 时延）引用率最高的独立实测之一。
4. **[Dissecting the NVIDIA Hopper Architecture through Microbenchmarking and Multiple Level Analysis](https://arxiv.org/abs/2501.12084)**（Luo/Fan/Li/Du/Liu/Wang/Chu, HKUST(GZ)/哈工大深圳；实测 **H800 PCIe**；本地 [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2501.12084_Hopper微基准剖析v2.pdf)｜[逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2501.12084_Hopper微基准剖析v2_解析.md)）——3 号的扩展版（33 页）：多层级分析 Hopper 的指令延迟、缓存层级（L2 双分区四类延迟分解）、异步执行（TMA 延迟/吞吐/形状敏感性、GEMM 1.5×）与张量核心（wgmma vs mma 达峰率 95% vs 62.9%），是本专题专利机制（3.1/3.3 节）最系统的独立行为学印证。
5. **NVIDIA Blackwell Architecture Technical Brief v2.1**（NVIDIA 官方，含 GB300 Ultra 更新；本地 [PDF](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1.pdf)｜[逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md)）——官方口径：第五代张量核心格式矩阵（FP4/FP6/FP8/INT8/FP16/BF16/TF32/FP64）、Transformer Engine、NVLink5、每 GPU 算力规格表；经全文检索确认 SM 数量/频率/缓存、TMA、TMEM、tcgen05 等微观机制**全部未提及**，故仅作规格基准与术语锚点。注：完整的 Blackwell Architecture 白皮书在 resources.nvidia.com 需表单获取，本简报是其公开等价物。

## 六、与既有专题的交叉关系

| 关联专题 | 交叉点 |
|----------|--------|
| [12_Task_Management_Unit](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/12_Task_Management_Unit/研究综述.md) | TMU 交付的 grid/CGA 由本专题 9 号（CWD 负载均衡）接入 SM；PDL（程序化依赖启动）与本专题 mbarrier 事务同属"异步化"主线 |
| [13_Work_Distribution_Unit](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/13_Work_Distribution_Unit/研究综述.md) | 9 号"按使能 SM 数初始化计数器"是 US9,594,599B1 比例分发在 CGA 时代的回声；两级 crossbar（13 号）与本专题 DSMEM 的 GXBAR 为不同层级互连 |
| [14_Front_End_and_Task_Graphs](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/14_Front_End_and_Task_Graphs/研究综述.md) | CUDA Graphs 批量提交 + 本专题 TMA/mbarrier 异步流水，共同压低 kernel 间与 kernel 内空隙 |
| [15_AMD_Front_End_CP_Dispatch](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/15_AMD_Front_End_CP_Dispatch/研究综述.md) | 对照：AMD 无 TMA/DSMEM 等价物（CP/IB+doorbell 走命令流路线）；NVIDIA mbarrier vs AMD 波前计数/ATC 屏障 |
| [16_WDU_to_SM_and_SPI_SQ](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/16_WDU_to_SM_and_SPI_SQ/研究综述.md) | US12333311B2（CGA all-or-none 两级分发）与 9 号 US20230289211A1 为同源思想的两个世代；US20240356866A1（XBar 动态目的选择）为 DSMEM/多播流量的互连底座 |
| 07_Register_File / 06_Warp_Scheduling_SIMT | 8 号寄存器配置专利与 07 号寄存器文件文献、06 号 warp 调度文献互补（占用率/延迟隐藏的机制与策略两端） |

## 七、引用网络观察

- **同日姊妹案互引**：1/2/4/5/6/9 号 CROSS-REFERENCE 互列（各列约 10 件），是 NVIDIA 惯用的"集群申请"策略；7 号母案、3 号印度优先权案则是后续补强。
- **授权轨迹**：截至 2026-08，集群中已见授权版 US12141082B2（TMA）、US12499052B2（TMA 姊妹案 17/691,422）、US12020035B2/US12608212B2（多播）、US12536056B2（mbarrier）、US12248788B2（DSMEM 母案）——核心机制均已获授权，非纸面专利。
- **外部跟进**：TMA 案被引含 Intel、Arm、Qualcomm、壁仞；wgmma 案被引含壁仞、摩尔线程、Imagination、Intel；mbarrier 案被引亦含多家非 NVIDIA 实体——异步张量搬运 + 事务屏障已成行业参照系。国产 GPGPU（壁仞/摩尔线程）在张量核心与同步机制上密集引用该集群。
- **AMD US20250004730A1** 跟进 8 号（IR 变换选择按估计指标），显示寄存器/编译策略线的跨厂商竞争。

## 八、负结果与局限

1. **Blackwell 原生 SM 专利稀缺**：以 "tensor memory allocation" 等关键词检索到的 Blackwell 世代申请（如 US20260147493A1）暂无公开 PDF，未收入；本专题以 Hopper 集群 + 2023–2025 补强案为主体。
2. **官方白皮书 gated**：完整 Blackwell Architecture 白皮书需 resources.nvidia.com 表单提交；本专题收录页面内嵌的 Technical Brief v2.1（31 页，含 Blackwell Ultra）作为官方口径替代。RTX Blackwell 白皮书 CDN 直链已失效（404），未强求。
3. **扫描版处理**：10 件专利 PDF 均为扫描件（无文本层），解析文本依据 Google Patents 页面提取全文，关键附图逐篇视觉核对（各解析文末注明核对页码）。页面引用计数存在排版伪影（如 CITED BY 段重复粘贴 backward 列表），各解析已按去重口径标注。
4. **SM_100 vs SM_120 口径差异**：论文 1/2 分别测消费级（GB203）与数据中心（B200），两者 SM 微架构参数不同（L1 容量、张量指令集兼容等），引用时须区分。
5. 内网 PatentsDB 经既往核查仅收录 NVIDIA 部分族，且无 Blackwell 新案，本专题未动用网检索额度。

## 九、推荐阅读路径

1. 先读本综述第二、三节建立"异步 SM"框架，再按 3.1→3.3→3.2 顺序读 TMA、mbarrier、wgmma 三篇解析（机制上数据先行、同步次之、计算居后）；
2. 论文 [2 号（B200 in-depth）](https://arxiv.org/abs/2512.02189) 的 TMEM/tcgen05 章节与 wgmma 解析对读，理解 Hopper→Blackwell 张量路径断裂；
3. DSMEM 解析 + 论文 3 号 DSMEM 时延数据对读，再回看 16 号专题 US12333311B2（CGA 分发）；
4. 关注寄存器/占用率方向者：8 号解析 + 07_Register_File 文献；
5. 需要 Blackwell 官方规格锚点时查 Technical Brief v2.1 第 8 节（Blackwell Tensor Core Architecture）。

## 十、专题文件清单

| 文件 | 类型 |
|------|------|
| [研究综述](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/研究综述.md) | 综述 |
| US20230289304A1.pdf + [TMA多维数据异步访问_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289304A1_TMA多维数据异步访问_解析.md) | 专利 + 解析 |
| US12020035B2.pdf + [TMA可编程多播_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US12020035B2_TMA可编程多播_解析.md) | 专利 + 解析 |
| US20240176663A1.pdf + [TMA张量映射缓存_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20240176663A1_TMA张量映射缓存_解析.md) | 专利 + 解析 |
| US20230289398A1.pdf + [wgmma组矩阵乘加_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289398A1_wgmma组矩阵乘加_解析.md) | 专利 + 解析 |
| US20230289242A1.pdf + [mbarrier异步事务同步_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289242A1_mbarrier异步事务同步_解析.md) | 专利 + 解析 |
| US20230315655A1.pdf + [快速数据同步_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230315655A1_快速数据同步_解析.md) | 专利 + 解析 |
| US20250173152A1.pdf + [分布式共享内存_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20250173152A1_分布式共享内存_解析.md) | 专利 + 解析 |
| US20240378089A1.pdf + [基于性能指标的寄存器配置_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20240378089A1_基于性能指标的寄存器配置_解析.md) | 专利 + 解析 |
| US20230289211A1.pdf + [线程组可扩展负载均衡_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20230289211A1_线程组可扩展负载均衡_解析.md) | 专利 + 解析 |
| US20240231830A1.pdf + [工作负载分配_解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/US20240231830A1_工作负载分配_解析.md) | 专利 + 解析（外围） |
| arXiv2507.10789_Blackwell微基准剖析.pdf + [逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2507.10789_Blackwell微基准剖析_解析.md) | 论文 + 解析 |
| arXiv2512.02189_Blackwell微基准深入.pdf + [逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2512.02189_Blackwell微基准深入_解析.md) | 论文 + 解析 |
| arXiv2402.13499_Hopper微基准剖析.pdf + [逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2402.13499_Hopper微基准剖析_解析.md) | 论文 + 解析 |
| arXiv2501.12084_Hopper微基准剖析v2.pdf + [逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/arXiv2501.12084_Hopper微基准剖析v2_解析.md) | 论文 + 解析 |
| NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1.pdf + [逐篇解析](file:///Users/chisuhua/source/myresearch/research/01_GPU_Architecture/17_Blackwell_SM_Internals/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md) | 官方简报 + 解析 |

---

*本综述基于 Google Patents 页面提取全文 + 扫描件附图视觉核对（10 件专利逐篇解析见上表）、arXiv 论文与 NVIDIA 官方技术简报全文提取（5 份逐篇解析见上表）整理（2026-08-20）。专利法律状态/族谱以 Google Patents 页面为准；论文实测数据以原文为准（注：两篇 Hopper 论文实测机型均为 H800 PCIe 而非 H100）。最后更新：2026-08-20*

