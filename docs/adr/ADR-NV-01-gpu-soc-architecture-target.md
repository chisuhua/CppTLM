# ADR-NV-01: gpu_soc 独立 SoC 仿真目标

> **状态**: ✅ 已确认
> **日期**: 2026-06-24
> **影响**: Phase 8 完整路径（13 周 / ~5100 LOC / 14 新模块 + 3 Python 子包）
> **类别**: SoC 架构目标定义（与 apu_soc 并行）
> **来源**: 6 轮 brainstorming + 调研 gpgpu-sim SM_120 paper + 本地 notes `04_Knowledge/D01-gpu-architecture/`

---

## 1. 背景

CppTLM 当前**只有一个 SoC 架构目标**——apu_soc（CPU + GPU 融合，对应 gem5 apu_se.py 形态）。Phase 7.A 已落地 GPU 基础设施，Phase 7.B-F 已规划。

但调研 gpgpu-sim + 本地 notes 后识别出 **NVIDIA GPU 完整 5 级层次**（Chip → GPC → TPC → SM → Sub-core）有独立仿真价值：

| 视角 | apu_soc | gpu_soc (本文) |
|------|---------|----------------|
| **首要用户** | APU 融合验证 | 工业性能建模 / SoC 选型 |
| **抽象层级** | Transaction-accurate | Phase-accurate |
| **工作负载** | CPU 流量源 | WorkloadGenerator + TraceReplay |
| **验证基线** | gem5 apu_se.py 行为 | gpgpu-sim 数值 |
| **roadmap** | Phase 7.A-F (35d) | Phase 8.A-C (13w) |

用户 2026-06-24 明确决策："决策点 1=保持 Phase 7.B scope（A），决策点 2=gpgpu-sim 仅借鉴（A），决策点 3=走正式 OpenSpec 流程（A），路径 Y=核心复现 gpu_soc"。

---

## 2. 决策

### 2.1 ✅ 定义 gpu_soc 为 CppTLM 第二个独立 SoC 仿真目标

**8 项核心决策（来自 6 轮 brainstorming）**：

| # | 决策 | 选项 | 影响范围 |
|---|------|------|---------|
| **D1** | 与 apu_soc 关系 | 共享 GPU 子模块（GpuCluster/GpcCluster/TpcCluster/ComputeCluster 单一实现，引入 `GpuClusterSharedInterface`） | 顶层代码可复用 |
| **D2** | 核心定位 | 工业性能建模（phase-accurate，sub-core 内部 black-box） | 不做 cycle-accurate 5 管线 |
| **D3** | SKU 范围 | 参数化 + 预置库（`cpptlm.nvidia` 子包） | `gb203_consumer`/`gb200_datacenter`/`gh100_hopper`/`ga100_ampere` |
| **D4** | 工作负载 | WorkloadGenerator + TraceReplay（`cpptlm.gpu_workload` 子包） | 5 类 pattern: GEMM/FlashAttn/vector_add/stencil/sparse |
| **D5** | 验证 | gpgpu-sim 区间对照（带宽 ±15%, 延迟 ±20%） | 5 类场景 |
| **D6** | 时间线 | 独立 Phase 8 路径（与 apu_soc 并行） | 13 周（4+6+3） |
| **D7** | 实施策略 | 3 阶段渐进 | Phase 8.A / 8.B / 8.C |
| **D8** | 抽象层级 | Phase-accurate（sub-core 内部 black-box 化，5+V 管线不分开建模） | 分数 cycle 输出 |

### 2.2 ✅ 借鉴 gpgpu-sim 不集成

- ❌ **不集成** gpgpu-sim 代码
- ✅ **借鉴** 其设计原则（5 级层次、4 sub-core/SM、5+V 管线、12 精度 TC）
- ✅ **参考** 其已发布 microbenchmark 数值作为验证基线（NVIDIA 论文 + Jarmusch 2507.10789）
- 理由：避免外部依赖；保留 CppTLM 独立演化能力

### 2.3 ✅ 与 apu_soc 共享机制

引入 `include/tlm/gpu/gpu_cluster_shared_interface.hh` 抽象层：
- `GpuClusterSharedInterface` 接口（`set_gpu_topology` / `get_gpu_topology` / `tick`）
- `GpuCluster` 类同时继承 `SimModule` 和 `GpuClusterSharedInterface`
- apu_soc 的 `ApuSoC::incorporate_parent` 与 gpu_soc 的 `GpuSocTLM` 都引用此接口

**复用清单**：
- F12 三类（`GpuComputeUnitTLM` / `VectorRegFileTLM` / `WavefrontTLM`）—— gpu_soc 直接 import
- 4 级 cluster 容器（GpuCluster/GpcCluster/TpcCluster/ComputeCluster）—— 单一实现
- `CacheTLM` / `MemoryTLM` / `RouterTLM` / `LinkTLM` / `ComputeReqBundle` —— 已有基础设施

### 2.4 ❌ 反模式（明确不做）

- ❌ Cycle-accurate 5+V 管线（与 phase-accurate 矛盾）
- ❌ 完整 TensorCore 12 精度（首期 6 精度：FP4/FP6/FP8/FP16/BF16/TF32）
- ❌ 集成 gpgpu-sim 代码
- ❌ 真实 kernel 编译/执行（非仿真器职责）
- ❌ CPU/GPU coherence（属于 apu_soc）
- ❌ dGPU + PCIe + Disjoint NoC（Phase 9 备选）
- ❌ 重新实现 F12 三类（apu_soc 已建）

---

## 3. 实施

### 3.1 14 个新模块 + 3 阶段

| # | 模块 | Phase | LOC | 抽象层级 |
|:-:|------|:---:|:---:|------|
| 1 | MemoryClusterTLM | 8.A | 200 | phase-accurate |
| 2 | SharedMemoryTLM | 8.A | 250 | cycle-accurate (bank conflict) |
| 3 | GpuNoC (mesh) | 8.A | 300 | phase-accurate |
| 4 | KernelLaunchTLM | 8.A | 200 | phase-accurate |
| 5 | SubCoreTLM | 8.B | 400 | black-box pipe |
| 6 | WarpSchedulerTLM | 8.B | 350 | phase-accurate |
| 7 | ScoreboardTLM | 8.B | 200 | phase-accurate |
| 8 | TensorCoreTLM | 8.B | 300 | phase-accurate |
| 9 | PipelineTLM | 8.B | 500 | phase-accurate |
| 10 | L2PartitionTLM | 8.B | 200 | phase-accurate |
| 11 | TccTLM | 8.C | 200 | phase-accurate |
| 12 | TmaTLM | 8.C | 200 | phase-accurate |
| 13 | DsmTLM | 8.C | 200 | phase-accurate |
| 14 | PowerModelTLM | 8.C | 150 | post-hoc stat |
| | **小计 impl** | | **3650** | |
| | **+ tests** | | **650** | |
| | **+ Python** | | **800** | |
| | **总** | | **5100** | |

### 3.2 3 阶段里程碑 [Oracle 二次审查 Status Update — 2026-08-18]

> **⚠️ Phase 8.B/8.C phase-accurate 验收标准降级**: 自 ADR-X.15 v3.0 dGPU 板卡决策起,Phase 8.B 的 4 Adapter (WarpScheduler/Scoreboard/Pipeline/TensorCore) **不实施**,Phase-accurate 仿真仅对 **synthetic workload** 有效;**真实 CUDA kernel 上的 phase-accurate 研究由 PTX-EMU `libptxemu_device.so` 承担**(per ADR-NV-02 §2.4)。
>
> **M2/M3 验收点修订**: 8.B M2 (±15% gpgpu-sim 带宽) 仅在 synthetic workload 下验证;8.C M3 (±20% 延迟) 同样限定 synthetic workload。详见 `ADR-NV-02-phase8b-d1-strategy.md` Status Update。

| Phase | 周数 | 任务数 | 关键交付 | 验收点 |
|:---:|:---:|:---:|------|------|
| **8.A** | 4 | 8 | MemoryCluster + SharedMemory + GpuNoC + KernelLaunch + GpuSocTLM 顶层 + GpuClusterSharedInterface | M1: 端到端跑通, 1 SM × 1M cycles < 5s |
| **8.B** ⚠️ | 6 | 8 | SubCore + WarpScheduler + Scoreboard + TensorCore + Pipeline + L2Partition + 5 类 microbenchmark | M2: ⚠️ synthetic workload 下 gpgpu-sim ±15% 带宽; 1 GB203 × 1M cycles < 60s |
| **8.C** ⚠️ | 3 | 9 | Tcc + Tma + Dsm + PowerModel + cpptlm.{nvidia,gpu_workload,gpu_soc} + apu_soc 集成 | M3: ⚠️ synthetic workload 下 ±20% 延迟 |

### 3.3 Python API（`cpptlm.nvidia` / `cpptlm.gpu_workload` / `cpptlm.gpu_soc`）

```python
import cpptlm.nvidia as nv
import cpptlm.gpu_workload as gw
import cpptlm.gpu_soc as gs

# 1. 预置 SKU
topo = nv.gb203_consumer()  # 或 gb200_datacenter() / gh100_hopper() / ga100_ampere()

# 2. 参数化
topo = nv.gpu_topology(sm_arch="blackwell_sm_120", num_gpc=4, ...)

# 3. Workload
wl = gw.GEMM(m=4096, n=4096, k=4096, dtype="FP16")  # 或 TraceReplay()

# 4. 仿真 + 报告
sim = gs.simulate(topo=topo, workload=wl, duration_cycles=1_000_000)
print(sim.report())  # 与 gpgpu-sim 对照
```

### 3.4 文件组织

```
include/tlm/gpu/             # 14 个新 .hh + 1 顶层 GpuSocTLM + 1 GpuClusterSharedInterface
src/tlm/gpu/                 # 对应 .cc
include/bundles/             # 3 个新 bundle (shared_memory / warp_state / tensor_core)
cpptlm/cpptlm/{nvidia,gpu_workload,gpu_soc}/   # 3 个 Python 子包
configs/templates/gpu_soc/   # 4 个 SKU JSON 蓝图
test/                        # 14 个 .cc 单元测试 + 3 个 .cc 集成测试
docs/soc_arch/modules/       # 15 个新微架构 doc
docs/adr/ADR-NV-01-...       # 本文档
```

---

## 4. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| **R1** | 5+V 管线过复杂 | 中 | 高 | Phase 8.B 简化为 black-box pipe（分数 cycle 输出），不做 17-bit ctrl code |
| **R2** | TC 12 精度爆炸 | 中 | 中 | 首期 6 精度（FP4/FP6/FP8/FP16/BF16/TF32），剩余通过模拟精度可扩展 |
| **R3** | Trace 格式不统一 | 中 | 中 | 首期 NCU `.ncu-rep` + CuPTI `.cupti-trace` 双格式 |
| **R4** | apu_soc/gpu_soc 共享接口冲突 | 中 | 中 | 引入 `GpuClusterSharedInterface` 抽象层（已落地） |
| **R5** | gpgpu-sim baseline 不可复现 | 低 | 中 | 用已发布 microbenchmark 数据（Jarmusch 2507.10789 + Luo 2501.12084），不依赖其代码 |

---

## 5. 验收标准

| 阶段 | 编译 | 单测 | 集成 | 性能 | 文档 |
|------|------|------|------|------|------|
| **8.A** | ✅ | `[gpu][smem][memcluster][noc][kernel_launch]` | `gpu_soc_phase8a.json` 端到端 | 1 SM × 1M cyc < 5s | 5 个微架构 doc |
| **8.B** | ✅ | `[gpu][sb][sched][pipe][tc][l2][subcore]` | 5 类 microbenchmark | 1 GB203 × 1M cyc < 60s | 6 个微架构 doc |
| **8.C** | ✅ | `[gpu][tcc][tma][dsm][power][apu_soc]` | apu_soc Phase 7.F 集成 | 完整 5 类报告 | 4 个微架构 doc + PowerModel 输出 |
| **整体** | Release+Debug | 730+ pass (703 + 30 新) | 222→237+ Python pass | 满足 gpgpu-sim 区间对照 | docs_sync 0 missing |

---

## 6. 参考文献

### Spec 与 Plan
- **SOC 架构文档**: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../superpowers/specs/2026-06-24-gpu-soc-architecture.md)（677 行，commit `801f8ea`）
- **实施路径文档**: [`docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md`](../superpowers/plans/2026-06-24-gpu-soc-roadmap.md)（1100 行，commit `3993129`）
- **roadmap 父文档**: [`docs/superpowers/plans/2026-06-20-future-work-roadmap.md`](../superpowers/plans/2026-06-20-future-work-roadmap.md)

### 调研资料
- **gpgpu-sim SM_120 paper**: https://zartbot.github.io/micro_arch/nvidia/sm_120/paper.html
- **Jarmusch et al. 2507.10789**: *Dissecting the NVIDIA Blackwell Architecture with Microbenchmarks* (arXiv)
- **Luo et al. 2501.12084**: *Dissecting the NVIDIA Hopper Architecture through Microbenchmarking* (arXiv)
- **本地 notes**: `/workspace/project/mynotes/04_Knowledge/D01-gpu-architecture/`（blackwell/hopper drafts + 19 概念 + 6 主题 + simt-research）

### CppTLM 关联
- **兄弟 SoC**: apu_soc（[`docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md`](../superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md)）
- **兄弟 ADR**: `docs/soc_arch/adr/ADR-SOC-01..05.md`（5 份 SOC 决策：Coherence / CU 粒度 / Wavefront / HSAPP / 目录）
- **复用 F12 三类**: GpuComputeUnitTLM / VectorRegFileTLM / WavefrontTLM（Phase 7.B 实施中）
- **复用基础设施**: CacheTLM / MemoryTLM / RouterTLM / LinkTLM / CoherentXBarTLM

### 命名空间分层
- `docs/adr/ADR-X.*.md` —— 框架层决策（X = 框架）
- `docs/adr/ADR-SOC-*` —— SoC 应用层决策（应用层）
- `docs/adr/ADR-NV-*` —— NVIDIA GPU 仿真决策（**新命名空间**，本文档定义）

---

## 7. 修订历史

- **2026-06-24 v1.0 (本文)** — 初版签发
  - 6 轮 brainstorming 产出
  - 8 项决策 D1-D8 完整记录
  - 14 个新模块 + 3 阶段实施路径
  - 5 项风险 R1-R5 + 缓解
  - 命名空间 `ADR-NV-*` 引入（与 X-* / SOC-* 并列）

---

**关联 OpenSpec 变更**：
- `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/` — ✅ Archived (2026-07-02, commit `e8280fe`)
- `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` — 🟢 Ready to Start (前置 8.A 已完成)
- `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/` — 🔄 Draft (依赖 8.B M2 验收)

## Status Update

### 2026-07-03 — Phase 8.A 已归档，Phase 8.B 启动 (refresh)

- **触发事件**: Phase 8.A 8 个任务 + Oracle 审查 + OpenSpec 归档全部完成 (commit `e8280fe` 2026-07-02)
- **基线影响**: 测试基线 703 → **764/764** (+61 tests)，`[gpu]` 14 → **75 cases**, `[phase8a]` 43 cases 全绿
- **架构影响**: 本 ADR 的 8 项核心决策 (D1-D8) 通过 Phase 8.A 实施验证：
  - **D1** 共享 GpuCluster 子模块 ✅ (GpuClusterSharedInterface 落地，apu_soc 兼容)
  - **D2** 工业性能建模 (phase-accurate, sub-core black-box) ✅ (M1 验收：1 SM × 1M < 5s)
  - **D3** SKU 范围参数化 ✅ (4 阶段集成测试通过)
  - **D4** 工作负载模型 ✅ (KernelLaunchTLM 落地)
  - **D5** gpgpu-sim 验证基线 ⏳ (留待 Phase 8.B Task 15 区间对照)
  - **D6** 时间线 13 周 ⏳ (8.A 4w ✅，8.B 6w 🟢，8.C 3w 🔄)
  - **D7** 3 阶段渐进 ✅ (8.A 已完成)
  - **D8** Phase-accurate 抽象层级 ✅ (M1 sub-core black-box 输出分数 cycle)
- **下一步行动**:
  - Phase 8.B Task 9 (ScoreboardTLM ≥12 entries) 启动 — 0.5d
  - Phase 8.B Task 10-14 (5 核心模块) 并行 — 4d
  - F4 (Phase 7.C 6×6 state table) brainstorming 启动 — 2d
  - F12b-LD (PTX-EMU 集成) 仍待外部团队对齐
- **关联文档**: [`docs/roadmap/`](../../roadmap/) (实时看板), [`docs/superpowers/plans/2026-06-20-future-work-roadmap.md`](../../superpowers/plans/2026-06-20-future-work-roadmap.md) (single source of truth)

**维护**: CppTLM 开发团队
**下次 review**: Phase 8.B Task 9-14 完成后 (预计 2026-07-10)

### 2026-07-15 — D8 Phase-accurate 策略在 Phase 8.B D1-Full 中的演进说明

**触发事件**：ADR-NV-02 将 Phase 8.B 目标从 D1-Lite 升级为 D1-Full（Scoreboard/Pipeline/TensorCore/WarpScheduler 4 组件注入 PTX-EMU），要求澄清 D8 与 D1-Full 的关系。

**D8 不变的内核**：Phase-accurate 定位不变——sub-core 对外仍输出分数 cycle 的执行时间，不做 cycle-accurate 5 管线建模（反模式表第一项仍有效）。

**演进部分**：D8 原描述"sub-core 内部 black-box 化，5+V 管线不分开建模"适用于 Phase 8.A（验证通过，M1 验收）。Phase 8.B D1-Full 在 sub-core 内部引入 4 个可独立注入的模块（ScoreboardTLM / PipelineTLM / TensorCoreTLM / WarpSchedulerTLM），供 PTX-EMU 集成使用。这是**抽象层级保持不变前提下的内部白箱化**，而非退回 cycle-accurate。具体变更由 ADR-NV-02 记录。

**D1-Full 版本下 D8 兼容解读**：
- Phase-accurate 输出 ✅ 保持（SubCoreTLM tick() 仍返回分数 cycle）
- 5+V 管线不 cycle-accurate 建模 ✅ 保持（PipelineTLM 仍为查表模型，非 17-bit ctrl code）
- Sub-core 内部模块化 ✅ 新增（为 PTX-EMU 集成提供注入接口，不影响 standalone 模式）

**关联文档**：`docs/adr/ADR-NV-02-phase8b-d1-strategy.md` §3.6 Status Update + 第 329 行"与 ADR-NV-01 的关系"。
