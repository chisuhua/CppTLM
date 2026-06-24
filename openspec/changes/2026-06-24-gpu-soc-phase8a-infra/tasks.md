# Tasks: gpu_soc Phase 8.A — 基础设施

> **Status**: 🔄 Draft
> **Parent**: `proposal.md` + `design.md` (2026-06-24-gpu-soc-phase8a-infra)

## 1. Task 列表

> **TDD 模式**：每个任务 5 步（写测试 → 验失败 → 写实现 → 验通过 → commit），完整代码见 `docs/superpowers/plans/2026-06-24-gpu-soc-phase8a.md`

- [ ] **Task 1**: SharedMemoryTLM 接口 + bank conflict 单元测试 + commit
- [ ] **Task 2**: MemoryClusterTLM 多通道 round-robin 单元测试 + commit
- [ ] **Task 3**: GpuNoC mesh XY 路由单元测试 + commit
- [ ] **Task 4**: KernelLaunchTLM AQL 简化 dispatch 单元测试 + commit
- [ ] **Task 5**: GpuClusterSharedInterface + 4 级 cluster 改造 + apu_soc 兼容测试 + commit
- [ ] **Task 6**: GpuSocTLM 顶层 + REGISTER_MODULE + 单元测试 + commit
- [ ] **Task 7**: 集成测试 + JSON 配置 (gb203_v1.json) + 端到端验证 + commit
- [ ] **Task 8**: 5 个微架构 doc + 性能验收 (1 SM × 1M < 5s) + docs_sync + commit

## 2. 验收点 (Acceptance Gates)

- [ ] **G1 单元测试**: `[gpu][smem][memcluster][noc][kernel_launch]` 全 pass
- [ ] **G2 apu_soc 兼容**: `[gpu]` 14 cases + `[phase7]` 1 case 全 pass（不破坏）
- [ ] **G3 端到端**: `test_gpu_soc_phase8a.cc` pass
- [ ] **G4 性能 M1**: 1 SM × 1M cycles < 5s
- [ ] **G5 文档**: `docs_sync_check.sh --strict` 0 missing
- [ ] **G6 格式**: `format.sh --check` clean

## 3. 实施后节点

- [ ] **Oracle 审查**: 调用 oracle subagent 对所有 commit + 验收结果做审查
- [ ] **OpenSpec 归档**: Oracle 批准后，`openspec mv 2026-06-24-gpu-soc-phase8a-infra archive/`

## 4. 依赖关系

- **无外部依赖**（首个 Phase 8 change）
- **后续 change 依赖**：Phase 8.B (`2026-06-24-gpu-soc-phase8b-core`) 依赖本 change 的 GpuCluster 完善
