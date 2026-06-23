# gpu_soc 架构设计 — CppTLM 独立 GPU SoC 仿真目标

> **Document ID**: SOC-ARCH-002-gpu-soc
> **Version**: 1.0
> **Date**: 2026-06-24
> **Status**: 🔄 Draft（待 user review）
> **Author**: Sisyphus（brainstormed 6 轮 + 推荐方案 A）
> **Parent Roadmap**: [`docs/superpowers/plans/2026-06-20-future-work-roadmap.md`](../plans/2026-06-20-future-work-roadmap.md)
> **Companion Spec**: [`docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md`](./2026-06-12-phase7-apu-fused-soc-design.md)（apu_soc，并行路径）
> **Reference**: [gpgpu-sim SM_120 paper](https://zartbot.github.io/micro_arch/nvidia/sm_120/paper.html) + 本地 notes `04_Knowledge/D01-gpu-architecture/`（借鉴不集成）
> **关联微架构 doc**: `docs/soc_arch/modules/gpu-*.md`

> **本文档定位**：**架构 + 实施**双视角——含文件清单、LOC 估算、阶段任务分解、Python API 设计。**实施计划**由 `writing-plans` skill 产出（`docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md`）。**OpenSpec 变更提案**同步创建于 `openspec/changes/2026-06-24-gpu-soc-architecture/`。

---

## 0. 阅读引导

本文档定义 **gpu_soc** —— CppTLM 第二个独立 SoC 仿真目标，与 **apu_soc** 并行；二者共享 GPU 子模块，顶部差异化。本文档是 6 轮 brainstorming 的产出，决策汇总见 §11。

**结构**：
- **§1 范围与目标** —— gpu_soc 是什么、不是什么、关键设计原则
- **§2 关键设计决策 D1-D8** —— 6 轮 brainstorming 决策汇总
- **§3 5 级层次与 14 个新模块** —— 借鉴 NVIDIA 简化到 4 级
- **§4 Python API 设计** —— `cpptlm.nvidia` + `cpptlm.gpu_workload` + `cpptlm.gpu_soc`
- **§5 3 阶段实施路径** —— Phase 8.A / 8.B / 8.C
- **§6 验证策略** —— gpgpu-sim 区间对照 + 5 类场景
- **§7 模块组织与目录** —— 14 个 .hh/.cc 位置
- **§8 文件清单 + LOC 估算** —— 3650 LOC 新增
- **§9 风险与缓解 R1-R5** —— 5 项关键风险
- **§10 反模式** —— 明确不做（cycle-accurate / 12 精度 / dGPU / gpgpu-sim 集成）
- **§11 决策点汇总** —— 8 项 D1-D8
- **§12 文档配套** —— 新 ADR + 实施路径 doc + OpenSpec 变更
- **§13 修订历史**

**读者路径**：
- 关心"做不做/顺序" → §1, §2, §5, §6, §9
- 关心"为什么" → §2, §10, §11
- 关心"怎么做" → §3, §4, §7, §8 + 引用子 spec/plan

---

## 1. 范围与目标

### 1.1 gpu_soc 总体目标

在 CppTLM v2.1 + Phase 7.A GPU 基础设施基础上，**端到端实现独立 GPU SoC 仿真**——单 GPU 芯片（GpuCluster + GpuNoC + MemoryCluster + KernelLauncher），对位 gem5 apu_se.py 独立 GPU 形态 + NVIDIA GB203 消费级 / GB200 数据中心级。

**核心目标**：
- ✅ **5 级层次参数化**：Chip → GPC(N) → TPC(M) → SM(K) → Sub-core(4)（sub-core 内部黑盒化）
- ✅ **工业性能建模**：phase-accurate 抽象，关键输出带宽/延迟/吞吐/命中率
- ✅ **可配置 SKU 库**：GB203 消费级 / GB200 数据中心 / GH100 Hopper / GA100 Ampere 预置函数
- ✅ **WorkloadGenerator + TraceReplay**：参数化 kernel 序列 + NCU/CuPTI trace 导入
- ✅ **gpgpu-sim 区间验证**：5 类场景数值对照（带宽 ±15%, 延迟 ±20%）
- ✅ **与 apu_soc 共享子模块**：GpuCluster/GpcCluster/TpcCluster/ComputeCluster 单一实现

**显式不在范围**：
- ❌ **完整 cycle-accurate 5+V 管线**（sub-core 内部黑盒，分数 cycle 输出）
- ❌ **完整 TensorCore 12 精度**（首期 6 精度：FP4/FP6/FP8/FP16/BF16/TF32）
- ❌ **dGPU + PCIe + Disjoint NoC**（Phase 9 备选）
- ❌ **集成 gpgpu-sim 代码**（仅借鉴设计原则 + 已发布 microbenchmark 数据）
- ❌ **CPU/GPU coherence**（这是 apu_soc 范畴；gpu_soc 是纯 GPU 端）
- ❌ **真实 kernel 编译/执行**（非仿真器职责；用 WorkloadGenerator 描述行为）

### 1.2 命名约定澄清

| CppTLM 术语 | NVIDIA 对位 | gpgpu-sim 对位 |
|-------------|-------------|----------------|
| `GpuSoc` | 1 GPU chip (1 GpuCluster) | `gpu()` |
| `GpuCluster` | 1 GPU die | `gpu_compute_cluster` |
| `GpcCluster` | GPC (Graphics Processing Cluster) | `gpc` |
| `TpcCluster` | TPC (Texture Processing Cluster) | `tpc` |
| `ComputeCluster` | 1-2 SM | `compute_unit` |
| `GpuComputeUnitTLM` | 1 SM (4 sub-core) | `shader_core` (含 sub-core) |
| `SubCoreTLM` (Phase 8.B) | 1 sub-core (4 warp sched) | `scheduler` |
| `WarpSchedulerTLM` | 1 warp scheduler | `warp_scheduler` |
| `TensorCoreTLM` | TensorCore (5th-gen Blackwell) | `tensor_core` |
| `SharedMemoryTLM` | shared memory + L1 unified | `smem` |
| `MemoryClusterTLM` | HBM/GDDR7 + MC | `dram` |

### 1.3 与 apu_soc 的边界

| 维度 | apu_soc (Phase 7) | gpu_soc (Phase 8) |
|------|-------------------|-------------------|
| **共享子模块** | GpuCluster/GpcCluster/TpcCluster/ComputeCluster（单一实现，apu_soc 与 gpu_soc 都引用） | 同左 |
| **顶层差异化** | CpuCluster + CoherentXBarTLM + MemoryCluster | GpuCluster + GpuNoC + MemoryCluster + KernelLauncher |
| **抽象层级** | Transaction-accurate | Phase-accurate |
| **关键 deliverable** | 完整 APU 集成（CPU↔GPU coherence） | 独立 GPU 性能模型 |
| **roadmap 阶段** | Phase 7.A-F (35d) | Phase 8.A-C (13w) |
| **用户场景** | APU 融合演示 | SoC 选型 / 架构探索 |
| **验证基线** | gem5 apu_se.py 行为 | gpgpu-sim 数值 |
| **首要价值** | 端到端可工作 SoC | 可配置性能模型 |

---

## 2. 关键设计决策 D1-D8

### D1 — 与 apu_soc 关系：共享 GPU 子模块
- ✅ 单一 GpuCluster/GpcCluster/TpcCluster/ComputeCluster 实现
- ✅ apu_soc 顶部 = CpuCluster + CoherentXBarTLM + GpuCluster + MemoryCluster
- ✅ gpu_soc 顶部 = GpuCluster + GpuNoC + MemoryCluster + KernelLauncher
- ✅ cpptlm.library 提供两种顶层 API（`apu_soc()` / `gpu_soc()`）
- 理由：避免代码重复；apu_soc 与 gpu_soc 互哺（apu_soc 可享用 gpu_soc 的 SubCore + TensorCore 等高级仿真）

### D2 — 核心定位：工业性能建模
- ✅ Phase-accurate 抽象
- ✅ 关键输出：带宽 / 延迟 / 吞吐 / cache 命中率 / snoop 流量 / TCC 合并比
- ❌ 不做 cycle-accurate 5 管线 + scoreboard 状态机（sub-core 内部黑盒化）
- 理由：CppTLM 仿真目标是"系统级行为"而非"单 cycle 准确"，phase-accurate 平衡精度与速度

### D3 — SKU 范围：参数化 + 预置库
- ✅ `cpptlm.nvidia.gpu_topology(sm_arch, num_gpc, ...)` 全参数化
- ✅ 预置函数：`gb203_consumer()` / `gb200_datacenter()` / `gh100_hopper()` / `ga100_ampere()`
- ✅ 用户可继承 Blueprint 自定义 variant
- 理由：覆盖"用户用预置"与"用户自定义"两种需求

### D4 — 工作负载：WorkloadGenerator + TraceReplay
- ✅ `cpptlm.gpu_workload.WorkloadGenerator`（参数化 kernel 序列）
- ✅ `cpptlm.gpu_workload.TraceReplay`（NCU/CuPTI trace 导入）
- ✅ 5 类 pattern：GEMM / FlashAttn / vector_add / stencil / sparse
- 理由：兼顾"参数化探索"与"真实 workload 复现"

### D5 — 验证：gpgpu-sim 区间对照
- ✅ 5 类关键场景 microbenchmark
- ✅ 精度目标：带宽 ±15%, 延迟 ±20%, 吞吐 ±20%, cache 命中率 ±5pp
- ✅ 参考 gpgpu-sim 已发布 microbenchmark 数据（不依赖其代码）
- 理由：用户决策"借鉴 gpgpu-sim 不集成"——区间对照而非精确复现

### D6 — 时间线：独立 Phase 8 路径
- ✅ gpu_soc 作为独立 Phase 8 路径（Phase 8.A / 8.B / 8.C）
- ✅ 与 apu_soc Phase 7.B-F 并行，互不阻塞
- ✅ apu_soc 后期（Phase 7.D-F）可享用 gpu_soc 早期成果（共享子模块）
- 理由：用户明确"并行于 apu_soc"

### D7 — 实施策略：3 阶段渐进
- ✅ Phase 8.A 基础设施 (4w) — GpuNoC + SharedMemory + MemoryCluster + KernelLaunch
- ✅ Phase 8.B 核心仿真 (6w) — SubCore + Pipeline + TensorCore + WarpScheduler + Scoreboard + L2Partition
- ✅ Phase 8.C 高级特性 (3w) — Tcc + Tma + Dsm + PowerModel
- 理由：风险可控 + 每阶段独立验证 + 与 apu_soc 持续互哺

### D8 — 抽象层级：Phase-accurate
- ✅ SubCore 内部 5+V 管线黑盒化（分数 cycle 输出）
- ✅ Scoreboard 抽象为 12 entries 队列（不做 17-bit ctrl code 模拟）
- ❌ 不做 5 warps/sub-core CGGTY 阈值精确模拟
- 理由：性能建模目标是"系统吞吐 + 延迟分布"，不需要 cycle-by-cycle 准确

---

## 3. 5 级层次与 14 个新模块

### 3.1 完整 4 级层次结构

```
GpuSoc (顶层, Phase 8.A 起)
├── GpuNoC (mesh interconnect)              [8.A 新]
│   ├── Router (XY 路由, V-pipe NUM_VC=1)
│   └── Link (延迟 + 带宽参数)
├── MemoryCluster (HBM2e / GDDR7)           [8.A 新]
│   ├── MemoryController × N 通道
│   └── Arbiter (通道分配)
└── GpuCluster (共享 with apu_soc)          [P5 已有 stub, 8.A 完善]
    └── GpcCluster (共享) × N
        └── TpcCluster (共享) × M
            └── ComputeCluster (共享) × K
                └── GpuComputeUnitTLM         [F12 已建, gpu_soc 复用]
                    ├── VectorRegFileTLM      [F12 已建]
                    └── WavefrontTLM          [F12 已建]
                ↓ Phase 8.A 加 ↓
                ├── SharedMemoryTLM           [8.A 新]
                ↓ Phase 8.B 加 ↓
                ├── TensorCoreTLM             [8.B 新]
                ├── PipelineTLM (5+V 抽象)     [8.B 新]
                ├── WarpSchedulerTLM          [8.B 新]
                ├── ScoreboardTLM             [8.B 新]
                └── L2PartitionTLM            [8.B 新]
                ↓ Phase 8.C 加 ↓
                ├── TccTLM (write coalescing) [8.C 新]
                ├── TmaTLM (async copy)       [8.C 新]
                ├── DsmTLM (inter-SM shmem)   [8.C 新]
                └── PowerModelTLM (80W+1W/SM) [8.C 新]
```

### 3.2 14 个新模块清单

| # | 模块 | Phase | LOC 估 | 抽象层级 | 复用量 |
|:-:|------|:---:|:---:|------|------|
| 1 | MemoryClusterTLM | 8.A | 200 | phase-accurate | 复用 MemoryTLM |
| 2 | SharedMemoryTLM | 8.A | 250 | cycle-accurate (bank conflict) | 新建 |
| 3 | GpuNoC (mesh) | 8.A | 300 | phase-accurate | 复用 RouterTLM/LinkTLM |
| 4 | KernelLaunchTLM | 8.A | 200 | phase-accurate | 类比 GPUTLM 模式 |
| 5 | SubCoreTLM | 8.B | 400 | black-box pipe | 复用 ChStreamModuleBase |
| 6 | WarpSchedulerTLM | 8.B | 350 | phase-accurate | 复用 SubCoreTLM API |
| 7 | ScoreboardTLM | 8.B | 200 | phase-accurate | 12 entries 队列 |
| 8 | TensorCoreTLM | 8.B | 300 | phase-accurate | 6 精度参数化 |
| 9 | PipelineTLM | 8.B | 500 | phase-accurate | 5+V 抽象，分数 cycle |
| 10 | L2PartitionTLM | 8.B | 200 | phase-accurate | 扩展 CacheTLM |
| 11 | TccTLM | 8.C | 200 | phase-accurate | 复用 DualPortStreamAdapter |
| 12 | TmaTLM | 8.C | 200 | phase-accurate | mbarrier 抽象 |
| 13 | DsmTLM | 8.C | 200 | phase-accurate | inter-SM shmem |
| 14 | PowerModelTLM | 8.C | 150 | post-hoc stat | 80W + 1W/SM 经验模型 |
| | **合计** | | **3650 LOC** | | |

### 3.3 已有模块（gpu_soc 直接复用）

| 模块 | 来源 | 复用方式 |
|------|------|---------|
| GpuComputeUnitTLM | apu_soc F12 | 共享头文件 + 注册 |
| VectorRegFileTLM | apu_soc F12 | 同上 |
| WavefrontTLM | apu_soc F12 | 同上 |
| GpuCluster | P5 stub, 8.A 完善 | 引入 `GpuClusterSharedInterface` 抽象层 |
| GpcCluster | P5 stub, 8.A 完善 | 同上 |
| TpcCluster | P5 stub, 8.A 完善 | 同上 |
| ComputeCluster | P5 stub, 8.A 完善 | 同上 |
| CacheTLM | v0 已实施 | L2PartitionTLM 继承 |
| MemoryTLM | v0 已实施 | MemoryClusterTLM 多通道编排 |
| RouterTLM/LinkTLM | v0 已实施 | GpuNoC mesh 拓扑构建 |
| CoherentXBarTLM | P0 已实施 | gpu_soc 不使用（无 CPU 侧） |
| ComputeReqBundle/ComputeRespBundle | 7.A 已实施 | gpu_soc 复用 |

---

## 4. Python API 设计

### 4.1 `cpptlm.nvidia` 子包

```python
import cpptlm.nvidia as nv

# === 1. 参数化 GPU 拓扑生成 ===
topo = nv.gpu_topology(
    # SM 架构
    sm_arch="blackwell_sm_120",      # blackwell_sm_120 / hopper_sm_90 / ada_sm_89 / ampere_sm_80
    # 4 级层次
    num_gpc=9, num_tpc_per_gpc=6, num_sm_per_tpc=2,
    num_subcore_per_sm=4,            # NVIDIA sub-core 数 (恒为 4)
    # TensorCore 配置
    tensor_core_precisions=["FP4","FP6","FP8","FP16","BF16","TF32"],
    # 内存
    mem_type="GDDR7",                # HBM3e / HBM3 / HBM2e / GDDR7 / GDDR6X
    mem_capacity_gb=72,
    mem_bandwidth_gbs=1176,
    mem_channels=12,                 # 384-bit / 32-bit = 12
    # 缓存
    l2_partitioned=True,             # Hopper 风格双分区 / GB203 风格单分区
    l2_capacity_mb=96,
    shared_mem_kb_per_sm=100,        # 99 KB opt-in
    # 协议（Phase 8.C 起）
    coherence_protocol="write_through",  # write_through / MOESI_AMD / MESI_GPU
    tcc_enabled=True,
    tma_enabled=False,               # Hopper+ 异步
    dsm_enabled=False,               # Hopper+ inter-SM shmem
)

# === 2. 预置 SKU 蓝图 ===
gb203 = nv.gb203_consumer()       # 110 SM, GDDR7 1176 GB/s
gb200 = nv.gb200_datacenter()     # 148 SM, HBM3e 8 TB/s, NVLink 5
gh100 = nv.gh100_hopper()         # 132 SM, HBM3 3 TB/s
ga100 = nv.ga100_ampere()         # 108 SM, HBM2e

# === 3. Variant (派生) ===
gb203_minimal = gb203.with_sm_count(4)        # 4 SM 用于单元测试
gb203_4gpc_2tpc_2cu = (gb203
    .with_gpc(4)
    .with_tpc_per_gpc(2)
    .with_sm_per_tpc(1))                       # 4 GPC × 2 TPC × 1 SM = 8 SM

# === 4. 导出与验证 ===
topo.to_json("configs/gpu_soc_gb203.json")
topo.to_dot("docs/topology/gpu_soc_gb203.svg")    # 可视化
topo.validate()                                    # 校验参数一致性
```

### 4.2 `cpptlm.gpu_workload` 子包

```python
import cpptlm.gpu_workload as gw

# === 1. WorkloadGenerator (参数化 kernel 序列) ===
wl1 = gw.WorkloadGenerator(
    kernel_pattern="GEMM",            # GEMM / FlashAttn / stencil / vector_add / sparse
    grid_size=(128, 128, 1),
    block_size=(32, 8, 1),
    data_type="FP16",
    shared_mem_kb=64,
    access_pattern="coalesced",       # coalesced / strided / random
    num_kernels=100,
    kernel_issue_interval=1000,       # cycles
)

# === 2. TraceReplay (NCU/CuPTI 格式) ===
wl2 = gw.TraceReplay(
    trace_path="traces/gpt3_inference.ncu-rep",
    cycle_granularity=1,              # 每个 cycle 推进
    format="ncu",                     # ncu / cupti / custom
)

# === 3. 5 类 pattern 快捷构造器 ===
wl_gemm = gw.GEMM(m=4096, n=4096, k=4096, dtype="FP16")
wl_fa = gw.FlashAttention(batch=8, head=16, seq_len=512)
wl_vecadd = gw.VectorAdd(n=1024*1024)
wl_stencil = gw.Stencil3D(n=512, points=7)
wl_spmv = gw.SparseSpMV(rows=10_000, cols=10_000, density=0.01)
```

### 4.3 `cpptlm.gpu_soc` 顶层

```python
import cpptlm.gpu_soc as gs

# === 1. 仿真执行 ===
sim = gs.simulate(
    topo=gb203,
    workload=wl1,
    duration_cycles=1_000_000,
    metrics=[
        "bandwidth",                # GB/s
        "latency_p99",              # cycles
        "cache_hit_rate",           # 0-1
        "snoop_req_per_sec",
        "tcc_coalescing_ratio",
        "tensor_core_utilization",
    ],
)

# === 2. 报告 ===
print(sim.report())  # 输出 + 与 gpgpu-sim 对照表

# === 3. 与 apu_soc 集成（Phase 8.C 起） ===
import cpptlm.apu_soc as apu
apu_sim = apu.simulate(
    cpu_topology=cpu_cluster,
    gpu_topology=gb203,             # gpu_soc 复用
    duration_cycles=1_000_000,
)
```

---

## 5. 3 阶段实施路径

### 5.1 Phase 8.A — 基础设施 (4 周, ~950 LOC)

**目标**：让 GpuCluster 真正"动起来"，先打通 bundle 流通

| 模块 | LOC | 依赖 | 验收 |
|------|:---:|------|------|
| MemoryClusterTLM | 200 | MemoryTLM 多通道 | 单元测试：通道分配 + 带宽验证 |
| SharedMemoryTLM | 250 | bundles + bank conflict | 单元测试：bank conflict 模拟 |
| GpuNoC (mesh) | 300 | RouterTLM/LinkTLM 复用 | 单元测试：XY 路由 + 延迟注入 |
| KernelLaunchTLM | 200 | GPUTLM 模式复用 | 单元测试：AQL 简化 launch |
| 完善 GpuCluster/GpcCluster/TpcCluster/ComputeCluster stub | — | 引入 `GpuClusterSharedInterface` | apu_soc 不破坏 |

**验收点 M1**：
- `gpu_soc_phase8a.json` 端到端配置文件
- 单元测试：SharedMemory bank conflict / MemoryCluster 通道分配
- 集成测试：CU → SharedMemory → L1 → NoC → Memory 闭环
- 性能：1 SM 仿真 1M cycles < 5 秒（单核）

### 5.2 Phase 8.B — 核心仿真 (6 周, ~1950 LOC)

**目标**：实现真正的 microarch 仿真（sub-core + pipeline + TC）

| 模块 | LOC | 备注 | 验收 |
|------|:---:|------|------|
| SubCoreTLM | 400 | 4 scheduler 抽象，5+V pipe 黑盒 | 单元测试：scheduler 优先级 |
| WarpSchedulerTLM | 350 | round-robin + priority, 5-warp CGGTY 阈值 | 单元测试：5-warp 加速比 |
| ScoreboardTLM | 200 | ≥12 entries, 17-bit ctrl code 抽象 | 单元测试：RAW hazard 检测 |
| TensorCoreTLM | 300 | 6 精度参数化（FP4/FP6/FP8/FP16/BF16/TF32） | 单元测试：各精度延迟模型 |
| PipelineTLM | 500 | P0/V/P1/P2/P3/P4 抽象，分数 cycle 输出 | 单元测试：5 cycle FFMA 模型 |
| L2PartitionTLM | 200 | multi-slice L2（近/远分区） | 单元测试：分区延迟差异 |

**验收点 M2**：
- 5 类场景 microbenchmark：GEMM / FlashAttn / vector_add / stencil / sparse
- 与 gpgpu-sim 区间对照（带宽 ±15%）
- Coalescer 行为验证
- 性能：1 GB203 (110 SM) 仿真 1M cycles < 60 秒

### 5.3 Phase 8.C — 高级特性 (3 周, ~750 LOC)

**目标**：与 apu_soc 共享子模块 + TCC + TMA + 验证报告

| 模块 | LOC | 备注 | 验收 |
|------|:---:|------|------|
| TccTLM | 200 | write coalescing (DualPortStreamAdapter) | 单元测试：合并比 ≥ 4x |
| TmaTLM | 200 | async copy 抽象 + mbarrier | 单元测试：load 488cyc / store 33cyc |
| DsmTLM | 200 | inter-SM shmem 抽象 | 单元测试：230cyc 远程读取 |
| PowerModelTLM | 150 | 80W 基础设施 + 1W/SM 经验模型 | 单元测试：P = 80 + N×1 公式 |
| + 与 apu_soc Phase 7.F 集成 | — | 共享 GpuClusterSharedInterface | apu_soc 端到端测试 |

**验收点 M3**：
- 5 类场景完整验证报告
- 与 gpgpu-sim 数值对照（带宽 ±15%, 延迟 ±20%）
- 与 apu_soc Phase 7.F 端到端集成
- 完整 PowerModel 输出（per-SM 能耗）

### 5.4 总工期

**Phase 8.A + 8.B + 8.C = 4 + 6 + 3 = 13 周**（约 3 周 4 人并行关键路径）

**关键路径**：8.A (GpuCluster 完善) → 8.B (SubCore 核心) → 8.C (TCC 验证) → 验证报告

**并行机会**：
- 8.A MemoryCluster 与 SharedMemory 可并行
- 8.B WarpScheduler / Scoreboard / TensorCore 可并行
- 8.C TccTLM 与 TmaTLM 可并行

---

## 6. 验证策略

### 6.1 验证基线

**gpgpu-sim + gem5 apu_se.py 数值对照**（用户决策"借鉴不集成"——仅参考其已发布数据）

5 类关键场景：

| 场景 | 配置 | 关键指标 |
|------|------|---------|
| **GEMM** | FP16, M=N=K=4096 | TFLOPS / 内存带宽 |
| **FlashAttention** | batch=8, head=16, seq=512 | 延迟 p99 / 带宽 |
| **vector_add** | n=1024×1024 | 内存带宽 |
| **stencil** | 3D 7-point, N=512³ | 内存带宽 / cache 命中率 |
| **sparse SpMV** | matrix=10k×10k, density=0.01 | cache 命中率 / 内存带宽 |

### 6.2 精度目标

| 指标 | 精度目标 | 备注 |
|------|:---:|------|
| 内存带宽 | ±15% | 与 gpgpu-sim 同 size 仿真对照 |
| 延迟 p99 | ±20% | 工作负载特征依赖 |
| 吞吐 (TFLOPS) | ±20% | TC 利用率敏感 |
| Cache 命中率 | ±5pp | 绝对百分点 |
| TCC 合并比 | 趋势一致 | 不强求数值 |
| PowerModel | ±30% | 经验模型简化 |

### 6.3 验证报告产出

每阶段产出：
- `docs/validation/phase8a_8b_8c_report.md` —— 阶段验证报告
- `docs/validation/gpgpu_sim_comparison.csv` —— 数值对照表
- `docs/validation/perf_dashboard.html` —— 可视化 dashboard（可选）

### 6.4 里程碑

- **M1 (Phase 8.A end, week 4)**: Single SM 可执行简单 kernel
- **M2 (Phase 8.B end, week 10)**: 完整 GB203 仿真可运行
- **M3 (Phase 8.C end, week 13)**: 5 类场景验证报告 + 与 apu_soc 集成

---

## 7. 模块组织与目录

### 7.1 新增目录结构

```
include/tlm/gpu/                    (已存在, Phase 7.A 起)
├── gpu_tlm.hh                       (v0 黑盒发起器, F12 升级为 GpuComputeUnitTLM)
├── compute_unit_tlm.hh              (F12 新, rename from gpu_tlm)
├── vector_regfile_tlm.hh            (F12 新)
├── wavefront_tlm.hh                 (F12 新)
├── kernel_launch_tlm.hh             (8.A 新)
├── shared_memory_tlm.hh             (8.A 新)
├── memory_cluster_tlm.hh            (8.A 新)
├── gpu_noc_tlm.hh                   (8.A 新, mesh 顶层)
├── subcore_tlm.hh                   (8.B 新)
├── warp_scheduler_tlm.hh            (8.B 新)
├── scoreboard_tlm.hh                (8.B 新)
├── tensor_core_tlm.hh               (8.B 新)
├── pipeline_tlm.hh                  (8.B 新)
├── l2_partition_tlm.hh              (8.B 新)
├── tcc_tlm.hh                       (8.C 新)
├── tma_tlm.hh                       (8.C 新)
├── dsm_tlm.hh                       (8.C 新)
├── power_model_tlm.hh               (8.C 新)
├── gpu_soc_tlm.hh                   (8.A 新, 顶层)
└── gpu_cluster_shared_interface.hh  (8.A 新, 共享抽象层)

src/tlm/gpu/                          (新增, 与 include/ 对应)
├── ... 同 .cc 实现

include/bundles/                      (扩展)
├── compute_bundles_tlm.hh           (已有)
├── warp_state_bundle.hh             (8.B 新, sub-core 状态)
├── tensor_core_bundle.hh            (8.B 新, TC 指令)
└── shared_memory_bundle.hh          (8.A 新, shmem access)

cpptlm/cpptlm/nvidia/                 (Python 子包, 新建)
├── __init__.py
├── topology.py                      (gpu_topology / 预置 SKU)
├── blueprint.py                     (NvidiaGPUBlueprint + Variant)
├── export.py                        (to_json / to_dot / validate)
└── sku_library.py                   (gb203 / gb200 / gh100 / ga100)

cpptlm/cpptlm/gpu_workload/          (Python 子包, 新建)
├── __init__.py
├── generator.py                     (WorkloadGenerator)
├── replay.py                        (TraceReplay)
└── patterns.py                      (5 类 pattern 构造器)

cpptlm/cpptlm/gpu_soc/               (Python 顶层)
├── __init__.py
├── simulate.py                      (simulate 入口)
└── report.py                        (report 报告生成)

configs/templates/gpu_soc/            (JSON 蓝图, 新建)
├── gpu_soc_gb203_v1.json
├── gpu_soc_gb200_v1.json
├── gpu_soc_gh100_v1.json
└── gpu_soc_ga100_v1.json
```

### 7.2 注册宏

所有 `ChStreamModuleBase` 派生类（如 `SharedMemoryTLM` / `MemoryClusterTLM`）通过 `REGISTER_CHSTREAM` 宏注册到 `include/chstream_register.hh`。

所有 `SimModule` 派生类（如 `GpuSocTLM`）通过 `REGISTER_MODULE` 宏注册到 `include/modules_cluster.hh`（追加到 9 个已有 SimModule 之后）。

### 7.3 与 apu_soc 共享机制

引入 `include/tlm/gpu/gpu_cluster_shared_interface.hh`：

```cpp
// GpuClusterSharedInterface —— apu_soc 和 gpu_soc 共用
class GpuClusterSharedInterface {
public:
    virtual ~GpuClusterSharedInterface() = default;
    virtual void set_gpu_topology(const GpuTopology& topo) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
    virtual void tick() = 0;
    virtual StatGroup* get_stats_group() = 0;
};
```

apu_soc 的 `ApuSoC::incorporate_parent` 与 gpu_soc 的 `GpuSocTLM` 都引用此接口，避免代码重复。

---

## 8. 文件清单 + LOC 估算

### 8.1 新增文件汇总

| 阶段 | 新增头文件 | 新增测试 | 新增配置 | LOC 估算 |
|------|-----------|----------|----------|----------|
| **8.A** | 5 (shared_mem / memory_cluster / gpu_noc / kernel_launch / gpu_soc + shared_iface) | 5 | 4 | 1250 |
| **8.B** | 6 (subcore / warp_sched / scoreboard / tensor_core / pipeline / l2_partition) | 6 | 2 | 2150 |
| **8.C** | 4 (tcc / tma / dsm / power_model) | 4 | 1 | 900 |
| **小计** | 15 头文件 | 15 测试 | 7 配置 | **~4300 LOC** |
| **Python 库** | nvidia + gpu_workload + gpu_soc 子包 | pytest | — | **~800 LOC** |
| **总** | 15 头文件 + 3 Python 子包 | 15+ 测试 | 7+ 配置 | **~5100 LOC** |

### 8.2 修改文件清单

| 文件 | 阶段 | 修改内容 |
|------|------|----------|
| `include/chstream_register.hh` | 8.A | +5 行注册 (新 ChStream 模块) |
| `include/modules_cluster.hh` | 8.A | +1 行 (GpuSocTLM) |
| `include/tlm/cluster/gpu_cluster.hh` | 8.A | 引入 GpuClusterSharedInterface |
| `include/tlm/cluster/{gpc,tpc,compute}_cluster.hh` | 8.A | stub 完善 |
| `include/tlm/gpu/gpu_tlm.hh` | 8.A | rename 为 compute_unit_tlm，共享给 apu_soc |
| `docs/adr/README.md` | 8.A | append NV-01 |
| `docs/soc_arch/adr/ADR-SOC-01..05.md` | 8.A | 状态更新（不动决策） |
| `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` | 8.A | 追加 Phase 8 节 |
| `AGENTS.md` | 8.C | STRUCTURE 节加 `cpptlm/cpptlm/{nvidia,gpu_workload,gpu_soc}/` |

### 8.3 微架构 doc（gpu_soc 补充）

| 文档 | 阶段 | 状态 |
|------|------|------|
| `docs/soc_arch/modules/gpu-soc.md` | 8.A | 新建 (顶层) |
| `docs/soc_arch/modules/gpu-noc-mesh.md` | 8.A | 新建 |
| `docs/soc_arch/modules/gpu-shared-memory.md` | 8.A | 新建 |
| `docs/soc_arch/modules/gpu-memory-cluster.md` | 8.A | 新建 |
| `docs/soc_arch/modules/gpu-kernel-launch.md` | 8.A | 新建 (扩展 SOC-04) |
| `docs/soc_arch/modules/gpu-subcore.md` | 8.B | 新建 |
| `docs/soc_arch/modules/gpu-warp-scheduler.md` | 8.B | 新建 |
| `docs/soc_arch/modules/gpu-scoreboard.md` | 8.B | 新建 |
| `docs/soc_arch/modules/gpu-tensor-core.md` | 8.B | 新建 |
| `docs/soc_arch/modules/gpu-pipeline.md` | 8.B | 新建 |
| `docs/soc_arch/modules/gpu-l2-partition.md` | 8.B | 新建 |
| `docs/soc_arch/modules/gpu-tcc.md` | 8.C | 新建 (扩展 Phase 7.D TCC) |
| `docs/soc_arch/modules/gpu-tma.md` | 8.C | 新建 |
| `docs/soc_arch/modules/gpu-dsm.md` | 8.C | 新建 |
| `docs/soc_arch/modules/gpu-power-model.md` | 8.C | 新建 |

总计 15 个新微架构 doc。

---

## 9. 风险与缓解 R1-R5

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| **R1** | 5+V 管线过复杂 | 中 | 高 | Phase 8.B 简化为 black-box pipe（分数 cycle 输出），不做 17-bit ctrl code |
| **R2** | TC 12 精度爆炸 | 中 | 中 | 首期 6 精度，剩余通过模拟精度可扩展（不重新实现） |
| **R3** | Trace 格式不统一 | 中 | 中 | 首期 NCU `.ncu-rep` + CuPTI `.cupti-trace` 双格式；其他格式延期 |
| **R4** | apu_soc/gpu_soc 共享接口冲突 | 中 | 中 | 引入 `GpuClusterSharedInterface` 抽象层，两条路径都引用 |
| **R5** | gpgpu-sim baseline 不可复现 | 低 | 中 | 用其已发布 microbenchmark 数值（NVIDIA 论文 + Jarmusch 2507.10789），不依赖其代码 |

---

## 10. 反模式（明确不做）

| 反模式 | 理由 | 替代方案 |
|--------|------|---------|
| ❌ Cycle-accurate 5+V 管线 | 与 phase-accurate 定位矛盾；工作量爆炸 | Black-box pipe，分数 cycle 输出 |
| ❌ 12 精度 TensorCore 全实现 | 90% 用例只用 6 种 | 首期 6 精度，模拟精度可扩展 |
| ❌ 集成 gpgpu-sim 代码 | 绑死外部依赖 | 借鉴设计 + 参考已发布数据 |
| ❌ 真实 kernel 编译/执行 | 非仿真器职责 | WorkloadGenerator 描述行为 |
| ❌ CPU/GPU coherence | 属于 apu_soc 范畴 | gpu_soc 是纯 GPU 端 |
| ❌ dGPU + PCIe + Disjoint NoC | Phase 9 备选 | 当前 focus 独立 GPU 性能模型 |
| ❌ 完整 SASS/PTX 解释 | 仿真器职责外 | WorkloadGenerator 抽象 |
| ❌ 重新实现 F12 三类 | apu_soc 已建 | gpu_soc 共享 apu_soc 产出 |

---

## 11. 决策点汇总

| # | 决策 | 选项 | 影响 |
|---|------|------|------|
| **D1** | 与 apu_soc 关系 | 共享 GPU 子模块 | 顶层代码可复用 |
| **D2** | 核心定位 | 工业性能建模（phase-accurate） | 不做 cycle-accurate |
| **D3** | SKU 范围 | 参数化 + 预置库 | cpptlm.nvidia 子包 |
| **D4** | 工作负载 | WorkloadGenerator + TraceReplay | cpptlm.gpu_workload 子包 |
| **D5** | 验证 | gpgpu-sim 区间对照 | 5 类场景 ±15% 带宽 |
| **D6** | 时间线 | 独立 Phase 8 路径 | 与 apu_soc 并行 |
| **D7** | 实施策略 | 3 阶段渐进 | Phase 8.A / 8.B / 8.C |
| **D8** | 抽象层级 | Phase-accurate | sub-core 内部黑盒化 |

---

## 12. 文档配套

按用户要求"创建新的ADR, SOC架构文档，实施路径文档"：

| 文档 | 路径 | 阶段 | 状态 |
|------|------|------|------|
| **SOC 架构文档**（本文） | `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` | 8.A 启动前 | 🔄 Draft（待 review） |
| **新 ADR** | `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` | 8.A 启动前 | ⏳ 待签发 |
| **实施路径 doc** | `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` | 8.A 启动后（writing-plans skill 产出） | ⏳ 待写 |
| **OpenSpec 变更** | `openspec/changes/2026-06-24-gpu-soc-architecture/{proposal,design,specs/,tasks}.md` | 8.A 启动前 | ⏳ 待创建 |
| **微架构 doc** (15 个) | `docs/soc_arch/modules/gpu-*.md` | 每阶段同步 | ⏳ 分阶段写 |

**新 ADR 命名空间**：
- `ADR-NV-XX` —— NV = NVIDIA（与 ADR-X 框架层、ADR-SOC 应用层并列）
- 模板复用 `docs/adr/ADR-P1-TEMPLATE.md`

---

## 13. 修订历史

- **2026-06-24 v1.0 (本文)** — 初版，6 轮 brainstorming 产出 + 推荐方案 A + 8 项决策 D1-D8 + 14 个新模块 + 3 阶段实施 + 验证策略
- **待 user review** — 等待用户对 spec 的批准
- **待 OpenSpec 创建** — 与本 spec 同步
- **待新 ADR 签发** — `ADR-NV-01-gpu-soc-architecture-target.md`
- **待 writing-plans 实施** — `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md`
- **Phase 8.A 启动后** — 状态更新（不动决策）

---

**关联文档清单**：
- 父 roadmap: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`
- 兄弟 spec (apu_soc): `docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md`
- gpgpu-sim SM_120 paper: https://zartbot.github.io/micro_arch/nvidia/sm_120/paper.html
- 本地 notes: `/workspace/project/mynotes/04_Knowledge/D01-gpu-architecture/`
- roadmap 中 5 份 SOC ADR: `docs/soc_arch/adr/ADR-SOC-01..05.md`
- 微架构 doc 索引: `docs/soc_arch/modules/`

---

**维护**: CppTLM 开发团队
**下次 review**: 与 user spec review 一致
