---
id: phase-9
kind: phase
status: active
phase_refs: []
主题: dGPU-first track (per ADR-088 v3 + ADR-NV-01/02)
---

## Phase 9: dGPU-first Track（与 Phase 7 APU 并行）

> **来源**: 架构 handoff 声明 `Phase 9 (dGPU-first, per ADR-088 v3)` 与 Phase 7 APU 并行
> **核心 ADR**:
>   - [ADR-NV-01](docs/adr/ADR-NV-01-gpu-soc-architecture-target.md) — gpu_soc 独立 SoC 仿真目标（13 周 / ~5100 LOC / 14 新模块）
>   - [ADR-NV-02](docs/adr/ADR-NV-02-phase8b-d1-strategy.md) — Phase 8.B D1-Lite 渐进策略
>   - [ADR-SOC-06](docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) — cpptlm-v0.5 MVP：dGPU Board (CP→TMU→Cuda Core→PTX-EMU warp)
> **外部参考**: UsrLinuxEmu [ADR-088](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted 2026-08-16
> **关联 Change**: [`openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/`](openspec/changes/2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/) (active, 0/45 tasks)
> **目标形态**: dGPU 板卡（PCIe device semantics），由 PTX-EMU (Phase 7 集成 S1) 作为 warp-level functional backend
> **范围约束**: 与 APU-first (Phase 7) 共享 GPU 子模块（`GpuCluster`/`GpcCluster`/`TpcCluster`/`ComputeCluster` 单一实现，引入 `GpuClusterSharedInterface`）

### 已采纳的架构决策（D1-D8，来自 ADR-NV-01 §2.1）

| 决策 | 采纳方案 | 影响 |
|------|----------|------|
| **D1** 与 apu_soc 关系 | 共享 GPU 子模块（`GpuClusterSharedInterface`） | 顶层代码可复用 |
| **D2** 核心定位 | 工业性能建模（phase-accurate，sub-core 内部 black-box） | 不做 cycle-accurate 5 管线 |
| **D3** SKU 范围 | 参数化 + 预置库（`cpptlm.nvidia` 子包） | `gb203_consumer` / `gb200_datacenter` / `gh100_hopper` / `ga100_ampere` |
| **D4** 工作负载 | `WorkloadGenerator` + `TraceReplay`（`cpptlm.gpu_workload` 子包） | 5 类 pattern：GEMM / FlashAttn / vector_add / stencil / sparse |
| **D5** 验证 | gpgpu-sim 区间对照（带宽 ±15%, 延迟 ±20%） | 5 类场景 |
| **D6** 时间线 | 独立 Phase 8 路径（与 apu_soc 并行） | 13 周（4 + 6 + 3） |
| **D7** 实施策略 | 3 阶段渐进 | Phase 8.A / 8.B / 8.C |
| **D8** 抽象层级 | Phase-accurate（sub-core 内部 black-box 化，5+V 管线不分开建模） | 分数 cycle 输出 |

### Phase 9 子任务（与 Phase 8.A-C 对齐）

| 子任务 | ADR-NV 章节 | 状态 | 范围 | 验收 |
|--------|:-----------:|:----:|------|------|
| **9.A gpu_soc 基础设施** | ADR-NV-01 §2.1 / Phase 8.A | 🟡 Pending | `cpptlm.nvidia` 子包 + `GpuClusterSharedInterface` + `WorkloadGenerator` 5 类 pattern | 子包加载 + WorkloadGenerator demo |
| **9.B D1-Lite 集成** | ADR-NV-02 / Phase 8.B | 🟡 Pending | 6 模块接口设计 + 与 PTX-EMU 真实 API 集成（WarpScheduler / Scoreboard / Pipeline / TensorCore / L2Partition / SubCore） | cpptlm_d1_full 模块联动 |
| **9.C dGPU Board MVP** | ADR-SOC-06 / v0.5 | 🟡 Pending | PCIe device semantics（`DGpuBar` + Doorbell + SubmitQueue + CQ）+ CP→TMU→Cuda Core→PTX-EMU warp pipeline + H2D DMA | `cpptlm_dgpu_board` 端到端；`s2-dgpu-board` change 完成 |
| **9.D gpgpu-sim 验证** | ADR-NV-01 §2.1 D5 | 🟡 Pending | 5 类 microbenchmark vs gpgpu-sim ±15% 带宽 / ±20% 延迟 | `test/python/test_gpgpu_sim_comparison.py` |

### 与 Phase 7 APU-first 的并行关系

| 维度 | Phase 7 (APU) | Phase 9 (dGPU) |
|------|----------------|----------------|
| **目标硬件** | APU 融合 CPU+GPU（gem5 apu_se.py 形态） | 独立 dGPU 板卡（PCIe device） |
| **首要用户** | APU 融合验证 | 工业性能建模 / SoC 选型 |
| **GPU 模块来源** | 自建 `ComputeUnitTLM`（黑盒优先 per D2） | 共享 `GpuCluster` 子模块 + PTX-EMU 作为 warp-level backend |
| **验证基线** | gem5 apu_se.py 行为 | gpgpu-sim 数值 |
| **当前活跃工作** | Phase 7.A ✅ Done; 7.B-F 🟡 Pending | Phase 9.C（dGPU Board MVP, s2 change 0/45） |

### 依赖关系

```
Phase 7.A (GPU 基础设施, ✅ Done)
   ↓ 提供 GPU 子模块骨架
Phase 9.A (gpu_soc 基础设施)
   ↓ 共享 GPU 子模块 + 引入 GpuClusterSharedInterface
Phase 9.B (D1-Lite 集成)
   ↓ 替换 PTX-EMU 内部组件
Phase 9.C (dGPU Board MVP) ← 当前 s2-dgpu-board change
   ↓ 依赖 Phase 9.A + 9.B
Phase 9.D (gpgpu-sim 验证)
   ↓ 真实 dGPU 链路 + 对照基线
```

### 当前状态（2026-08-26）

- **openspec change** `2026-08-21-cpptlm-v05-mvp-s2-dgpu-board` 已提交（proposal + design + 45 tasks），未开始 execute（0/45）
- **依赖的前置**: Phase 7.A GPU 基础设施 ✅ Done; PtxEmuSubmoduleMVP (s1) 已落地
- **阻塞因素**: 无 — 等待用户启动 execute
- **D1-Full 升级路径**: s1 plan T-s1-1..T-s1-4 + commit `d909407` cuda-core-adapter activate 已推进