# 05. dGPU SoC v1.0 SM/CU Compute Unit 架构 — Warp 调度 + 寄存器文件 + TMA + mbarrier + DSMEM/TMEM

> **类别**: SoC Architecture > 子系统架构 (L6 SM/CU 内核)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS（§3.6 L6 SM/CU 内核层）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md)（Phase 7.B 规划中）
> - [`docs/soc_arch/modules/cuda-core-adapter.md`](../modules/cuda-core-adapter.md)（v0.5 MVP,深度集成 PTX-EMU）
> - [`docs/soc_arch/modules/gpu-gputlm.md`](../modules/gpu-gputlm.md)
> **关联研究综述**:
> - [`docs/research/SM/overview.md`](../../research/SM/overview.md)（NVIDIA Hopper→Blackwell SM 综述,10 专利 + 4 论文 + 1 官方简报）
> - [`docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`](../../research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md)（Blackwell 官方简报解析）
> - [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md)
> **关联 ADR**: ADR-SOC-02（CU 黑盒/白盒）/ ADR-SOC-06 D2/D5（CU 蓝图 + 深度集成 PTX-EMU）/ ADR-SOC-09 D1（共享 CU 蓝图 + 双 vendor）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.6 L6 SM/CU 内核层的**子系统架构**详细化文档。

- 想快速理解 L6 层结构 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 NVIDIA SM 内部 → 读 §3(Hopper SM_90 + Blackwell SM_100/120)
- 想理解 AMD CDNA 3 → 读 §4(MI300X + SIMD32/64 + wave64)
- 想理解 Warp 调度 → 读 §5(TDT tree + Wave scheduler)
- 想理解寄存器文件 → 读 §6(pre-RA 估计 + register cliff)
- 想理解 TMA 异步张量搬运 → 读 §7(3 件专利:核心 + 多播 + 描述符缓存)
- 想理解 mbarrier 同步 → 读 §8(SYNCS + store-and-arrive)
- 想理解 DSMEM 分布式共享 → 读 §9(per US20250173152A1)
- 想理解 TMEM 张量内存 → 读 §10(per arXiv 2512.02189)
- 想理解 v1.0 战略对齐 → 读 §11
- 想理解配置 Schema → 读 §12
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §13
- 想评估风险 → 读 §14

---

## 1. 范围与目标

### 1.1 L6 SM/CU 内核层定位

**L6 SM/CU 内核层** = dGPU SoC v1.0 系统拓扑的**计算执行层**,负责:

- **NVIDIA 路径**:Warp × 4 (32 threads) + 张量核心 + mbarrier + TMA + DSMEM + TMEM
- **AMD 路径**:SIMD32/64 wavefront + Matrix Core (MFMA) + LDS + split workgroup + HQD
- **CppTLM ComputeUnitTLM**:蓝图模式 `cu_template + cu_count`(per `ComputeCluster`)+ 双 vendor 路径切换

**L6 层模块清单**(per `00-overview` §3.6):

| 模块 | 角色 | v1.0 状态 |
|------|------|---------|
| **ComputeUnitTLM** | CU 蓝图(双 vendor 共享) | ✅ v0.5 MVP 黑盒 + v1.0 蓝图 |
| **CudaCoreAdapter** | NVIDIA SM 微架构探索器(深度集成 PTX-EMU) | ✅ v0.5 MVP(per ADR-SOC-06 D2) |
| **WarpScheduler** | 4 warp 调度器(NVIDIA) | ✅ v0.5 简化 |
| **MFMA Core** | AMD Matrix Core | 🔵 v1.0 MVP 新增基础 |
| **VectorRegFile** | 256KB/SM 寄存器文件 | ✅ v0.5 简化 |
| **SharedMemory** | 228KB Hopper / 99KB Blackwell SM_120 | ✅ v0.5 简化 |
| **TMA / mbarrier / DSMEM / TMEM** | NVIDIA Hopper/Blackwell 独有 | 🔵 v1.0 MVP wgmma;v1.1 tcgen05/TMEM |

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R05-R06)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| NV ComputeUnit | ✅ 黑盒 warp + wgmma + FP8/FP16/BF16/TF32 | + 白盒 warp + tcgen05 + TMEM + FP4/FP6/MX | 00-overview D7 / ADR-SOC-09 D2（Proposed） |
| AMD ComputeUnit | ✅ 黑盒 + wave64 + MFMA + split workgroup + HQD | + 白盒完整 | D7 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.0 PASS 的 §3.6 L6 SM/CU 内核层 + §4-bis 范围矩阵 R05-R06 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

### 2.1 L6 层数据流总览(双 vendor)

```
                    ┌─────────────────────────────────────────────┐
                    │            L5 WDU → SM/CU 分发             │
                    │  - SubmitQueue.dispatch_to_core             │
                    │  - nv_mode: Crossbar (Round-Robin/DDS)     │
                    │  - amd_mode: SPI 两级仲裁 + SQ              │
                    └───────────────────┬─────────────────────────┘
                                        │ CTA/wavefront dispatched
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │      ComputeUnitTLM (蓝图模式)            │
                  │  - cu_template + cu_count                 │
                  │  - 双 vendor 路径切换                      │
                  │  - ComputeCluster 蓝图复制(per ComputeCluster)│
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ NVIDIA 路径        │    │ AMD 路径               │
              │ SM_90/SM_100/SM_120│    │ CDNA 3 SIMD32/64       │
              │ Warp × 4 (32 thr)  │    │ Wavefront               │
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                            │
        ┌───────────────▼────────────┐    ┌──────────▼──────────────┐
        │ Warp 调度 + 寄存器配置    │    │ Wavefront 调度          │
        │ + TMA + mbarrier + DSMEM  │    │ + MFMA + split workgroup│
        └─────────┬─────────────────┘    └──────────┬──────────────┘
                  │                                 │
        ┌─────────▼─────────────────┐    ┌──────────▼──────────────┐
        │ wgmma (Hopper 4 gen)    │    │ MFMA Matrix Core        │
        │ v1.0: FP8/FP16/BF16/TF32 │    │ + LDS(64KB per CU)    │
        │ v1.1: tcgen05 + TMEM     │    │ + HQD 两级仲裁          │
        │       + FP4/FP6/MX      │    │                        │
        └─────────┬─────────────────┘    └──────────┬──────────────┘
                  │                                 │
                  └─────────────┬───────────────────┘
                                │
                                ▼
                  ┌──────────────────────────────────────────┐
                  │        L7 Memory Hierarchy               │
                  │  - HBM3e + L2 + L1 + Shared Memory       │
                  │  - DSMEM (v1.1) + TMEM (v1.1)           │
                  └──────────────────────────────────────────┘
```

### 2.2 关键交互注释

- **L5 → L6**:SubmitQueue.dispatch_to_core 发送 CTA(NVIDIA)或 wavefront(AMD)
- **CU 蓝图模式**:`cu_template` 定义 NV/AMD 单实例,`cu_count` 控制复制数(per ComputeCluster)
- **NVIDIA 内部**:Warp × 4 → 寄存器文件 → 调度 → 执行(wgmma/tcgen05)
- **AMD 内部**:Wavefront → SIMD32/64 → MFMA + LDS

---

## 3. NVIDIA SM 内部架构

### 3.1 SM 世代演进(per `docs/research/SM/overview.md` §四)

| 世代 | 张量核心 | 关键特性 |
|------|---------|---------|
| **Hopper SM_90** | 4 代 wgmma(per US20230289398A1) | TMA + mbarrier + DSMEM + thread block cluster(CGA) |
| **Blackwell SM_100/120** | 5 代 tcgen05 + TMEM | CTA pair 共享 + FP4/FP6/MX + 双 die |
| **消费级 SM_120** | wgmma 不兼容,仅 tcgen05(SM_120A 专属变体) | 寄存器 256KB/SM + SMEM ≈99KB |

### 3.2 Hopper SM_90(数据中心)

**关键参数**(per `docs/research/SM/overview.md` §四 + arXiv 2402.13499):

| 参数 | 值 |
|------|-----|
| 寄存器文件 | 256KB/SM |
| Shared Memory | 228KB/SM |
| L1/SMEM 统一缓存 | ~256KB(228KB SMEM 档) |
| FP 格式 | FP8/FP16/BF16/TF32/FP64 |
| Warp 调度 | 4 warp schedulers |
| DSMEM SM-to-SM 延迟 | 180 clocks(低于 L2 32%) |

**关键子系统**:
- **TMA**(per US20230289304A1):张量描述符 + 异步搬运 + 字节计数直达 mbarrier
- **wgmma**(per US20230289398A1):4 warps×32 threads,worker thread 状态机(FIG. 6A "H100 TC Instruction 64k MACs, 32 cycles")
- **mbarrier**(per US20230289242A1):SYNCS 加速单元 + 双计数器(线程 arrive + 硬件事务字节)
- **DSMEM**(per US20250173152A1):GPC 内 CGA SM 互访 + SM2SM 网络

### 3.3 Blackwell SM_100/120(数据中心 + 消费级)

**关键变化**(per `docs/research/SM/overview.md` §四):

1. **张量核心第五代 + TMEM**:tcgen05 单线程发射 MMA + 操作数来自 SMEM 与新设 Tensor Memory(每 SM 专属,`tcgen05.ld/st/cp` 显式管理)
2. **低精度扩张**:原生 FP4/FP6 + 第二代 Transformer Engine + MX 格式
3. **CTA pair 执行**:相邻 rank 的两个 CTA 共享操作数,映射到同一 TPC
4. **卷积数据流硬化**:collector buffer 缓存并复用权重(B 操作数),weight-stationary
5. **解压缩引擎(DE)**:硬件解压数据库/压缩数据
6. **双 die**(GB100/B200):两芯经片内 NV-HBI 互连,呈现为单一 GPU
7. **消费级差异**(SM_120,RTX 5080 GB203):寄存器 256KB/SM + SMEM ≈99KB + GDDR7 替代 HBM + 不兼容 wgmma

### 3.4 关键专利参考

| 专利 | 主题 | 用途 |
|------|------|------|
| US20230289304A1 | TMA 多维数据异步访问(TMAU 核心) | Tensor descriptor + access descriptor + 坐标三层模型 |
| US12020035B2 | TMA 可编程多播(LRC 跟踪) | 多播元数据在 L2 Request Coalescer 剥离,响应回程复制 |
| US20240176663A1 | TMA 张量映射缓存(描述符缓存) | GCC 并行前瞻预取 |
| US20230289398A1 | wgmma warp 组矩阵乘加 | 4 warps×32 threads + worker thread 状态机 |
| US20230289242A1 | mbarrier 异步事务同步(SYNCS) | 4×257-bit 全相联 write-back barrier cache |
| US20230315655A1 | 快速数据同步(store-and-arrive) | 数据+通知合成单消息(REDS_CGA.ARRIVE_TCNT) |
| US20250173152A1 | 分布式共享内存 DSMEM | GPCARB/GXBAR + 4GB/256×16MB CTAID 分段 |
| US20240378089A1 | 基于性能指标的寄存器配置 | pre-RA 预测性减压 + register cliff 局部搜索 |
| US20230289211A1 | 线程组可扩展负载均衡(CWD/CGA 准入) | 集中式 CWD all-or-none 并发 |
| US9921847B2 | 树形线程管理(TDT) | 每 CTA 一棵 TDT 节点树(PC + WIA 索引 + warp 掩码) |
| US20240231830A1 | 工作负载分配(SpMV 线程级均衡) | 二分查找分块 + offset expansion 前缀和 |

### 3.5 arXiv 微基准论文(实测数据)

| 论文 | 测试 | 关键发现 |
|------|------|---------|
| arXiv 2402.13499 (H800) | Hopper 微基准 | DSMEM SM-to-SM 180 clocks + wgmma 完成 128 clocks |
| arXiv 2501.12084 (H800 v2) | Hopper 多层级分析 | L2 双分区四类延迟分解 + TMA 形状敏感性 |
| arXiv 2507.10789 (RTX 5080 GB203) | Blackwell 消费级 | 寄存器 256KB/SM + SMEM ≈99KB + wgmma 不可用 + tcgen05 尚未落地 |
| arXiv 2512.02189 (B200) | Blackwell 数据中心 in-depth | TMEM 256KB/SM (512×128 lane, 16 TB/s 读) + tcgen05.mma 延迟恒定 ≈11 cycles |

---

## 4. AMD CDNA 3(MI300X)

### 4.1 关键参数(per `docs/research/WDUtoSM/overview.md` + `00-overview` §3.6)

| 参数 | 值 |
|------|-----|
| 拓扑 | **8 XCD chiplets × 38 CU = 304 CU(全卡总数,非 per die)** |
| SIMD | SIMD32/64 per CU |
| Wavefront | **wave64(CDNA 特性,非 wave32;wave32 是 RDNA 特性)** |
| Tensor Core | Matrix Core(MFMA) |
| LDS(Shared Memory) | 64KB per CU |
| 寄存器文件 | 256KB per CU(典型) |
| HQD | 硬件队列描述符(per US8310492B2) |
| IB | 间接缓冲(per US20210304349A1) |
| KFD IOCTL | 用户态驱动(per US9176795B2) |

### 4.2 关键专利参考(AMD)

| 专利 | 主题 | 用途 |
|------|------|------|
| US8310492B2 | HQD 硬件队列调度 | Queue Descriptor + ring buffer |
| US20210304349A1 | 迭代 IB(Indirect Buffer) | Iterative IB 链式 |
| US20220091847A1 | IB 预取 | IB prefetch |
| US11822956B2 | 自适应线程组分发 | Adaptive threadgroup dispatch |
| US9176795B2 | 用户态图形处理分发 | User-mode graphics dispatch |
| US10579388B2 | 着色器核心资源分配策略 | SPI 两级仲裁 |
| US9135077B2 | 波前重组计算优化 | Wavefront 重组(fill/steal/share) |
| EP3785113B1 | 反馈引导工作组拆分分发 | split workgroup + load-rating |
| EP3803583B1 | 多内核波前调度器 | 两级调度 + 双阈值迟滞节流 |
| US20230206382A1 | 动态工作组分发 | SE 级策略 |

### 4.3 MI300X 拓扑细节

**8 XCD chiplets × 38 CU = 304 CU(全卡总数)**:
- **XCD**(Accelerator Complex Die):每 XCD 含 38 CU + 共享 L2(~8MB)
- **全卡共享 Infinity Cache 256MB(L3 级)**
- **HBM3 192GB + 5.3 TB/s bandwidth**

### 4.4 AMD SIMD 与 Wavefront

- **SIMD32**:执行宽度概念；CDNA 3 主路径为 wave64，wave32 仅为 RDNA 兼容语境
- **SIMD64**:**wave64**(CDNA 2/3 主流量)
- **CDNA 3 仅 wave64**(per `docs/research/SM/overview.md` §六)
- **RDNA(消费级)有 wave32**:与 CDNA(数据中心)不同

---

## 5. Warp 调度

### 5.1 NVIDIA 4 Warp 调度器

**SM_90/SM_100**:
- 4 warp schedulers per SM
- 每 warp scheduler 每 cycle 可发射 1 条 warp instruction
- 支持乱序发射(独立 warp)

**Blackwell SM_120 变化**:
- 4 warp schedulers 保持
- tcgen05 引入 **per-thread 调度**(取消 warp 同步)
- warp-level 分发延迟降低

### 5.2 TDT 树形线程管理(per US9921847B2)

**Tree-based Thread Management** 核心机制:

- 每 CTA 一棵 **TDT(TDT Node Tree)**节点 + WIA 索引 + warp 掩码)
- 分歧路径成为**可并行调度**的叶节点
- 消除分歧串行化引起的 barrier/spinlock 死锁

**CppTLM 实施**(v1.0 MVP):
- **v0.5 简化**:仅支持单 warp 调度(无 TDT tree)
- **v1.0 MVP**:基础 TDT 节点 + 分歧路径并行调度
- **v1.1 完整版**:TDT tree 完整 + 优化

### 5.3 调度策略 API(per US20240036952A1)

- **spread 放置 hint**:CTA 分散到多个 SM
- **balance 放置 hint**:CTA 均衡到 SM
- **cluster 维度 set/get**:CGA 集群大小控制
- **硬件并发上限查询**:per SM 最大并发 CTA 数

### 5.4 AMD Wavefront 调度

- **SPI 两级仲裁**(per US10579388B2):HQD 内队列级 + HQD 间 pipe 级
- **多内核波前调度器**(per EP3803583B1):按 kernel 优先级分组的两级调度 + 双阈值迟滞节流
- **Wavefront 重组**(per US9135077B2):回收 SIMD 尾部空闲算力

---

## 6. 寄存器文件配置(per US20240378089A1)

### 6.1 关键创新

**Berson 独任的编译器侧专利**(per `docs/research/SM/overview.md` §3.5):

- 以**预编译期估计的性能指标**统一驱动:
  - IR 变换选择
  - pre-RA 预测性减压
  - 可撤销回溯
- "register cliff"(每线程等量寄存器块数阶梯)**局部搜索**
- **下崖换并发线程数、上崖换 spill 减少**

**权利要求锁定**:"指标须在寄存器分配前测得(=估计)"。

### 6.2 实施影响

- **`__launch_bounds__` / maxrregcount / occupancy 决策背后机制**
- **直接决定 SM 上 resident warp 数与延迟隐藏能力**
- **CppTLM 集成**:`VectorRegFileTLM::regfile_size_per_thread` 配置项
- **v1.0 MVP**:256KB/SM(典型 Blackwell)+ maxrregcount 调节
- **v1.1 完整版**:基于 IR 变换的预编译期指标驱动

### 6.3 寄存器文件大小对比

| 架构 | 寄存器文件大小 |
|------|---------------|
| NVIDIA Hopper SM_90 | 256KB/SM |
| NVIDIA Blackwell SM_100(B200) | 256KB/SM(数据中心) |
| NVIDIA Blackwell SM_120(GB203 消费级) | 256KB/SM |
| AMD CDNA 3(MI300X) | 256KB per CU(典型) |

**结论**:NVIDIA 与 AMD 寄存器文件大小一致(256KB),便于双 vendor 蓝图共享。

---

## 7. TMA 异步张量搬运(3 件专利)

### 7.1 TMAU 核心(per US20230289304A1)

**TMA 多维数据异步访问** 核心机制(per `docs/research/SM/overview.md` §3.1):

- 每 SM 配一台紧耦合 **Tensor Memory Access Unit**(TMAU)
- **3 层参数模型**:
  - **Tensor descriptor**:张量映射(维数/尺寸/步长/swizzle)
  - **Access descriptor**:访问模式
  - **指令内坐标**
- **多维坐标在硬件内翻译为多个内存地址**
- 单条指令**异步搬运 KB–MB 级块**并拆分为不超 L2 line 的子请求
- **完成通知不落 TMAU**——barrier 地址随每个子请求下发,由内存系统按字节数直接更新 mbarrier 事务计数
- **tile/im2col 双模式** + **128B swizzle** 直接供数 wgmma

**对应**:PTX `cp.async.bulk.tensor` + driver API `cuTensorMapEncodeTiled/Im2Col`

### 7.2 多播(per US12020035B2)

**TMA 可编程多播**(LRC 跟踪):

- **多播元数据在 L2 Request Coalescer(LRC)处剥离并存入跟踪表**
- L2 slice 按普通单播读一次
- **响应回程重新挂上最多 16 个 CTA 的接收方列表**
- 沿 response crossbar 单包传输,仅在分叉点复制
- **"读一次、多 SM 共享内存同时落点"** → 对应 PTX multicast 变体的 `ctaMask`

**GEMM 权重广播的带宽/能耗收益即出于此**。

### 7.3 描述符缓存(per US20240176663A1)

**TMA 张量映射缓存**(Hopper 发布后生态补强):

- TMAU 内 **descriptor cache** 以描述符全局地址为标签
- **miss 时经 GCC 与当前请求并行前瞻预取**
- 软件面配套 `prefetch.tensormap`(.const/.param)
- 点名 GH100,描述符 64B/128B 架构相关

**三件合起来**:"生成→预取→缓存→消费" 闭环,回答 "Hopper/Blackwell GEMM 主循环里数据怎么到 SM":
- 描述符暖缓存 → 单指令坐标准备 → 硬件翻译多地址 → 多播分发 → 字节计数直达 mbarrier

### 7.4 CppTLM 实施

| TMA 子模块 | v1.0 MVP | v1.1 完整版 |
|-----------|---------|------------|
| TMAU 核心 | ✅ 基础 | + tensor descriptor 完整支持 |
| 多播(LRC) | ❌ | ✅ |
| 描述符缓存 | ❌ | ✅ per GH100 64B/128B |

---

## 8. mbarrier 同步基石(2 件专利)

### 8.1 mbarrier SYNCS(per US20230289242A1)

**mbarrier 异步事务同步** 核心机制:

- **内存驻留事务屏障**(Phase/Arrive Count/Lock/Transaction Count/Expected Arrive Count 字段)
- **SM 内 SYNCS 加速单元**:
  - **4×257-bit 全相联 write-back barrier cache**
  - **32 项 CAM try-wait buffer**
  - **coalescer**
- **双计数器归零**:
  - 线程 arrive 计数 + 硬件事务字节计数
- 统一线程同步 + TMA/DMA/multicast 异步事务的等待
- 对应 PTX `mbarrier.expect_tx / arrive-on / test_wait`

**授权**:US12536056B2(2026-08 状态)

### 8.2 store-and-arrive(per US20230315655A1)

**快速数据同步** 与 mbarrier 案互为表里:

- 把 **store-and-arrive**(数据写 + 屏障更新合成单消息)立为 ISA 一等公民
- **REDS_CGA.ARRIVE_TCNT**(RedStore 加 cluster arrive transaction count)
- 跨 SM producer-consumer 同步从 3–4 个 L2 往返降到 **~0.5 往返**
- 配 L2 中介队列把该能力扩展出 GPC/CGA 范围

**两案合起来**:Hopper/Blackwell 异步流水同步体系的两半:
- US20230315655A1 管数据+通知如何过互连
- US20230289242A1 管屏障对象如何在 SM 内缓存/加速/等待

### 8.3 CppTLM 实施

| mbarrier 子模块 | v1.0 MVP | v1.1 完整版 |
|---------------|---------|------------|
| mbarrier SYNCS | ✅ 基础屏障 | + 完整 Phase/Arrive Count + 257-bit 字段 |
| store-and-arrive | ❌ | ✅ |

---

## 9. DSMEM 分布式共享内存(per US20250173152A1)

### 9.1 关键机制

**分布式共享内存** 核心机制:

- GPC 内 **CGA**(cluster)各 SM 的本地共享内存经**专用 SM2SM 网络**(GPCARB/GXBAR,与 MXBAR/L2 路径物理隔离)互访
- **4GB/256×16MB CTAID 分段地址窗口**
- 源侧两级路由表 + 目的侧 ShMemBase CAM
- **uTLB 地址 hash**
- phase 翻转 MEMBAR 与合并写确认
- **CGABAR.ARRIVE/WAIT** 集群屏障与 CGA 退出/flush 协议

### 9.2 Blackwell 的 CTA pair/CGA 操作数共享

- **CTA pair**:相邻 rank 的两个 CTA 共享操作数,映射到同一 TPC
- 走 TPC 内专用通信网络
- cluster/DSMEM 机制在张量核心侧的直接运用

### 9.3 性能参数

| 参数 | 值 |
|------|-----|
| SM-to-SM 延迟 | **180 clocks**(per arXiv 2402.13499 实测,Hopper) |
| 相对 L2 | 低于 L2 32% |
| 地址窗口 | 4GB/256×16MB CTAID 分段 |
| 网络隔离 | 专用 GPCARB/GXBAR(物理隔离) |

### 9.4 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R21)
- **v1.1 完整版**:`DSMEM` 模块 + ShMemBase CAM + uTLB + CGABAR 协议

---

## 10. TMEM 张量内存(per arXiv 2512.02189)

### 10.1 关键参数(Blackwell B200 实测)

**TMEM(Tensor Memory)** 是 Blackwell SM 新设的张量内存(per arXiv 2512.02189):

| 参数 | 值 |
|------|-----|
| 容量 | **256KB/SM** |
| 列数 × lane | **512 列 × 128 lane** |
| 读带宽 | **~16 TB/s** |
| 最优 tile | 64×64 |
| 操作码 | `tcgen05.ld/st/cp` 显式管理 |

### 10.2 与 wgmma 的对比

| 维度 | wgmma(Hopper) | tcgen05(Blackwell) |
|------|---------------|-------------------|
| 发射方式 | warp group 协作 | **单线程发射** |
| 操作数来源 | 共享内存 + 寄存器 | **共享内存 + TMEM** |
| warp 同步 | 需要 | **取消** |
| 张量操作调度 | warp 级别 | **每线程** |
| CTA pair | 不支持 | **支持(2 CTA 共享)** |

### 10.3 关键发现(per arXiv 2512.02189)

- **tcgen05.mma 延迟恒定 ≈11 cycles**(与尺寸无关)
- **wgmma 延迟随尺寸线性 32-128 cycles**
- 密集/稀疏 GEMM 与训练负载性能提升:
  - B200 混合精度训练 ResNet-50:**1.85×**
  - GPT-1.3B:**1.55×**
  - 能效:**+32%**

### 10.4 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R22)
- **v1.1 完整版**:`TMEM` 模块 + tcgen05.mma + 256KB/SM + 512×128 lane + 16 TB/s 读带宽

---

## 11. v1.0 战略对齐

### 11.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| L6 层 NV ComputeUnit v1.0 | ✅ 黑盒 warp + wgmma + FP8/FP16/BF16/TF32 | ✅ §3 + §5 |
| L6 层 NV ComputeUnit v1.1 | + 白盒 + tcgen05 + TMEM + FP4/FP6/MX | ✅ §3 + §10 |
| L6 层 AMD ComputeUnit v1.0 | ✅ 黑盒 + wave64 + MFMA + split workgroup + HQD | ✅ §4 |
| L6 层 CppTLM ComputeUnitTLM 蓝图 | ✅ cu_template + cu_count | ✅ §1.3 |

### 11.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| NV Warp 调度 | ✅ 基础 4 schedulers | + TDT tree 完整 |
| NV wgmma | ✅ FP8/FP16/BF16/TF32 | + 全格式 |
| NV tcgen05 | ❌ | ✅ per arXiv 2512.02189 |
| NV TMA | ✅ 基础 | + 多播 + 描述符缓存 |
| NV mbarrier | ✅ 基础 | + store-and-arrive |
| NV DSMEM | ❌ | ✅ per US20250173152A1 |
| NV TMEM | ❌ | ✅ 256KB/SM |
| AMD SIMD32/64 | ✅ wave64(主)+ wave32(备) | + 完整 |
| AMD MFMA | ✅ 基础 | + 完整 |
| AMD split workgroup | ✅(per EP3785113B1) | + load-rating 优化 |
| AMD wavefront 重组 | ✅(per US9135077B2) | + 多内核调度器 |

### 11.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-02 | CU 黑盒/白盒(已 Superseded by ADR-SOC-06 D2) |
| ADR-SOC-06 D2/D5 | CU 蓝图 + 深度集成 PTX-EMU |
| 00-overview D7 / ADR-SOC-09 D2 | v1.0 NVIDIA+AMD dual vendor 蓝图 |

---

## 12. 配置 Schema

### 12.1 顶层 JSON Schema

```json
{
  "name": "compute_unit_0",
  "type": "ComputeUnitTLM",
  "params": {
    "vendor_mode": "nv_mode",          // "nv_mode" (默认) | "amd_mode"
    "cu_template": "compute_unit_template",
    "cu_count": 4,                     // 蓝图复制数
    
    // NVIDIA 参数
    "nv_sm_version": "sm_100",          // "sm_90" | "sm_100" | "sm_120"
    "nv_warp_size": 32,
    "nv_warp_schedulers": 4,
    "nv_register_file_kb": 256,
    "nv_shared_memory_kb": 99,         // SM_120: 99; SM_90: 228; SM_100 B200: 228
    "nv_tensor_core_gen": 4,            // 4 = wgmma; 5 = tcgen05 (v1.1)
    "nv_tma_enabled": true,             // TMA 异步张量搬运
    "nv_mbarrier_enabled": true,        // mbarrier 同步
    "nv_dsmen_enabled": false,          // v1.1
    "nv_tmem_enabled": false,           // v1.1
    
    // AMD 参数
    "amd_wave_size": 64,                // wave64
    "amd_simd_width": 64,
    "amd_lds_kb": 64,                   // LDS per CU
    "amd_mfma_enabled": true,
    "amd_split_workgroup": true,
    "amd_wavefront_reform": true,
    
    // 共享参数
    "kernel_launch_interval": 100,      // CU issue interval
    "max_outstanding_per_cu": 16
  }
}
```

### 12.2 连接(connection)示例

```json
{
  "connections": [
    {
      "src": "submit_queue_0.dispatch_to_core",
      "dst": "compute_cluster_0.cta_in"
    },
    {
      "src": "compute_unit_0.tensor_out",
      "dst": "memory_cluster_0.writeback"
    }
  ]
}
```

---

## 13. ADR/微架构/OpenSpec 引用矩阵

### 13.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-02 | CU 黑盒/白盒粒度(已 Superseded by ADR-SOC-06 D2) |
| ADR-SOC-03 | wavefront 抽象(已 Superseded by ADR-SOC-06 D2) |
| ADR-SOC-06 D2/D5 | CU 蓝图 + 深度集成 PTX-EMU |
| 00-overview D7 / ADR-SOC-09 D2 | v1.0 NVIDIA+AMD dual vendor 蓝图 |

### 13.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **ComputeUnitTLM** | [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md) |
| **CudaCoreAdapter** | [`docs/soc_arch/modules/cuda-core-adapter.md`](../modules/cuda-core-adapter.md) |
| **KernelLaunchTLM** | [`docs/soc_arch/modules/gpu-kernel-launch.md`](../modules/gpu-kernel-launch.md) |
| **SharedMemory** | [`docs/soc_arch/modules/gpu-shared-memory.md`](../modules/gpu-shared-memory.md) |
| **VectorRegFile** | (引用 `minimal_warp_scheduler_tlm.md` / `tensor_core_tlm.md` / `scoreboard_tlm.md`) |

### 13.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/SM/overview.md`](../../research/SM/overview.md) | NVIDIA Hopper→Blackwell SM 综述 |
| [`docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`](../../research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md) | Blackwell 官方简报 |
| [`docs/research/SM/CudaCore/US20230289211A1_线程组可扩展负载均衡_解析.md`](../../research/SM/CudaCore/US20230289211A1_线程组可扩展负载均衡_解析.md) | CWD/CGA 准入 |
| [`docs/research/SM/TMA/US20230289304A1_TMA多维数据异步访问_解析.md`](../../research/SM/TMA/US20230289304A1_TMA多维数据异步访问_解析.md) | TMAU 核心 |
| [`docs/research/SM/TMA/US12020035B2_TMA可编程多播_解析.md`](../../research/SM/TMA/US12020035B2_TMA可编程多播_解析.md) | TMA 多播 |
| [`docs/research/SM/TMA/US20230289242A1_mbarrier异步事务同步_解析.md`](../../research/SM/TMA/US20230289242A1_mbarrier异步事务同步_解析.md) | mbarrier SYNCS |
| [`docs/research/SM/TMA/US20250173152A1_分布式共享内存_解析.md`](../../research/SM/TMA/US20250173152A1_分布式共享内存_解析.md) | DSMEM |
| [`docs/research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md`](../../research/SM/Tensor/US20230289398A1_wgmma组矩阵乘加_解析.md) | wgmma |
| [`docs/research/SM/CudaCore/arXiv2512.02189_Blackwell微基准深入_解析.md`](../../research/SM/CudaCore/arXiv2512.02189_Blackwell微基准深入_解析.md) | TMEM/tcgen05 实测 |
| [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md) | NVIDIA WDU + AMD SPI/SQ |

---

## 14. 风险与缓解 R1-R6

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | Blackwell SM_120 消费级不兼容 wgmma,需 tcgen05 专用路径 | 🟡 中 | v1.0 MVP 默认数据中心 SM_100 + wgmma;消费级 v1.1 追加 tcgen05 |
| **R2** | TMEM 容量与带宽建模(256KB/SM, 16 TB/s 读) | 🟡 中 | v1.1 完整版基于 arXiv 2512.02189 实测建模 |
| **R3** | DSMEM SM-to-SM 延迟(180 clocks, 低于 L2 32%)建模 | 🟡 中 | v1.1 完整版基于 arXiv 2402.13499 实测 |
| **R4** | AMD wave32 vs wave64 混淆(CDNA 仅 wave64;RDNA 有 wave32) | 🟢 低 | per `docs/research/SM/overview.md` §六澄清;`00-overview` v3.0 §3.6 已区分 |
| **R5** | 双 vendor 蓝图共享的微架构差异(per-SM 256KB 寄存器 vs per-CU 256KB) | 🟢 低 | 寄存器大小一致;调度策略由 nv_mode/amd_mode 分支 |
| **R6** | 11 件 NVIDIA 专利 + 10 件 AMD 专利的实施工作量 | 🟡 中 | v1.0 MVP 仅实施核心 wgmma + MFMA + 基础 DSMEM;v1.1 完整版追加 |

---

## 15. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L6 SM/CU 子系统架构,基于 NVIDIA Hopper/Blackwell + AMD CDNA3 + CppTLM ComputeUnitTLM 蓝图 + Warp 调度 + 寄存器配置 + TMA/mbarrier/DSMEM/TMEM) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS