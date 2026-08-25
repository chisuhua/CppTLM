---
id: phase-7
kind: phase
status: active
phase_refs: []
主题: CPU+GPGPU Fused SoC (APU-first, 进行中) 🚀
---

## Phase 7: CPU+GPGPU Fused SoC (APU-first)

> **规划依据**: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](docs/research-cpptlm-gpu-fused-soc-survey.md)（2026-06-11 调研）
> **蓝图**: gem5 `configs/example/apu_se.py`（APU 形态，shared DDR + MOESI_AMD_Base + GPU_VIPER）
> **目标形态**: APU-first（单 RubySystem + 单 DDR 共享）；dGPU 留作 APU 完成后的 Phase 2 备选
> **总估算**: 16-22 周（Phase 7.A → 7.F）；MVP（Phase 7.A + 7.C）≈ 7-9 周
> **范围约束**: CPU 建模不在范围内（沿用 `TrafficGenTLM` / `CPUTLM`）；零新增外部依赖

### 已采纳的架构决策（D1-D5）

| 决策 | 采纳方案 | 适用阶段 | ADR |
|------|----------|----------|-----|
| **D1 协议策略** | 分步走：Phase 7.A 用简化 I/S/M 三态 + write-through bypass；Phase 7.C 升级为 MOESI + GPU_VIPER 简化版（C++ `switch` 表，**不**复制 gem5 slicc） | 7.A → 7.C | [ADR-SOC-01](docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md) |
| **D2 CU 粒度** | 黑盒优先（只发请求，不模拟 5-stage pipeline / ISA / SIMD） | 7.A → 7.F | [ADR-SOC-02](docs/soc_arch/adr/ADR-SOC-02-cu-granularity.md) |
| **D3 Wavefront/Coalescing** | 抽象（用 `coalescing_factor` 参数代替精确模拟） | 7.A → 7.F | [ADR-SOC-03](docs/soc_arch/adr/ADR-SOC-03-wavefront-coalescing-abstraction.md) |
| **D4 HSAPP/CP/Dispatcher** | 简化到极致（用 `KernelLaunchTLM` ~150 行代替 HSA 三件套 3000+ 行） | 7.A → 7.F | [ADR-SOC-04](docs/soc_arch/adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md) |
| **D5 目录结构** | 新建 `include/tlm/gpu/` 子目录（`compute_unit_tlm.hh` / `tcc_tlm.hh` / `kernel_launch_tlm.hh` / `pcie_bridge_tlm.hh`） | 7.A 起 | [ADR-SOC-05](docs/soc_arch/adr/ADR-SOC-05-gpu-directory-structure.md) |

### Phase 7 子任务（与调研报告 §4 对齐）

| 子任务 | 调研报告对应 | 状态 | 范围 | 风险 | 验收 |
|--------|:------------:|:----:|------|------|------|
| **7.A GPU 基础设施** | Phase 0（2-3 周） | ✅ Done (2026-06-11) | 新增 `include/bundles/compute_bundles_tlm.hh` + `include/tlm/gpu/gpu_tlm.hh` v0 + `REGISTER_CHSTREAM` 注册 + `configs/gpu_standalone.json` | 🟢 Low | `cpptlm_tests "[gpu]"` 通过；`gpu_standalone.json` 可执行 |
| **7.B ComputeUnit 黑盒** | Phase 1（3-4 周） | 🟡 Pending | `ComputeUnitTLM`（tick loop 发请求 + inflight_kernel_reqs_ map + workgroup_progress_）；CrossbarTLM 扩展 GPU 地址路由；CacheTLM 临时 bypass GPU 请求 | 🟡 Med-High | `configs/apu_demo_v1.json` 端到端运行；`cpptlm_tests "[gpu][phase6]"` |
| **7.C Coherence Protocol 集成** | Phase 2（4-5 周） | 🟡 Pending | CacheTLM protocol-aware 改造（`CacheLine = {data, state, sharers}` + 6×6 状态转换表）；CoherenceDomain 与 CacheTLM 集成（snoop callback + lookup_home_node） | 🔴 **High（最高风险）** | `configs/apu_demo_v2.json` 跨 cache 一致性；`cpptlm_tests "[coherence][gpu]"` |
| **7.D TCC Bridge + 内存层次** | Phase 3（3-4 周） | 🟡 Pending | `TCC_TLM`（DualPortStreamAdapter，write coalescing + snoop fan-in）；`MemoryTLM` 扩展 `hbm_mode` 参数 | 🟡 Med | `configs/apu_demo_v3.json` 写合并；`cpptlm_tests "[gpu][tcc]"` |
| **7.E Multi-CU + NoC** | Phase 4（2-3 周） | 🟡 Pending | `ComputeUnitTLM` 数组模式（`num_cus=4`）；`BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar；复用 `RouterTLM` / `LinkTLM` / `NoCFlitBundle` | 🟢 Low | `configs/apu_demo_v4.json` 4 CU 并发；`cpptlm_tests "[gpu][noc]"` |
| **7.F Full APU SoC Demo** | Phase 5（2-3 周） | 🟡 Pending | `configs/apu_full_soc.json`（2× TrafficGenTLM + 4× ComputeUnitTLM + TCC + Crossbar + Memory + 单 `CoherenceDomain("apu_domain")`）；`test/python/test_apu_soc.py` 端到端验证；统计 dashboard | 🟢 Low | JSON 配置可读、trace 可观察、测试可断言 |

### Phase 7 备选 — dGPU: Disjoint Coherence + PCIe Bridge

> **触发条件**: Phase 7.A-F 全部完成后；且评估 ROI 通过
> **范围**: 双独立 `CoherenceDomain`（`cpu_domain` + `gpu_domain`）+ `PCIBridge_TLM`（DualPortStreamAdapter，latency 100-500 cycles）+ `HBM_TLM`（独立 GPU memory）+ 两组独立 NoC 拓扑
> **蓝图**: gem5 `configs/example/gpufs/Disjoint_VIPER.py` + `src/dev/amdgpu/amdgpu_device.py`
> **估算**: 4-6 周（仅在 Phase 7 全部完成后启动）

### 最高风险与缓解

- **风险**: CacheTLM protocol-aware 改造（Phase 7.C）。当前 CacheTLM 只有 `std::map<addr, data>`，无 state machine、无 snoop。
- **缓解**:
  - 不复制 gem5 slicc（5000+ 行不可读）；用 C++ `switch` 表驱动（`uint8_t state_transition[6][6]`）
  - CacheTLM 改造分两小步：先加 `CoherenceState` 标签（不做 snoop），再加 snoop callback
  - `[coherence]` 标签下每个状态转换独立单元测试
- **触发升级条件**: Phase 7.C 测试发现 CacheTLM 改造导致 5+ 测试回归 → 暂停 coherence 方案，回到 Phase 7.B 的 write-through bypass 策略

### 可 defer 内容（不影响 MVP）

- 完整 wavefront 调度（Phase 7.E+）
- LDS 建模（Phase 7.F+）
- PCIe bridge / dGPU（Phase 7 备选）
- HSA runtime / ROCm KFD（Post MVP）
- 完整 GPU_VIPER slicc state machine（永远不做）
- DMA engine（Post MVP）