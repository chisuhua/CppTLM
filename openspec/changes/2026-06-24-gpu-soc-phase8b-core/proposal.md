# OpenSpec Change: gpu_soc Phase 8.B — 核心仿真

> **Change ID**: `2026-06-24-gpu-soc-phase8b-core`
> **Status**: 🔄 Draft（待 Phase 8.A 完成后启动）
> **Date**: 2026-06-24
> **Author**: Sisyphus
> **Target Release**: Phase 8.B end (~6 weeks)
> **Depends on**: `2026-06-24-gpu-soc-phase8a-infra`（M1 必须先完成）

---

## 1. Why（动机）

**Phase 8.A 完成后**：
- ✅ gpu_soc 4 级 cluster 容器从 stub 升级为功能完整
- ✅ 4 个核心模块（MemoryCluster / SharedMemory / GpuNoC / KernelLaunch）落地
- ✅ GpuSocTLM 顶层可独立运行端到端
- ❌ **但**：sub-core 内部仍黑盒（不做 warp scheduler / scoreboard / pipeline / TC）

**目标（Phase 8.B 完成后）**：
- ✅ SubCoreTLM 实现 4 scheduler 抽象
- ✅ WarpSchedulerTLM 实现 round-robin + 5-warp CGGTY 阈值
- ✅ ScoreboardTLM 实现 ≥12 entries RAW hazard 检测
- ✅ TensorCoreTLM 实现 6 精度参数化（FP4/FP6/FP8/FP16/BF16/TF32）
- ✅ PipelineTLM 实现 5+V 抽象
- ✅ L2PartitionTLM 实现 multi-slice 近/远分区
- ✅ 5 类 microbenchmark 端到端跑通
- ✅ M2 验收：5 类场景 gpgpu-sim 区间对照（带宽 ±15%），1 GB203 × 1M < 60s

---

## 2. What Changes（变更内容）

### 新增文件（6 个 .hh + 6 个 .cc + 2 个 bundle + 7 个 test + 1 个集成测试 + 6 个微架构 doc）

| 类型 | 路径 | 阶段 |
|------|------|:---:|
| Bundle | `include/bundles/warp_state_bundle.hh` | 8.B |
| Bundle | `include/bundles/tensor_core_bundle.hh` | 8.B |
| Header | `include/tlm/gpu/subcore_tlm.hh` | 8.B |
| Header | `include/tlm/gpu/warp_scheduler_tlm.hh` | 8.B |
| Header | `include/tlm/gpu/scoreboard_tlm.hh` | 8.B |
| Header | `include/tlm/gpu/tensor_core_tlm.hh` | 8.B |
| Header | `include/tlm/gpu/pipeline_tlm.hh` | 8.B |
| Header | `include/tlm/gpu/l2_partition_tlm.hh` | 8.B |
| Impl | `src/tlm/gpu/{subcore,warp_scheduler,scoreboard,tensor_core,pipeline,l2_partition}_tlm.cc` | 8.B |
| Test | `test/test_{subcore,warp_scheduler,scoreboard,tensor_core,pipeline,l2_partition}_tlm.cc` | 8.B |
| Test | `test/test_gpu_soc_phase8b.cc` | 8.B 集成 |
| Config | `configs/templates/gpu_soc/gpu_soc_phase8b.json` | 8.B 集成 |
| Python | `test/python/test_gpgpu_sim_comparison.py` | 8.B 验证 |
| Doc | `docs/soc_arch/modules/gpu-subcore.md` | 8.B |
| Doc | `docs/soc_arch/modules/gpu-warp-scheduler.md` | 8.B |
| Doc | `docs/soc_arch/modules/gpu-scoreboard.md` | 8.B |
| Doc | `docs/soc_arch/modules/gpu-tensor-core.md` | 8.B |
| Doc | `docs/soc_arch/modules/gpu-pipeline.md` | 8.B |
| Doc | `docs/soc_arch/modules/gpu-l2-partition.md` | 8.B |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `include/chstream_register.hh` | +6 行注册 |
| `include/tlm/cluster/compute_cluster.hh` | 集成 SubCoreTLM + WarpSchedulerTLM |
| `include/tlm/cluster/gpu_cluster.hh` | 集成 L2PartitionTLM（per GPC L2 slice） |

---

## 3. Goals

| 编号 | 标准 | 验证 |
|------|------|------|
| **G1** | 6 个核心模块单元测试全 pass | `[gpu][subcore][sched][sb][tc][pipe][l2]` |
| **G2** | 5 类 microbenchmark 跑通 | `test_gpu_soc_phase8b.cc` |
| **G3** | gpgpu-sim 带宽 ±15% | `test/python/test_gpgpu_sim_comparison.py` |
| **G4** | 性能 M2 达标 | 1 GB203 (110 SM) × 1M cycles < 60s |
| **G5** | 文档 | 6 个微架构 doc + docs_sync 0 missing |
| **G6** | Phase 8.A 不破坏 | apu_soc 兼容性测试全绿 |

---

## 4. Non-Goals

- ❌ Tcc / Tma / Dsm / PowerModel（→ 8.C）
- ❌ 完整 12 精度 TC（首期 6 精度）
- ❌ 17-bit ctrl code / 5-warp CGGTY 精确（按 D2 简化）
- ❌ dGPU + PCIe（Phase 9 备选）

---

## 5. 6 个核心模块设计要点

### SubCoreTLM（400 LOC）
- 内部: 4×WarpSchedulerTLM + 1×ScoreboardTLM + 1×PipelineTLM + 1×TensorCoreTLM
- 对外: black-box，对外只输出"分数 cycle 的执行时间"
- `tick()` 推进 1 cycle
- `get_current_cycle() -> uint64_t`

### WarpSchedulerTLM（350 LOC）
- 简化模型：`scheduling_latency_cycles(active_warps, dep_chain_cyc) -> uint32_t`
- CGGTY 阈值：≤4 warps = 完整延迟（如 268 cyc/iter），≥5 warps = 6× 加速（如 45 cyc/iter）
- round-robin + priority 队列

### ScoreboardTLM（200 LOC）
- ≥12 entries 队列
- `allocate(sb_id)` / `release(sb_id)` / `has_free_entry()`
- RAW hazard 检测：13th allocate 失败

### TensorCoreTLM（300 LOC）
- 6 精度枚举：FP4/FP6/FP8/FP16/BF16/TF32
- 简化模型：所有精度 29 cyc 延迟 / 23 cyc 吞吐（按 SM_120 统一管线）
- `latency(precision) -> uint32_t` / `throughput_cyc(precision) -> uint32_t`

### PipelineTLM（500 LOC）
- 6 管线：P0_INT_FP32 / V_SIMD / P1_FP64 / P2_SFU / P3_LSU / P4_TC
- 简化：`execute(instr, pipe_id) -> cycles_used`（查表）
- 例如 FFMA on P0 = 4.22 cyc，FDIV on P2 = 50.76 cyc

### L2PartitionTLM（200 LOC）
- multi-slice 模型（GB203 96 MB / 9 slice 或 Hopper 50 MB / 2 slice）
- 简化：`access_latency(gpc_id, slice_id) -> uint32_t`
- 同 GPC slice = 79 cyc，跨 GPC slice = 180 cyc（按 SM_120 paper）

---

## 6. 5 类 microbenchmark

| 场景 | 配置 | 关键指标 | gpgpu-sim baseline (GB/s) |
|------|------|---------|:---:|
| GEMM | M=N=K=4096, FP16 | 带宽 | 700 |
| FlashAttn | b=8, h=16, seq=512 | 带宽 | 470 |
| vector_add | n=1024² | 带宽 | 1176 |
| stencil | 3D 7-point, N=512³ | 带宽 | 940 |
| sparse SpMV | 10k×10k, density=0.01 | 带宽 | 230 |

**对照方法**：`test/python/test_gpgpu_sim_comparison.py` 用 pytest 跑 5 类，每类误差 ≤15%。

---

## 7. Risks

| # | 风险 | P | I | 缓解 |
|---|------|:-:|:-:|------|
| **R1** | 5+V 管线模型不准确 | 中 | 高 | 简化查表（按 SPEC §D8 决策） |
| **R2** | L2Partition 拓扑选择（GB203 单片 vs Hopper 双片） | 中 | 中 | 参数化 `partitioned: bool` |
| **R3** | 5 类 microbenchmark 误差超 ±15% | 中 | 高 | 简化模型可调，不强求 |

---

## 8. 关联文档

- **Spec**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` §5.2
- **Plan**: `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.B
- **ADR**: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`
- **依赖**: `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/`

---

## 9. 验收节点

| 节点 | 时机 | 标准 |
|------|------|------|
| M2 单元测试 | Task 9-14 完成 | G1 |
| M2 集成测试 | Task 15 完成 | G2 |
| M2 验证对照 | Task 15 完成 | G3 |
| M2 性能 | Task 16 完成 | G4 |
| M2 文档 | Task 16 完成 | G5 |
| M2 兼容性 | Task 15 完成 | G6 |
| Oracle 审查 | 全部 Task 完成 | — |
| OpenSpec 归档 | Oracle 批准 | → archive/ |
