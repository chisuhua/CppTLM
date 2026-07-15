# Phase 7 — APU CPU+GPGPU 融合 SoC 整体架构

> **Document ID**: ARCH-011-Phase7
> **Version**: 2.0
> **Date**: 2026-06-12
> **Status**: 🔄 Draft（待用户 review）
> **Author**: 整合自 Phase 7 调研报告 + Phase 7.A 实施 spec
> **Parent Roadmap**: [`roadmap.md`](../../roadmap.md) §Phase 7
> **调研参考**: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../research-cpptlm-gpu-fused-soc-survey.md)
> **实施 spec**（位于 [`docs/superpowers/specs/`](../superpowers/specs/)）:
> - **APU 整体实施 spec**: [`2026-06-12-phase7-apu-fused-soc-design.md`](../superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md) — 包含 6 阶段代码 + 验收 + 文件清单 + LOC 估算
> - Phase 7.A 实施 spec: [`2026-06-11-phase7a-gpu-infra-design.md`](../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md)
> - Phase 7.A 实施 plan: [`../plans/2026-06-11-phase7a-gpu-infra.md`](../superpowers/plans/2026-06-11-phase7a-gpu-infra.md)
> **微架构文档索引**: [`docs/soc_arch/modules/`](../modules/)（30+ 文档）

> **本文档定位**: **架构参考视角**——讲"是什么 / 为什么"。**不**含代码片段、文件路径、LOC 估算、测试命令等实施细节（见 [`docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md`](../superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md) 实施 spec）。

---

## 0. 阅读引导

本文档是 **Phase 7 APU CPU+GPGPU 融合 SoC 的顶层架构文档**——覆盖 6 个子阶段（7.A–7.F），整合 3 份历史调研文档的差异：

| 源文档 | 范围 | 在本文档的角色 |
|--------|------|---------------|
| [调研报告](../research-cpptlm-gpu-fused-soc-survey.md) | 6 阶段全景 + 决策 D1–D5 + gem5 蓝本 | §2 系统拓扑 + §4 设计决策 + §5 关键架构模式 |
| [Phase 7.A 实施 spec](../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) | Phase 7.A 详细实施（代码 + 验收） | §3 阶段递进 7.A 节点引用 + §10 关联文档 |

**本文档结构**:
- **§1 范围与目标** — Phase 7 整体定义 + 6 子阶段总览
- **§2 系统拓扑** — APU/dGPU 形态对比 + 6 阶段递进图 + Coherence 域策略
- **§3 阶段-模块关系** — 6 阶段依赖矩阵（已实施 + 规划中模块）
- **§4 关键设计决策 D1–D5** — 协议、CU 粒度、Coalescing、HSA 简化、目录结构（含取舍图）
- **§5 关键架构模式** — 黑盒 CU / 共享 Coherence / TCC 写合并 / Mesh 互连
- **§6 风险与缓解** — R1–R8 跨阶段风险 + 降级策略
- **§7 反模式（明确不做）** — 11 项排除项
- **§8 兼容性分析** — 复用 v0 模块 + 扩展点 + Bundle 字段对位
- **§9 决策点汇总** — D1–D12
- **§10 关联文档** — 微架构 doc 索引
- **§11 修订历史**

**读者路径**:
- 关心"做不做 / 顺序" → 读 §1, §3, §6
- 关心"为什么" → 读 §2, §4, §5, §7, §8
- 关心"怎么做" → 读 `docs/superpowers/specs/` 下的实施 spec（已含代码 + 验收）

---

## 1. 范围与目标

### 1.1 Phase 7 总体目标

在 CppTLM v2.1 基础上**端到端实现 APU 形态 CPU+GPGPU 融合 SoC 仿真**——2 个 CPU 流量源 + 4 个 GPU Compute Unit + 共享 memory + 单一 Coherence 域，对位 gem5 `configs/example/apu_se.py` 的 MI300X 集成卡形态。

**六大核心目标**:

| # | 目标 | 描述 |
|---|------|------|
| G1 | **Bundle 扩展** | 在 `CacheReq/Resp` 基础上扩展 GPU 维度字段（kernel_id / workgroup_id / wavefront_id / coalescing_factor） |
| G2 | **GPGPU 黑盒** | `ComputeUnitTLM`（黑盒）+ `TCC`（GPU L2）+ 简化版 HSA Dispatcher |
| G3 | **APU coherence** | 跨 CPU↔GPU Cache 共享 MOESI 域 |
| G4 | **GPU memory hierarchy** | GPU 走 TCC → DRAM（HBM-like 延迟） |
| G5 | **多 CU 并行** | 4 CU 经 GPU 内部 mesh NoC 路由到 TCC |
| G6 | **Full APU SoC Demo** | JSON 配置 + Python 端到端测试 + 统计 dashboard |

**显式不在范围**:
- ❌ CPU 建模改进（沿用 v0 `TrafficGenTLM` / `CPUTLM`）
- ❌ dGPU 离散卡 + PCIe + Disjoint Network（Phase 7 完成后评估）
- ❌ 真实 GPU 内部微架构（5-stage pipeline / SIMD / LDS / wavefront 调度）—— 黑盒替代
- ❌ 完整 slicc 协议栈（用 C++ `switch` 表简化）

### 1.2 6 子阶段总览

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Phase 7 Roadmap (APU-first)                      │
│                                                                        │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────┐
│  │  7.A    │  │  7.B    │  │  7.C    │  │  7.D    │  │  7.E    │  │ 7.F │
│  │  GPU    │→ │ Compute │→ │Coherence│→ │  TCC +  │→ │  Multi  │→ │APU  │
│  │  Infra  │  │  Unit   │  │Protocol │  │ Memory  │  │  CU +   │  │Demo │
│  │  (GPUTLM│  │  黑盒   │  │ 集成    │  │  层次   │  │  NoC    │  │     │
│  │   v0)   │  │         │  │ (MOESI) │  │  (HBM)  │  │  (mesh) │  │     │
│  │  ✅Done │  │  Pending│  │  Pending│  │  Pending│  │  Pending│  │Pend │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────┘
│       │             │             │             │             │         │
│       │             │             │             │             │         │
│       ▼             ▼             ▼             ▼             ▼         ▼
│  Validate     Add CU        Add MOESI     Add TCC +      Add multi   Full
│  Bundle       black box     + Snoop      GPU L2 +      CU +        APU
│  in Stream                   Filter       HBM mode      mesh NoC    end-to-end
│  Adapter                                                                  demo
│                                                                        │
│  累计 16-22 周 (4-5.5 月)                                                 │
└────────────────────────────────────────────────────────────────────────┘
```

| 子阶段 | 主题 | 状态 | 风险 | 工期 | 关键架构增量 |
|--------|------|------|------|------|------------|
| **7.A** | GPU 基础设施 | ✅ **已实施** | Medium | 2-3 周 | GPU Bundle 扩展 + GPUTLM v0 |
| **7.B** | ComputeUnit 黑盒 | 🟡 Pending | Medium-High | 3-4 周 | 黑盒 CU + 简化 HSA Dispatcher |
| **7.C** | Coherence Protocol 集成 | 🟡 Pending | **High（最高风险）** | 4-5 周 | MOESI 6 状态 + 跨域 snoop 广播 |
| **7.D** | TCC Bridge + 内存层次 | 🟡 Pending | Medium | 3-4 周 | GPU L2 (TCC) + HBM 模式 |
| **7.E** | Multi-CU + NoC 集成 | 🟡 Pending | Medium | 2-3 周 | 4× CU 经 mesh NoC 路由 |
| **7.F** | Full APU SoC Demo | 🟡 Pending | Low-Medium | 2-3 周 | 端到端 APU 配置 + Python e2e |
| **7 备选 dGPU** | Discrete GPU + HBM2 | 🟡 Pending | Medium | 4-6 周 | PCIe + Disjoint NoC（APU 后评估）|

**累计工作量**: 16-22 周（APU 主路径）+ 4-6 周（dGPU 备选）

### 1.3 命名约定澄清

历史文档中"Phase 0–5" 与本项目路线图"Phase 7.A–7.F" 是**同一组阶段的两种命名**：

| 调研报告命名 | 本项目路线图命名 | 阶段主题 |
|--------------|------------------|----------|
| Phase 0 | **Phase 7.A** | GPU 基础设施 |
| Phase 1 | **Phase 7.B** | ComputeUnit 黑盒 |
| Phase 2 | **Phase 7.C** | Coherence Protocol 集成 |
| Phase 3 | **Phase 7.D** | TCC Bridge + 内存层次 |
| Phase 4 | **Phase 7.E** | Multi-CU + NoC 集成 |
| Phase 5 | **Phase 7.F** | Full APU SoC Demo |

**本项目采用 Phase 7.A–F 命名**（与 `roadmap.md` 一致）。调研报告中的"Phase 0–5" 是早期命名。

---

## 2. 系统拓扑

### 2.1 整体形态对比（APU vs dGPU）

```
┌────────────────────────────── APU 形态（Phase 7 主路径）──────────────────────────────┐
│                                                                                         │
│   ┌──────────────┐  ┌──────────────┐   ┌──────────────┐  ┌──────────────┐              │
│   │   CPU Core   │  │   CPU Core   │   │   GPU CU    │  │   GPU CU    │              │
│   │  (L1 cache)  │  │  (L1 cache)  │   │  (L1 cache)  │  │  (L1 cache)  │              │
│   └──────┬───────┘  └──────┬───────┘   └──────┬───────┘  └──────┬───────┘              │
│          │                 │                  │                 │                      │
│          └────────┬────────┘                  └────────┬────────┘                      │
│                   │                                  │                                │
│                   ▼                                  ▼                                │
│          ┌──────────────────┐              ┌──────────────────┐                       │
│          │  CoherentXBar    │              │       TCC        │                       │
│          │  + SnoopFilter   │◄────────────►│   (GPU L2)       │                       │
│          └────────┬─────────┘              └────────┬─────────┘                       │
│                   │                                  │                                │
│                   └──────────────┬───────────────────┘                                │
│                                  ▼                                                    │
│                         ┌──────────────┐                                              │
│                         │   DDR (HBM)  │                                              │
│                         └──────────────┘                                              │
│                                                                                         │
│   • 单一 Coherence 域 (MOESI)                                                          │
│   • 共享 DDR/HBM 物理内存                                                              │
│   • 跨 CPU↔GPU Cache 通过 CoherentXBar + SnoopFilter 同步                              │
│                                                                                         │
└─────────────────────────────────────────────────────────────────────────────────────────┘

┌────────────────────────────── dGPU 形态（Phase 7 备选）──────────────────────────────┐
│                                                                                       │
│  ┌─── Host APU Domain (MOESI) ───┐         ┌─── dGPU Domain (MOESI) ───┐             │
│  │                               │         │                          │             │
│  │  ┌──────────┐   ┌──────────┐  │  PCIBridge│  ┌──────────┐ ┌──────────┐ │            │
│  │  │ CPU Core │   │ CPU Core │  │  (200 cy) │  │ GPU CU   │ │ GPU CU   │ │            │
│  │  │  (L1)    │   │  (L1)    │  │◄────────►│  │  (L1)    │ │  (L1)    │ │            │
│  │  └────┬─────┘   └────┬─────┘  │         │  └────┬─────┘ └────┬─────┘ │            │
│  │       └──────┬───────┘        │         │       └─────┬──────┘       │            │
│  │              ▼                │         │             ▼              │            │
│  │     ┌──────────────┐         │         │    ┌──────────────┐        │            │
│  │     │   DDR        │         │         │    │   HBM2       │        │            │
│  │     └──────────────┘         │         │    └──────────────┘        │            │
│  └──────────────────────────────┘         └────────────────────────────┘             │
│                                                                                       │
│   • 双独立 Coherence 域                                                                │
│   • 独立内存 (Host DDR vs dGPU HBM2)                                                  │
│   • 跨域通过 PCIBridge + CoherenceBridge (协议转换 + 地址映射)                          │
│   • Disjoint Network (双独立 NoC)                                                     │
│                                                                                       │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

**关键架构差异**:

| 维度 | APU 形态 | dGPU 形态 |
|------|----------|-----------|
| Coherence 域 | 单一 (CPU+GPU 共享) | 双独立 (Host + dGPU) |
| 内存 | 共享 DDR/HBM | 独立 Host DDR + dGPU HBM2 |
| 桥接 | CoherentXBar (snoop) | PCIBridge (200cy 延迟) + CoherenceBridge |
| NoC | 共享 | Disjoint Network (双独立) |
| 适用场景 | 集成卡 (笔记本/Mobile) | 离散卡 (Desktop/HPC) |
| 工作量 | 16-22 周 (Phase 7 主路径) | 4-6 周 (备选，加 25-30%) |

**用户决策 D6**: APU-first 路径。dGPU 形态仅在 APU 完成后用户决策启动。

### 2.2 APU 拓扑（最终态 — Phase 7.F）

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          APU Domain（MOESI_AMD 6-state）                    │
│                                                                          │
│  ┌──── CPU Cluster ──────────────┐  ┌──── GPU Cluster ─────────────┐     │
│  │                                │  │                              │     │
│  │  ┌──────────┐  ┌──────────┐   │  │  ┌──────────┐  ┌──────────┐  │     │
│  │  │TrafficGen│  │TrafficGen│   │  │  │Compute   │  │Compute   │  │     │
│  │  │  TLM #0  │  │  TLM #1  │   │  │  │Unit TLM  │  │Unit TLM  │  │     │
│  │  │ (CPU 端) │  │ (CPU 端) │   │  │  │  #0      │  │  #1      │  │     │
│  │  └─────┬────┘  └─────┬────┘   │  │  └─────┬────┘  └─────┬────┘  │     │
│  │        │              │        │  │        │              │        │     │
│  │        ▼              ▼        │  │        ▼              ▼        │     │
│  │  ┌──────────┐  ┌──────────┐    │  │  ┌──────────────────────────┐ │     │
│  │  │CacheTLM  │  │CacheTLM  │    │  │  │  GPU Internal Mesh NoC   │ │     │
│  │  │(MOESI)   │  │(MOESI)   │    │  │  │  (RouterTLM × N)         │ │     │
│  │  │  L1 CPU  │  │  L1 CPU  │    │  │  └────────────┬─────────────┘ │     │
│  │  └─────┬────┘  └─────┬────┘    │  │               │                │     │
│  │        │              │        │  │               ▼                │     │
│  │        └──────┬───────┘        │  │        ┌──────────┐          │     │
│  │               ▼                │  │        │ TCC TLM  │          │     │
│  │        ┌──────────┐           │  │        │(GPU L2 + │          │     │
│  │        │CacheTLM  │           │  │        │coalesce) │          │     │
│  │        │(MOESI)   │           │  │        └─────┬────┘          │     │
│  │        │  L2 CPU  │           │  │              │                │     │
│  │        └─────┬────┘           │  │              │                │     │
│  │              │                │  │              │                │     │
│  └──────────────┼────────────────┘  └──────────────┼────────────────┘     │
│                 │                                  │                       │
│                 └──────────────┬───────────────────┘                       │
│                                ▼                                            │
│              ┌──────────────────────────────────────┐                       │
│              │       CoherentXBarTLM               │                       │
│              │  (snoop broadcast via SnoopFilter)   │                       │
│              │                                      │                       │
│              │  功能:                               │                       │
│              │  • 地址路由 (CPU 端 ↔ Mem/TCC 端)   │                       │
│              │  • snoop probe 广播到同域 Cache     │                       │
│              │  • snoop response 收集与路由        │                       │
│              └─────────────────┬────────────────────┘                       │
│                                │                                            │
│                                ▼                                            │
│              ┌──────────────────────────────────────┐                       │
│              │         MemoryTLM (HBM 模式)         │                       │
│              │                                      │                       │
│              │  容量: 8-16 GB (HBM2 stack)          │                       │
│              │  延迟: rd 100cy / wr 120cy            │                       │
│              │  双 Bundle: CacheReq + ComputeReq     │                       │
│              └──────────────────────────────────────┘                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

**架构要点**:
- **2 个 CPU 端** (TrafficGenTLM) → 各自 L1 cache → 共享 L2 cache
- **4 个 GPU 端** (ComputeUnitTLM) → 内部 mesh NoC → TCC (GPU L2) → 共享 Memory
- **CoherentXBar** 处理跨域 snoop 广播（SnoopFilter 减负）
- **单一 MOESI 域** 覆盖所有 cache + TCC

**关键模块类型**（v2.3, 2026-06-19 更新）:

| 实例名 | 类型 | 说明 |
|--------|------|------|
| `xbar` | **CoherentXBarTLM** | APU 顶层跨域总线（继承 CrossbarTLM, 加 snoop broadcast 通道, Phase 7.A/7.B write-through 透传, 7.C 引入 6×6 state table） |

### 2.3 6 阶段递进（与最终态的差异）

每阶段相对于最终态的架构简化：

```
7.A  ┌──────────┐                          ┌──────────┐
     │ GPUTLM   │ ─── direct ──────────────►│MemoryTLM │
     │  (v0)    │                          │ (rd/wr)  │
     └──────────┘                          └──────────┘
     - 验证 GPU Bundle 能在 StreamAdapter 管线流通
     - bypass Cache / Crossbar / Coherence

7.B  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
     │TrafficGen│──►│CacheTLM │──►│CrossbarTLM│──►│MemoryTLM│
     │  (CPU)   │   │(简化)   │   │           │   └──────────┘
     └──────────┘   └──────────┘   └──────────┘
     ┌──────────┐
     │ComputeU  │──┐
     │(CU black)│  │   (CU 请求也走 Crossbar, 共享 memory)
     └──────────┘──┘
     - 1 CU 黑盒 + 1 CPU + Crossbar
     - write-through 简化协议 (CacheTLM 简化版)
     - bypass coherence (write-through)

7.C  ┌──────────┐   ┌──────────┐   ┌──────────────────┐   ┌──────────┐
     │TrafficGen│──►│CacheTLM │──►│  CoherentXBarTLM  │──►│MemoryTLM│
     │  (CPU)   │   │(MOESI 6)│   │  + SnoopFilter    │   │          │
     └──────────┘   └──────────┘   └──────────────────┘   └──────────┘
     ┌──────────┐
     │ComputeU  │──►CacheTLM (MOESI) ── snoop ──▲
     └──────────┘
     - CacheTLM 升级为 MOESI 6 状态
     - 引入 SnoopFilter + CoherentXBar snoop 广播
     - 跨 CPU↔GPU Cache 共享 coherence 域

7.D  ┌──────────┐   ┌──────────┐   ┌──────────────────┐   ┌──────────┐
     │ComputeU  │──►│CacheTLM │──►│  CoherentXBarTLM  │──►│MemoryTLM│
     └────┬─────┘   │(MOESI)  │   └──────────────────┘   │ (HBM)   │
          │         └──────────┘                          └──────────┘
          ▼
     ┌──────────┐
     │ TCC TLM  │  (写合并 + snoop fan-in)
     └────┬─────┘
          ▼
     ┌──────────┐
     │MemoryTLM │
     │ (HBM)    │
     └──────────┘
     - 引入 TCC (GPU L2) 在 CU 与 Memory 之间
     - MemoryTLM 升级 HBM 模式 (双 Bundle)

7.E  ┌────────────┐   ┌────────────┐   ┌──────────┐
     │ComputeU #0│   │ComputeU #1│   │ComputeU #2│   ... (×4)
     └─────┬──────┘   └─────┬──────┘   └─────┬──────┘
           └────────┬────────┘────────┬────────┘
                    ▼
          ┌──────────────────┐
          │  GPU Mesh NoC    │   (RouterTLM × N, LinkTLM × M)
          │  (BidirectionalPortAdapter<N>)
          └────────┬─────────┘
                   ▼
                ┌──────┐
                │ TCC │
                └──┬───┘
                   ▼
              ┌────────┐
              │Memory │
              └────────┘
     - 4× CU 并行经 mesh 路由到 TCC
     - 复用现有 RouterTLM/LinkTLM/NoCFlitBundle

7.F  ┌──────────────────────────────────────────────────────────────┐
     │                  完整 APU SoC 拓扑                            │
     │  2× TrafficGen + 2× CacheTLM (CPU) + 4× ComputeU + 1× TCC │
     │  + CoherentXBar (snoop) + MemoryTLM (HBM)                  │
     │  单一 CoherenceDomain("apu_domain", MOESI_AMD)             │
     └──────────────────────────────────────────────────────────────┘
     - 端到端 APU 演示 + Python 测试 + 统计 dashboard
```

**关键递进逻辑**:
- **7.A → 7.B**: 加入 CPU 端 + Crossbar，验证共享总线
- **7.B → 7.C**: 简化协议 → 真实 MOESI，引入 coherence 域
- **7.C → 7.D**: 加入 GPU 端 cache hierarchy (TCC 作为 GPU L2)
- **7.D → 7.E**: 单 CU → 多 CU + mesh NoC
- **7.E → 7.F**: 各组件集成 + 端到端验证

### 2.4 Coherence 域策略

```
┌──────────────────────────────────────────────────────────────┐
│                   Coherence 域拓扑（APU 形态）                  │
│                                                              │
│  ┌──── CoherenceDomain("apu_domain", MOESI_AMD) ────┐        │
│  │                                                  │        │
│  │  成员（共享 MOESI 状态）:                        │        │
│  │  • CPU L1 caches (2 个)                        │        │
│  │  • CPU L2 cache (1 个)                         │        │
│  │  • GPU L1 caches (4 个)                        │        │
│  │  • GPU TCC (1 个, GPU L2)                      │        │
│  │                                                  │        │
│  │  协议: MOESI 6 状态机 (I/S/E/M/O/T)            │        │
│  │  实现: C++ switch 表 (非 slicc)               │        │
│  │  减负: SnoopFilter (SnoopFilterCacheTLM)      │        │
│  └──────────────────────────────────────────────────┘        │
│                                                              │
│  跨域流量:                                                   │
│  • CPU 写 → snoop 广播到 GPU L1s → GPU invalidate           │
│  • GPU 写 → snoop 广播到 CPU L1s → CPU invalidate           │
│  • CPU/GPU 读 → miss → TCC (snoop fan-in) → Memory          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

**MOESI 6 状态机（I/S/E/M/O/T）**:

| 状态 | 含义 | 持有者 | 备注 |
|------|------|--------|------|
| **I** (Invalid) | 无有效副本 | (无) | 初始态 |
| **S** (Shared) | 只读共享 | 多 cache 可有 | 多 sharer |
| **E** (Exclusive) | 独占（未修改） | 单 cache | 唯一持有，未 dirty |
| **M** (Modified) | 独占（已修改） | 单 cache | 唯一持有，已 dirty |
| **O** (Owned) | 持有已修改（可被 share） | 单 cache | 数据脏，但可被其他 cache 读 |
| **T** (Transient) | 瞬态（等待 snoop response） | 单 cache | 中间态 |

**6×6 状态转换表**详见 [`cache-protocol.md`](../modules/cache-protocol.md)。

**域边界处理**:
- **域内**: 任何 cache 写 → snoop 广播到同域其他 cache → 维护一致性
- **域间（dGPU 备选）**: `CoherenceBridge` 桥接 → 协议转换（MOESI_AMD ↔ MESI_GPU）+ 地址映射（Host 物理地址 ↔ VRAM）

### 2.5 数据流图

**CPU 端读流程**:
```
CPU (TrafficGen)
  │ ① 生成 CacheReq (address=X, is_read=1)
  ▼
CPU L1 Cache
  │ ② tag 查找 → hit/miss
  ├── hit ──► ③ 直接返回数据 (latency 5cy)
  │
  └── miss ──► ④ 转发到 CoherentXBar
                 │
                 ▼
              CoherentXBar
                 │ ⑤ snoop 广播到同域其他 cache (via SnoopFilter)
                 │ ⑥ 收 snoop response
                 │
                 ▼
              目标 cache (e.g., GPU L1) or TCC
                 │ ⑦ 命中返回 / 未命中转发到 Memory
                 │
                 ▼
              Memory (HBM)
                 │ ⑧ 读数据 (latency 100cy)
                 │
                 ▼
              响应回 CPU L1 (latency 总计 ~200cy)
```

**GPU 端写流程（含 TCC 写合并）**:
```
GPU CU (ComputeUnitTLM)
  │ ① 生成 ComputeReq (address=X, is_write=1, data=...)
  ▼
GPU L1 Cache
  │ ② tag 查找 → miss (写未命中)
  │ ③ 转发到 CoherentXBar
  │
  ▼
CoherentXBar
  │ ④ snoop 广播 (通过 SnoopFilter 过滤)
  │ ⑤ 收 snoop response → 同域其他 cache 失效该行
  │
  ▼
TCC (GPU L2, write-coalescing)
  │ ⑥ 同一地址连续写 → 合并 (64B cache line 粒度)
  │ ⑦ 批量写回 Memory
  │
  ▼
Memory (HBM)
  │ ⑧ 写 (latency 120cy, 带宽限流)
  │
  ▼
响应回 GPU (latency 总计 ~300cy)
```

---

## 3. 阶段-模块关系

### 3.1 6 阶段 vs 已实现模块（v2.1）

| 阶段 | 依赖已实现模块 | 关键复用 |
|------|----------------|----------|
| **7.A** | `ChStreamModuleBase` + `StreamAdapter` 单端口 + Bundle 体系 | (无新增模块) |
| **7.B** | 7.A 全部 | `TrafficGenTLM` / `CrossbarTLM` / `MemoryTLM` 复用 |
| **7.C** | 7.A + `CoherenceDomain` (基础设施已实施) | `CoherenceState` 枚举 + `home_node` 机制 |
| **7.D** | 7.A + 7.C + `DualPortStreamAdapter` (NICTLM 已用) | `MemoryTLM` 扩展（双 Bundle 注册） |
| **7.E** | 7.B + 7.D + `RouterTLM` / `LinkTLM` / `BidirectionalPortAdapter` | `NoCFlitBundle` 已有 |
| **7.F** | 7.A–7.E 全部 | 全部 + `cpptlm` Python 库 |

### 3.2 6 阶段 vs 规划中模块（微架构 doc 索引）

| 阶段 | 依赖规划中模块 | 引入文档 |
|------|----------------|----------|
| **7.B** | `ComputeUnitTLM` + `KernelLaunchTLM` | [`gpu-compute_unit.md`](../modules/gpu-compute_unit.md) + [`gpu-kernel-launch.md`](../modules/gpu-kernel-launch.md) |
| **7.C** | `CacheTLM` 升级 + `CoherenceDomain` 集成 + `SnoopFilter` + `CoherentXBar` | [`cache-protocol.md`](../modules/cache-protocol.md) + [`coherence-protocol.md`](../modules/coherence-protocol.md) + [`snoop_filter.md`](../modules/snoop_filter.md) + [`coherent_xbar.md`](../modules/coherent_xbar.md) |
| **7.D** | `TCC_TLM` + `MemoryTLM` HBM 模式（双 Bundle） | [`gpu-tcc.md`](../modules/gpu-tcc.md) + [`memory-hbm.md`](../modules/memory-hbm.md) |
| **7.E** | `NoCTopologyBuilder`（规划中）+ 4×CU mesh | [`noc.common.md`](../modules/noc.common.md) |
| **7.F** | 全部 | (集成) |
| **7 备选 dGPU** | `PCIBridgeTLM` + `CoherenceBridge` | [`gpu-pcie_bridge.md`](../modules/gpu-pcie_bridge.md) + [`coherence-bridge.md`](../modules/coherence-bridge.md) |

### 3.3 阶段-模块依赖图

```
                        ┌─────────────────┐
                        │ 已实施 v0 模块  │
                        │ (10 个 TLM +    │
                        │  基础设施)      │
                        └────────┬────────┘
                                 │
            ┌────────────────────┼────────────────────┐
            │                    │                    │
            ▼                    ▼                    ▼
     ┌─────────────┐      ┌─────────────┐      ┌──────────────┐
     │  7.A GPUTLM │      │  7.B CU 黑盒│      │  7.C CacheTLM│
     │   (GPUTLM)  │      │ (ComputeU + │      │  协议升级     │
     │  验证 Bundle│      │  KernelLau) │      │ (MOESI 6态)  │
     └──────┬──────┘      └──────┬──────┘      └──────┬───────┘
            │                    │                    │
            └─────────┬──────────┘                    │
                      │                               │
                      ▼                               ▼
               ┌─────────────┐                ┌──────────────┐
               │  7.D TCC +  │                │  7.E 4×CU +  │
               │ Memory HBM  │                │  mesh NoC    │
               └──────┬──────┘                └──────┬───────┘
                      │                               │
                      └─────────────┬─────────────────┘
                                    ▼
                          ┌──────────────────┐
                          │  7.F 完整 APU    │
                          │  端到端演示      │
                          └──────────────────┘
                                    │
                                    ▼
                          ┌──────────────────┐
                          │  7 备选 dGPU    │
                          │ (APU 完成后评估) │
                          └──────────────────┘
```

---

## 4. 关键设计决策 D1–D5

> **来源**: [调研报告](../research-cpptlm-gpu-fused-soc-survey.md) §3 + 用户 2026-06-11 决策

### 4.1 D1 协议策略（分步走）

> **ADR**: [`ADR-SOC-01-coherence-protocol-strategy.md`](../adr/ADR-SOC-01-coherence-protocol-strategy.md)

**决策**: 不一次性实现完整 MOESI，而是**按阶段递进**：

```
阶段          协议                  状态机复杂度
─────────────────────────────────────────────────
7.A           简化 I/S/M 三态       无 cache (write-through bypass)
7.B           简化 I/S/M 三态       1 个 cache (write-through)
7.C           MOESI 6 状态           C++ switch 表
7.D           MOESI + GPU 扩展      跨域 snoop fan-in
7.E           MOESI                  多 CU 并发 snoop fanout
7.F           MOESI                  端到端验证
```

**取舍图**:

```
                    协议完整性
                         ▲
                         │
         full MOESI      │            ┌─ MOESI_AMD 完整
         + GPU_VIPER     │            │  (slicc, 3000+ 行)
                         │            │
                         │      ┌─────┤
                         │      │     │  ← D1 采纳: 分步走
                         │      │     │     (Phase 7.C 简化 6 状态)
                         │      │     │
         simplified I/S/M│      │     │
                         │      │     │
                         │      └─────┘
                         │  Phase 7.B 简化协议
                         │
                         └──────────────────────► 开发成本
                                低               高
```

**触发降级条件**: Phase 7.C 测试发现 5+ 测试回归 → 回 Phase 7.B write-through bypass，coherence 留待 Phase 8。

### 4.2 D2 CU 粒度（黑盒优先）

> **ADR**: [`ADR-SOC-02-cu-granularity.md`](../adr/ADR-SOC-02-cu-granularity.md)

**决策**: ComputeUnit 作为**黑盒发起器**，不模拟 5-stage pipeline / ISA / SIMD / LDS / wavefront 调度。

```
┌─────────────────────────── 真实 GPU Compute Unit (gem5) ───────────────────────────┐
│                                                                                    │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │  5-Stage    │  │  SIMD Lane  │  │  Register   │  │  LDS        │              │
│  │  Pipeline   │  │  (16-64)    │  │  File       │  │  (Local     │              │
│  │  (Fetch/Dec │  │             │  │             │  │  Data Share) │              │
│  │  /Exe/Mem/  │  │  Wavefront  │  │             │  │             │              │
│  │  Writeback) │  │  (32-64     │  │             │  │             │              │
│  │             │  │  threads)   │  │             │  │             │              │
│  └─────────────┘  └─────────────┘  └─────────────┘  └─────────────┘              │
│  ~3000+ 行 C++ 代码 (ComputeUnit.py + Wavefront.py + ...)                       │
│                                                                                    │
└────────────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │  CppTLM 简化（黑盒替代）
                                    ▼
┌───────────────────── CppTLM ComputeUnitTLM (黑盒) ─────────────────────────┐
│                                                                              │
│  tick() 周期发出 ComputeReqBundle（read/write）                                │
│  维护 inflight_kernel_reqs_ + workgroup_progress_                            │
│  跟踪 kernel 完成时间 + launch latency                                       │
│                                                                              │
│  ~250 行 C++ 代码                                                             │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

**理由**: CppTLM 仿真目标是大规模 SoC 行为（CPU+GPU 协同 + coherence + NoC 拥塞），不需要精确到每个 GPU 时钟周期。

### 4.3 D3 Wavefront/Coalescing（抽象）

> **ADR**: [`ADR-SOC-03-wavefront-coalescing-abstraction.md`](../adr/ADR-SOC-03-wavefront-coalescing-abstraction.md)

**决策**: 引入 `coalescing_factor` 标量参数，**不**模拟真实 coalescer 状态机。

```
真实模型 (gem5):
  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │Wavefront │  │Wavefront │  │Wavefront │   (32 lanes each)
  │  addr=A  │  │  addr=B  │  │  addr=C  │
  └─────┬────┘  └─────┬────┘  └─────┬────┘
        │             │             │
        └──────────┬──┴─────────────┘
                   ▼
        ┌──────────────────────┐
        │ VIPERCoalescer       │   (state machine + merge logic)
        │ - 合并相邻 wavefront  │
        │ - 计算 coalesce_factor│   (e.g., 4.0 means 4x reduction)
        └──────────┬───────────┘
                   ▼
              memory reqs (数量减少 4x)

CppTLM 抽象 (采纳):
        ComputeReqBundle
        ┌──────────┐
        │coalesce_ │
        │factor=4  │   ← 标量参数 setter (默认 1)
        └──────────┘
              ▼
        memory reqs (直接按 factor 倍数缩放)
```

**理由**: coalescer 行为是 GPU 内部细节，对 APU SoC 行为（coherence 流量、NoC 拥塞）影响有限；抽象为参数即可。

### 4.4 D4 HSA Runtime 简化（极致）

> **ADR**: [`ADR-SOC-04-hsapp-cp-dispatcher-simplification.md`](../adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md)

**决策**: KernelLaunchTLM ~150 行，**不**实现完整 HSAPP + GPUCommandProcessor + GPUDispatcher。

```
真实模型 (gem5):
  ┌──────────────┐    ┌──────────────────┐    ┌──────────────┐
  │ HSA Packet   │───►│  GPU Command     │───►│   GPU        │
  │ Processor    │    │  Processor       │    │   Dispatcher │
  │ (AQL queue)  │    │  (queue mgmt)    │    │  (CU select) │
  │ ~1500 行     │    │  ~800 行         │    │ ~700 行      │
  └──────────────┘    └──────────────────┘    └──────────────┘

CppTLM 简化 (采纳):
  ┌──────────────────────────────┐
  │  KernelLaunchTLM (~150 行)  │
  │  - tick() 中按 interval 发  │
  │    kernel launch 命令        │
  │  - 接受 HSAQueueDescriptor   │
  │  - 简化队列管理               │
  └──────────────────────────────┘
```

**理由**: HSA runtime 细节对 SoC 行为影响小；只需模拟"CPU 发 launch → GPU 收到 → 开始计算"的事件流。

### 4.5 D5 目录结构（`include/tlm/gpu/` 子目录）

> **ADR**: [`ADR-SOC-05-gpu-directory-structure.md`](../adr/ADR-SOC-05-gpu-directory-structure.md)

**决策**: 新建 `include/tlm/gpu/` 子目录，与 `cpptlm::rtl::*` 和 `tlm::*` 形成清晰分层。

```
include/tlm/
├── cache_tlm.hh              (v0 已有)
├── memory_tlm.hh             (v0 已有)
├── crossbar_tlm.hh           (v0 已有)
├── arbiter_tlm.hh            (v0 已有)
├── router_tlm.hh             (v0 已有)
├── nic_tlm.hh                (v0 已有)
├── link_tlm.hh               (v0 已有)
├── cpu_tlm.hh                (v0 已有)
├── traffic_gen_tlm.hh        (v0 已有)
└── gpu/                      (Phase 7 新建子目录)
    ├── gpu_tlm.hh                (Phase 7.A ✅ 已实施)
    ├── common_gpu_tlm.hh         (Phase 7.B 🟡)
    ├── compute_unit_tlm.hh       (Phase 7.B 🟡)
    ├── kernel_launch_tlm.hh      (Phase 7.B 🟡)
    ├── tcc_tlm.hh                (Phase 7.D 🟡)
    └── pcie_bridge_tlm.hh        (Phase 7 备选 dGPU 🟡)
```

**理由**: 与 `cpptlm::rtl::*`（RTL 桥接）、`tlm::*`（顶层 TLM 模块）形成清晰的命名空间分层；GPU 模块数量 ≥ 5，独立子目录合理。

---

## 5. 关键架构模式

### 5.1 黑盒计算单元（Black-box Compute Unit）

**模式描述**: ComputeUnit 内部不模拟 pipeline，仅作为"请求发生器 + 进度跟踪器"。

```
真实 GPU Compute Unit:                      CppTLM 黑盒:
┌─────────────────────────┐                 ┌─────────────────────┐
│ 5-stage pipeline        │                 │ tick() 循环         │
│ ↓                       │                 │  - 发 ComputeReq   │
│  ┌────┐  ┌────┐  ┌────┐ │                 │  - 收 ComputeResp  │
│  │IF/ID│→│EX │→│MEM│ │                 │  - 推进进度         │
│  └────┘  └────┘  └────┘ │                 │                     │
│ ↓                       │                 │ 维护状态:           │
│ Wavefront scheduler     │                 │  inflight_reqs_    │
│  ┌──────────┐          │                 │  progress_         │
│  │ Wave 0   │          │                 │                     │
│  │ Wave 1   │          │                 │ ~250 行             │
│  │ Wave 2   │          │                 │                     │
│  │ ...      │          │                 │                     │
│  └──────────┘          │                 └─────────────────────┘
│ ↓                       │                          ↑
│ SIMD lane execution     │                          │
│  ┌──┐┌──┐┌──┐┌──┐     │                          │ 等价语义
│  │L0│L1│L2│L3│...    │                          │ (请求完成时间
│  └──┘└──┘└──┘└──┘     │                          │  频率分布)
│ ↓                       │                          │
│ LDS (shared mem)        │                          │
│ Global mem (HBM)        │                          │
│                         │                          │
│ ~3000+ 行               │                          │
└─────────────────────────┘                 ─────────────────────────
```

**对外接口（仅 2 个端口 + tick 循环）**:
- 输入端口: 接受 `ComputeReqBundle`（来自 TCC 或其他源）
- 输出端口: 发送 `ComputeReqBundle`（kernel 内部 memory access）
- tick(): 按 `kernel_issue_interval_` 周期发起新请求

### 5.2 共享 Coherence 域

**模式描述**: 单一 `CoherenceDomain` 覆盖所有 CPU/GPU cache + TCC，跨域通过 snoop 广播维护一致性。

```
APU 域 (CoherenceDomain "apu_domain"):
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  成员（snoop 目标）:                                         │
│  • CPU L1 #0, #1  (2 个)                                    │
│  • CPU L2         (1 个)                                    │
│  • GPU L1 #0..#3  (4 个)                                    │
│  • TCC            (1 个, GPU L2)                             │
│                                                              │
│  协议: MOESI 6 状态 (I/S/E/M/O/T)                            │
│  SnoopFilter: SnoopFilterCacheTLM (1024 sets × 4 ways)       │
│                                                              │
│  snoop 流程:                                                 │
│  ┌─────────────┐                                             │
│  │ CPU L1 #0   │ write addr=X                                │
│  │ (发起者)    │──────┐                                      │
│  └─────────────┘      │                                      │
│                       ▼                                      │
│              ┌──────────────────┐                            │
│              │ SnoopFilter      │  ← 过滤 (e.g., X 不在 GPU L1)│
│              └────────┬─────────┘                            │
│                       │ (snoop targets: GPU L1 #0..#3, L2)    │
│                       ▼                                      │
│              snoop request ──► 5 个 cache                    │
│              snoop response ◄── 5 个 cache                   │
│              ┌────────────┐                                   │
│              │ 5 个 ACK   │ → 合并为单个 response            │
│              └────────────┘                                   │
│                       │                                      │
│                       ▼                                      │
│              CPU L1 #0 收到完整 response                       │
│              (所有其他 cache 已 invalidate X)                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

**关键设计**:
- **简化协议**: 6 状态 + 6×6 状态转换表（详见 [`cache-protocol.md`](../modules/cache-protocol.md)）
- **SnoopFilter 减负**: 减少冗余 snoop 流量（详见 [`snoop_filter.md`](../modules/snoop_filter.md)）
- **同步查询**: `get_snoop_targets(addr)` 同步返回 cache_id 列表

### 5.3 TCC 写合并（Write Coalescing）

**模式描述**: TCC（GPU L2）作为 GPU 端写入聚合点，同一 cache line 的连续写合并为一次 memory 写。

```
GPU 端写 (无合并):                 GPU 端写 (TCC 合并):
┌─────────┐                       ┌─────────┐
│ CU #0   │──► write X (data=A)  │ CU #0   │──► write X (data=A)
└─────────┘   │                   └────┬────┘   │
              │                        │
              ▼                        ▼
┌─────────┐   write X (data=B)  ┌─────────┐
│ CU #1   │──►                    │  TCC    │  (merge A+B → latest)
└─────────┘   │                   │ (L2)   │       │
              │                        │       │
              ▼                        ▼       │
         ┌─────────┐              ┌─────────┐  │
         │Memory   │              │Memory   │  │
         │  (HBM)  │              │  (HBM)  │  │
         └─────────┘              └─────────┘  │
              ▲                                  │
              │                                  │
         2 writes (带宽 2x)                1 write (节省 1x)
```

**关键参数**:
- **合并粒度**: 64B cache line（v0 简化，可调至 4KB page）
- **合并窗口**: 时间窗口（如 100 cycle）或写满触发
- **保序**: TCC 内部加顺序锁（避免写失序）

详见 [`gpu-tcc.md`](../modules/gpu-tcc.md)。

### 5.4 Mesh 内部互连（Phase 7.E）

**模式描述**: 4 个 CU 经 mesh NoC 路由到 TCC，复用现有 `RouterTLM` / `LinkTLM` / `NoCFlitBundle`。

```
4×4 Mesh 拓扑（CU + TCC 节点）:
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Router  │─────│  Router  │─────│  Router  │─────│  Router  │
│  (CU #0) │     │  (CU #1) │     │  (CU #2) │     │  (CU #3) │
└─────┬────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
      │               │               │               │
      │               │               │               │
┌─────┴────┐     ┌────┴─────┐     ┌────┴─────┐     ┌────┴─────┐
│  Router  │─────│  Router  │─────│  Router  │─────│  Router  │
│  (CU #4) │     │  (CU #5) │     │  (CU #6) │     │  (CU #7) │
└─────┬────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
      │               │               │               │
      │               │               │               │
┌─────┴────┐     ┌────┴─────┐     ┌────┴─────┐     ┌────┴─────┐
│  Router  │─────│  Router  │─────│  Router  │─────│  Router  │
│  (CU #8) │     │  (CU #9) │     │  (CU #10)│     │  (CU #11)│
└─────┬────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
      │               │               │               │
      │               │               │               │
┌─────┴────┐     ┌────┴─────┐     ┌────┴─────┐     ┌────┴─────┐
│  Router  │─────│  Router  │─────│  Router  │─────│  Router  │
│  (CU #12)│     │  (CU #13)│     │  (TCC)   │     │  (CU #15)│
└──────────┘     └──────────┘     └──────────┘     └──────────┘
                                          │
                                          ▼
                                    ┌──────────┐
                                    │ MemoryTLM│
                                    └──────────┘

  • 16 RouterTLM (5 端口: N/E/S/W/Local)
  • 24 LinkTLM (双向)
  • XY 路由 (v0 简化, 自适应路由 Phase 7+)
  • NUM_VC=1 (v0 简化, NUM_VC=4 真实)
```

**关键设计**:
- **复用**: `RouterTLM`（5 端口 XY 路由 + Credit 控流）+ `LinkTLM`（物理链路延迟）
- **新加**: `NoCTopologyBuilder`（JSON `topology="4x4_mesh"` → 构造 16 routers + 24 links）
- **v0 简化**: NUM_VC=1，XY 路由（无自适应）

详见 [`noc.common.md`](../modules/noc.common.md)。

---

## 6. 风险与缓解

### 6.1 跨阶段 R1–R8（综合）

| # | 风险 | 概率 | 影响 | 跨阶段范围 | 缓解 |
|---|------|------|------|-----------|------|
| **R1** | **Phase 7.C coherence 死锁/livelock** | 高 | 高 | 7.C | 5+ 回归 → 回 write-through bypass；缓存一致性强度可降级 |
| **R2** | **Phase 7.D TCC 写合并失序** | 中 | 中 | 7.D | TCC 内部加顺序锁；统计对比优化前/后 |
| **R3** | **Phase 7.E mesh 拥塞** | 中 | 中 | 7.E | CommMonitor 监控；NoC 配置可调 |
| **R4** | **MOESI 状态机不完整**（slicc 简化） | 中 | 中 | 7.C | 单元测试覆盖全部 6×6 转换；与 gem5 reference 对照 |
| **R5** | **MemoryTLM 不接受 ComputeReqBundle** | 中 | 中 | 7.D | Phase 7.D 实施双 Bundle 注册 |
| **R6** | **统计准确性**——黑盒模型可能高估/低估 | 中 | 中 | 7.B–F | 与 gem5 apu_se.py reference 对照（区间验证） |
| **R7** | **CPU 模型简化**——TrafficGenTLM 不是真实 CPU | 中 | 中 | 7.B–F | 文档明确：v0 仅作 CPU 流量源 |
| **R8** | **多 Bundle 兼容**——CacheReq 与 ComputeReq 路径冲突 | 中 | 中 | 7.A–D | CacheReqBundle + ComputeReqBundle 字段对位；AdapterHandle 路由 |

### 6.2 降级策略

**降级触发**: Phase 7.C coherence 实施后**测试回归 ≥ 5 项**。

**降级路径**:
```
Phase 7.C (MOESI)  ──[5+ 回归]──►  Phase 7.B (write-through)  ──[回归修复后]──►  Phase 8
   ↑                                                                          │
   └──────────────────[完成 coherence 重设计后]───────────────────────────────┘
```

**降级影响**:
- Phase 7.D (TCC) 推迟到 Phase 8
- Phase 7.E/F (Multi-CU + Demo) 同步推迟
- APU 端到端演示仍可基于 Phase 7.B (write-through bypass) 演示

### 6.3 阶段特定风险

详见各微架构 doc 的"风险与缓解"节（如 [`cache-protocol.md`](../modules/cache-protocol.md) §9）。

---

## 7. 反模式（明确不做）

> **来源**: 调研报告 §7 + 用户决策 D1–D5

| 反模式 | 理由 | 替代方案 |
|--------|------|----------|
| ❌ 复制 gem5 `slicc` 文件 | 3000+ 行 Python 难维护；C++ 状态机已足够 | 用 C++ `switch` 表实现简化 6 状态 |
| ❌ 模拟 5-stage pipeline | 仿真目标是大规模 SoC 行为，不是单 GPU cycle | 黑盒 ComputeUnitTLM |
| ❌ 真实 ISA / SIMD | 同上 | 黑盒 |
| ❌ 完整 HSAPP + GPUCommandProcessor + GPUDispatcher | 3000+ 行；AQL 包结构复杂 | 简化 KernelLaunchTLM ~150 行 |
| ❌ dGPU 优先 | APU 形态已 80% 覆盖用户场景；dGPU 工作量 +50% | APU-first 路径，dGPU 备选 |
| ❌ 真实 coalescer 状态机 | 抽象 `coalescing_factor` 参数已足够 | 标量参数 setter |
| ❌ 完整 PCIe ECAM (4KB) | 256B 兼容 config 已足够 dGPU 启动 | 256B 兼容 + 基础 MSI |
| ❌ 真实 VC allocator (4-8 VC/port) | 性能 vs 复杂度权衡 | v0 NUM_VC=1，Phase 7+ 可选 |
| ❌ 真实 PAR-BS 调度器 | 调度器内部细节 | v0 FR-FCFS 简化 |
| ❌ 集成 DRAMSim2 | 外部依赖增加；纯 C++ 简化已足够 | 纯 C++ DRAMCtrlTLM |
| ❌ CPU 模型重做 | 沿用 TrafficGenTLM / CPUTLM 即可 | 复用 v0 |

---

## 8. 兼容性分析

### 8.1 复用 v0 模块清单

| v0 模块 | Phase 7 复用方式 | 影响 |
|---------|-------------------|------|
| `ChStreamModuleBase` | 7.A+ 全部 GPU 模块派生 | ✅ 无修改 |
| `StreamAdapter<>` 单端口 | GPUTLM / ComputeUnitTLM | ✅ 无修改 |
| `MultiPortStreamAdapter<>` | TCC_TLM 多端口 | ✅ 无修改 |
| `BidirectionalPortAdapter<N>` | 4×CU 内部 mesh | ✅ 无修改 |
| `DualPortStreamAdapter` | TCC (TCP 侧 ↔ Memory 侧) | ✅ 无修改（已用于 NICTLM） |
| `ChStreamModuleBase::on_config_loaded()` | JSON 解析 | ✅ 已实施 |
| `tlm_stats::Scalar/Distribution/Average` | 全部 GPU 模块统计 | ✅ 无修改 |
| `bundle_serialization::memcpy` | ComputeReqBundle 序列化 | ✅ POD 兼容 |
| `CoherenceDomain` (基础设施) | Phase 7.C 集成 | ✅ 已实施 |
| `CoherenceState` 枚举 (I/S/E/M/O/T) | MOESI 状态机 | ✅ 已实施 |
| `Bundle` 体系 (CacheReq/Resp) | 字段对位 | ✅ 扩展 ComputeReq 加 GPU 4 字段 |

### 8.2 扩展点（不兼容 v0 的部分）

| 项 | v0 | Phase 7 | 解决方式 |
|----|----|---------|---------|
| **Bundle 扩展** | 8 字段（CacheReqBundle） | +4 字段（ComputeReqBundle） | 组合而非继承（保持 POD 兼容） |
| **MemoryTLM** | 单 Bundle（CacheReq） | 双 Bundle（CacheReq + ComputeReq） | Phase 7.D 扩展注册 |
| **CacheTLM** | 简化协议 | MOESI 6 状态 | Phase 7.C 协议升级（保留 v0 简化版作为 NoncoherentCache） |
| **CrossbarTLM** | 4 CPU 端口 | 4 CPU + N mem 端口 | Phase 7.C 继承扩展为 `CoherentXBarTLM` |
| **StreamAdapter 数量** | 单 / 多 / 双向 / 双端口 | 同 v0 | ✅ 无新形态 |
| **CoherenceDomain** | API + 跨域桥接占位 | 真实集成到 CacheTLM | Phase 7.C 实施 |

### 8.3 Bundle 字段对位表

**ComputeReqBundle vs CacheReqBundle**（Phase 7.A 已实施）:

| 字段 | CacheReq | ComputeReq | 说明 |
|------|----------|------------|------|
| `transaction_id` | ✅ | ✅ | 透传 |
| `parent_id` | ✅ | ✅ | 透传 |
| `fragment_id` | ✅ | ✅ | 透传 |
| `fragment_total` | ✅ | ✅ | 透传 |
| `address` | ✅ | ✅ | 透传 |
| `size` | ✅ | ✅ | 透传 |
| `is_write` | ✅ | ✅ | 透传 |
| `data` | ✅ | ✅ | 透传 |
| `kernel_id` | ❌ | ✅ | GPU 维度（HSAPP AQL dispatch_id） |
| `workgroup_id` | ❌ | ✅ | GPU 维度（CU dispWorkgroup） |
| `wavefront_id` | ❌ | ✅ | GPU 维度（SIMD wfSlotId） |
| `coalescing_factor` | ❌ | ✅ | 抽象（v0=1，Phase 7.B 暴露 setter） |

**关键设计**:
- **组合而非继承**: `ComputeReqBundle` 嵌入（不 `public CacheReqBundle` 继承）8 字段 + 加 4 GPU 字段
- **POD 兼容**: 保持 `memcpy` 序列化兼容（`bundle_serialization.hh`）
- **GPU 字段语义对位 gem5**: `kernel_id ≈ AQL dispatch_id`, `workgroup_id ≈ CU dispWorkgroup`, `wavefront_id ≈ SIMD wfSlotId`

---

## 9. 决策点汇总

| # | 决策 | 阶段 | 采纳方案 | 备选 |
|---|------|------|----------|------|
| **D1** | 协议策略 | 7.A–F | 分步走（Phase 7.A 简化 → Phase 7.C MOESI） | 直接 MOESI / slicc |
| **D2** | CU 粒度 | 7.B+ | 黑盒 | 5-stage pipeline / 完整 SIMD |
| **D3** | Wavefront/Coalescing | 7.B+ | 抽象 `coalescing_factor` 参数 | 完整 coalescer 状态机 |
| **D4** | HSAPP/CP/Dispatcher | 7.B+ | 简化 KernelLaunchTLM ~150 行 | 完整 HSA 三件套 3000+ 行 |
| **D5** | 目录结构 | 7.A+ | `include/tlm/gpu/` 子目录 | 平铺在 `include/tlm/` |
| **D6** | APU-first vs dGPU-first | 7.A+ | APU-first | dGPU-first（高 50% 工作量） |
| **D7** | CacheTLM MOESI 触发降级 | 7.C | 5+ 测试回归 → 回 write-through bypass | 直接修复到底 |
| **D8** | MemoryTLM 双 Bundle | 7.D | 同时注册 CacheReq + ComputeReq | 单一 Bundle + 协议转换 |
| **D9** | TCC 写合并粒度 | 7.D | 64B cache line | 4KB page |
| **D10** | NoC 拓扑 | 7.E | Mesh 4×4 (gem5 蓝图) | Crossbar / Torus |
| **D11** | Coherence 域 | 7.F | 单一 MOESI 域（APU） | 多域 (Phase 7 备选) |
| **D12** | dGPU 触发条件 | 7 备选 | APU 完成后用户决策 | 强制实施 |

---

## 10. 关联文档

### 10.1 源文档

- [调研报告](../research-cpptlm-gpu-fused-soc-survey.md) — gem5 蓝本 6 阶段全景
- [Phase 7.A 实施 spec](../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) — Phase 7.A 代码级实施细节
- [Phase 7.A 实施 plan](../superpowers/plans/2026-06-11-phase7a-gpu-infra.md) — Phase 7.A 任务分解
- [Roadmap Phase 7](../../roadmap.md) — 路线图

### 10.2 微架构文档（30+ 文档，按阶段分组）

**已实施（v2.1 + Phase 7.A）**:
- CPU 端: [`cpu-cputlm.md`](../modules/cpu-cputlm.md) | [`cpu-traffic_gen.md`](../modules/cpu-traffic_gen.md) | [`cpu-cpusim_legacy.md`](../modules/cpu-cpusim_legacy.md)
- 内存: [`memory-memtlm.md`](../modules/memory-memtlm.md)
- Cache: [`cache-l1.md`](../modules/cache-l1.md)
- Interconnect: [`interconnect-crossbar.md`](../modules/interconnect-crossbar.md) | [`interconnect-arbiter.md`](../modules/interconnect-arbiter.md)
- NoC: [`noc-router.md`](../modules/noc-router.md) | [`noc-nic.md`](../modules/noc-nic.md)
- Coherence: [`coherence-domain.md`](../modules/coherence-domain.md)
- RTL: [`rtl-hybrid_cache.md`](../modules/rtl-hybrid_cache.md) | [`rtl-fragment_mapper.md`](../modules/rtl-fragment_mapper.md)
- GPU: [`gpu-gputlm.md`](../modules/gpu-gputlm.md) (Phase 7.A)

**规划中（Phase 7.B–F + 7 备选 dGPU）**:
- GPU 跨阶段: [`gpu.common.md`](../modules/gpu.common.md)
- GPU 端: [`gpu-compute_unit.md`](../modules/gpu-compute_unit.md) | [`gpu-kernel-launch.md`](../modules/gpu-kernel-launch.md) | [`gpu-tcc.md`](../modules/gpu-tcc.md) | [`gpu-pcie_bridge.md`](../modules/gpu-pcie_bridge.md)
- Cache 跨阶段: [`cache.common.md`](../modules/cache.common.md)
- Cache 端: [`cache-l2.md`](../modules/cache-l2.md) | [`cache-protocol.md`](../modules/cache-protocol.md) | [`cache-replacement.md`](../modules/cache-replacement.md) | [`cache-noncoherent.md`](../modules/cache-noncoherent.md)
- Interconnect 端: [`interconnect-bridge.md`](../modules/interconnect-bridge.md) | [`comm_monitor.md`](../modules/comm_monitor.md) | [`coherent_xbar.md`](../modules/coherent_xbar.md) | [`snoop_filter.md`](../modules/snoop_filter.md)
- NoC 端: [`noc.common.md`](../modules/noc.common.md)
- Coherence 端: [`coherence-protocol.md`](../modules/coherence-protocol.md) | [`coherence-bridge.md`](../modules/coherence-bridge.md)
- Memory 端: [`memory-simple.md`](../modules/memory-simple.md) | [`memory-dram.md`](../modules/memory-dram.md) | [`memory-hbm.md`](../modules/memory-hbm.md) | [`memory-qos.md`](../modules/memory-qos.md)
- IO 端: [`io-pio.md`](../modules/io-pio.md) | [`io-dma.md`](../modules/io-dma.md) | [`io-disk.md`](../modules/io-disk.md) | [`io-terminal.md`](../modules/io-terminal.md) | [`io-uart.md`](../modules/io-uart.md) | [`io-ether.md`](../modules/io-ether.md) | [`io-pci.md`](../modules/io-pci.md) | [`io-nvram.md`](../modules/io-nvram.md)

### 10.3 关键参考（gem5 蓝本）

- gem5 APU 形态: `configs/example/apu_se.py`（MI300X 集成卡）
- gem5 dGPU 形态: `configs/example/gem5_library/x86-mi300x-gpu.py`（ViperBoard + HBM2）
- gem5 协议: `src/mem/ruby/protocol/GPU_VIPER.slicc` + `MOESI_AMD_Base-{msg,dir,dma}.sm`

---

## 11. 修订历史

- **2026-06-11**: 调研报告初版（[`docs/research-cpptlm-gpu-fused-soc-survey.md`](../research-cpptlm-gpu-fused-soc-survey.md)） — gem5 蓝本 6 阶段全景
- **2026-06-11**: Phase 7.A 实施 spec 初版（[`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md)） — Phase 7.A 代码级实施
- **2026-06-11**: Phase 7.A 实施 plan 初版（[`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../superpowers/plans/2026-06-11-phase7a-gpu-infra.md)） — Phase 7.A 任务分解
- **2026-06-11**: Phase 7.A 实施 6 commits — GPUTLM v0 上线
- **2026-06-11**: B0/B1/B2 13 个微架构 doc — 已实施模块文档化
- **2026-06-12**: B3.1 + B3.2 共 6 个 GPU 微架构 doc — GPU 路径文档化
- **2026-06-12**: B3.3 共 12 个 Cache/Interconnect/NoC/Coherence 微架构 doc
- **2026-06-12**: B3.4 共 12 个 Memory/IO 微架构 doc
- **2026-06-12**: **APU 整合 spec v1.0**（位于 `docs/superpowers/specs/`，混合架构+实施）
- **2026-06-12**: **本文档 v2.0** — 纯架构 spec，移至 `docs/soc_arch/specs/`；移除代码片段/文件路径/LOC/测试命令；补充架构图表与设计说明；保留 D1–D12 决策；引用 [`docs/superpowers/specs/`](../superpowers/specs/) 实施 spec 作为代码级参考
- **Phase 7.B (未来)**: 实施后更新本文档（CU 黑盒架构增量化）
- **Phase 7.C (未来)**: 7.C 实施 + 风险 R1 状态更新
- **Phase 7.D (未来)**: 7.D 实施 + R5 修复确认
- **Phase 7.E (未来)**: 7.E 实施 + 4×CU mesh 架构图
- **Phase 7.F (未来)**: APU SoC Demo 上线 + 端到端架构验证
- **Phase 7 备选 dGPU (未来)**: dGPU 启动决策 + Disjoint NoC 架构

---

**关联文档清单**:
- 调研报告: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../research-cpptlm-gpu-fused-soc-survey.md)
- Phase 7.A 实施 spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md)
- Phase 7.A 实施 plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../superpowers/plans/2026-06-11-phase7a-gpu-infra.md)
- 微架构文档索引: [`docs/soc_arch/modules/`](../modules/)
- Roadmap Phase 7: [`roadmap.md`](../../roadmap.md) §Phase 7
- APU 模块 README: [`docs/soc_arch/modules/README.md`](../modules/README.md)
