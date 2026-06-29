# OpenSpec Change: gpu_soc Phase 8.A — 基础设施

> **Change ID**: `2026-06-24-gpu-soc-phase8a-infra`
> **Status**: 🔄 Draft（待用户审查 + F12 依赖确认）
> **Date**: 2026-06-24
> **Author**: Sisyphus（基于 6 轮 brainstorming + spec 801f8ea + plan 3993129 + ADR-NV-01 3aa810b）
> **Target Release**: Phase 8.A end (~4 weeks)
> **Depends on**: **F12 (Phase 7.B: GpuComputeUnitTLM + VectorRegFileTLM + WavefrontTLM, ~10d, 🔴 待启动, 见 roadmap §3)** — F12 必须先 merge + Oracle 审查 APPROVED,否则 Task 5-8 不能开始

---

## 1. Why（动机）

**当前状态**：
- apu_soc Phase 7.A 已落地 GPU 基础设施（`GPUTLM v0` + `ComputeReqBundle`）
- gpu_soc 4 级 cluster 容器（GpuCluster/GpcCluster/TpcCluster/ComputeCluster）仅有 P5 stub
- 缺 4 个核心模块：`MemoryClusterTLM` / `SharedMemoryTLM` / `GpuMeshNoC` / `KernelLaunchTLM`
- 缺顶层 GpuSocTLM + 共享抽象层 `GpuClusterSharedInterface`
- **结果**：gpu_soc 当前**不能跑任何端到端 GPU 仿真**——没有内存、NoC、kernel launch

**目标（Phase 8.A 完成后）**：
- ✅ 4 级 cluster 容器从 stub 升级为功能完整（与 apu_soc 共享）
- ✅ 4 个核心模块落地（MemoryCluster / SharedMemory / GpuMeshNoC / KernelLaunch）
- ✅ GpuSocTLM 顶层可独立运行
- ✅ apu_soc 兼容性测试全绿（不破坏现有 `[gpu]` 14 cases + `[phase7]` 1 case）
- ✅ M1 验收：1 SM × 1M cycles < 5s（单核）

---

## 2. What Changes（变更内容）

### 新增文件（4 个 .hh + 4 个 .cc + 1 个 bundle + 1 个 shared interface + 5 个微架构 doc）

| 类型 | 路径 | 阶段 |
|------|------|:---:|
| Header | `include/tlm/gpu/shared_memory_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/memory_cluster_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/gpu_mesh_noc_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/kernel_launch_tlm.hh` | 8.A |
| Header | `include/tlm/gpu/gpu_soc_tlm.hh` | 8.A 顶层 |
| Header | `include/tlm/gpu/gpu_cluster_shared_interface.hh` | 8.A 共享层 |
| Impl | `src/tlm/gpu/shared_memory_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/memory_cluster_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/gpu_mesh_noc_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/kernel_launch_tlm.cc` | 8.A |
| Impl | `src/tlm/gpu/gpu_soc_tlm.cc` | 8.A |
| Test | `test/test_shared_memory_tlm.cc` | 8.A |
| Test | `test/test_memory_cluster_tlm.cc` | 8.A |
| Test | `test/test_gpu_mesh_noc_tlm.cc` | 8.A |
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

### 延后到 8.B 的文件

以下文件原列于 8.A "新增文件"，但实际作用域属于 8.B（SubCore/WarpScheduler/Scoreboard/TensorCore/Pipeline 之间的类型安全通信），不在 8.A 基础设施范围：

- `include/bundles/shared_memory_bundle.hh` → **8.B**（与 SubCore/WarpScheduler 的 register bundle 同步落地）

Tasks 1-4 实现使用直接字段（`size_kb_` / `banks_` / `channels_` / `kernel_id_` 等）而非 bundle，单元测试覆盖已验证 737/737 全绿。Bundle 在 8.B 多模块协同时再补充。

---

## 3. Goals（成功标准）

| 编号 | 标准 | 验证方式 |
|------|------|---------|
| **G1** | 4 个核心模块通过单元测试 | `cpptlm_tests "[shared_memory][memory_cluster][noc][kernel_launch]"` 全 pass |
| **G2** | GpuClusterSharedInterface 兼容 apu_soc | `cpptlm_tests "[gpu][phase7]"` 仍 14+1 cases pass |
| **G3** | 端到端 GPU 仿真跑通 | `test/test_gpu_soc_phase8a.cc` pass（CU→SMEM→L1→NoC→Mem 闭环） |
| **G4** | 性能 M1 达标 | 1 SM × 1M cycles < 5s（单核） |
| **G4+** | 性能 M1+ multi-SM 达标 | 4 SM × 100K cycles < 5s（验证 multi-SM contention, O(N²) 检查） |
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
| **R1** | 4 级 cluster stub 改造破坏 apu_soc | 中 | 高 | GpuClusterSharedInterface 抽象层（Task 5a-5e）+ ApuSoC::incorporate_parent 保留旧 `GpuCluster*` 路径作为 fallback + 纯虚函数 fail-fast |
| **R2** | SharedMemory bank conflict 模型不准确（thread-count heuristic 而非地址模式） | 中 | 中 | 简化模型：1 + (num_threads-1) cyc；仅覆盖 4-way conflict。**已知简化**: 升级路径 design.md §10 (8.B) |
| **R3** | F12 未完成导致 8.A Task 5-8 集成测试 `GpuComputeUnitTLM` 引用解析失败 | 高 | 高 | **前置门**: 8.A Task 1-4 独立模块（不依赖 F12）可单独完成；Task 5-8 在 F12 落地后才启动 |
| **R4** | `GpuMeshNoC` 类名冲突（已与 `cluster/gpu_noc_cluster.hh` 旧 `GpuNoC` 区分） | 低 | 中 | 新类名 `GpuMeshNoC` + 同步更新所有引用 |
| **R5** | namespace 不匹配（`tlm::` vs `cpptlm::tlm::`） | 低 | 高 | ChStreamModuleBase 派生 → `namespace tlm`（与 `gpu_tlm.hh` 一致）；SimModule 派生（GpuSocTLM / GpuClusterSharedInterface / GpuCluster）→ `namespace cpptlm::tlm`（与 `apu_soc.hh` 一致）。B2 决策: 与现有 cluster 模块对齐 |
| **R6** | F12 的 CU 无 scheduler,8.A Task 5/7 端到端测试无法触发 `requests_completed > 0`（循环依赖） | 高 | 高 | **Option A 决策**: F12 范围扩展含 MinimalWarpScheduler (~300-400 LOC),F12 总预算 650-800→950-1200 LOC,工期 1-2→2-3 周 |
| **R7** | GB202 192 SM × 4 sub-core 单 issue 模型导致 IPC 低估 4× | 中 | 高 | F12 GpuComputeUnitTLM 内部用 `SubCoreSlot` struct × 4 + round-robin 派发,保留 4-way parallelism |
| **R8** | MemoryClusterTLM `channels:4` 与 GB203 实际 8 channels (256-bit GDDR7) 不符 | 中 | 中 | GpuTopology 扩展 `mem_bus_bits` + `mem_channels` 字段,JSON 示例改为 `channels:8` |
| **R9** | Wavefront coalescing_factor 常数模型与真实地址模式 coalescing 失真 | 中 | 中 | 文档化于 design.md §10 "已知简化",8.B 升级为地址模式函数 |
| **R10** | TensorCore/SFU/LSU 完全缺失 (8.A 仅通用 load/store) | 中 | 高 | 文档化于 design.md §10 升级路径 8.B/8.C,8.A 不支持 TC/SFU/LSU 操作 |

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
