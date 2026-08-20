# Spec Delta: gpu_soc Phase 8.B — 核心仿真

> **Change**: `2026-06-24-gpu-soc-phase8b-core`
> **Date**: 2026-06-24 · **Revised**: 2026-07-03 (D1-Full, ADR-NV-02)
> **Type**: ADDED

---

## ADDED Requirements

### REQ-GPU-8B-1: ScoreboardTLM (实现 `tlm::IScoreboardInternal`)

`include/tlm/gpu/scoreboard_tlm.hh` 必须提供：
- **新增内部接口** `include/tlm/gpu/scoreboard_interface.hh`：`tlm::IScoreboardInternal` 纯虚基类 (4 方法)
- 类 `tlm::ScoreboardTLM` 继承 `ChStreamModuleBase` **和** `tlm::IScoreboardInternal`
- 构造参数：`name`, `EventQueue*`, `entries` (默认 12)
- 方法 `has_free_entry()`, `allocate(sb_id) -> bool`, `release(sb_id) -> bool`, `tick()`
- 内部数据结构: `uint32_t bitmask_` (O(1) 位操作，32 bit 够 12 entries)
- 模块类型字符串：`"ScoreboardTLM"`

测试：`12 个 allocate 全部成功，第 13 个 allocate 失败`（has_free_entry() 返回 false）

### REQ-GPU-8B-2a: WarpSchedulerTLM (重命名 Minimal + CGGTY)

`include/tlm/gpu/warp_scheduler_tlm.hh` 必须提供：
- 重命名 `MinimalWarpSchedulerTLM` → `WarpSchedulerTLM`
- 保留 uint32_t 接口（F12a 已与 PTX-EMU 对齐）: `add_warp/remove_warp/schedule_next/update_state/all_warps_finished/tick`
- 新增方法 `scheduling_latency_cycles(active_warps, dep_chain_cyc) -> uint32_t`
- 简化 CGGTY 模型：`active_warps < 5` → 返回 `dep_chain_cyc`；`active_warps >= 5` → 返回 `dep_chain_cyc / 6`
- 模块类型字符串：`"WarpSchedulerTLM"`
- 保留旧 `"MinimalWarpSchedulerTLM"` 注册项 + `[[deprecated]]`

测试：`(4 warps, 268 cyc) == 268` 且 `(5 warps, 268 cyc) == 44` (6× 加速)

### REQ-GPU-8B-2b: CppTLMWarpSchedulerAdapter (新)

`include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.hh` 必须提供：
- 类继承 PTX-EMU 纯虚基类 `WarpScheduler`
- 内部持有 `WarpSchedulerTLM*`, 桥接 `WarpContext*` ↔ `uint32_t`
- 额外方法: `set_execution_mode/get_execution_mode/schedule_with_migration` 提供默认实现
- 依赖 PTX-EMU `include/ptxsim/warp_scheduler.h` (仅 F12b-LD 编译)

### REQ-GPU-8B-3: PipelineTLM (实现 `tlm::IPipelineLatencyInternal`)

`include/tlm/gpu/pipeline_tlm.hh` 必须提供：
- **新增内部接口** `include/tlm/gpu/pipeline_interface.hh`：枚举 `tlm::PipelineId` (P0_INT_FP32=0, V_SIMD=1, P1_FP64=2, P2_SFU=3, P3_LSU=4, P4_TC=5) + `tlm::IPipelineLatencyInternal` 纯虚基类
- 类 `tlm::PipelineTLM` 继承 `ChStreamModuleBase` **和** `tlm::IPipelineLatencyInternal`
- 方法 `get_fractional_cycles(instruction, pipe_id) -> double`
- 必须实现的查表项：`IADD3`=2.22, `FFMA`=4.22, `VIADD.U8x4`=4.0, `DFMA`=64.13, `MUFU.RCP`=44.28, `LDG`=30.0, `HMMA`=29.0
- 枚举值 0-5 **必须**与 PTX-EMU `PipelineId` 一致（Adapter static_cast 转换）
- 模块类型字符串：`"PipelineTLM"`

### REQ-GPU-8B-4: TensorCoreTLM (实现 `tlm::ITensorCoreTimingInternal`)

`include/tlm/gpu/tensor_core_tlm.hh` 必须提供：
- **新增内部接口** `include/tlm/gpu/tensor_core_interface.hh`：枚举 `tlm::TcPrecision` (FP4=0, FP6=1, FP8=2, FP16=3, BF16=4, TF32=5) + `tlm::ITensorCoreTimingInternal` 纯虚基类
- 类 `tlm::TensorCoreTLM` 继承 `ChStreamModuleBase` **和** `tlm::ITensorCoreTimingInternal`
- 方法 `get_latency(precision) -> uint32_t`（所有精度返回 29）
- 方法 `get_throughput_cycles(precision) -> uint32_t`（所有精度返回 23）
- 枚举值 0-5 **必须**与 PTX-EMU `TcPrecision` 一致（Adapter static_cast 转换）
- 模块类型字符串：`"TensorCoreTLM"`

### REQ-GPU-8B-5: L2PartitionTLM (multi-slice)

`include/tlm/gpu/l2_partition_tlm.hh` 必须提供：
- 类 `tlm::L2PartitionTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `slices`, `capacity_mb`, `partitioned`
- 方法 `access_latency(gpc_id, slice_id) -> uint32_t`
- slice 归属 GPC 判定: `slice_id ∈ [gpc_id * slices_per_gpc, (gpc_id+1) * slices_per_gpc)`
- 简化模型：同 GPC slice = 79 cyc，跨 GPC slice = 180 cyc (按 SM_120 paper)
- 模块类型字符串：`"L2PartitionTLM"`

### REQ-GPU-8B-6: SubCoreTLM (SMContext 包裹 + 双模式 tick)

`include/tlm/gpu/subcore_tlm.hh` 必须提供：
- 类 `tlm::SubCoreTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `num_warps` (默认 32)
- **新增**: `set_sm_context(ptxsim::SMContext*)` — D1 模式注入入口 (仅 F12b-LD 时非空)
- `tick()` 双模式: `sm_ctx_ == nullptr` (独立模式, 内部组件) / `sm_ctx_ != nullptr` (D1, 调用 `exe_once()`)
- 方法 `get_current_cycle() -> uint64_t`
- 内部组件: 4 WarpScheduler + 1 Scoreboard + 1 Pipeline + 1 TC
- 模块类型字符串：`"SubCoreTLM"`

### REQ-GPU-8B-7: Adapter 层 (Task 15, 仅 F12b-LD 编译)

`include/tlm/gpu/adapter/` 目录必须提供 3 个 Adapter：
- `cpptlm_scoreboard_adapter.hh`: 继承 PTX-EMU `IScoreboard`, 桥接 `uint32_t(reg_id, warp_id)` ↔ `ScoreboardTLM::allocate(release_id)`
- `cpptlm_pipeline_adapter.hh`: 继承 PTX-EMU `IPipelineLatencyProvider`, 桥接 `int(stmt_type)` ↔ `string(instr)` 转发
- `cpptlm_tensor_core_adapter.hh`: 继承 PTX-EMU `ITensorCoreTiming`, 直接转发 (枚举值一致)
- 依赖 PTX-EMU `include/ptxsim/{scoreboard_interface,pipeline_interface,tensor_core_interface}.h`

### REQ-GPU-8B-8: 集成测试 (Level 1 + Level 2/3, 双轨验证)

`test/test_gpu_soc_phase8b.cc` (Level 1, 无 PTX-EMU)：
- 加载 `configs/templates/gpu_soc/gpu_soc_phase8b.json`
- 5 类合成 workload 跑通

`test/python/test_gpgpu_sim_comparison.py` (Level 3, F12b 后)：
- 真实 CUDA kernel 5 类: GEMM (FP16, M=N=K=4096) / FlashAttn (b=8, h=16, seq=512) / vector_add (n=1024²) / stencil (3D, N=512³) / sparse SpMV (10k×10k, 0.01)
- vs gpgpu-sim: 带宽 ±15%
- vs standalone PTX-EMU: timing ±10%

### REQ-GPU-8B-9: 微架构 doc

6 个新微架构 doc：`docs/soc_arch/modules/gpu-{subcore,warp-scheduler,scoreboard,tensor-core,pipeline,l2-partition}.md`

---

---

## MODIFIED Requirements

### MOD-COMPUTE-CLUSTER-1: 集成 SubCoreTLM

`include/tlm/cluster/compute_cluster.hh` 现在内含 1 个 `SubCoreTLM` 替代之前的 1 个 `GpuComputeUnitTLM`（保留 GpuComputeUnitTLM 作为兼容层）。

### MOD-GPU-CLUSTER-1: 集成 L2PartitionTLM

`include/tlm/cluster/gpu_cluster.hh` 现在每 GPC 持有 1 个 `L2PartitionTLM` 实例。

---

## REMOVED Requirements

无
