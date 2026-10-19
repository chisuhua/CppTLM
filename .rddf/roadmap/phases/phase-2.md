---
id: phase-2
kind: phase
status: completed
phase_refs: []
主题: GPGPU 多层 SimModule + ApuSoC 顶层容器
---

## phase-2: GPGPU 多层 SimModule + ApuSoC 顶层容器

> **真源**: [`docs/designs/dgpu-soc/multi-level-hybrid-simulation.md`](../../docs/designs/dgpu-soc/multi-level-hybrid-simulation.md) (2026-10-19 迁出至 docs/designs/dgpu-soc/, per docs/architecture 真源原则)
> **关键文档**: [`docs/architecture/多层次混合仿真.md`](../../docs/architecture/) 已迁出, 现位于 docs/designs/dgpu-soc/

### 9 类 SimModule 派生类 (P2-P5, per `include/tlm/cluster/`)

| 类 | 头 | 关键职责 |
|----|-----|----------|
| **CpuCluster** | `include/tlm/cluster/cpu_cluster.hh` | CPU 侧: 持有 CPUTLM/CacheTLM/MemoryTLM |
| **ComputeCluster** | `include/tlm/cluster/compute_cluster.hh` | 单 CU 蓝图复制 (cu_template + cu_count) |
| **TpcCluster** | `include/tlm/cluster/tpc_cluster.hh` | GPGPU TPC: 持有 2-8 个 ComputeCluster |
| **GpcCluster** | `include/tlm/cluster/gpc_cluster.hh` | GPGPU GPC: 持有 N 个 TpcCluster |
| **GpuCluster** | `include/tlm/cluster/gpu_cluster.hh` | 顶层 GPGPU: 持有 GpcCluster + 共享 L2 + 显存控制器 |
| **CacheCluster** | `include/tlm/cluster/cache_cluster.hh` | L1×N + L2 聚合 |
| **MemoryCluster** | `include/tlm/cluster/memory_cluster.hh` | 多通道 HBM/DDR 控制器 |
| **GpuNoCCluster** | `include/tlm/cluster/gpu_noc_cluster.hh` | GPU 端 mesh interconnect |
| **ApuSoC** | `include/tlm/cluster/apu_soc.hh` | 顶层 dGPU/APU SoC: CPU 侧 + GPGPU 侧 + Crossbar 互联 |

### 配置文件

- `configs/apu_soc_v1.json` — 顶层 SoC 配置 (CPU + GPU + Crossbar)
- `configs/apu_soc_full.json` — 完整 APU SoC
- `configs/apu_soc_phase7a.json` / `configs/apu_soc_phase7b.json` — 阶段性

### 历史归档

> **历史 note (2026-08 视角)**: 原 .rddf/roadmap.md phase-7 "CPU+GPGPU Fused SoC (APU-first, 进行中)" 是 2026-06 视角的命名, 当前已走 dGPU-first 路线 (per ADR-NV-01/02 + ADR-088 v3). 原 phase-7 命名是历史快照. dGPU-first 实施由 phase-9 (本文件) 协调.
>
> **策略调整原因**: APU-first vs dGPU-first 战略调整发生在 2026-08-26 (per UsrLinuxEmu ADR-088 Accepted 2026-08-16). APU-first phase 7.A 已完成 (2026-06-11), 其余 7.B-7.F 被 dGPU-first 战略取代.