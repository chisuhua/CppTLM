# Spec Delta: gpu_soc Phase 8.B — 核心仿真

> **Change**: `2026-06-24-gpu-soc-phase8b-core`
> **Date**: 2026-06-24
> **Type**: ADDED

---

## ADDED Requirements

### REQ-GPU-8B-1: ScoreboardTLM (≥12 entries)

`include/tlm/gpu/scoreboard_tlm.hh` 必须提供：
- 类 `tlm::ScoreboardTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `entries` (默认 12)
- 方法 `has_free_entry()`, `allocate(sb_id) -> bool`, `release(sb_id) -> bool`
- 模块类型字符串：`"ScoreboardTLM"`

测试：`12 个 allocate 全部成功，第 13 个 allocate 失败`（has_free_entry() 返回 false）

### REQ-GPU-8B-2: WarpSchedulerTLM (CGGTY 5-warp 阈值)

`include/tlm/gpu/warp_scheduler_tlm.hh` 必须提供：
- 类 `tlm::WarpSchedulerTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `max_warps` (默认 12)
- 方法 `scheduling_latency_cycles(active_warps, dep_chain_cyc) -> uint32_t`
- 简化模型：`active_warps < 5` → 返回 `dep_chain_cyc`；`active_warps >= 5` → 返回 `dep_chain_cyc / 6`
- 模块类型字符串：`"WarpSchedulerTLM"`

测试：`(4 warps, 268 cyc) == 268` 且 `(5 warps, 268 cyc) == 44` (6× 加速)

### REQ-GPU-8B-3: PipelineTLM (5+V 抽象 + 分数 cycle)

`include/tlm/gpu/pipeline_tlm.hh` 必须提供：
- 枚举 `tlm::PipelineId` (P0_INT_FP32, V_SIMD, P1_FP64, P2_SFU, P3_LSU, P4_TC)
- 类 `tlm::PipelineTLM` 继承 `ChStreamModuleBase`
- 方法 `execute(instruction, pipe_id) -> double`（查表返回 cycles）
- 必须实现的查表项：`IADD3`=2.22, `FFMA`=4.22, `VIADD.U8x4`=4.0, `DFMA`=64.13, `MUFU.RCP`=44.28, `LDG`=30.0, `HMMA`=29.0
- 模块类型字符串：`"PipelineTLM"`

### REQ-GPU-8B-4: TensorCoreTLM (6 精度统一管线)

`include/tlm/gpu/tensor_core_tlm.hh` 必须提供：
- 枚举 `tlm::TcPrecision` (FP4, FP6, FP8, FP16, BF16, TF32)
- 类 `tlm::TensorCoreTLM` 继承 `ChStreamModuleBase`
- 方法 `latency(precision) -> uint32_t`（所有精度返回 29）
- 方法 `throughput_cyc(precision) -> uint32_t`（所有精度返回 23）
- 模块类型字符串：`"TensorCoreTLM"`

### REQ-GPU-8B-5: L2PartitionTLM (multi-slice)

`include/tlm/gpu/l2_partition_tlm.hh` 必须提供：
- 类 `tlm::L2PartitionTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `slices`, `capacity_mb`, `partitioned`
- 方法 `access_latency(gpc_id, slice_id) -> uint32_t`
- 简化模型：同 GPC slice = 79 cyc，跨 GPC slice = 180 cyc
- 模块类型字符串：`"L2PartitionTLM"`

### REQ-GPU-8B-6: SubCoreTLM (black-box pipe)

`include/tlm/gpu/subcore_tlm.hh` 必须提供：
- 类 `tlm::SubCoreTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `num_warps` (默认 32)
- `tick()` 方法推进 1 cycle（内部 4 scheduler + scoreboard + pipeline + TC）
- 方法 `get_current_cycle() -> uint64_t`
- 模块类型字符串：`"SubCoreTLM"`

### REQ-GPU-8B-7: 集成测试

`test/test_gpu_soc_phase8b.cc` 必须验证：
- 加载 `configs/templates/gpu_soc/gpu_soc_phase8b.json`
- 5 类 microbenchmark 跑通（GEMM/FlashAttn/vector_add/stencil/sparse）
- 输出 metrics 包括 bandwidth, latency, tensor_core_utilization

### REQ-GPU-8B-8: gpgpu-sim 区间对照 (Python)

`test/python/test_gpgpu_sim_comparison.py` 必须用 pytest 验证 5 类场景：
- GEMM (FP16, M=N=K=4096): 带宽 ≈ 700 GB/s (±15%)
- FlashAttn (b=8, h=16, seq=512): 带宽 ≈ 470 GB/s (±15%)
- vector_add (n=1024²): 带宽 ≈ 1176 GB/s (±15%)
- stencil (3D 7-point, N=512³): 带宽 ≈ 940 GB/s (±15%)
- sparse SpMV (10k×10k, 0.01): 带宽 ≈ 230 GB/s (±15%)

### REQ-GPU-8B-9: 微架构 doc

6 个新微架构 doc：`docs/soc_arch/modules/gpu-{subcore,warp-scheduler,scoreboard,tensor-core,pipeline,l2-partition}.md`

---

## MODIFIED Requirements

### MOD-COMPUTE-CLUSTER-1: 集成 SubCoreTLM

`include/tlm/cluster/compute_cluster.hh` 现在内含 1 个 `SubCoreTLM` 替代之前的 1 个 `GpuComputeUnitTLM`（保留 GpuComputeUnitTLM 作为兼容层）。

### MOD-GPU-CLUSTER-1: 集成 L2PartitionTLM

`include/tlm/cluster/gpu_cluster.hh` 现在每 GPC 持有 1 个 `L2PartitionTLM` 实例。

---

## REMOVED Requirements

无
