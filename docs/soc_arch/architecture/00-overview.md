# dGPU SoC v1.0 总架构蓝图 — 融合 NVIDIA Blackwell + AMD CDNA 3/3.5 视角

> **类别**: SoC Architecture > 总架构蓝图 (v1.0)
> **状态**: ✅ v3.0 PASS (Oracle 评审 PASS-WITH-CONDITIONS + N1/N2 修复完成,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)（已建立,作为本总架构蓝图及后续 01-10 子架构文档归口）
> **关联文档**:
> - 系统级设计 [`docs/soc_arch/specs/apu-soc-design.md`](../specs/apu-soc-design.md)（1056 行,Phase 7 APU 视角）
> - PCIe EP 微架构 [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md)（948 行,PCIe IP 视角）
> - 9 类 SimModule 拓扑 [`include/tlm/cluster/`](../../../include/tlm/cluster/)（CpuCluster / ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC / ApuSoC）
> - 研究综述 [`docs/research/SM/overview.md`](../../research/SM/overview.md)（NVIDIA Hopper→Blackwell SM 内部专利与微基准综述）
> - 分发段综述 [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md)（NVIDIA WDU + AMD SPI/SQ 对照）
> - gem5 标准库与 AMD MI300 dGPU 综述 [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md)
> - **本版本修复 Oracle 评审 2027-02-09 暴露的 P0/P1 问题**: A1 端口表 / A2 OpenSpec 引用 / A3 层数统一 / A4 范围矩阵 / B1 R7 23 ABI 状态 / B2 PDL 错误 / B3 硬件规格 / B4 MI300X 拓扑 / B5 ADR 决策编号

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 战略下的**总架构蓝图**,面向研发与评审读者,提供 7 层架构的全局导航与关键设计决策。

- 想快速理解系统结构 → 读 §1(战略目标) + §2(系统拓扑 L1..L7 ASCII 图)
- 想理解每一层细节 → 读 §3(架构层详解)
- 想理解关键决策与权衡 → 读 §4(D1..D8) + §4-bis(v1.0 / v1.1 范围矩阵,**权威范围表**)
- 想理解数据如何流动 → 读 §5(命令流 / 数据流 / 配置流 / 中断流 4 视图)
- 想理解与现有实现的关系 → 读 §6(兼容性分析)
- 想查阅 ADR 与模块微架构映射 → 读 §7(引用矩阵)
- 想评估风险 → 读 §8(R1..R7 风险)

**配套文档层级**(自上而下):

```
docs/soc_arch/
├── architecture/                  # ★ 本目录 — 总架构蓝图 + 子系统架构
│   ├── 00-overview.md             # 本文档(v1.0 总架构蓝图)
│   ├── 01-host-interface.md       # (规划中) PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM
│   ├── 02-command-processor.md    # (规划中) CommandProcessor + Pm4Decoder(Nvidia PM4 + AMD PM4 TYPE3)
│   ├── 03-task-management-unit.md # (规划中) TMU + TMD + 依赖预取 + PDL
│   ├── 04-work-distribution.md    # (规划中) WDU + SubmitQueue + Crossbar + CGA Cluster
│   ├── 05-sm-compute-unit.md      # (规划中) CU/SM + Warp调度 + 寄存器文件 + DSMEM/TMEM
│   ├── 06-tensor-core.md          # (规划中) wgmma(Hopper) / tcgen05(Blackwell) + 5th Tensor Core
│   ├── 07-memory-system.md        # (规划中) HBM3e + L1/L2 + Shared Memory + DSMEM/TMEM
│   ├── 08-noc-interconnect.md     # (规划中) Mesh NoC + Router + NIC + 跨 cluster 通信
│   ├── 09-coherence-protocol.md   # (规划中) MOESI/GPU + CoherenceDomain + Bridge
│   └── 10-completion-notify.md    # (规划中) Doorbell + CompletionRing + MSI-X
├── adr/                           # ADR-SOC-01..08 架构决策记录(本版本不修订;由 Metis 审查的独立 change 负责)
└── modules/                       # IP 微架构文档(25+ 份,每 IP 一份;由独立 change 修订)
```

---

## 1. 范围与目标

### 1.1 dGPU SoC v1.0 战略

**dGPU SoC v1.0** = 在 v0.5 MVP(ADR-SOC-06) + Phase 8 PCIe EP 整合(2027-02-09 `2027-02-09-cpptlm-dgpu-pcie-ip-integration`)基础上,完成向**同时支持 NVIDIA + AMD 双 driver stack**的 dGPU SoC 形态跃迁。

| 维度 | v0.5 MVP | v1.0 MVP | v1.1 完整版 |
|------|---------|----------|------------|
| **driver stack** | UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS(per ADR-SOC-06) | CUDA + ROCm/KFD 双栈(运行时 config 切换) | 同 v1.0 MVP |
| **PCIe EP** | PcieEndpointTLM (4 端口, s2 单体, deprecated per `[[deprecated]]` 标注 429327d) | PcieEndpointIP (17 ports = req_in[17] + resp_out[17],per `include/tlm/pcie/pcie_endpoint_ip.hh:52-53`;内部 stream_id 路由) | 同 v1.0 MVP |
| **GPGPU 端** | ComputeUnitTLM 黑盒 + KernelLaunchTLM | CUDA path ComputeUnit + ROCm path ComputeUnit(共享 CU 蓝图 + 复制模式 per ComputeCluster) | 同 v1.0 MVP + 白盒 warp 完整版 |
| **命令协议** | NVIDIA PM4 method packet(per ADR-SOC-06 D5) | NVIDIA PM4 method packet + AMD PM4 TYPE3 双解码(per Phase 4 子链路) | 同 v1.0 MVP |
| **CP** | CommandProcessor 5-state FSM | CommandProcessor 5-state FSM + 双解码子模块(NvidiaMethodDecoder + AmdPm4Decoder) | 同 v1.0 MVP |
| **TMU** | 简化(TmuDispatchProcessor + 反压停 fetch) | TMU(TMD prefetch 基础 per US11182207B2) | TMU + 嵌套流(per US9928109B2) |
| **WDU→SM** | SubmitQueue 单 SM 简化路由(per ADR-SOC-06 D5) | WDU + Crossbar Round-Robin 简化版 + SubmitQueue | WDU + Crossbar DDS(per US20240356866A1) + CGA Cluster(per US12333311B2) |
| **Tensor Core** | 仅占位 (ScoreboardTLM / PipelineTLM / TensorCoreTLM) | wgmma(Hopper 4 warps×32,per US20230289398A1) + 5th Tensor Core FP8/FP16/BF16 路径 | + tcgen05(per arXiv 2512.02189) + TMEM + FP4/FP6/MX |
| **Memory** | MemoryTLM + MemoryCluster 简化 | HBM3e 控制器 + L2 + L1 + Shared Memory(全 GPU) | + DSMEM(per US20250173152A1) + TMEM(per arXiv 2512.02189) |
| **Coherence** | Phase 7.C 部分(CoherenceDomain + bridge) | MOESI/GPU 6×6 状态机(per ADR-SOC-01)+ 跨域桥接基础 | 完整跨域桥接 + snoop filter 优化 |
| **v1.0 战略价值** | 单 driver stack | 同时支持 CUDA + ROCm 双 driver stack + 共享 PCIe EP/HBM/L2/NoC | 追加 vendor 专用加速特性 |

> **注**: §4-bis 提供权威 v1.0 MVP / v1.1 完整版范围矩阵(每特性精确到"是否实施 + 实施阶段"),所有 D1..D8 决策与 §1/§3/§8 范围描述均对齐该矩阵。

### 1.2 关键决策:融合 NVIDIA Blackwell + AMD CDNA 3/3.5

**融合策略**:共享 PCIe/IP + 共享 HBM/Memory + 共享 L2/NoC/Cluster,仅在 **CU/SM 内核级 + 前端命令协议**层做双 vendor 路径。

| 层级 | 共享/双 vendor | 决策依据 |
|------|---------------|---------|
| **L1 Host 接口** | 共享 | PcieEndpointIP(per `include/tlm/pcie/pcie_endpoint_ip.hh`)+ SdmaEngineTLM + HostBypassTLM 17 ports(req_in[17] + resp_out[17]) |
| **L2 命令流** | 共享 | PushBuffer + Ring Buffer + Doorbell + CompletionRing(per 23 ABI 不变) |
| **L3 命令协议** | **双 vendor** | NVIDIA PM4 method packet(per US10489056B2 队列管理器) + AMD PM4 TYPE3 opcode(per US8310492B2 HQD) |
| **L4 TMU/TMD** | 共享 | TMU(per US11182207B2 依赖预取) + TMD 描述符 + 嵌套流事件(per US9928109B2, v1.1) |
| **L5 WDU→SM** | **双 vendor** | NVIDIA WDU + Crossbar(per US20240356866A1)+ CGA Cluster(per US12333311B2) **vs** AMD SPI→SQ(per US10579388B2)+ split workgroup(per EP3785113B1) |
| **L6 SM/CU 内核** | **双 vendor** | NVIDIA: Warp + wgmma(per US20230289398A1)+ mbarrier(per US20230289242A1)+ TMA(per US20230289304A1)+ tcgen05/TMEM(v1.1) **vs** AMD: SIMD32/64 wavefront + split workgroup + load-rating(per EP3785113B1) |
| **L7 Memory/Coherence** | 共享 | HBM3e + L2 + L1 + Shared Memory + DSMEM(v1.1 per US20250173152A1)+ TMEM(v1.1 per arXiv 2512.02189)+ CoherenceDomain(MOESI/GPU per ADR-SOC-01) |

### 1.3 多层 SimModule 拓扑(per `include/tlm/cluster/`)

CppTLM 9 类 SimModule P2-P5 层级容器 + ApuSoC 顶层:

```
ApuSoC (顶层)
├── CpuCluster (CPU 侧 P3: 持有 CPUTLM/CacheTLM/MemoryTLM)
│   ├── CPUTLM × N
│   ├── CacheTLM(L1) × N
│   └── MemoryTLM (CPU 私有 DDR)
├── GpuCluster (GPU 顶层 P4: 持有 GpcCluster + 共享 L2 + HBM 控制器)
│   ├── GpcCluster × M (P4: 持有 TpcCluster × N)
│   │   └── TpcCluster × N (P3: 持有 ComputeCluster × K)
│   │       └── ComputeCluster × K (P2: CU 蓝图复制 cu_template + cu_count)
│   │           └── ComputeUnitTLM × J (CU: Warp + SM 核心,双 vendor 路径切换)
│   ├── L2 CacheTLM(共享) × 1
│   └── HBM3e MemoryCluster × Channels
├── PcieEndpointIP (P3: 17 ports req_in[17] + resp_out[17])
├── SdmaEngineTLM (P3: PCIe master)
├── HostBypassTLM (P3: 软件 bring-up)
├── PcieRootComplexTLM (P3: 自研 RC PF0-only,per Oracle M2)
├── CacheCluster (P3: L1×N + L2 聚合)
├── MemoryCluster (P3: 多通道 HBM/DDR 控制器)
├── GpuNoC (P4: Mesh NoC interconnect)
└── Crossbar (CoherentXBar / NonCoherentCrossbar)
```

### 1.4 命名约定澄清

- **CU**(Compute Unit)= 顶层术语,可指 AMD SIMD CU 或 NVIDIA SM 内的 CU 子结构
- **SM**(Streaming Multiprocessor)= NVIDIA 术语,单物理核(SM_90 / SM_100 / SM_120)
- **SE / SA / SQ**(Shader Engine / Shader Array / Sequencer)= AMD 术语,与 GPC/TPC 类似
- **CGA / Cluster**(Cooperative Grid Array)= Hopper/Blackwell 跨 SM 协作机制(per US12333311B2)
- **GPC / TPC**(Graphics Processing Cluster / Texture Processing Cluster)= NVIDIA 多核层级
- **PDL**(Programmatic Dependent Launch)= CUDA 11.8+ 设备端依赖启动,硬件披露见 US20230236878A1(WSDU pre-exit)
- **DDS Crossbar** = Dynamic Destination Selection Crossbar(per US20240356866A1)
- **CDP**(CUDA Dynamic Parallelism)= 设备端嵌套并行,CUDA 5.0+,硬件基础见 US20210349763A1(与 PDL 是不同概念,PDL 是 CDP 的子集)
- **stream_id** = PCIe Requester ID 中 function 部分,用于 VF 路由(避免 N×16 端口爆炸,per `include/tlm/pcie/pcie_endpoint_ip.hh`)

---

## 2. 系统拓扑(7 层架构 L1..L7)

### 2.1 整体形态对比(NVIDIA vs AMD vs CppTLM 仿真视角)

| 层级 | NVIDIA Blackwell B200/B300 | AMD CDNA 3 MI300X/MI325X | CppTLM 仿真视角 |
|------|--------------------------|-------------------------|----------------|
| **L1 Host 接口** | NVLink ConnectX-8 SuperNIC + PCIe Gen6 | PCIe Gen5 x16 + KFD | PcieEndpointIP(17 ports)|
| **L2 命令流** | PushBuffer + Doorbell | Ring Buffer + AQL Queue + Doorbell | PushBuffer/Ring 抽象(per 23 ABI 不变) |
| **L3 命令协议** | PM4 method packet(NVIDIA 私有) | PM4 TYPE3 opcode(AMD 公开) | 双 decoder 子模块 |
| **L4 TMU** | Grid Scheduler + WSDU | MEC/PFP/ME + HWS | TMU(per US11182207B2) |
| **L5 WDU** | WDU + Work Distribution Crossbar + GPC | SPI(Shader Processor Input)+ SQ | SubmitQueue + DDS Crossbar(v1.1 完整版) |
| **L6 SM/CU** | ~144-160 SM/GPUs(per B200 公开口径)/ SM_100 + TMEM + tcgen05 | 304 CU / 1216 SIMD / wave64 / 8 XCD chiplets | ComputeUnitTLM(蓝图复制 + 双 vendor)|
| **L7 Memory** | HBM3e 192-270GB(per Blackwell 简报)/ 7.7-8 TB/s | HBM3 192GB / 5.3 TB/s + Infinity Cache 256MB(L3 级) | MemoryCluster(HBM 控制器) |

> **注**: 严格说 NVIDIA Blackwell 是 1 颗 GPU + 2 个 die(per NV-HBI 10 TB/s),SM 数量级在 144-160 之间,具体数量官方简报未载;AMD MI300X 是 8 XCD chiplets × 38 CU = 304 CU,非"4 SE × 2 die"或"per die"。

### 2.2 7 层架构总览(L1..L7)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          dGPU SoC v1.0 7-Layer Architecture                │
│                                                                             │
│  Host CPU (x86/ARM) ──PCIe Gen5/6──> [L1 Host Interface]                    │
│                                       ├─ PcieEndpointIP (17 ports)         │
│                                       ├─ SdmaEngineTLM                       │
│                                       └─ HostBypassTLM (软件 bring-up)       │
│                                                  │                          │
│                                                  ▼                          │
│                                       [L2 Command Stream]                    │
│                                       ├─ PushBuffer (NVIDIA)                 │
│                                       ├─ Ring Buffer (AMD AQL)               │
│                                       ├─ Doorbell + CompletionRing           │
│                                       └─ MSI-X interrupt                     │
│                                                  │                          │
│                                                  ▼                          │
│                                       [L3 Command Protocol]                  │
│                                       ├─ CommandProcessor (5-state FSM)      │
│                                       ├─ Pm4Decoder-NV (NVIDIA method)       │
│                                       └─ Pm4Decoder-AMD (TYPE3 opcode)       │
│                                                  │                          │
│                                                  ▼                          │
│                                       [L4 TMU/TMD]                           │
│                                       ├─ TMU (依赖预取 + PDL)                 │
│                                       ├─ TMD Cache                           │
│                                       └─ Stream Event 嵌套流                  │
│                                                  │                          │
│                                                  ▼                          │
│                                       [L5 WDU → SM/CU 分发]                  │
│                                       ├─ WDU + SubmitQueue                    │
│                                       ├─ Crossbar (Round-Robin v1.0 / DDS v1.1)│
│                                       ├─ CGA Cluster (v1.1)                  │
│                                       └─ AMD SPI→SQ 分发路径 (v1.0+)         │
│                                                  │                          │
│                                                  ▼                          │
│                                       [L6 SM/CU 内核]                        │
│                          ┌────────────────────────────────┐                 │
│                          │ NVIDIA 路径 (v1.0):              │                 │
│                          │   Warp × 4 (32 threads)         │                 │
│                          │   + wgmma (Hopper)              │                 │
│                          │   + TMA 异步张量搬运             │                 │
│                          │   + mbarrier 同步                │                 │
│                          │   + FP8/FP16/BF16/TF32           │                 │
│                          │ NVIDIA 路径 (v1.1 +):           │                 │
│                          │   + tcgen05 + TMEM              │                 │
│                          │   + FP4/FP6/MX                  │                 │
│                          │   + DSMEM (cluster)             │                 │
│                          ├────────────────────────────────┤                 │
│                          │ AMD 路径 (v1.0+):                │                 │
│                          │   SIMD32/64 wavefront            │                 │
│                          │   + split workgroup              │                 │
│                          │   + load-rating 反馈             │                 │
│                          │   + HQD 两级仲裁                 │                 │
│                          │   + Wavefront 重组               │                 │
│                          └────────────────────────────────┘                 │
│                                                  │                          │
│                                                  ▼                          │
│                                       [L7 Memory Hierarchy]                  │
│                                       ├─ HBM3e Controller (192-270GB)       │
│                                       ├─ L2 共享 Cache (per GPU)             │
│                                       ├─ L1 Cache + Shared Memory             │
│                                       ├─ DSMEM 分布式共享 (v1.1, 跨 SM)      │
│                                       ├─ TMEM 张量内存 (v1.1, Blackwell)    │
│                                       └─ Coherence (MOESI/GPU + Bridge)      │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.3 数据流视图(命令 / 数据 / 配置 / 中断)

```
命令流 (Command Stream):
   Host CPU → PushBuffer/Ring Buffer → CommandProcessor → TMU → WDU → SM/CU

数据流 (Data Stream):
   Host ↔ PcieEndpointIP ↔ MemoryCluster (HBM3e) ↔ L2 ↔ L1/SMEM ↔ SM/CU 内核

配置流 (Configuration Stream):
   JSON config → ModuleFactory → SimModule 拓扑 → 各模块 on_config_loaded()
   + Driver probe (BAR/MSI-X enumeration) + Runtime params (workgroup size 等)

中断流 (Interrupt Stream):
   SM/CU 完成 → CompletionRing → MSI-X → PcieEndpointIP → Host CPU
   + Error / Fault (IOMMU fault → MSI-X interrupt)
```

---

## 3. 架构层详解(7 层 L1..L7)

### 3.1 L1 Host 接口层 — PcieEndpointIP + SdmaEngineTLM + HostBypassTLM

**L1.1 PcieEndpointIP 端口(真实定义 per `include/tlm/pcie/pcie_endpoint_ip.hh`)**:

| 端口类型 | 数量 | 类型 | 用途 |
|---------|-----|------|------|
| `req_in[NUM_PORTS]` | 17 | `cpptlm::InputStreamAdapter<bundles::PcieTlpBundle>` | PCIe TLP 请求入口(per function:PF=port[0],VF0..VF15=port[1..16]) |
| `resp_out[NUM_PORTS]` | 17 | `cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle>` | PCIe TLP 响应出口(per function 同上) |

**关键设计**:
- **NUM_PORTS = 17**:1 PF + 16 VF,避免 N×16 端口爆炸
- **stream_id 路由**:内部用 `stream_id`(PCIe Requester ID 的 function 部分)区分 VF,避免端口按 VF 数量级展开(per `docs/architecture/14` §3 端口图)
- **MSI-X / Config Space / BAR 内部组件**:非端口,通过 `pool_.config_of(stream_id)` / `pool_.msix_of(stream_id)` 等内部方法访问(per `pcie_endpoint_ip.hh:85-89`)

**L1.2 内部组件分层**(per `docs/architecture/14` §1.2):
- **链路层** `PcieLinkLayerTLM` — FC token bucket + DLLP 处理(per `2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc`)
- **PHY 数字控制**`PciePhyDigitalCtrlTLM` — LTSSM 11 态(per `2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl`)
- **SR-IOV VF Pool** `PcieSriovVfPoolTLM` — 1 PF + 16 VF(per `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool`)
- **Bypass Mux** `PcieBypassMux` — 3 态(Full / Bypass / Partial,per `2026-10-06`)
- **AXI Stream Adapter** `Axi4StreamAdapter` — AXI ↔ TLP 边界(per `2026-11-03-cpptlm-dgpu-axi-stream-adapter`)
- **AXI4Mapper** `Axi4Mapper` — AXI4 OOO + rid 关联(per `2026-12-22-cpptlm-dgpu-axi4-mapper`)
- **Host Bypass** `HostBypassTLM` — 软件 bring-up 跳过 RC BFM(per `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc`)
- **Root Complex** `PcieRootComplexTLM` — 自研 RC,枚举 PF0-only(per `2027-01-19` + Oracle M2)

**L1.3 SdmaEngineTLM**(per ADR-SOC-07 D3):
- PCIe master 端,H2D/D2H 数据搬运
- IOMMU fault 错误路径(per ADR-SOC-08)
- BAR0 doorbell + 23 ABI 通道(per `test_sdma_engine_*.cc`)

### 3.2 L2 命令流层 — PushBuffer / Ring Buffer / Doorbell

**L2.1 PushBuffer(NVIDIA 风格)**:
- 主机应用把命令数据结构的指针写入 pushbuffer(per US9489763B2)
- GPU 异步读取 pushbuffer 并执行
- 与 AMD Ring Buffer 同源但协议不同

**L2.2 Ring Buffer / AQL Queue(AMD 风格)**:
- AMD 驱动 + KFD 使用 ring buffer / AQL queue
- 与 PushBuffer 等价但格式不同

**L2.3 Doorbell**(强序 write):
- 主机通过 doorbell ring 通知 GPU 新任务到达
- **PCIe 保序 write**(per docs/research/PCIe/PCIe_上的保序write.md)
- MMU ordering pipe + dummy non-posted read flush
- Gen5 x16 latency 周期精度仅 PCIe 范围(per docs-archived/adr/ADR-X.16 §周期精度注记)

**L2.4 CompletionRing**(完成环):
- 任务完成后向 ring 写 completion entry
- 主机读 ring 确认完成(per completion-ring.md)
- Doorbell 强序 + CQ 保序(per NVIDIA Completion Queue + Doorbell 强序)

**L2.5 MSI-X 中断**:
- GPU → Host 中断投递
- mask/unmask/PBA 状态机(per ADR-SOC-08 P1)

### 3.3 L3 命令协议层 — 双 vendor 解码

**L3.1 NVIDIA PM4 method packet**(per Pm4Decoder-NV):
- 4 method_addr ranges: DISPATCH_DIRECT / EVENT_WRITE / RELEASE_MEM / ACQUIRE_MEM(per ADR-SOC-06 D5)
- 18 opcodes MVP 简化(per ADR-SOC-06 D5)

**L3.2 AMD PM4 TYPE3 opcode**(per Pm4Decoder-AMD):
- AMD HQD 硬件队列调度(per US8310492B2)
- IB(Indirect Buffer)prefetch(per US20220091847A1)
- 用户态图形处理分发(per US9176795B2)
- 自适应线程组分发(per US11822956B2)

**L3.3 共享 CommandProcessor 5-state FSM**(IDLE → FETCH → DECODE → DISPATCH → COMPLETE)
- 共用于 NVIDIA / AMD 双路径
- D5 决策(per ADR-SOC-06)

### 3.4 L4 TMU/TMD 层

**L4.1 TMU**(Task Management Unit, per docs/research/TMU/overview.md):
- 三种形态:
  - Task/Work Unit 207 = TMU 300 + WDU 340(per US20130198760A1)
  - 独立 TMU 单元 215/234(per US9535815B2 / US11182207B2)
  - Work Scheduler/Distribution Unit 230 (WSDU, Hopper 合并, per **US20230236878A1**)

**L4.2 TMD**(Task Metadata Descriptor, per docs/research/TMU/TMD.md):
- 完整字段布局:Init / Sched / Exec / QueueState / HW-only / Dep / Queue 区
- Dependent TMD enable flag + Dependent TMD pointer(per US20130198760A1)
- Consumer Flags(per US11182207B2)用于依赖任务预取
- 优先级分层 Scheduler Table

**L4.3 依赖预取**(v1.0 MVP):
- TMU 内 Dependency/Prefetch Unit 410(per US11182207B2)
- 在生产者执行期间并行预取消费者描述符
- 延迟可削减 50%

**L4.4 PDL**(Programmatic Dependent Launch, v1.1):
- 设备端依赖启动,CUDA 11.8+ 起 Hopper/Blackwell 支持
- 硬件基础:**US20230236878A1**(WSDU/pre-exit, Hopper 2022 申请,2023 公开)
- 子任务直接提交给 TMU,绕过 pushbuffer/front end/CPU
- CppTLM v1.0 MVP 不实施,推迟至 v1.1 完整版

**L4.5 嵌套流事件**(v1.1):
- Wait Event / Signal Event(per US9928109B2)
- 跨 stream 依赖无 CPU 介入

> **PDL 与 CDP 的区分**:CDP(CUDA Dynamic Parallelism)= CUDA 5.0+ 设备端嵌套并行,硬件基础 US20210349763A1;PDL(Programmatic Dependent Launch)= CUDA 11.8+ 更精细的依赖启动,硬件基础 US20230236878A1。PDL 是 CDP 的演进(更细粒度依赖),不可混用专利号。

### 3.5 L5 WDU → SM/CU 分发层

**L5.1 NVIDIA 路径**(per docs/research/WDUtoSM/overview.md §三):

```
TMU 234 → WDU 236 → Work Distribution Crossbar 330 → GPC 208 (Pipeline Manager 305) → SM 310
```

- **DDS Crossbar**(US20240356866A1, **v1.1**):super destination group + 二维仲裁 + 逐周期动态目的选择
- **CGA Cluster**(US12333311B2, **v1.1**):thread block cluster 跨 SM 协作 + all-or-none 并发
- **两级 WD**(GPU2GPC/GPC2SM):speculative launch + 13 号 US9594599B1 比例分发
- **v1.0 MVP 简化**:Round-Robin crossbar(放弃 DDS 动态目的选择),推迟到 v1.1 完整版

**L5.2 AMD 路径**(per docs/research/WDUtoSM/overview.md §四, **v1.0+**):

```
CP → SPI (Shader Processor Input 202) → SQ → CU / SIMD wavefront
```

- **SPI 两级仲裁**(per US10579388B2):HQD 内队列级 + HQD 间 pipe 级
- **split workgroup**(per EP3785113B1):workgroup 拆分 + load-rating 反馈
- **Wavefront 重组**(per US9135077B2):阈值判定 + fill/steal/share 原子重分配
- **多内核波前调度器**(per EP3803583B1):双阈值迟滞节流

**L5.3 SubmitQueue**(CppTLM 实现, per submit-queue.md):
- WDU 分发网络(per ADR-SOC-06 D5)
- per-cluster pending FIFO(32 slot) + per-core active(4 slot)
- CppTLM 简化:单 SM 路由 + 反压停 fetch

### 3.6 L6 SM/CU 内核层(双 vendor 双路径)

**L6.1 NVIDIA 路径 — SM_100 (Blackwell) + SM_90 (Hopper)**:

| 特性 | Hopper SM_90 | Blackwell SM_100/120 | CppTLM 实施阶段 |
|------|--------------|----------------------|----------------|
| 张量核心 | 4 代(per US20230289398A1 wgmma) | 5 代(tcgen05 + TMEM, per arXiv 2512.02189) | v1.0: wgmma / v1.1: tcgen05 |
| TMEM | N/A | 256KB/SM, 512 列×128 lane, 16 TB/s 读(per arXiv 2512.02189) | v1.1 |
| TMA | US20230289304A1 | 同 Hopper + 描述符缓存 US20240176663A1 | v1.0 |
| mbarrier | US20230289242A1 SYNCS | 同 Hopper | v1.0 |
| DSMEM | US20250173152A1(180 clocks SM-to-SM) | 同 Hopper + CTA pair 共享 | v1.1 |
| FP 格式 | FP8/FP16/BF16/TF32/FP64 | + FP4/FP6/MX(OCP microscaling) | v1.0: FP8/16/BF16/TF32 / v1.1: +FP4/FP6/MX |
| 寄存器文件 | 256KB/SM | 256KB/SM(消费级 SM_120 保持) | v1.0(简化) |
| Warp 调度 | 4 warp schedulers | 4 warp schedulers + per-thread 调度(tcgen05 取消 warp 同步) | v1.0(简化) |
| 共享内存 | 228KB/SM | 99KB/SM(消费级 GB203) / 228KB/SM(数据中心 B200) | v1.0(简化) |

**L6.2 AMD 路径 — CDNA 3 MI300X / MI325X**(per AMD 官方 + WDUtoSM overview):

| 特性 | CDNA 3 MI300X | CppTLM 实施阶段 |
|------|---------------|----------------|
| CU 数 | **8 XCD chiplets × 38 CU = 304 CU**(全卡总数,非 per die) | v1.0 |
| SIMD | SIMD32/64 per CU | v1.0 |
| Wavefront | **wave64(CDNA 特性,非 wave32;wave32 是 RDNA 特性)** | v1.0 |
| Tensor Core | Matrix Core(MFMA) | v1.0(简化)|
| 数据流 | MFMA + LDS + Split workgroup | v1.0 |
| 共享内存 | LDS(Local Data Share)64KB per CU | v1.0 |
| 寄存器文件 | 256KB per CU(典型) | v1.0 |
| HQD | 硬件队列描述符(per US8310492B2) | v1.0 |
| IB | 间接缓冲(per US20210304349A1) | v1.0 |
| KFD IOCTL | 用户态驱动(per US9176795B2) | v1.0 |

**L6.3 CppTLM ComputeUnitTLM 蓝图**(per `include/tlm/gpu/compute_unit_tlm.hh`):
- `cu_template` + `cu_count`(per `ComputeCluster`, per ADR-SOC-06)
- 黑盒 MVP 路径(v0.5)+ 白盒 warp 级(v0.5 完整版)
- 双 vendor 路径作为可选模式(per JSON config param `nv_mode` / `amd_mode`)

### 3.7 L7 Memory Hierarchy + Coherence

**L7.1 HBM3e Controller**:
- NVIDIA Blackwell: 192-270GB per GPU, 7.7-8 TB/s(per Blackwell 简报)
- AMD MI300X: 192GB HBM3, 5.3 TB/s
- CppTLM 仿真:`MemoryCluster` 多通道 + `MemoryTLM` DRAM 模型

**L7.2 L2 Cache 共享**:
- NVIDIA B200: ~60MB L2(per die,公开口径) + 跨 die 共享(per NV-HBI)
- AMD MI300X: ~8MB L2(per XCD)+ Infinity Cache 256MB(L3 级,全卡共享)
- CppTLM 仿真:`CacheCluster` L2 聚合

**L7.3 L1 + Shared Memory**:
- NVIDIA: L1/SMEM 统一缓存(228KB Hopper / 99KB Blackwell 消费)
- AMD: LDS(Local Data Share)64KB per CU
- CppTLM 仿真:`CacheTLM` L1 + `SharedMemoryTLM`

**L7.4 DSMEM**(Hopper/Blackwell 独有, per US20250173152A1, **v1.1**):
- GPC 内 cluster SM 互访
- 4GB/256×16MB CTAID 分段地址窗口
- SM-to-SM 延迟 180 clocks(低于 L2 32%,per arXiv 2402.13499)
- CppTLM v1.0 MVP 不实施,推迟至 v1.1 完整版

**L7.5 TMEM**(Blackwell 独有, per arXiv 2512.02189, **v1.1**):
- 256KB/SM, 512×128 lane, 16 TB/s 读
- 64×64 最优 tile
- CppTLM v1.0 MVP 不实施,推迟至 v1.1 完整版

**L7.6 Coherence**:
- MOESI/GPU 6 状态 × 6 事件(per ADR-SOC-01)
- `CoherenceDomain` + `CoherenceBridge` + `SnoopFilter`(per modules 目录)
- CPU↔GPU coherence(per Phase 7.C,部分实施)
- CppTLM 仿真:`coherence-protocol.md` + `coherence-domain.md` + `coherence-bridge.md` + `snoop_filter.md`

---

## 4. 关键设计决策 D1..D8

### D1. 融合 NVIDIA + AMD 双 driver stack 战略 ✅

**决策**:dGPU SoC v1.0 同时支持 CUDA + ROCm 双 driver stack,通过共享 PcieEndpointIP/HBM/L2/NoC + 双 vendor 内核(SM_90/100 vs CDNA 3)实现。

**理由**:
- **生态价值**:同时服务 AI/HPC(NVIDIA)+ 部分 AMD 客户(ROCm 已商业化)
- **复用价值**:PcieEndpointIP/HBM 共享 + 23 ABI 不变
- **风险可控**:双 vendor 内核隔离,任一路径失败不影响另一路径

**备选**:
- ❌ 单 vendor 路径(NVIDIA 独占):失去 AMD 生态 + 客户
- ❌ 通用 GPGPU 内核:失去 vendor 优化性能

### D2. CU 蓝图复制策略 ✅

**决策**:ComputeUnitTLM 采用蓝图模式 `cu_template` + `cu_count`,所有 CUDA 与 ROCm 路径共享同一份 CU 代码,运行时通过 `nv_mode` / `amd_mode` config param 切换。

**理由**:
- **代码复用**:单一份 CU 实现,降低维护成本
- **配置灵活**:JSON config 决定模式,无需代码分支
- **测试覆盖**:共享测试框架

**备选**:
- ❌ 双份 CU 模板:代码冗余
- ❌ 通用 GPGPU 指令集:失去 vendor 优化

### D3. PCIe EP 17 ports(req_in[17] + resp_out[17])架构 ✅

**决策**:dGPU SoC v1.0 采用 PcieEndpointIP(17 ports per `include/tlm/pcie/pcie_endpoint_ip.hh:52-53`)替代 PcieEndpointTLM(4 端口, deprecated per `[[deprecated]]` 标注 429327d)。

**理由**:
- SR-IOV 支持(per `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool`)
- 23 ABI 头冻结(per ADR-088 §D5)+ 仓内实现 19/19 函数 + 4 回调 typedef 契约(per `src/abi/cpptlm_emulator.cc` 433 行);UsrLinuxEmu 集成未闭环(per R5)
- 已 Phase 8 整合交付完成(per `2027-02-09-cpptlm-dgpu-pcie-ip-integration` HEAD 429327d)

**备选**:
- ❌ 沿用 PcieEndpointTLM 4 端口:不支持 SR-IOV,性能不足

### D4. Memory Hierarchy 分层(HBM3e + L2 + L1 + DSMEM/TMEM)✅

**决策**:7 层 memory hierarchy 共享 vendor 路径,DSMEM/TMEM 为 NVIDIA 独有,通过 v1.0/v1.1 分阶段实施。

**理由**:
- **共享价值**:HBM/L2/L1 共享设计,减少重复
- **NVIDIA 扩展**:DSMEM/TMEM 通过可选项注入(per JSON config `enable_dsmem: true`)

**备选**:
- ❌ 全 vendor 隔离:失去共享价值
- ❌ 无 DSMEM/TMEM:失去 Hopper/Blackwell 关键优势

### D5. TMU/TMD 共享 + 依赖预取 + PDL 推迟 ✅

**决策**:TMU/TMD 架构共享(per US11182207B2 依赖预取);PDL(per **US20230236878A1**, WSDU/pre-exit, Hopper 2022)+ 嵌套流事件(per US9928109B2)推迟到 v1.1 完整版。

**理由**:
- **MVP 简化**:v1.0 MVP 仅实施 TMD prefetch 基础
- **完整版推迟**:PDL 与嵌套流事件为 v1.1(per §4-bis 范围矩阵)

**备选**:
- ❌ 全 PDL 实施:v1.0 时间预算不足

### D6. WDU + Crossbar(Round-Robin v1.0 / DDS v1.1)+ CGA Cluster(v1.1)✅

**决策**:v1.0 MVP 实施 SubmitQueue + WDU + Round-Robin crossbar;v1.1 完整版追加 DDS(per US20240356866A1)+ CGA Cluster(per US12333311B2)。AMD SPI→SQ 路径 v1.0 即完整实施。

**理由**:
- **NVIDIA 路径 v1.0 MVP 简化**:Round-Robin 已足够验证功能;DDS/CGA Cluster 为 v1.1 性能优化
- **AMD 路径 v1.0 完整**:ROCm 路径作为备选,通过 config 切换
- **双 path 共享**:SubmitQueue 作为分发网络,双 vendor 共用

**备选**:
- ❌ 仅 NVIDIA 路径:失去 ROCm
- ❌ 仅 AMD 路径:失去 CUDA 主流
- ❌ v1.0 完整 DDS:DDS 实施复杂度高,推迟到 v1.1

### D7. Tensor Core 渐进实施(wgmma v1.0 → tcgen05 v1.1)✅

**决策**:v1.0 MVP 实施 wgmma(Hopper 4 warp×32,per US20230289398A1)+ 简化 TensorCoreTLM(FP8/FP16/BF16/TF32);v1.1 完整版追加 tcgen05(per arXiv 2512.02189)+ TMEM + FP4/FP6/MX。

**理由**:
- **MVP 简化**:wgmma 已覆盖 Hopper 主流用例
- **完整版追加**:tcgen05 + TMEM 为 Blackwell 专用,推迟到 v1.1

**备选**:
- ❌ 直接 tcgen05:跳过 Hopper 测试覆盖

### D8. Coherence 完整实施(MOESI/GPU + 跨域桥接)✅

**决策**:v1.0 实施 MOESI/GPU 6 状态 × 6 事件 + CoherenceDomain + Bridge 基础 + SnoopFilter 基础(per ADR-SOC-01 Phase 7.C);v1.1 完整版追加完整跨域桥接 + snoop filter 优化。

**理由**:
- **CPU↔GPU coherence**:APU + dGPU 双形态必需
- **跨域桥接**:CPU coherence ↔ GPU coherence(v1.0 基础,v1.1 完整)

**备选**:
- ❌ 仅 GPU 端 coherence:CPU↔GPU 失 coherence,违反 ADR-SOC-01

---

## 4-bis. v1.0 MVP / v1.1 完整版 范围矩阵(权威)

> 本节是**唯一权威范围表**,§1.1 / §3 / D1..D8 / §8 风险缓解均对齐本表。

| # | 特性 | v1.0 MVP | v1.1 完整版 | 关联决策 / 引用 |
|---|------|---------|------------|----------------|
| **R01** | driver stack | CUDA + ROCm/KFD 双栈 | 同 v1.0 | D1 |
| **R02** | PcieEndpointIP 17 ports | ✅ req_in[17] + resp_out[17] | 同 v1.0 | D3 |
| **R03** | PcieEndpointTLM 4 端口 | deprecated(per 429327d `[[deprecated]]`) | 维持 deprecated | D3 |
| **R04** | ComputeUnitTLM 蓝图模式 | ✅ cu_template + cu_count | 同 v1.0 | D2 |
| **R05** | NV ComputeUnit 路径 | ✅ 黑盒 warp 级 + wgmma + FP8/FP16/BF16/TF32 | + 白盒 warp 级 + tcgen05 + TMEM + FP4/FP6/MX | D7 |
| **R06** | AMD ComputeUnit 路径 | ✅ 黑盒 + wave64 + MFMA + split workgroup + HQD | + 白盒完整 | L6.2 |
| **R07** | CommandProcessor 5-state FSM | ✅ 共享 | 同 v1.0 | D5 |
| **R08** | Pm4Decoder-NV | ✅ 4 method_addr + 18 opcodes 简化 | + 全 18 opcodes | D5 |
| **R09** | Pm4Decoder-AMD | ✅ TYPE3 opcode 基础 | + 全 TYPE3 opcode | D5 |
| **R10** | TMU 基础(TMD 调度) | ✅ 优先级分层 + TMD prefetch(US11182207B2) | 同 v1.0 | D5 |
| **R11** | TMU 嵌套流事件 | ❌ 推迟 | ✅ US9928109B2 嵌套流 | D5 |
| **R12** | TMU PDL | ❌ 推迟 | ✅ US20230236878A1 WSDU/pre-exit | D5 |
| **R13** | WDU 基础 | ✅ SubmitQueue + WDU | 同 v1.0 | D6 |
| **R14** | Crossbar Round-Robin | ✅ v1.0 MVP | 同 v1.0 | D6 |
| **R15** | Crossbar DDS 动态目的 | ❌ 推迟 | ✅ US20240356866A1 | D6 |
| **R16** | CGA Cluster | ❌ 推迟 | ✅ US12333311B2 all-or-none | D6 |
| **R17** | AMD SPI→SQ | ✅ 完整路径(US10579388B2 + EP3785113B1 + US9135077B2 + EP3803583B1) | 同 v1.0 | D6 |
| **R18** | HBM3e Controller | ✅ 简化(192-270GB NVIDIA / 192GB AMD) | + 多通道 HBM + RAS | D4 |
| **R19** | L2 Cache | ✅ 共享 + 跨域桥接基础 | + 优化 + snoop filter | D4 / D8 |
| **R20** | L1 + Shared Memory | ✅ 基础 | + 优化 | D4 |
| **R21** | DSMEM | ❌ 推迟 | ✅ US20250173152A1 | D4 |
| **R22** | TMEM | ❌ 推迟 | ✅ arXiv 2512.02189(256KB/SM)| D4 / D7 |
| **R23** | Coherence MOESI/GPU | ✅ 6 状态 × 6 事件(per ADR-SOC-01)| 同 v1.0 | D8 |
| **R24** | Coherence 跨域桥接 | ✅ 基础 | ✅ 完整 + snoop filter 优化 | D8 |
| **R25** | CompletionRing + MSI-X | ✅ 基础(mask/unmask/PBA per ADR-SOC-08 P1)| 同 v1.0 | L2 |
| **R26** | 23 ABI 头冻结 | ✅ | ✅ | ADR-SOC-07 D5 + ADR-088 §D5 |
| **R27** | 23 ABI 函数实现 | ✅ **仓内 19/19 函数全部实现** + 4 回调 typedef 契约(`include/abi/cpptlm_emulator.h` 19 声明 + `src/abi/cpptlm_emulator.cc` 433 行 19 定义);UsrLinuxEmu 侧 4 回调注册与端到端集成未闭环 | ⚠️ 取决于 UsrLinuxEmu ADR-089 v0.5 节奏 | R5 |

---

## 5. 数据流图(4 视图)

### 5.1 命令流(Command Stream)

```
Host CPU
  │ (用户态 driver ioctl,CUDA 或 ROCm)
  ▼
Kernel Driver (CUDA / ROCm)
  │ (写入 PushBuffer / Ring Buffer)
  ▼
PcieEndpointIP.req_in[stream_id] (PCIe TLP / Posted Write,经 stream_id 路由)
  │ (TLP 解码)
  ▼
CommandProcessor.fetch
  │ (5-state FSM: IDLE → FETCH)
  ▼
Pm4Decoder-NV.decode / Pm4Decoder-AMD.decode
  │ (PM4 method packet / TYPE3 opcode)
  ▼
CommandProcessor.dispatch
  │ (5-state FSM: DECODE → DISPATCH)
  ▼
TMU.fetch
  │ (TMD 描述符预取,per US11182207B2)
  ▼
TMU.schedule (Scheduler Table)
  │ (优先级分层)
  ▼
WDU.distribute
  │ (v1.0: Round-Robin / v1.1: Work Distribution Crossbar per US20240356866A1)
  │ (AMD 路径: SPI→SQ per US10579388B2)
  ▼
CGA Cluster all-or-none admission(v1.1) / HQD 两级仲裁(AMD)
  │
  ▼
SubmitQueue.enqueue + dispatch_to_core
  │
  ▼
CU/SM.on_cta_arrival
  │ (5-state FSM: DISPATCH → COMPLETE)
  ▼
Per-tick execution + WarpState 镜像
  │
  ▼
on_warp_complete → SQ.on_warp_complete → TMU.on_complete
  │
  ▼
CQ.push → host_notify
```

### 5.2 数据流(Data Stream)

```
Host CPU Memory
  │ (H2D via DMA)
  ▼
PcieEndpointIP.req_in[stream_id] (per AXI4Mapper + Axi4StreamAdapter)
  │ (AXI4 write to HBM)
  ▼
MemoryCluster (HBM3e)
  │
  ▼
L2 Cache Cluster (shared,per Crossbar)
  │ (CoherentXBar routing)
  ▼
L1 Cache + Shared Memory (per SM)
  │
  ▼
SM/CU register file + TMEM(v1.1)
  │
  ▼
Tensor Core / FMA / SIMD
  │
  ▼
L1 → L2 → HBM3e (回写)
  │
  ▼
PcieEndpointIP.resp_out[stream_id] → Host (D2H via DMA)
```

### 5.3 配置流(Configuration Stream)

```
JSON config (apu_soc_v1.json / dgpu_soc_with_pcie_ip.json)
  │
  ▼
ModuleFactory.instantiateAll
  │ (validate topology + instantiate SimModule)
  ▼
SimModule.on_config_loaded
  │ (各模块解析 config)
  ▼
Driver probe (BAR/MSI-X enumeration,经 PcieRootComplexTLM PF0-only)
  │ (per `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc`)
  ▼
Runtime params (workgroup size / cluster size / etc.)
  │ (via PcieEndpointIP req_in[stream_id] config slave)
  ▼
Module runtime state ready
```

### 5.4 中断流(Interrupt Stream)

```
SM/CU 完成 (or error / fault)
  │
  ▼
CompletionRing.push
  │
  ▼
MSI-X interrupt (per ADR-SOC-08 P1 mask/unmask/PBA)
  │
  ▼
PcieEndpointIP.resp_out[stream_id] (MSI-X 中断 TLP 投递)
  │
  ▼
Host CPU interrupt handler
  │
  ▼
Kernel Driver 处理 + 用户态通知
```

---

## 6. 与现有架构的兼容性

### 6.1 复用 v0.5 / Phase 8 已实施模块

| 模块 | v0.5/Phase 8 状态 | v1.0 复用方式 |
|------|------------------|----------------|
| `PcieEndpointIP` (17 ports) | ✅ Phase 4-8 完整 | 直接复用 |
| `SdmaEngineTLM` | ✅ 已实施 | 直接复用 |
| `HostBypassTLM` | ✅ Phase 7 | 直接复用 |
| `PcieRootComplexTLM` | ✅ Phase 7 (PF0-only) | 直接复用 |
| `Axi4StreamAdapter` | ✅ Phase 5 | 直接复用 |
| `Axi4Mapper` | ✅ Phase 6 | 直接复用 |
| `ComputeUnitTLM` (黑盒 MVP) | ✅ v0.5 | 升级为蓝图模式 + 双 vendor |
| `KernelLaunchTLM` | ✅ v0.5 | 直接复用 |
| `DGpuBoardTLM` shell | ✅ s2 单体 (deprecated) | 升级为 D6 v1.0 shell + 双 vendor 路径分发 |
| `CommandProcessor` 5-state FSM | ✅ v0.5 | 直接复用 |
| `Pm4Decoder` (NVIDIA method packet) | ✅ v0.5 | 升级为 Pm4Decoder-NV + Pm4Decoder-AMD 双 decoder |
| `TmuDispatchProcessor` (简化 TMU) | ✅ v0.5 | 升级为 TMU(per US11182207B2) |
| `SubmitQueue` (单 SM 路由) | ✅ v0.5 | 升级为 SubmitQueue + WDU + DDS Crossbar |
| `CudaCoreAdapter` (深度集成 PTX-EMU) | ✅ v0.5 | 直接复用 + 扩展 wgmma/tcgen05 |
| `PtxEmuSubmoduleMVP` | ✅ v0.5 | 直接复用 |
| `MemoryCluster` (HBM 简化) | ✅ v0.5 | 升级为 HBM3e 多通道 + L2 + DSMEM/TMEM |
| `CacheCluster` | ✅ 已实施 | 升级为 L1×N + L2 聚合 + SharedMemory |
| `MemoryTLM` | ✅ 已实施 | 直接复用 |
| `CoherenceDomain` + Bridge | ✅ Phase 7.C 部分 | 升级为完整 MOESI/GPU + 跨域桥接 |
| `Crossbar` (CoherentXBar) | ✅ 已实施 | 直接复用 + 升级 DDS 动态目的选择(v1.1) |
| `GpuNoC` (Mesh) | ✅ 已实施 | 直接复用 + 升级 cluster SM-to-SM 通信(v1.1) |
| 9 类 SimModule 容器 (P2-P5) | ✅ ApuSoC | 直接复用 |

### 6.2 扩展点(不兼容 v0.5)

| 项 | v0.5 | v1.0 | 解决方式 |
|----|------|------|---------|
| **CU 双 vendor 路径** | 单 NVIDIA 路径 | NVIDIA + AMD 双路径 | 蓝图模式 + config 切换 |
| **ComputeUnitTLM 模板** | 直接 new | 蓝图 + cu_count 复制 | `ComputeCluster` 蓝图模式(per ADR-SOC-06) |
| **Pm4Decoder** | NVIDIA only | NVIDIA + AMD 双 decoder | 拆分 Pm4Decoder-NV + Pm4Decoder-AMD |
| **Tensor Core** | 占位 ScoreboardTLM/PipelineTLM | wgmma(Hopper v1.0) + tcgen05(TMEM v1.1) | 扩展 TensorCoreTLM |
| **Memory Hierarchy** | HBM 简化 | HBM3e + L2 + L1 + SMEM + DSMEM + TMEM | 扩展 MemoryCluster + CacheCluster + SharedMemoryTLM |
| **Coherence** | Phase 7.C 部分 | MOESI/GPU + 跨域桥接 | 完整 coherence-protocol.md + coherence-bridge.md + snoop_filter.md |

### 6.3 Bundle 字段对位(扩展 ComputeReqBundle)

```
ComputeReqBundle v0.5:
  - transaction_id, parent_id, fragment_id, fragment_total
  - address, size, is_write, data
  - kernel_id, workgroup_id, wavefront_id
  - coalescing_factor

ComputeReqBundle v1.0 扩展:
  - + nv_path / amd_path(双 vendor 路径)
  - + cluster_size(CGA cluster, v1.1)
  - + tensor_format(FP4/FP6/FP8/FP16/BF16/TF32)
  - + mem_barrier_phase(mbarrier 同步)
  - + wavefront_size(32/64, AMD wave64 主, RDNA wave32 备)
```

---

## 7. ADR 与模块微架构引用矩阵

### 7.1 ADR 引用矩阵

> ADR-SOC-01..05 是单决策文档(无 D 编号);ADR-SOC-06 / 07 是多决策文档(有 D1..Dn 编号)。

| 架构层 | 关联 ADR | 决策摘要 |
|--------|---------|---------|
| **L1 物理** | ADR-SOC-07 全文 | Board/SOC 分层 + PCIe EP 归属 SOC |
| **L1 Host** | ADR-SOC-07 全文 | PcieEndpointIP/SdmaEngineTLM 归属 SOC(注:ADR-SOC-07 决策对象是 PcieEndpointTLM 4 端口冻结;17 端口 PcieEndpointIP 由 `2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool` 引入) |
| **L2 命令流** | ADR-SOC-06 D5 | UsrLinuxEmu IOCTL 0x27/0x29/0x01 |
| **L3 命令协议** | ADR-SOC-04 全文 | HSAPP/CP/Dispatcher 极简化 |
| **L4 TMU** | ADR-SOC-06 D5 | TMU 简化(MVP)+ v1.1 完整版追加 |
| **L5 WDU** | ADR-SOC-06 D5 | SubmitQueue + WDU 分发网络 |
| **L6 SM/CU** | ADR-SOC-02 全文 | CU 黑盒/白盒(per v0.5 → v1.0 蓝图) |
| **L6 SM/CU** | ADR-SOC-03 全文 | Wavefront 抽象(per TMU 简化) |
| **L7 Memory** | ADR-SOC-01 全文 | Coherence 协议分步走 |
| **L7 Coherence** | ADR-SOC-01 全文 | MOESI/GPU 6 状态 × 6 事件 |

### 7.2 模块微架构引用矩阵

| 模块 | 微架构文档 | 关联 OpenSpec |
|------|-----------|---------------|
| **PcieEndpointIP** | [`docs/soc_arch/modules/dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md) + [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md) | [`2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool`](../../../openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/) + [`2027-02-09-cpptlm-dgpu-pcie-ip-integration`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-pcie-ip-integration/) |
| **SdmaEngineTLM** | [`docs/soc_arch/modules/dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md) | (per `2026-08-26-cpptlm-dgpu-sdma-engine`,已归档 `archive/`) |
| **HostBypassTLM** | (待新建) | [`2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc`](../../../openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/) |
| **PcieRootComplexTLM** | (待新建) | [`2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc`](../../../openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/) |
| **Axi4StreamAdapter** | (待新建) | [`2026-11-03-cpptlm-dgpu-axi-stream-adapter`](../../../openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/) |
| **Axi4Mapper** | (待新建) | [`2026-12-22-cpptlm-dgpu-axi4-mapper`](../../../openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/) |
| **ComputeUnitTLM** | [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md) | (per v0.5 redo archive `2026-08-19-cpptlm-v05-redo`) |
| **CommandProcessor** | [`docs/soc_arch/modules/command-processor.md`](../modules/command-processor.md) | (per v0.5 redo) |
| **Pm4Decoder** | [`docs/soc_arch/modules/pm4-decoder.md`](../modules/pm4-decoder.md) | (per v0.5 redo) |
| **TmuDispatchProcessor** | [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md) | (per v0.5 redo) |
| **SubmitQueue** | [`docs/soc_arch/modules/submit-queue.md`](../modules/submit-queue.md) | (per v0.5 redo) |
| **CudaCoreAdapter** | [`docs/soc_arch/modules/cuda-core-adapter.md`](../modules/cuda-core-adapter.md) | (per v0.5 redo) |
| **PtxEmuSubmoduleMVP** | [`docs/soc_arch/modules/ptx-emu-submodule-mvp.md`](../modules/ptx-emu-submodule-mvp.md) | (per v0.5 redo) |
| **KernelLaunchTLM** | [`docs/soc_arch/modules/gpu-kernel-launch.md`](../modules/gpu-kernel-launch.md) | (per v0.5 redo) |
| **MemoryCluster** | [`docs/soc_arch/modules/gpu-memory-cluster.md`](../modules/gpu-memory-cluster.md) | (per v0.5 redo) |
| **CacheCluster** | (引用 cache-l1.md / cache-l2.md) | (per v0.5 redo) |
| **SharedMemory** | [`docs/soc_arch/modules/gpu-shared-memory.md`](../modules/gpu-shared-memory.md) | (per v0.5 redo) |
| **GpuNoC** | [`docs/soc_arch/modules/gpu-noc-mesh.md`](../modules/gpu-noc-mesh.md) | (per v0.5 redo) |
| **CoherenceDomain** | [`docs/soc_arch/modules/coherence-domain.md`](../modules/coherence-domain.md) | (per ADR-SOC-01) |
| **CoherenceBridge** | [`docs/soc_arch/modules/coherence-bridge.md`](../modules/coherence-bridge.md) | (per ADR-SOC-01) |
| **CoherenceProtocol** | [`docs/soc_arch/modules/coherence-protocol.md`](../modules/coherence-protocol.md) | (per ADR-SOC-01) |
| **SnoopFilter** | [`docs/soc_arch/modules/snoop_filter.md`](../modules/snoop_filter.md) | (per ADR-SOC-01) |
| **CompletionRing** | [`docs/soc_arch/modules/completion-ring.md`](../modules/completion-ring.md) | (per `2026-08-26-cpptlm-dgpu-board-soc-split`,已归档) |

### 7.3 与 `docs/soc_arch/specs/apu-soc-design.md` 的差异

| 维度 | apu-soc-design.md (Phase 7 APU 视角) | 00-overview.md (v1.0 dGPU SoC 视角) |
|------|------------------------------------|-----------------------------------|
| **形态** | APU(CPU + GPU 集成) | dGPU SoC(独立 GPU + PCIe EP) |
| **决策** | D1..D5(Phase 7 APU 决策) | D1..D8(v1.0 dGPU SoC 决策) |
| **重点** | CPU↔GPU coherence + 黑盒 CU | 双 vendor 内核 + PCIe EP 17 ports |
| **范围** | Phase 7.A-F 6 子阶段 | v1.0 MVP + v1.1 完整版两阶段 |
| **driver** | 单 driver stack | CUDA + ROCm 双 driver stack |

### 7.4 与 `docs/architecture/14-pcie-ip-microarchitecture.md` 的差异

| 维度 | architecture/14 (PCIe IP 视角) | 00-overview.md (dGPU SoC v1.0 视角) |
|------|--------------------------------|--------------------------------------|
| **范围** | PCIe EP 7 阶段微架构(链路层→PHY→SR-IOV→AXI→Mapper→HostBypass→RC) | dGPU SoC v1.0 全栈(7 层) |
| **层级** | PCIe EP 内部组件 | PCIe EP + GPU + CPU + Memory + Coherence |
| **决策** | (PCIe EP 设计决策,已实施) | dGPU SoC 战略决策(融合 NVIDIA + AMD) |

---

## 8. 风险与缓解 R1..R7

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 双 vendor 内核实现工作量过大 | 🟡 中 | 蓝图模式共享 CU,运行时 config 切换 |
| **R2** | AMD ROCm 生态成熟度不足(NVIDIA 主流量) | 🟡 中 | NVIDIA 路径优先,AMD 作为备选 |
| **R3** | Tensor Core (tcgen05 + TMEM) 实施复杂度高 | 🟡 中 | v1.0 MVP 仅 wgmma,v1.1 完整版追加(per §4-bis R05) |
| **R4** | DSMEM/TMEM 集群通信延迟建模不准 | 🟡 中 | v1.0 MVP 推迟,v1.1 完整版实施(per §4-bis R21/R22) |
| **R5** | **23 ABI 头冻结 + 仓内 19/19 函数全部实现 + UsrLinuxEmu 侧 4 回调集成未闭环** | 🟡 中 | R27 已在 §4-bis 如实反映;UsrLinuxEmu ADR-089 v0.5 节奏决定完整 23 ABI 闭环交付时间 |
| **R6** | Crossbar DDS 动态目的选择延迟模拟不准 | 🟡 中 | v1.0 MVP 简化为 Round-Robin,v1.1 完整版追加 DDS(per §4-bis R15) |
| **R7** | 跨层编号与范围矩阵的内部一致性(本 Oracle 评审已暴露 P0/P1) | 🔴 高 | 本文档 v2 已对齐 §4-bis 矩阵;后续 01-10 子架构文档逐份 Oracle 评审 |

---

## 9. 反模式(明确不做)

- ❌ **复制 NVIDIA Hopper/Blackwell SM 完整 RTL**:CppTLM 是 TLM 行为级仿真,非 RTL 仿真
- ❌ **复制 AMD CDNA CU 完整 RTL**:同上
- ❌ **真实 SIMT 状态机 100% 精度**:抽象 `coalescing_factor` 参数已足够(per ADR-SOC-03)
- ❌ **完整 KFD (Kernel Fusion Driver)**:仅实施必要 IOCTL 路径
- ❌ **完整 HSAPP/HSA runtime 3000+ 行**:KernelLaunchTLM ~150 行简化(per ADR-SOC-04)
- ❌ **真实 PTX 指令集执行**:依赖 PTX-EMU 子模块黑盒(per ADR-SOC-06 D2 深度集成)
- ❌ **真实 sgemm/dgemm 性能数值**:仅相对 timing,非精确 cycle
- ❌ **完整 PCIe ECAM 4KB**:256B 兼容 config 已足够
- ❌ **真实 VC allocator (4-8 VC/port)**:NUM_VC=1 简化
- ❌ **真实 PAR-BS 调度器**:FR-FCFS 简化(per apu-soc-design.md §7)
- ❌ **集成 DRAMSim2**:纯 C++ MemoryTLM 已足够
- ❌ **CPU 模型重做**:沿用 TrafficGenTLM / CPUTLM(per apu-soc-design.md §7)

---

## 10. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(v1.0 战略 + 融合 NVIDIA + AMD 决策) |
| 2027-02-09 | v2.0-fixed | Sisyphus | Oracle 评审 FAIL → 修复 P0(PcieEndpointIP 端口语义 / OpenSpec 引用 / 层数统一 / 范围矩阵)+ P1(R7 23 ABI / PDL 错误 / 硬件规格 / MI300X 拓扑 / ADR 决策编号)+ P2(链接/术语/标注) |
| 2027-02-09 | v3.0-PASS | Sisyphus | Oracle 复审 PASS-WITH-CONDITIONS → 修复 N1(23 ABI 部分交付,18/23 函数,非"0 命中")+ N2(ADR-089 v0.5 版本对齐仓内引用) → PASS
| 2027-02-09 | v3.1-PASS | Sisyphus | 23 ABI 状态复核:仓内 19/19 函数全部实现 + 4 回调 typedef 契约(per `src/abi/cpptlm_emulator.cc` 实测),R27/R5 同步 → PASS |

**关联 OpenSpec 计划**:
- 归属 change: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)(已建立)
- 01-10 子架构文档:由本 change 后续 task 实施,每份独立 Oracle 评审
- ADR-SOC 修订/新建:由独立 OpenSpec change(`cpptlm-dgpu-soc-v1-adr-revision`,待启动)负责,需先 Metis 审查
- 模块微架构修订:由独立 OpenSpec change(`cpptlm-dgpu-soc-v1-modules-update`,待启动)负责,需先 Metis 审查

**下次更新**:Oracle 复审通过后归档 v2 → v3 标记 PASS