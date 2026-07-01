# Tasks: gpu_soc Phase 8.A — 基础设施

> **Status**: 🧊 Tasks 1-4 Frozen (Oracle APPROVED 2026-06-30) | ✅ F12a Completed 2026-06-30 | 🚀 Tasks 5-8 Unblocked
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8a-infra)

## 1. Task 列表

> **TDD 模式**：每个任务 5 步（写测试 → 验失败 → 写实现 → 验通过 → commit），完整代码见 `docs/superpowers/plans/2026-06-24-gpu-soc-phase8a.md`

- [x] **Task 1**: SharedMemoryTLM 接口 + bank conflict 单元测试 + commit (`b8bd411`)
- [x] **Task 2**: MemoryClusterTLM 多通道 round-robin 单元测试 + commit (`6410ea9`)
- [x] **Task 3**: GpuMeshNoC mesh XY 路由单元测试 + commit (`d164497`)
- [x] **Task 4**: KernelLaunchTLM AQL 简化 dispatch 单元测试 + commit (`b8bd411`)
- [x] **Task 5a**: `GpuClusterSharedInterface` 接口 + `GpuTopology` 结构体 + commit  — **F12a unblocked**
- [x] **Task 5b**: `GpuCluster` 改造（实现接口）+ commit  — **F12a unblocked**
- [x] **Task 5c**: `GpcCluster` + `TpcCluster` + `ComputeCluster` stub 完善（实现接口）+ commit  — **F12a unblocked**
- [x] **Task 5d**: `ApuSoC::incorporate_parent` 改用 `dynamic_cast<GpuClusterSharedInterface*>` + commit  — **F12a unblocked**
- [x] **Task 5e**: `test_gpu_cluster_shared.cc` + apu_soc regression + commit  — **F12a unblocked**
- [x] **Task 6**: GpuSocTLM 顶层 + REGISTER_MODULE + 单元测试 + commit  — **F12a unblocked**
- [x] **Task 7**: 集成测试 + JSON 配置 (gb203_v1.json) + 端到端验证 + commit  — **F12a unblocked**
- [ ] **Task 8**: 5 个微架构 doc + 性能验收 (1 SM × 1M < 5s) + docs_sync + commit

## 2. 验收点 (Acceptance Gates)

- [x] **G1 单元测试 (Tasks 1-4)**: `[phase8a]` 全 pass (34 cases / 82 assertions) — 含 `[gpu][noc]`, `[gpu][kernel_launch]`, `[gpu][memcluster]`, `[gpu][smem]`
- [x] **F12 Gate**: `grep -rE 'class GpuComputeUnitTLM|class VectorRegFileTLM|class WavefrontTLM|class MinimalWarpScheduler' include/tlm/gpu/` 返回 4 个匹配 (2026-06-30)
- [ ] **G2 apu_soc 兼容**: `[gpu]` 14 cases + `[phase7]` 1 case 全 pass（不破坏）
- [ ] **G3 端到端**: `test_gpu_soc_phase8a.cc` pass
- [ ] **G4 性能 M1**: 1 SM × 1M cycles < 5s
- [ ] **G4+ 性能 M1+**: 4 SM × 100K cycles < 5s (multi-SM contention check)
- [ ] **G5 文档**: `docs_sync_check.sh --strict` 0 missing
- [ ] **G6 格式**: `format.sh --check` clean

## 3. 实施后节点

- [x] **Oracle 审查 (Tasks 1-4)**: 2026-06-30 APPROVED for implementation freeze
  - 737/737 tests pass (15471 assertions)
  - 4 modules: `namespace tlm` consistent, `ModuleFactory::registerObject` complete
  - apu_soc compatibility: `[gpu]` 48 cases pass, no regression
  - Hidden issues identified (non-blocking, deferred to 8.B):
    1. StreamAdapter registration missing for all 4 modules
    2. KernelLaunchTLM tick() pure counter (no dispatch)
    3. MemoryClusterTLM tick() unconditionally increments
    4. SharedMemoryTLM stride_bytes ignored in formula
- [ ] **Oracle 审查 (Full)**: Tasks 5-8 完成后复查 + F12 验证
- [ ] **OpenSpec 归档**: Full Oracle 批准后，`openspec mv 2026-06-24-gpu-soc-phase8a-infra archive/`

### 3.1 Tasks 1-4 实施冻结记录

| Item | Status | Evidence |
|------|--------|----------|
| SharedMemoryTLM | ✅ Frozen | `include/tlm/gpu/shared_memory_tlm.hh` + test `[gpu][smem][phase8a]` 9 cases |
| MemoryClusterTLM | ✅ Frozen | `include/tlm/gpu/memory_cluster_tlm.hh` + test `[gpu][memcluster][phase8a]` 8 cases |
| GpuMeshNoC | ✅ Frozen | `include/tlm/gpu/gpu_mesh_noc.hh` + test `[gpu][noc][phase8a]` 10 cases |
| KernelLaunchTLM | ✅ Frozen | `include/tlm/gpu/kernel_launch_tlm.hh` + test `[gpu][kernel_launch][phase8a]` 7 cases |
| G1 Gate | ✅ Verified | `[phase8a]` 34 cases / 82 assertions, all pass |
| F12 Gate | ✅ Unblocked | `grep -rE 'class GpuComputeUnitTLM|class VectorRegFileTLM|class WavefrontTLM|class MinimalWarpScheduler' include/tlm/gpu/` returns 4 matches; F12a plan: `docs/superpowers/plans/2026-06-30-f12a-gpu-core-modules.md`; F12b-LD design: `docs/superpowers/specs/2026-06-30-f12-ptxemu-ldpreload-design.md` |

## 4. 依赖关系

### 4.1 前置门 (Dependency Gates)

- **🟢 F12 Gate CLEARED 2026-06-30**: `grep -rE 'class GpuComputeUnitTLM|class VectorRegFileTLM|class WavefrontTLM|class MinimalWarpScheduler' include/tlm/gpu/` 返回 4 个匹配。Task 5 (GpuClusterSharedInterface + 4 级 cluster) 和 Task 7 (端到端集成) **已解除阻塞**, 可启动。
- F12 验证通过证据:
  - `SubCoreSlot`: `include/tlm/gpu/sub_core_slot.hh`
  - `WavefrontTLM`: `include/tlm/gpu/wavefront_tlm.hh` + `src/tlm/gpu/wavefront_tlm.cc` + `test/test_wavefront_tlm.cc`
  - `VectorRegFileTLM`: `include/tlm/gpu/vector_regfile_tlm.hh` + `src/tlm/gpu/vector_regfile_tlm.cc` + `test/test_vector_regfile_tlm.cc`
  - `MinimalWarpSchedulerTLM`: `include/tlm/gpu/minimal_warp_scheduler_tlm.hh` + `src/tlm/gpu/minimal_warp_scheduler_tlm.cc` + `test/test_minimal_warp_scheduler_tlm.cc`
  - `GpuComputeUnitTLM`: `include/tlm/gpu/gpu_compute_unit_tlm.hh` + `src/tlm/gpu/gpu_compute_unit_tlm.cc` + `test/test_gpu_compute_unit_tlm.cc` + `test/test_gpu_compute_unit_integration.cc`
  - 注册: `include/chstream_register.hh` 含全部 4 个 `ModuleFactory::registerObject`
  - CMake: `src/CMakeLists.txt` `CORE_SOURCES` 含全部 4 个 `.cc`
  - 测试: 755/755 pass (15517 assertions)
- F12b-LD 集成设计: `docs/superpowers/specs/2026-06-30-f12-ptxemu-ldpreload-design.md` (PTX-EMU `LD_PRELOAD` 路径)

- **🟢 Task 1-4 独立**: SharedMemoryTLM / MemoryClusterTLM / GpuMeshNoC / KernelLaunchTLM **可与 F12 并行实施**,不依赖 F12

### 4.2 后续 change 依赖

- Phase 8.B (`2026-06-24-gpu-soc-phase8b-core`) 依赖本 change 的 GpuCluster 完善
- Phase 8.C (`2026-06-24-gpu-soc-phase8c-advanced`) 依赖 8.B + 8.A
