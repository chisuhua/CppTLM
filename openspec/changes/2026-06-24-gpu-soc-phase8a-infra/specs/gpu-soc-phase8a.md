# Spec Delta: gpu_soc Phase 8.A — 基础设施

> **Change**: `2026-06-24-gpu-soc-phase8a-infra`
> **Date**: 2026-06-24
> **Type**: ADDED（新增能力）

---

## ADDED Requirements

### REQ-GPU-8A-1: SharedMemoryTLM (32 bank, configurable size)

`include/tlm/gpu/shared_memory_tlm.hh` 必须提供：
- 类 `tlm::SharedMemoryTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `size_kb`, `banks`
- 方法 `bank_conflict_cycles(num_threads, stride_bytes) -> uint32_t`
- 简化模型：base 1 cycle + 每个 conflict way +1 cycle
- 模块类型字符串：`"SharedMemoryTLM"`

测试 `test/test_shared_memory_tlm.cc` 必须验证 4-way conflict 返回 `1 + 3 = 4` cycles。

### REQ-GPU-8A-2: MemoryClusterTLM (多通道 round-robin)

`include/tlm/gpu/memory_cluster_tlm.hh` 必须提供：
- 类 `tlm::MemoryClusterTLM` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `channels`, `capacity_gb`
- 方法 `allocate_channel(request_id) -> uint32_t`（round-robin）
- 方法 `get_channels() -> uint32_t`
- 模块类型字符串：`"MemoryClusterTLM"`

测试 `test/test_memory_cluster_tlm.cc` 必须验证 4-channel round-robin 5 次调用回到 channel 0。

### REQ-GPU-8A-3: GpuNoC (mesh XY 路由)

`include/tlm/gpu/gpu_noc_tlm.hh` 必须提供：
- 类 `tlm::GpuNoC` 继承 `ChStreamModuleBase`
- 构造参数：`name`, `EventQueue*`, `dim`, `hops_latency`
- 方法 `route_latency(src, dst) -> uint32_t`（XY 路由 + hops × latency）
- 模块类型字符串：`"GpuNoC"`

测试 `test/test_gpu_noc_tlm.cc` 必须验证 2x2 mesh 中 (0,0)→(1,1) 路由延迟 = (1+1) × hops_latency。

### REQ-GPU-8A-4: KernelLaunchTLM (AQL 简化)

`include/tlm/gpu/kernel_launch_tlm.hh` 必须提供：
- 类 `tlm::KernelLaunchTLM` 继承 `ChStreamModuleBase`
- 4 个 setter: `kernel_id`, `workgroup_size`, `grid_size`, `kernel_launch_interval`
- `tick()` 方法按 interval 周期发 KernelDesc
- 模块类型字符串：`"KernelLaunchTLM"`

### REQ-GPU-8A-5: GpuClusterSharedInterface 抽象层

`include/tlm/gpu/gpu_cluster_shared_interface.hh` 必须提供：
- 结构体 `tlm::GpuTopology` (num_gpc, num_tpc_per_gpc, num_sm_per_tpc, num_subcore_per_sm, warp_size)
- 类 `tlm::GpuClusterSharedInterface` 抽象接口：
  - `set_gpu_topology(GpuTopology)`
  - `get_gpu_topology() -> GpuTopology`
  - `tick()`
  - `get_module_type() -> std::string`

`include/tlm/cluster/gpu_cluster.hh` 必须改为同时继承 `SimModule` 和 `GpuClusterSharedInterface`。
`include/tlm/cluster/{gpc,tpc,compute}_cluster.hh` 必须 stub 完善并实现接口。

### REQ-GPU-8A-6: GpuSocTLM 顶层

`include/tlm/gpu/gpu_soc_tlm.hh` 必须提供：
- 类 `tlm::GpuSocTLM` 继承 `SimModule`
- 4 个 getter: `get_gpu_cluster()`, `get_noc()`, `get_memory_cluster()`, `get_kernel_launch()`
- `tick()` 方法推进仿真
- 模块类型字符串：`"GpuSocTLM"`

`include/modules_cluster.hh` 必须追加 `REGISTER_MODULE(GpuSocTLM)`。

### REQ-GPU-8A-7: 注册宏

`include/chstream_register.hh` 必须追加 5 行注册：
- `REGISTER_CHSTREAM(SharedMemoryTLM)`
- `REGISTER_CHSTREAM(MemoryClusterTLM)`
- `REGISTER_CHSTREAM(GpuNoC)`
- `REGISTER_CHSTREAM(KernelLaunchTLM)`
- `REGISTER_CHSTREAM(GpuSocTLM)`

### REQ-GPU-8A-8: 端到端集成测试

`test/test_gpu_soc_phase8a.cc` 必须验证：
- 加载 `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`
- 跑 1000 cycles
- `memory_cluster.requests_completed > 0`

### REQ-GPU-8A-9: 配置文件

`configs/templates/gpu_soc/gpu_soc_gb203_v1.json` 必须定义：
- 1 个 GpuSocTLM 顶层
- 1 个 KernelLaunchTLM
- 1 个 GpuComputeUnitTLM
- 1 个 SharedMemoryTLM (size_kb=64, banks=32)
- 1 个 GpuNoC (dim=2, hops_latency=2)
- 1 个 MemoryClusterTLM (channels=4, capacity_gb=8)

### REQ-GPU-8A-10: 微架构 doc

5 个新微架构 doc：
- `docs/soc_arch/modules/gpu-soc.md` (顶层)
- `docs/soc_arch/modules/gpu-shared-memory.md`
- `docs/soc_arch/modules/gpu-memory-cluster.md`
- `docs/soc_arch/modules/gpu-noc-mesh.md`
- `docs/soc_arch/modules/gpu-kernel-launch.md`

---

## MODIFIED Requirements

无（纯新增）

## REMOVED Requirements

无
