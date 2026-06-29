# Tasks: gpu_soc Phase 8.A — 基础设施

> **Status**: 🔄 Draft
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

- [x] **G1 单元测试 (Tasks 1-4)**: `[shared_memory][memory_cluster][noc][kernel_launch]` 全 pass (32 cases / 72 assertions)
- [ ] **G2 apu_soc 兼容**: `[gpu]` 14 cases + `[phase7]` 1 case 全 pass（不破坏）
- [ ] **G3 端到端**: `test_gpu_soc_phase8a.cc` pass
- [ ] **G4 性能 M1**: 1 SM × 1M cycles < 5s
- [ ] **G5 文档**: `docs_sync_check.sh --strict` 0 missing
- [ ] **G6 格式**: `format.sh --check` clean

## 3. 实施后节点

- [ ] **Oracle 审查**: 调用 oracle subagent 对所有 commit + 验收结果做审查
- [ ] **OpenSpec 归档**: Oracle 批准后，`openspec mv 2026-06-24-gpu-soc-phase8a-infra archive/`

## 4. 依赖关系

### 4.1 前置门 (Dependency Gates)

- **🔴 F12 Gate**: Task 5 (GpuClusterSharedInterface + 4 级 cluster) 和 Task 7 (端到端集成) **必须等待 F12 (Phase 7.B) 完成后启动**
  - F12 验证：`grep -rE 'class GpuComputeUnitTLM|class VectorRegFileTLM|class WavefrontTLM' include/tlm/gpu/` 应至少有 3 个匹配
  - F12 commit: roadmap §3 F12 已 merge + Oracle 审查 APPROVED
  - 若 F12 未完成，Task 5e 和 Task 7 测试用 `GpuComputeUnitTLM` 占位跳过，集成测试改为 stub-only

- **🟢 Task 1-4 独立**: SharedMemoryTLM / MemoryClusterTLM / GpuMeshNoC / KernelLaunchTLM **可与 F12 并行实施**,不依赖 F12

### 4.2 后续 change 依赖

- Phase 8.B (`2026-06-24-gpu-soc-phase8b-core`) 依赖本 change 的 GpuCluster 完善
- Phase 8.C (`2026-06-24-gpu-soc-phase8c-advanced`) 依赖 8.B + 8.A
