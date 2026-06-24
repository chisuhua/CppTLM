# Spec Delta: gpu_soc Phase 8.C — 高级特性

> **Change**: `2026-06-24-gpu-soc-phase8c-advanced`
> **Date**: 2026-06-24
> **Type**: ADDED

---

## ADDED Requirements

### REQ-GPU-8C-1: TccTLM (write coalescing)

`include/tlm/gpu/tcc_tlm.hh` 必须提供：
- 类 `tlm::TccTLM` 继承 `ChStreamModuleBase`
- 方法 `submit_write(addr, size, data)`, `flush()`, `transactions_issued()`
- 简化模型：4 个连续 16B 写 → 1 个 64B 事务（合并比 ≥ 4×）
- 模块类型字符串：`"TccTLM"`

### REQ-GPU-8C-2: TmaTLM (async copy)

`include/tlm/gpu/tma_tlm.hh` 必须提供：
- 类 `tlm::TmaTLM` 继承 `ChStreamModuleBase`
- 方法 `load_latency(size_b, swizzle) -> uint32_t`, `store_latency(size_b) -> uint32_t`
- 简化模型（按 SM_120 paper §9.1）：
  - Load 1D 1024B = 488 cyc
  - Load 2D 1024B = 620 cyc
  - Store 2D 1024B = 33 cyc

### REQ-GPU-8C-3: DsmTLM (inter-SM shmem)

`include/tlm/gpu/dsm_tlm.hh` 必须提供：
- 类 `tlm::DsmTLM` 继承 `ChStreamModuleBase`
- 方法 `remote_read_latency() = 230 cyc`, `remote_write_latency() = 36 cyc`

### REQ-GPU-8C-4: PowerModelTLM (经验模型)

`include/tlm/gpu/power_model_tlm.hh` 必须提供：
- 类 `tlm::PowerModelTLM` 继承 `ChStreamModuleBase`
- 构造参数：`num_sm`
- 方法 `compute_power(tc_saturated) -> double`
- 简化模型：`P = 80 + N × 1.0 (+ 30 if TC saturated)` (per SM_120 paper §11.3)

### REQ-GPU-8C-5: cpptlm.nvidia 拓扑生成

`cpptlm/cpptlm/nvidia/` 必须提供：
- `__init__.py` — 导出 `gpu_topology` / `gb203_consumer` / `gb200_datacenter` / `gh100_hopper` / `ga100_ampere`
- `topology.py` — `GpuTopology` dataclass + `gpu_topology(**kwargs)` factory
- `blueprint.py` — `NvidiaGPUBlueprint` + Variant 模式
- `export.py` — `to_json()` / `to_dot()` / `validate()`
- `sku_library.py` — 4 个 SKU 预置函数

### REQ-GPU-8C-6: cpptlm.gpu_workload 工作负载

`cpptlm/cpptlm/gpu_workload/` 必须提供：
- `generator.py` — `WorkloadGenerator` class
- `replay.py` — `TraceReplay` class (NCU + CuPTI 双格式)
- `patterns.py` — 5 个 pattern 构造器: `GEMM` / `FlashAttention` / `VectorAdd` / `Stencil3D` / `SparseSpMV`

### REQ-GPU-8C-7: cpptlm.gpu_soc 顶层

`cpptlm/cpptlm/gpu_soc/` 必须提供：
- `simulate.py` — `simulate(topo, workload, duration_cycles, metrics) -> SimulationResult`
- `report.py` — Markdown 报告生成 + gpgpu-sim 对照表
- `__init__.py` — 导出顶层 API

### REQ-GPU-8C-8: apu_soc 集成测试

`test/test_gpu_soc_phase8c.cc` 必须验证：
- 加载 apu_soc Phase 7.F 配置 + gpu_soc Phase 8.C 配置
- 验证 `factory->get<GpuCluster>("gpu_cluster")` 在两配置中返回同一实例
- 跑 100 cycles 后 `getCycleCount() == 100`

### REQ-GPU-8C-9: 完整验证报告

`docs/validation/phase8c_verification_report.md` 必须包含：
- 5 类场景带宽对照表（gpgpu-sim ±15%）
- 5 类场景延迟 p99 对照表（gpgpu-sim ±20%）
- Cache 命中率对照（±5pp）
- PowerModel 输出（per-SM 能耗）
- 与 apu_soc 集成验证结果

### REQ-GPU-8C-10: 微架构 doc

4 个新微架构 doc：`docs/soc_arch/modules/gpu-{tcc,tma,dsm,power-model}.md`

### REQ-GPU-8C-11: 文档同步

- `AGENTS.md` STRUCTURE 节加 `cpptlm/cpptlm/{nvidia,gpu_workload,gpu_soc}/` 3 个子包
- `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` 追加 Phase 8 完成状态

---

## MODIFIED Requirements

### MOD-AGENTS-1: 增 cpptlm.nvidia / gpu_workload / gpu_soc 3 子包

`AGENTS.md` STRUCTURE 节添加 3 个 Python 子包路径 + 描述。

### MOD-ROADMAP-1: Phase 8 完成状态

`docs/superpowers/plans/2026-06-20-future-work-roadmap.md` 追加：
- Phase 8 段落
- 3 个 milestone (M1/M2/M3) 状态: 全部 ✅ Done
- 4 个 commit hashes
- 测试基线: 703 → 730+/730+ pass

---

## REMOVED Requirements

无
