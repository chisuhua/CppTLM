# Phase 7 — APU CPU+GPGPU 融合 SoC 整体架构设计

> **Document ID**: IMPL-011-Phase7
> **Version**: 2.0
> **Date**: 2026-06-12
> **Status**: 🔄 Draft（待用户 review）
> **Author**: 整合自 Phase 7 调研报告 + Phase 7.A spec + Phase 7.A plan
> **Parent Roadmap**: [`roadmap.md`](../../roadmap.md) Phase 7（6 子阶段 7.A–7.F）
> **Research Reference**: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md)
> **参考架构**: [`docs/soc_arch/specs/apu-soc-design.md`](../../soc_arch/specs/apu-soc-design.md) — 本文架构纯参考（语言 + 图表）
> **子文档**:
> - Phase 7.A 详细 spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](./2026-06-11-phase7a-gpu-infra-design.md)
> - Phase 7.A 实施 plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../plans/2026-06-11-phase7a-gpu-infra.md)
> **关联微架构文档**: [`docs/soc_arch/modules/`](../../soc_arch/modules/)（30+ 文档，详见 §11）

> **本文档定位**: **实施视角**——含代码片段、文件清单、LOC 估算、验收标准、阶段任务分解。**架构参考**见 [`docs/soc_arch/specs/apu-soc-design.md`](../../soc_arch/specs/apu-soc-design.md)。

---

## 0. 阅读引导

本文档是 **Phase 7 APU CPU+GPGPU 融合 SoC 的顶层架构设计**——覆盖 6 个子阶段（7.A–7.F），整合 3 份历史文档的差异：

| 源文档 | 范围 | 行数 | 在本文档的角色 |
|--------|------|------|---------------|
| 调研报告 | 6 阶段全景 + 决策 D1–D5 + gem5 蓝本 | 1004 | §2 架构总览 + §4 设计决策 + §6 阶段实施细节 |
| Phase 7.A spec | 仅 7.A 详细设计（Bundle + GPUTLM） | 660 | §6.1 引用：Phase 7.A 详尽内容 |
| Phase 7.A plan | 仅 7.A 任务分解 | 972 | §6.1 引用：Phase 7.A 具体实施步骤 |

**本文档结构**:
- **§1 范围与目标** — APU SoC 整体定义 + 6 子阶段总览
- **§2 架构总览** — SoC 拓扑、模块布局、Coherence 域
- **§3 阶段-模块依赖矩阵** — 6 阶段 vs 已实现 + 规划中模块
- **§4 关键设计决策 D1–D5** — 协议、CU 粒度、Coalescing、HSA 简化、目录结构
- **§5 微架构文档索引** — 30+ 文档按模块类型分组
- **§6 阶段实施细节** — 6 子阶段逐个简述（7.A 引用详细 spec/plan）
- **§7 验收标准** — 整体 APU SoC 演示 + 阶段 gate
- **§8 风险与缓解** — R1–R8 跨阶段风险
- **§9 反模式（明确不做）** — 抄 gem5 slicc / 完整 ISA / 真实 GPU pipeline
- **§10 与已有架构兼容性** — 复用 v0 模块、Bundle 扩展、StreamAdapter 适配
- **§11 文件清单 + LOC 估算** — 6 阶段新增/修改文件汇总
- **§12 决策点汇总** — D1–D5 + 各阶段子决策
- **§13 修订历史**

**读者路径**:
- 关心"做不做 / 顺序" → 读 §1, §3, §7, §8
- 关心"为什么" → 读 §2, §4, §9, §10
- 关心"怎么做" → 读 §5（微架构 doc 索引）+ §6.1–§6.6 + 引用子 spec/plan
- 关心"架构概览"（参考） → 读 [`docs/soc_arch/specs/apu-soc-design.md`](../../soc_arch/specs/apu-soc-design.md)

---

## 1. 范围与目标（Scope & Goals）

### 1.1 Phase 7 总体目标

在 CppTLM v2.1 基础上**端到端实现 APU 形态 CPU+GPGPU 融合 SoC 仿真**——2 个 CPU 流量源 + 4 个 GPU Compute Unit + 共享 memory + 单一 Coherence 域，对位 gem5 `configs/example/apu_se.py` 的 MI300X 集成卡形态。

**核心目标**:
- ✅ **Bundle 扩展**：在 CacheReq/Resp 基础上扩展 GPU 维度字段（kernel_id / workgroup_id / wavefront_id / coalescing_factor）
- ✅ **GPGPU 黑盒**：ComputeUnitTLM（黑盒）+ TCC（GPU L2）+ Kernel Launch 简化版 HSA Dispatcher
- ✅ **APU coherence**：跨 CPU↔GPU Cache 共享 MOESI 域
- ✅ **GPU memory hierarchy**：GPU 走 TCC → DRAM（HBM-like 延迟）
- ✅ **多 CU 并行**：4 CU 经 GPU 内部 mesh NoC 路由到 TCC
- ✅ **Full APU SoC Demo**：JSON 配置 + Python 端到端测试 + 统计 dashboard

**显式不在范围**:
- ❌ CPU 建模改进（沿用 `TrafficGenTLM` / `CPUTLM`）
- ❌ dGPU 离散卡 + PCIe + Disjoint Network（Phase 7 完成后评估，见 §6.6 备选）
- ❌ 真实 GPU 内部微架构（5-stage pipeline / SIMD / LDS / wavefront 调度）—— 黑盒替代
- ❌ 完整 slicc 协议栈（用 C++ `switch` 表简化）

### 1.2 6 子阶段总览

| 子阶段 | 主题 | 状态 | 风险 | 工期 | 关键交付 |
|--------|------|------|------|------|----------|
| **7.A** | GPU 基础设施 | ✅ **已完成**（Phase 7.A commit `828f037`） | Medium | 2-3 周 | `ComputeReqBundle` / `GPUTLM v0` |
| **7.B** | ComputeUnit 黑盒 | 🟡 Pending | Medium-High | 3-4 周 | `ComputeUnitTLM` + `KernelLaunchTLM` |
| **7.C** | Coherence Protocol 集成 | 🟡 Pending | **High（最高风险）** | 4-5 周 | `CacheTLM` MOESI + `CoherenceDomain` 集成 |
| **7.D** | TCC Bridge + 内存层次 | 🟡 Pending | Medium | 3-4 周 | `TCC_TLM` + `MemoryTLM` HBM mode |
| **7.E** | Multi-CU + NoC 集成 | 🟡 Pending | Medium | 2-3 周 | 4× CU + 内部 mesh（复用 RouterTLM） |
| **7.F** | Full APU SoC Demo | 🟡 Pending | Low-Medium | 2-3 周 | `apu_full_soc.json` + Python e2e 测试 |
| **7 备选 dGPU** | Discrete GPU + HBM2 | 🟡 Pending（仅 APU 完成后评估） | Medium | 4-6 周 | `PCIBridgeTLM` + disjoint NoC |

**累计工作量**: 16-22 周（4-5.5 月）—— **APU-first 路径**。

### 1.3 命名约定澄清

历史文档中"Phase 0–5" 与本项目路线图"Phase 7.A–7.F" 是**同一组阶段的两种命名**：

| 调研报告命名 | 本文档 + roadmap 命名 | 阶段主题 |
|--------------|----------------------|----------|
| Phase 0 | **Phase 7.A** | GPU 基础设施 |
| Phase 1 | **Phase 7.B** | ComputeUnit 黑盒 |
| Phase 2 | **Phase 7.C** | Coherence Protocol 集成 |
| Phase 3 | **Phase 7.D** | TCC Bridge + 内存层次 |
| Phase 4 | **Phase 7.E** | Multi-CU + NoC 集成 |
| Phase 5 | **Phase 7.F** | Full APU SoC Demo |

**本项目采用 Phase 7.A–F 命名**（与 `roadmap.md` 一致）。调研报告中的"Phase 0–5"是早期命名。

---

## 2. 架构总览

> **参考**: 架构图表与设计说明见 [`docs/soc_arch/specs/apu-soc-design.md` §2](../../soc_arch/specs/apu-soc-design.md)。

### 2.1 APU SoC 拓扑（最终态 — Phase 7.F）

```
┌─────────────────────────────────────────────────────────────────────┐
│                          APU Domain（MOESI）                        │
│                                                                     │
│  ┌──── CPU Cluster ─────┐    ┌──── GPU Cluster ─────┐               │
│  │                      │    │                      │               │
│  │  ┌────────────┐      │    │  ┌────────────┐      │               │
│  │  │ TrafficGen │ ──┐  │    │  │ ComputeUnit│ ──┐  │               │
│  │  │   TLM #0   │   │  │    │  │   TLM #0   │   │  │               │
│  │  └────────────┘   │  │    │  └────────────┘   │  │               │
│  │                   │  │    │                   │  │               │
│  │  ┌────────────┐   │  │    │  ┌────────────┐   │  │               │
│  │  │ TrafficGen │ ──┤  │    │  │ ComputeUnit│ ──┤  │               │
│  │  │   TLM #1   │   │  │    │  │   TLM #1   │   │  │               │
│  │  └────────────┘   │  │    │  └────────────┘   │  │               │
│  │                   │  │    │         ⋮          │  │               │
│  │                   ▼  │    │  ┌────────────┐   │  │               │
│  │  ┌────────────┐      │    │  │ ComputeUnit│ ──┘  │               │
│  │  │ CacheTLM   │      │    │  │   TLM #3   │      │               │
│  │  │ (MOESI)    │      │    │  └────────────┘      │               │
│  │  └────────────┘      │    │                      │               │
│  │         ⋮            │    │                      │               │
│  │  ┌────────────┐      │    │  ┌────────────┐      │               │
│  │  │ CacheTLM   │      │    │  │   TCC      │      │               │
│  │  │ (MOESI)    │      │    │  │   TLM      │      │               │
│  │  └────────────┘      │    │  │ (write-coalesce)│               │
│  │                      │    │  └────────────┘      │               │
│  └──────────┬───────────┘    └──────────┬───────────┘               │
│             │                           │                           │
│             ▼                           ▼                           │
│  ┌────────────────────────────────────────────────────────┐        │
│  │            CoherentXBarTLM (snoop broadcast)           │        │
│  │    - get_snoop_targets(addr) ← SnoopFilter             │        │
│  │    - 广播 snoop 到同域 cache                             │        │
│  └────────────────────────┬───────────────────────────────┘        │
│                           │                                        │
│                           ▼                                        │
│  ┌────────────────────────────────────────────────────────┐        │
│  │              MemoryTLM (HBM 模式)                       │        │
│  │    - rd 100cy / wr 120cy                                │        │
│  │    - 双 Bundle 注册（CacheReq + ComputeReq）            │        │
│  └────────────────────────────────────────────────────────┘        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 6 阶段递进（与最终态的差异）

| 阶段 | 与最终态的差异 | 简述 |
|------|---------------|------|
| **7.A** | 1 GPUTLM → 1 MemoryTLM 直连，bypass Cache/Crossbar | 验证 GPU Bundle 类型能在 StreamAdapter 管线中流通 |
| **7.B** | 1 CU + 1 TrafficGen + 1 CacheTLM + 1 CrossbarTLM + 1 MemoryTLM | CU 黑盒可发起 kernel 请求；CPU + GPU 共享 crossbar |
| **7.C** | + SnoopFilter + CoherentXBar（带 snoop 广播） | CacheTLM MOESI 化，CPU↔GPU 共享 coherence 域 |
| **7.D** | + TCC_TLM（CU 与 Memory 之间）+ Memory HBM mode | TCC 做 write coalescing + coherent probe fan-in |
| **7.E** | 4×CU + 1×GPU Crossbar（mesh 拓扑，复用 RouterTLM） | 多 CU 并行 kernel 请求经 mesh NoC 路由 |
| **7.F** | 完整 APU 配置（2 TrafficGen + 4 CU + 全套 coherence + TCC + HBM） | 端到端 APU SoC 演示 + Python 测试 + 统计 dashboard |

### 2.3 Coherence 域策略

**APU 形态（Phase 7 路径）**:
- 单一 `CoherenceDomain("apu_domain", MOESI_AMD)` 覆盖所有 CPU/GPU cache + TCC
- 简化协议：6 状态机（I/S/E/M/O/T），C++ `switch` 表实现
- SnoopFilter 减少冗余 snoop 流量

**dGPU 形态（Phase 7 备选，APU 完成后评估）**:
- 双独立 CoherenceDomain（Host APU + dGPU），通过 `CoherenceBridge` 桥接
- 协议转换（MOESI_AMD ↔ MESI_GPU 或 MOESI_AMD ↔ MOESI_AMD）
- 地址映射（Host 物理地址 ↔ dGPU VRAM）
- Disjoint NoC（双独立 NoC，通过 BridgeTLM 桥接）
- **不在 Phase 7 主路径**（用户决策 D2：APU-first）

---

## 3. 阶段-模块依赖矩阵

### 3.1 6 阶段 vs 已实现模块（v2.1）

| 阶段 | 依赖已实现模块 | 关键复用 |
|------|----------------|----------|
| **7.A** | `ChStreamModuleBase` + `StreamAdapter` 单端口 + Bundle 体系 | (无) |
| **7.B** | 7.A 全部 | `TrafficGenTLM` / `CrossbarTLM` / `MemoryTLM` 复用 |
| **7.C** | 7.A + `CoherenceDomain` (基础设施已实施) | `CoherenceState` 枚举 + `home_node` 机制 |
| **7.D** | 7.A + 7.C + `DualPortStreamAdapter` (NICTLM 已用) | `MemoryTLM` 扩展（双 Bundle 注册） |
| **7.E** | 7.B + 7.D + `RouterTLM` / `LinkTLM` / `BidirectionalPortAdapter` | `NoCFlitBundle` 已有 |
| **7.F** | 7.A–7.E 全部 | 全部 + `cpptlm` Python 库 |

### 3.2 6 阶段 vs 规划中模块（微架构 doc 索引）

| 阶段 | 依赖规划中模块（🟡） | 微架构 doc |
|------|---------------------|-----------|
| **7.B** | `ComputeUnitTLM` + `KernelLaunchTLM` | [`gpu-compute_unit.md`](../../soc_arch/modules/gpu-compute_unit.md) + [`gpu-kernel-launch.md`](../../soc_arch/modules/gpu-kernel-launch.md) |
| **7.C** | `CacheTLM` 升级 + `CoherenceDomain` 集成 + `SnoopFilter` + `CoherentXBar` | [`cache-protocol.md`](../../soc_arch/modules/cache-protocol.md) + [`coherence-protocol.md`](../../soc_arch/modules/coherence-protocol.md) + [`snoop_filter.md`](../../soc_arch/modules/snoop_filter.md) + [`coherent_xbar.md`](../../soc_arch/modules/coherent_xbar.md) |
| **7.D** | `TCC_TLM` + `MemoryTLM` HBM 模式（双 Bundle） | [`gpu-tcc.md`](../../soc_arch/modules/gpu-tcc.md) + [`memory-hbm.md`](../../soc_arch/modules/memory-hbm.md) |
| **7.E** | `NoCTopologyBuilder`（规划中）+ 4×CU mesh | [`noc.common.md`](../../soc_arch/modules/noc.common.md) |
| **7.F** | 全部 | (集成) |
| **7 备选 dGPU** | `PCIBridgeTLM` + `CoherenceBridge` | [`gpu-pcie_bridge.md`](../../soc_arch/modules/gpu-pcie_bridge.md) + [`coherence-bridge.md`](../../soc_arch/modules/coherence-bridge.md) |

---

## 4. 关键设计决策 D1–D5

> **来源**: 调研报告 §3 + 用户 2026-06-11 决策

### 4.1 D1 协议策略（分步走）

| 阶段 | 协议 | 实现 |
|------|------|------|
| Phase 7.A | 简化 I/S/M 三态 | write-through bypass（仅 GPUTLM，无 cache） |
| Phase 7.B | 简化 I/S/M | write-through（CU + CPU 共用 CacheTLM 简化协议） |
| Phase 7.C | **MOESI 6 状态** | 完整 `CoherenceProtocol` + 6×6 状态转换表 + SnoopFilter |
| Phase 7.D | MOESI + GPU TCP/TCC 扩展 | 跨域 snoop fan-in |
| Phase 7.E | 同 7.D | 多 CU 并发 snoop fanout |
| Phase 7.F | 同 7.D | 端到端验证 |

**触发降级条件**: 5+ 测试回归 → 回 Phase 7.B write-through bypass。

### 4.2 D2 CU 粒度（黑盒优先）

**不模拟**: 5-stage pipeline / ISA / SIMD / register file / LDS / wavefront 调度

**只模拟**:
- `ComputeUnitTLM` 通过 tick() 周期发出 `ComputeReqBundle`
- 维护 `inflight_kernel_reqs_` map + `workgroup_progress_`
- 跟踪 kernel 完成时间 + kernel launch latency

**理由**: CppTLM 仿真目标是大规模 SoC 行为（CPU+GPU 协同 + coherence + NoC 拥塞），不需要精确到每个 GPU 时钟周期。

### 4.3 D3 Wavefront/Coalescing（抽象）

**真实模型**: gem5 `VIPERCoalescer::coalesce_factor` 计算相邻 wavefront 的访存合并
**CppTLM 抽象**: 引入 `coalescing_factor` 标量参数（v0=1，Phase 7.B 暴露 setter），不精确模拟 coalescer 状态机

**理由**: coalescer 行为是 GPU 内部细节，对 APU SoC 行为（coherence 流量、NoC 拥塞）影响有限；抽象为参数即可。

### 4.4 D4 HSAPP/CP/Dispatcher（极致简化）

**真实模型**: gem5 `HSAPacketProcessor` + `GPUCommandProcessor` + `GPUDispatcher`（合计 3000+ 行 Python+C++）
**CppTLM 简化**: `KernelLaunchTLM` ~150 行，tick() 中按 `kernel_issue_interval_` 周期发 kernel launch 命令

**理由**: HSA runtime 细节对 SoC 行为影响小；只需模拟"CPU 发 launch → GPU 收到 → 开始计算"的事件流。

### 4.5 D5 目录结构（`include/tlm/gpu/` 子目录）

```
include/tlm/
├── cache_tlm.hh          (v0)
├── memory_tlm.hh         (v0)
├── crossbar_tlm.hh       (v0)
├── arbiter_tlm.hh        (v0)
├── router_tlm.hh         (v0)
├── nic_tlm.hh            (v0)
├── link_tlm.hh           (v0)
├── cpu_tlm.hh            (v0)
├── traffic_gen_tlm.hh    (v0)
└── gpu/                  (Phase 7 新建子目录)
    ├── gpu_tlm.hh        (Phase 7.A ✅)
    ├── compute_unit_tlm.hh   (Phase 7.B 🟡)
    ├── kernel_launch_tlm.hh  (Phase 7.B 🟡)
    ├── tcc_tlm.hh            (Phase 7.D 🟡)
    └── pcie_bridge_tlm.hh    (Phase 7 备选 dGPU 🟡)
```

**理由**: 与 `cpptlm::rtl::*`（RTL 桥接）、`tlm::*`（TCC 等顶层 TLM 模块）形成清晰的命名空间分层；GPU 模块数量 ≥ 5，独立子目录合理。

---

## 5. 微架构文档索引

**完整 30+ 微架构文档位于 [`docs/soc_arch/modules/`](../../soc_arch/modules/)**。本节按模块类型分组索引（与 §3 阶段-模块依赖对应）：

### 5.1 已实施模块（v2.1 + Phase 7.A）

| 模块 | 微架构 doc | 状态 |
|------|-----------|------|
| `CPUTLM` | [`cpu-cputlm.md`](../../soc_arch/modules/cpu-cputlm.md) | ✅ |
| `TrafficGenTLM` | [`cpu-traffic_gen.md`](../../soc_arch/modules/cpu-traffic_gen.md) | ✅ |
| `CPUSim` (legacy) | [`cpu-cpusim_legacy.md`](../../soc_arch/modules/cpu-cpusim_legacy.md) | ⚠️ Legacy |
| `MemoryTLM` | [`memory-memtlm.md`](../../soc_arch/modules/memory-memtlm.md) | ✅ |
| `CacheTLM` (v0) | [`cache-l1.md`](../../soc_arch/modules/cache-l1.md) | ✅ |
| `CrossbarTLM` | [`interconnect-crossbar.md`](../../soc_arch/modules/interconnect-crossbar.md) | ✅ |
| `ArbiterTLM` | [`interconnect-arbiter.md`](../../soc_arch/modules/interconnect-arbiter.md) | ✅ |
| `RouterTLM` | [`noc-router.md`](../../soc_arch/modules/noc-router.md) | ✅ |
| `NICTLM` | [`noc-nic.md`](../../soc_arch/modules/noc-nic.md) | ✅ |
| `LinkTLM` | (暂无独立 doc，集成在 noc.common) | ✅ |
| `CoherenceDomain` | [`coherence-domain.md`](../../soc_arch/modules/coherence-domain.md) | ✅ |
| `HybridCacheComponent` | [`rtl-hybrid_cache.md`](../../soc_arch/modules/rtl-hybrid_cache.md) | ✅ |
| `FragmentMapper` | [`rtl-fragment_mapper.md`](../../soc_arch/modules/rtl-fragment_mapper.md) | ✅ |
| `GPUTLM` (v0) | [`gpu-gputlm.md`](../../soc_arch/modules/gpu-gputlm.md) | ✅ Phase 7.A |

### 5.2 规划中模块（Phase 7.B–F + 7 备选 dGPU）

| 模块 | 微架构 doc | 引入阶段 |
|------|-----------|----------|
| GPU 跨阶段通用 | [`gpu.common.md`](../../soc_arch/modules/gpu.common.md) | Phase 7.B+ |
| `ComputeUnitTLM` | [`gpu-compute_unit.md`](../../soc_arch/modules/gpu-compute_unit.md) | Phase 7.B |
| `KernelLaunchTLM` | [`gpu-kernel-launch.md`](../../soc_arch/modules/gpu-kernel-launch.md) | Phase 7.B |
| `TCC_TLM` | [`gpu-tcc.md`](../../soc_arch/modules/gpu-tcc.md) | Phase 7.D |
| `PCIBridgeTLM` | [`gpu-pcie_bridge.md`](../../soc_arch/modules/gpu-pcie_bridge.md) | Phase 7 备选 dGPU |
| Cache 跨阶段通用 | [`cache.common.md`](../../soc_arch/modules/cache.common.md) | Phase 7.C+ |
| `L2CacheTLM` | [`cache-l2.md`](../../soc_arch/modules/cache-l2.md) | Phase 7.E |
| `CacheProtocol` 6×6 | [`cache-protocol.md`](../../soc_arch/modules/cache-protocol.md) | Phase 7.C |
| Replacement 4 策略 | [`cache-replacement.md`](../../soc_arch/modules/cache-replacement.md) | Phase 7.C+ |
| `NoncoherentCache` | [`cache-noncoherent.md`](../../soc_arch/modules/cache-noncoherent.md) | Phase 7.D+ |
| `CoherentXBarTLM` | [`coherent_xbar.md`](../../soc_arch/modules/coherent_xbar.md) | Phase 7.C |
| `SnoopFilterTLM` | [`snoop_filter.md`](../../soc_arch/modules/snoop_filter.md) | Phase 7.C |
| `CoherenceProtocol` | [`coherence-protocol.md`](../../soc_arch/modules/coherence-protocol.md) | Phase 7.C |
| `CoherenceBridge` | [`coherence-bridge.md`](../../soc_arch/modules/coherence-bridge.md) | Phase 7 备选 dGPU |
| `BridgeTLM` (链路) | [`interconnect-bridge.md`](../../soc_arch/modules/interconnect-bridge.md) | Phase 7.D+ |
| `CommMonitorTLM` | [`comm_monitor.md`](../../soc_arch/modules/comm_monitor.md) | Phase 7.B+ |
| NoC 跨阶段通用 | [`noc.common.md`](../../soc_arch/modules/noc.common.md) | Phase 7.E+ |
| `SimpleMemoryTLM` | [`memory-simple.md`](../../soc_arch/modules/memory-simple.md) | Phase 7.D+ |
| `DRAMCtrlTLM` | [`memory-dram.md`](../../soc_arch/modules/memory-dram.md) | Phase 7.D+ |
| `HBMTLM` | [`memory-hbm.md`](../../soc_arch/modules/memory-hbm.md) | Phase 7 备选 dGPU |
| `QoSMemCtrl` | [`memory-qos.md`](../../soc_arch/modules/memory-qos.md) | Phase 7.D+ |
| IO 全套 | [`io-pio.md`/`io-dma.md`/`io-disk.md`/`io-terminal.md`/`io-uart.md`/`io-ether.md`/`io-pci.md`/`io-nvram.md`](../../soc_arch/modules/) | Phase 7+ 备选 |

---

## 6. 阶段实施细节

### 6.1 Phase 7.A — GPU 基础设施（✅ 已完成）

**详细 spec**: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](./2026-06-11-phase7a-gpu-infra-design.md)
**详细 plan**: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../plans/2026-06-11-phase7a-gpu-infra.md)

**Scope 摘要**:
- 新建 `include/bundles/compute_bundles_tlm.hh`：`ComputeReqBundle` / `ComputeRespBundle`（CacheReqBundle 基础上加 GPU 4 字段）
- 新建 `include/tlm/gpu/gpu_tlm.hh`：`GPUTLM` v0（5 setter + tick() + 7 StatGroup）
- 扩展 `include/chstream_register.hh` 注册
- `configs/gpu_standalone.json` 端到端验证
- `test/test_gpu_standalone.cc` Catch2 测试（5/5 通过）

**实施 commits**: `8288ad1` / `828f037` / `fb6011b` / `4f07890` / `763d8d7` / `a1dc8b4`

**验收**: ✅ 5/5 GPU 测试通过；GPUTLM v0 在 StreamAdapter 管线中可流通。

**后续阶段依赖**: 7.B 复用 `ComputeReqBundle` + GPUTLM 的 setter 模式（`on_config_loaded` JSON 解析）。

### 6.2 Phase 7.B — ComputeUnit 黑盒（🟡 Pending）

**微架构 doc**: [`gpu-compute_unit.md`](../../soc_arch/modules/gpu-compute_unit.md) + [`gpu-kernel-launch.md`](../../soc_arch/modules/gpu-kernel-launch.md) + [`gpu.common.md`](../../soc_arch/modules/gpu.common.md)

**Scope**:
- 新建 `include/tlm/gpu/compute_unit_tlm.hh`：`ComputeUnitTLM`（黑盒发起器，与 GPUTLM 共享基类）
  - 抽出 `compute_unit_base` 共享基类
  - tick() 中按 `kernel_issue_interval_` 发 `ComputeReqBundle`（read/write）
  - 维护 `inflight_kernel_reqs_` map（含 `workgroup_id` 维度）
  - 跟踪 `workgroup_progress_`
- 新建 `include/tlm/gpu/kernel_launch_tlm.hh`：`KernelLaunchTLM`（简化版 HSA Dispatcher，~150 行）
  - tick() 中按 `kernel_launch_interval_` 发 kernel launch 命令
  - 接受 `HSAQueueDescriptor` 注入（CPU 配置）
- `GPUTLM` 升级：实现 `on_config_loaded` 读 JSON params（与 `TrafficGenTLM` 现存风格对齐）
- CrossbarTLM 扩展：支持 GPU 类请求的地址路由（高位地址映射到 GPU memory region）
- CacheTLM 扩展（可选）：在 CacheReqBundle 中识别 `is_gpu_request = true` → 绕过 CPU 侧 cache 一致性（GPU 请求走 bypass 路径）

**为什么不模拟 pipeline**: 见 D2 决策。

**Acceptance criteria**:
```json
// configs/apu_demo_v1.json: 2 TrafficGenTLM + 1 ComputeUnitTLM + 1 CrossbarTLM + 1 MemoryTLM
```
```bash
./build/bin/cpptlm_tests "[gpu][phase7]"  # CU 黑盒 + Phase 7.B 集成
# 统计: kernel 请求完成计数 > 0
```

**估计工作量**: 3-4 周

### 6.3 Phase 7.C — Coherence Protocol 集成（🟡 Pending, **最高风险**）

**微架构 doc**: [`cache-protocol.md`](../../soc_arch/modules/cache-protocol.md) + [`coherence-protocol.md`](../../soc_arch/modules/coherence-protocol.md) + [`snoop_filter.md`](../../soc_arch/modules/snoop_filter.md) + [`coherent_xbar.md`](../../soc_arch/modules/coherent_xbar.md) + [`cache.common.md`](../../soc_arch/modules/cache.common.md)

**Scope 摘要**:
- `CacheTLM` 升级：扩展 `CacheLine = {data, CoherenceState, sharers_bitmask}` + 6×6 状态转换表
- `CoherenceDomain` 集成：`ModuleFactory` 创建 CacheTLM 时查找所属 CoherenceDomain → 注册 snoop callback
- `CoherentXBarTLM`：继承 v0 CrossbarTLM + snoop 广播 + SnoopFilter 集成
- `SnoopFilterTLM`（2 种实现：SnoopFilterCache / SnoopFilterInvalidator）
- `CoherenceProtocol` 抽象（多协议实现 + `translate()` 协议转换）

**详细 6×6 状态转换**: 见 [`cache-protocol.md` §4](../../soc_arch/modules/cache-protocol.md)。

**R1 风险**: `cache-protocol.md` 死锁 / livelock + 触发升级条件（5+ 回归 → 回 write-through bypass）

**Acceptance criteria**:
```json
// configs/apu_demo_v2.json:
// cpu_domain { TrafficGenTLM + CacheTLM + CacheTLM } + gpu_domain { ComputeUnitTLM + CacheTLM }
// 跨域通过 ProtocolBridge
```
```bash
./build/bin/cpptlm_tests "[coherence][gpu]"  # MESI 6 状态转换
```

**估计工作量**: 4-5 周

### 6.4 Phase 7.D — TCC Bridge + 内存层次（🟡 Pending）

**微架构 doc**: [`gpu-tcc.md`](../../soc_arch/modules/gpu-tcc.md) + [`memory-hbm.md`](../../soc_arch/modules/memory-hbm.md)

**Scope 摘要**:
- 新建 `include/tlm/gpu/tcc_tlm.hh`：`TCC_TLM`（GPU L2）
  - 使用 `DualPortStreamAdapter`（TCP 侧 ↔ Directory/Memory 侧）
  - write coalescing（同一 address 连续写合并）
  - snoop fan-in 收集
- `MemoryTLM` 升级：扩展 `hbm_mode` 参数（高带宽高延迟）+ 接受 ComputeReqBundle/RespBundle（**双 Bundle 注册解决 R5 风险**）

**R5 风险**: MemoryTLM 不接受 ComputeReqBundle（v0 测试用 AdapterHandle 绕开）→ Phase 7.D 修复。

**Acceptance criteria**:
```json
// configs/apu_demo_v3.json: ComputeUnitTLM → TCC_TLM → MemoryTLM (HBM mode)
```
```bash
./build/bin/cpptlm_tests "[gpu][tcc]"  # TCC write coalescing
```

**估计工作量**: 3-4 周

### 6.5 Phase 7.E — Multi-CU + NoC 集成（🟡 Pending）

**微架构 doc**: [`noc.common.md`](../../soc_arch/modules/noc.common.md) + `gpu.commond.md` (已实施)

**Scope 摘要**:
- `ComputeUnitTLM` 数组模式：JSON 参数 `num_cus=4` 创建多 CU 实例（gem5 `CUs = [ComputeUnit(...) for _ in range(num_cu)]`）
- 复用 `RouterTLM` / `LinkTLM` / `NoCFlitBundle`（已存在）
- GPU 内部 mesh 拓扑：CU ↔ GPU Crossbar ↔ TCC

**不实施**（v0 简化）:
- 真实 VC allocator（`v0 NUM_VC=1`）
- 完整 PAR-BS 调度器（v0 用 FR-FCFS 简化）
- 自适应路由（v0 用 XY 路由）

**Acceptance criteria**:
```json
// configs/apu_demo_v4.json: 4× ComputeUnit + 1× GPUCrossbar + 1× TCC + 1× Memory
```
```bash
./build/bin/cpptlm_tests "[gpu][noc]"  # 4 CU 并发路由
```

**估计工作量**: 2-3 周

### 6.6 Phase 7.F — Full APU SoC Demo（🟡 Pending）

**Scope 摘要**:
- 完整 `configs/apu_full_soc.json`：
  - 2× TrafficGenTLM + 2× CacheTLM（CPU 侧）
  - 4× ComputeUnitTLM + 4× GPU 端 CacheTLM + 1× TCC（GPU 侧）
  - 1× CoherentXBarTLM + 1× MemoryTLM（HBM 模式）
  - 单一 `CoherenceDomain("apu_domain", MOESI_AMD)`
- Python 端到端测试 `test/python/test_apu_soc.py`
- 统计 dashboard 扩展：
  - GPU utilization（active CU 比例）
  - Kernel completion time（p50/p99）
  - Cache hit/miss rate（per cache）
  - NoC latency（flit E2E）
  - TCC coalescing ratio（合并写 / 总写）
  - Coherence 流量（snoop req/sec）

**Acceptance criteria**:
```bash
./cpptlm --config configs/apu_full_soc.json
./build/bin/cpptlm_tests "[apu_full_soc]"
python test/python/test_apu_soc.py
# 统计 dashboard 渲染
```

**估计工作量**: 2-3 周

### 6.7 Phase 7 备选 dGPU（🟡 Pending, **仅 APU 完成后评估**）

**微架构 doc**: [`gpu-pcie_bridge.md`](../../soc_arch/modules/gpu-pcie_bridge.md) + [`coherence-bridge.md`](../../soc_arch/modules/coherence-bridge.md) + [`memory-hbm.md`](../../soc_arch/modules/memory-hbm.md)

**Scope 摘要**（用户决策 D2：APU-first，本阶段仅 APU 完成后启动）:
- 新建 `include/tlm/gpu/pcie_bridge_tlm.hh`：`PCIBridgeTLM`（双独立 coherence 域桥接，latency 100-500 cyc）
- `HBM_TLM`（独立 HBM2 设备内存控制器，gem5 HBM2Stack 蓝图）
- 拓扑 DSL 扩展：Disjoint Network（双独立 NoC）
- `CoherenceBridge`（同协议透传 + 跨协议转换）

**触发条件**: Phase 7.F 完成后用户决策启动。

**估计工作量**: 4-6 周

---

## 7. 验收标准

### 7.1 阶段 Gate（每阶段完成门槛）

| 阶段 | 编译 | 单测 | 端到端 | 文档 | 零债务 |
|------|------|------|--------|------|--------|
| 7.A | ✅ | 5/5 GPU 测试 | `gpu_standalone.json` | 13 微架构 doc（含 gpu-gputlm） | ✅ |
| 7.B | ✅ | `[gpu][phase7]` | `apu_demo_v1.json` | +3 doc（compute_unit / kernellaunch / gpu.common） | ✅ |
| 7.C | ✅ | `[coherence][gpu]` | `apu_demo_v2.json` | +5 doc（cache 4 + coherent_xbar 等） | ✅ |
| 7.D | ✅ | `[gpu][tcc]` | `apu_demo_v3.json` | +2 doc（tcc / memory-hbm） | ✅ |
| 7.E | ✅ | `[gpu][noc]` | `apu_demo_v4.json` | +1 doc（noc.common） | ✅ |
| 7.F | ✅ | `[apu_full_soc]` | `apu_full_soc.json` + Python e2e | APU 整体 spec（本文档）+ 整合 spec | ✅ |

### 7.2 整体 APU SoC 演示（Phase 7.F 终点）

**功能验证**:
- ✅ 2 TrafficGenTLM + 4 ComputeUnitTLM 并发运行
- ✅ 端到端 kernel 启动 → CU 执行 → TCC 聚合 → Memory 写
- ✅ Coherence 跨域：CPU 写 → GPU snoop → GPU cache invalidate → GPU 重读
- ✅ NoC 路由：4 CU 经 mesh 到 TCC
- ✅ 统计 dashboard 渲染全部指标

**非功能验证**:
- ✅ 编译通过（Release + Debug）
- ✅ 所有测试通过
- ✅ docs_sync_check.sh --strict 零误报
- ✅ 零 TODO 残留
- ✅ 零混合代码风格（4 空格缩进 / 中文注释 / CamelCase）

### 7.3 阶段降级条件

**Phase 7.C 触发降级**: 5+ 测试回归 → 回 Phase 7.B write-through bypass，coherence 留待 Phase 8。

---

## 8. 风险与缓解

> **来源**: 调研报告 §5 + 各阶段子风险（见对应微架构 doc）

### 8.1 跨阶段 R1–R8（综合）

| # | 风险 | 概率 | 影响 | 跨阶段范围 | 缓解 |
|---|------|------|------|-----------|------|
| **R1** | **Phase 7.C coherence 死锁/livelock** | 高 | 高 | 7.C | 5+ 回归 → 回 write-through bypass；缓存一致性强度可降级 |
| **R2** | **Phase 7.D TCC write coalescing 失序** | 中 | 中 | 7.D | TCC 内部加顺序锁；统计上对比优化前/后 |
| **R3** | **Phase 7.E mesh 拥塞** | 中 | 中 | 7.E | CommMonitor 监控；NoC 配置可调 |
| **R4** | **MOESI 状态机不完整**（slicc 简化） | 中 | 中 | 7.C | 单元测试覆盖全部 6×6 转换；与 gem5 reference 对照 |
| **R5** | **MemoryTLM 不接受 ComputeReqBundle** | 中 | 中 | 7.D | Phase 7.D 实施双 Bundle 注册 |
| **R6** | **统计准确性**——黑盒模型可能高估/低估 | 中 | 中 | 7.B–F | 与 gem5 apu_se.py reference 对照（区间验证） |
| **R7** | **CPU 模型简化**——TrafficGenTLM 不是真实 CPU | 中 | 中 | 7.B–F | 文档明确：v0 仅作 CPU 流量源 |
| **R8** | **多 Bundle 兼容**——CacheReq 与 ComputeReq 路径冲突 | 中 | 中 | 7.A–D | CacheReqBundle + ComputeReqBundle 字段对位；AdapterHandle 路由 |

### 8.2 阶段特定风险

详见各微架构 doc 的"风险与缓解"节（如 [`cache-protocol.md`](../../soc_arch/modules/cache-protocol.md) §9）。

---

## 9. 反模式（明确不做）

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

## 10. 与已有架构兼容性

### 10.1 复用 v0 模块清单

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

### 10.2 不兼容性

| 项 | v0 | Phase 7 | 解决方式 |
|----|----|---------|---------|
| **Bundle 扩展** | 8 字段（CacheReqBundle） | +4 字段（ComputeReqBundle） | 组合而非继承（保持 POD 兼容） |
| **MemoryTLM** | 单 Bundle（CacheReq） | 双 Bundle（CacheReq + ComputeReq） | Phase 7.D 扩展注册 |
| **CacheTLM** | 简化协议 | MOESI 6 状态 | Phase 7.C 协议升级（保留 v0 简化版作为 NoncoherentCache） |
| **CrossbarTLM** | 4 CPU 端口 | 4 CPU + N mem 端口 | Phase 7.C 继承扩展为 `CoherentXBarTLM` |
| **StreamAdapter 数量** | 单 / 多 / 双向 / 双端口 | 同 v0 | ✅ 无新形态 |
| **CoherenceDomain** | API + 跨域桥接占位 | 真实集成到 CacheTLM | Phase 7.C 实施 |

### 10.3 Bundle 字段对位表

**ComputeReqBundle vs CacheReqBundle**（Phase 7.A 实施）:

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

---

## 11. 文件清单 + LOC 估算

### 11.1 6 阶段新增文件汇总

| 阶段 | 新增头文件 | 新增测试 | 新增配置 | LOC 估算 |
|------|-----------|----------|----------|----------|
| **7.A** | `compute_bundles_tlm.hh` + `gpu/gpu_tlm.hh` | `test_gpu_standalone.cc` | `gpu_standalone.json` | ~520 |
| **7.B** | `gpu/common_gpu_tlm.hh` + `gpu/compute_unit_tlm.hh` + `gpu/kernel_launch_tlm.hh` | `test_compute_unit.cc` | `apu_demo_v1.json` | ~1500 |
| **7.C** | `core/coherence_protocol.hh` + `interconnect/coherent_xbar_tlm.hh` + `interconnect/snoop_filter_tlm.hh` + `tlm/cache/cache_line.hh` | `test_coherence_protocol.cc` | `apu_demo_v2.json` | ~2200 |
| **7.D** | `gpu/tcc_tlm.hh` + `memory/hbm_tlm.hh` | `test_tcc.cc` | `apu_demo_v3.json` | ~1500 |
| **7.E** | `noc/noctopo_builder.hh` + `noc/routing_algorithm.hh` | `test_noc_topology.cc` | `apu_demo_v4.json` | ~1200 |
| **7.F** | (无新模块，集成) | `test/python/test_apu_soc.py` | `apu_full_soc.json` | ~800 |
| **7 备选 dGPU** | `gpu/pcie_bridge_tlm.hh` + `core/coherence_bridge.hh` | `test_pcie_bridge.cc` | `apu_with_dgpu.json` | ~1800 |
| **小计** | ~15 头文件 | ~6 测试 | ~7 配置 | **~9500 LOC** |

### 11.2 修改文件清单

| 文件 | 阶段 | 修改内容 |
|------|------|----------|
| `include/chstream_register.hh` | 7.A | +2 行注册 GPUTLM |
| `AGENTS.md` | 7.A | STRUCTURE 节加 `include/tlm/gpu/` |
| `docs/ONBOARDING.md` | 7.A | GPU 模块路径 |
| `include/tlm/crossbar_tlm.hh` | 7.C | 继承出 `CoherentXBarTLM` |
| `include/tlm/memory_tlm.hh` | 7.D | 双 Bundle 注册 |
| `include/core/coherence_domain.hh` | 7.C | 与 CacheTLM 集成 |
| `scripts/test/docs_sync_check.sh` | 7.A | VIRTUAL_PATHS 扩展 |
| `roadmap.md` | 7.A–F | Phase 7 状态更新 |

### 11.3 30+ 微架构 doc（B3 批次）

**位置**: `docs/soc_arch/modules/`

**分布**:
- ✅ 已实施 (15): cache-l1, memory-memtlm, cpu-cputlm, cpu-traffic_gen, cpu-cpusim_legacy, coherence-domain, interconnect-crossbar, interconnect-arbiter, noc-router, noc-nic, rtl-hybrid_cache, rtl-fragment_mapper, gpu-gputlm
- 🟡 规划中 (17): cache 5 (cache.common, cache-l2, cache-protocol, cache-replacement, cache-noncoherent) + interconnect 4 (interconnect-bridge, comm_monitor, coherent_xbar, snoop_filter) + noc 1 (noc.common) + coherence 2 (coherence-protocol, coherence-bridge) + memory 4 (simple, dram, hbm, qos) + io 8 (pio, dma, disk, terminal, uart, ether, pci, nvram) + GPU 5 (gpu.common, compute_unit, kernellaunch, tcc, pcie_bridge)

**总计**: 13 已实施 + 24 规划中 = **37 个微架构 doc**（B0–B3 全量）。

---

## 12. 决策点汇总

| # | 决策 | 阶段 | 采纳方案 | 备选 |
|---|------|------|----------|------|
| **D1** | 协议策略 | 7.A–F | 分步走（Phase 0 简化 → Phase 7.C MOESI） | 直接 MOESI / slicc |
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

## 13. 修订历史

- **2026-06-11**: 调研报告初版（[`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md)） — gem5 蓝本 6 阶段全景
- **2026-06-11**: Phase 7.A spec 初版（[同目录下子文档](./2026-06-11-phase7a-gpu-infra-design.md)） — 仅 7.A 详尽设计
- **2026-06-11**: Phase 7.A plan 初版（[`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../plans/2026-06-11-phase7a-gpu-infra.md)） — 仅 7.A 任务分解
- **2026-06-11**: Phase 7.A 实施 6 commits — `GPUTLM v0` 上线
- **2026-06-11**: B0/B1/B2 13 个微架构 doc — 已实施模块文档化
- **2026-06-12**: B3.1 + B3.2 共 6 个 GPU 微架构 doc — GPU 路径文档化
- **2026-06-12**: B3.3 共 12 个 Cache/Interconnect/NoC/Coherence 微架构 doc
- **2026-06-12**: B3.4 共 12 个 Memory/IO 微架构 doc
- **2026-06-12**: **APU 整合 spec v1.0**（位于 `docs/superpowers/specs/`，混合架构+实施）
- **2026-06-12**: **本文档 v2.0 恢复** — 在 `docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md` 保留完整实施 spec（架构+实施+代码+验收+LOC）；在 `docs/soc_arch/specs/apu-soc-design.md` 提供纯架构参考（语言+图表）
- **Phase 7.B (未来)**: 实施后更新本文档（7.B 实施 commit + 实施细节更新）
- **Phase 7.C (未来)**: 7.C 实施 + 风险 R1 状态更新
- **Phase 7.D (未来)**: 7.D 实施 + R5 修复确认
- **Phase 7.E (未来)**: 7.E 实施 + 4×CU 端到端测试
- **Phase 7.F (未来)**: APU SoC Demo 上线 + 统计 dashboard
- **Phase 7 备选 dGPU (未来)**: dGPU 启动决策 + Disjoint NoC 实施

---

**关联文档清单**:
- 调研报告: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md)
- **架构参考**: [`docs/soc_arch/specs/apu-soc-design.md`](../../soc_arch/specs/apu-soc-design.md) — 纯架构（语言+图表），本文档的精简版
- Phase 7.A 详尽 spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](./2026-06-11-phase7a-gpu-infra-design.md)
- Phase 7.A 实施 plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../plans/2026-06-11-phase7a-gpu-infra.md)
- 微架构文档索引: [`docs/soc_arch/modules/`](../../soc_arch/modules/)
- Roadmap Phase 7: [`roadmap.md`](../../roadmap.md) §Phase 7
- APU 模块 README: [`docs/soc_arch/modules/README.md`](../../soc_arch/modules/README.md)
