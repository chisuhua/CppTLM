# OpenSpec Change: gpu_soc Phase 8.A — 基础设施

> **Change ID**: `2026-06-24-gpu-soc-phase8a-infra`
> **Status**: 🔄 Draft（待用户审查）
> **Date**: 2026-06-24
> **Author**: Sisyphus（基于 6 轮 brainstorming + spec 801f8ea + plan 3993129 + ADR-NV-01 3aa810b）
> **Target Release**: Phase 8.A end (~4 weeks)
> **Depends on**: 无（首个 Phase 8 change）

---

## 1. Why（动机）

**当前状态**：
- apu_soc Phase 7.A 已落地 GPU 基础设施（`GPUTLM v0` + `ComputeReqBundle`）
- gpu_soc 4 级 cluster 容器（GpuCluster/GpcCluster/TpcCluster/ComputeCluster）仅有 P5 stub
- 缺 4 个核心模块：`MemoryClusterTLM` / `SharedMemoryTLM` / `GpuNoC` / `KernelLaunchTLM`
- 缺顶层 GpuSocTLM + 共享抽象层 `GpuClusterSharedInterface`
- **结果**：gpu_soc 当前**不能跑任何端到端 GPU 仿真**——没有内存、NoC、kernel launch

**目标（Phase 8.A 完成后）**：
- ✅ 4 级 cluster 容器从 stub 升级为功能完整（与 apu_soc 共享）
- ✅ 4 个核心模块落地（MemoryCluster / SharedMemory / GpuNoC / KernelLaunch）
- ✅ GpuSocTLM 顶层可独立运行
- ✅ apu_soc 兼容性测试全绿（不破坏现有 `[gpu]` 14 cases + `[phase7]` 1 case）
- ✅ M1 验收：1 SM × 1M cycles < 5s（单核）

---

## 2. What Changes（变更内容）

### 新增文件（4 个 .hh + 4 个 .cc + 1 个 bundle + 1 个 shared interface + 5 个微架构 doc）

| 类型 | 路径 | 阶段 |
|------|------|:---:|
| Bundle | `include/bundles/shared_memory_bundle.hh` | 8.A |
| Header | `include/tlm/gpu/shared_memory_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/memory_cluster_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/gpu_noc_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/kernel_launch_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/gpu_soc_tlm.hh` | 8.A 顶层 |
| Header | `include/tlm/gpu/gpu_cluster_shared_interface.hh` | 8.A 共享层 |
| Impl | `src/tlm/gpu/shared_memory_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/memory_cluster_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/gpu_noc_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/kernel_launch_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/gpu_soc_tlm.cc` | 8.A |
| Test | `test/test_shared_memory_tlm.cc` | 8.A |
| Test | `test/test_memory_cluster_tlm.cc` | 8.A |
| Test | `test/test_gpu_noc_tlm.cc` | 8.A |
| Test | `test/test_kernel_launch_tlm.cc` | 8.A |
| Test | `test/test_gpu_cluster_shared.cc` | 8.A |
| Test | `test/test_gpu_soc_phase8a.cc` | 8.A 集成 |
| Config | `configs/templates/gpu_soc/gpu_soc_gb203_v1.json` | 8.A |
| Doc | `docs/soc_arch/modules/gpu-soc.md` | 8.A |
| Doc | `docs/soc_arch/modules/gpu-shared-memory.md` | 8.A |
| Doc | `docs/soc_arch/modules/gpu-memory-cluster.md` | 8.A |
| Doc | `docs/soc_arch/modules/gpu-noc-mesh.md` | 8.A |
| Doc | `docs/soc_arch/modules/gpu-kernel-launch.md` | 8.A |

### 修改文件

| 文件 | 修改内容 |
|------|---------|
| `include/chstream_register.hh` | +5 行注册（4 个新 ChStream + GpuSocTLM） |
| `include/modules_cluster.hh` | +1 行 `REGISTER_MODULE(GpuSocTLM)` |
| `include/tlm/cluster/gpu_cluster.hh` | 引入 `GpuClusterSharedInterface` 实现 |
| `include/tlm/cluster/{gpc,tpc,compute}_cluster.hh` | stub 完善（实现接口） |

### 删除

无（纯新增 + 现有 stub 完善）

---

## 3. Goals（成功标准）

| 编号 | 标准 | 验证方式 |
|------|------|---------|
| **G1** | 4 个核心模块通过单元测试 | `cpptlm_tests "[gpu][smem][memcluster][noc][kernel_launch]"` 全 pass |
| **G2** | GpuClusterSharedInterface 兼容 apu_soc | `cpptlm_tests "[gpu][phase7]"` 仍 14+1 cases pass |
| **G3** | 端到端 GPU 仿真跑通 | `test/test_gpu_soc_phase8a.cc` pass（CU→SMEM→L1→NoC→Mem 闭环） |
| **G4** | 性能 M1 达标 | 1 SM × 1M cycles < 5s（单核） |
| **G5** | 文档同步 | `docs_sync_check.sh --strict` 0 missing |
| **G6** | 代码风格 | `format.sh --check` clean |

---

## 4. Non-Goals（明确不做）

- ❌ SubCore / WarpScheduler / Scoreboard / TensorCore / Pipeline（→ 8.B）
- ❌ L2Partition（→ 8.B）
- ❌ Tcc / Tma / Dsm / PowerModel（→ 8.C）
- ❌ Cycle-accurate 5+V 管线（按 D2 决策）
- ❌ 真实 kernel 编译/执行（按 ADR-NV-01 §10）
- ❌ dGPU + PCIe（Phase 9 备选）
- ❌ gpgpu-sim 代码集成（按 D5 决策）

---

## 5. Risks（风险）

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| **R1** | 4 级 cluster stub 改造破坏 apu_soc | 中 | 高 | GpuClusterSharedInterface 抽象层（Task 5） |
| **R2** | SharedMemory bank conflict 模型不准确 | 中 | 中 | 简化模型：1 + (num_threads-1) cyc，仅覆盖 4-way conflict |
| **R3** | MemoryCluster 多通道分配性能瓶颈 | 低 | 中 | 简化 round-robin，不模拟真实 DRAM 调度 |

---

## 6. 关联文档

- **Spec**: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../../docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md) §5.1
- **Plan**: [`docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md`](../../docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md) Phase 8.A
- **ADR**: [`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`](../../docs/adr/ADR-NV-01-gpu-soc-architecture-target.md)
- **Sibling spec (apu_soc)**: [`docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md`](../../docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md)

---

## 7. 验收节点

| 节点 | 时机 | 标准 |
|------|------|------|
| **M1 单元测试** | Task 1-6 完成 | G1 通过 |
| **M1 集成测试** | Task 7 完成 | G3 通过 |
| **M1 性能** | Task 8 完成 | G4 通过 |
| **M1 文档** | Task 8 完成 | G5+G6 通过 |
| **Oracle 审查** | 所有 Task 完成 | 详见 plan |
| **OpenSpec 归档** | Oracle 批准 | 此 change → `openspec/changes/archive/` |
