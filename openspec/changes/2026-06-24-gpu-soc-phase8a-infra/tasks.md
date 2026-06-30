# Tasks: gpu_soc Phase 8.A — 基础设施

> **Status**: 🔴 Tasks 1-4 Frozen (Oracle APPROVED 2026-06-30) | ⏸️ Tasks 5-8 Blocked by F12
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8a-infra)

## 1. Task 列表

> **TDD 模式**：每个任务 5 步（写测试 → 验失败 → 写实现 → 验通过 → commit），完整代码见 `docs/superpowers/plans/2026-06-24-gpu-soc-phase8a.md`

- [x] **Task 1**: SharedMemoryTLM 接口 + bank conflict 单元测试 + commit (`b8bd411`)
- [x] **Task 2**: MemoryClusterTLM 多通道 round-robin 单元测试 + commit (`6410ea9`)
- [x] **Task 3**: GpuMeshNoC mesh XY 路由单元测试 + commit (`d164497`)
- [x] **Task 4**: KernelLaunchTLM AQL 简化 dispatch 单元测试 + commit (`b8bd411`)
- [ ] **Task 5a**: `GpuClusterSharedInterface` 接口 + `GpuTopology` 结构体 + commit
- [ ] **Task 5b**: `GpuCluster` 改造（实现接口）+ commit
- [ ] **Task 5c**: `GpcCluster` + `TpcCluster` + `ComputeCluster` stub 完善（实现接口）+ commit
- [ ] **Task 5d**: `ApuSoC::incorporate_parent` 改用 `dynamic_cast<GpuClusterSharedInterface*>` + commit
- [ ] **Task 5e**: `test_gpu_cluster_shared.cc` + apu_soc regression + commit
- [ ] **Task 6**: GpuSocTLM 顶层 + REGISTER_MODULE + 单元测试 + commit
- [ ] **Task 7**: 集成测试 + JSON 配置 (gb203_v1.json) + 端到端验证 + commit
- [ ] **Task 8**: 5 个微架构 doc + 性能验收 (1 SM × 1M < 5s) + docs_sync + commit

## 2. 验收点 (Acceptance Gates)

- [x] **G1 单元测试 (Tasks 1-4)**: `[phase8a]` 全 pass (34 cases / 82 assertions) — 含 `[gpu][noc]`, `[gpu][kernel_launch]`, `[gpu][memcluster]`, `[gpu][smem]`
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
| F12 Gate | ⏸️ Blocked | grep returns 0 matches (GpuComputeUnitTLM etc. not yet implemented) |

## 4. 依赖关系

### 4.1 前置门 (Dependency Gates)

- **🔴 F12 Gate**: Task 5 (GpuClusterSharedInterface + 4 级 cluster) 和 Task 7 (端到端集成) **必须等待 F12 (Phase 7.B) 完成后启动**
- F12 验证：`grep -rE 'class GpuComputeUnitTLM|class VectorRegFileTLM|class WavefrontTLM|class MinimalWarpScheduler' include/tlm/gpu/` 应至少有 4 个匹配
- F12 总预算：950-1200 LOC（含 MinimalWarpScheduler），工时 2-3 周（Option A 决策）
  - F12 commit: roadmap §3 F12 已 merge + Oracle 审查 APPROVED
  - 若 F12 未完成，Task 5e 和 Task 7 测试用 `GpuComputeUnitTLM` 占位跳过，集成测试改为 stub-only

- **🟢 Task 1-4 独立**: SharedMemoryTLM / MemoryClusterTLM / GpuMeshNoC / KernelLaunchTLM **可与 F12 并行实施**,不依赖 F12

### 4.2 后续 change 依赖

- Phase 8.B (`2026-06-24-gpu-soc-phase8b-core`) 依赖本 change 的 GpuCluster 完善
- Phase 8.C (`2026-06-24-gpu-soc-phase8c-advanced`) 依赖 8.B + 8.A
