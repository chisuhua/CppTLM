# gpu_soc Phase 8.A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `openspec/changes/2026-06-24-gpu-soc-phase8a-infra` change —— 4 个核心模块（MemoryCluster / SharedMemory / GpuNoC / KernelLaunch）+ GpuClusterSharedInterface + GpuSocTLM 顶层，让 gpu_soc 端到端跑通；M1 验收（1 SM × 1M cycles < 5s, 5 类 microbenchmark 闭环, apu_soc 兼容）。

**Architecture:** Phase-accurate 工业性能建模（按 ADR-NV-01）。4 级层次（GpuSocTLM → GpuCluster → GpcCluster × N → TpcCluster × M → ComputeCluster × K）+ 3 个核心模块（GpuNoC / MemoryClusterTLM / KernelLaunchTLM）+ SharedMemoryTLM（per-SM）。通过 `GpuClusterSharedInterface` 抽象层与 apu_soc 共享 GpuCluster 容器。

**Tech Stack:**
- C++17（CppTLM 核心）
- Catch2 v3.7.0（测试）
- pre-commit + clang-format（强制 4 空格缩进）
- OpenSpec workflow（`openspec/changes/2026-06-24-gpu-soc-phase8a-infra/`）

---

## File Structure (新增/修改)

### 新增头文件（5 个 + 1 个顶层 + 1 个 shared interface = 7 个）
```
include/tlm/gpu/
├── shared_memory_tlm.hh        (Task 1)
├── memory_cluster_tlm.hh       (Task 2)
├── gpu_noc_tlm.hh              (Task 3)
├── kernel_launch_tlm.hh        (Task 4)
├── gpu_soc_tlm.hh              (Task 6 顶层)
└── gpu_cluster_shared_interface.hh  (Task 5 共享层)
```

### 新增 C++ 实现（与 .hh 一一对应）
```
src/tlm/gpu/*.cc                (Task 1-6)
```

### 新增 Bundle
```
include/bundles/shared_memory_bundle.hh   (Task 1)
```

### 新增测试（6 个）
```
test/test_shared_memory_tlm.cc    (Task 1)
test/test_memory_cluster_tlm.cc   (Task 2)
test/test_gpu_noc_tlm.cc          (Task 3)
test/test_kernel_launch_tlm.cc    (Task 4)
test/test_gpu_cluster_shared.cc   (Task 5)
test/test_gpu_soc_phase8a.cc      (Task 7 集成)
```

### 新增配置
```
configs/templates/gpu_soc/gpu_soc_gb203_v1.json  (Task 7)
```

### 新增微架构 doc（5 个）
```
docs/soc_arch/modules/
├── gpu-soc.md              (Task 8)
├── gpu-shared-memory.md    (Task 8)
├── gpu-memory-cluster.md   (Task 8)
├── gpu-noc-mesh.md         (Task 8)
└── gpu-kernel-launch.md    (Task 8)
```

### 修改文件
```
include/chstream_register.hh                 (+5 行注册 Task 1-4+6)
include/modules_cluster.hh                   (+1 行 GpuSocTLM Task 6)
include/tlm/cluster/gpu_cluster.hh           (实现 GpuClusterSharedInterface Task 5)
include/tlm/cluster/{gpc,tpc,compute}_cluster.hh   (stub 完善 Task 5)
```

---

## 实施任务（8 个 + 2 个验收节点 = 10 个任务）

> **TDD 模式说明**：每个实施任务遵循 5 步 TDD（写测试 → 验失败 → 写实现 → 验通过 → commit）。完整代码见主 plan `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.A 节。本 plan 列出**文件路径、关键 API、commit 消息**——工程实施时按主 plan 的 5 步结构执行。

### Task 1: SharedMemoryTLM 接口 + bank conflict 单元测试

**Files:** 见上 file structure

**关键 API**：
```cpp
class SharedMemoryTLM : public ChStreamModuleBase {
    SharedMemoryTLM(const std::string& name, EventQueue* eq, uint32_t size_kb, uint32_t banks);
    uint32_t bank_conflict_cycles(uint32_t num_threads, uint32_t stride_bytes) const;
};
// 简化：base 1 cyc + 每个 conflict way +1 cyc
```

**验收**：`./build/bin/cpptlm_tests "[gpu][smem]"` PASS

**Commit**：`feat(tlm/gpu): SharedMemoryTLM skeleton + bank conflict model (Phase 8.A Task 1)`

---

### Task 2: MemoryClusterTLM 多通道 round-robin

**关键 API**：
```cpp
class MemoryClusterTLM : public ChStreamModuleBase {
    MemoryClusterTLM(const std::string& name, EventQueue* eq, uint32_t channels, uint32_t capacity_gb);
    uint32_t allocate_channel(uint64_t request_id);
};
// 4-channel round-robin, 第 5 次回到 channel 0
```

**验收**：`./build/bin/cpptlm_tests "[gpu][memcluster]"` PASS

**Commit**：`feat(tlm/gpu): MemoryClusterTLM multi-channel round-robin (Phase 8.A Task 2)`

---

### Task 3: GpuNoC mesh XY 路由

**关键 API**：
```cpp
class GpuNoC : public ChStreamModuleBase {
    GpuNoC(const std::string& name, EventQueue* eq, uint32_t dim, uint32_t hops_latency);
    uint32_t route_latency(std::pair<uint32_t,uint32_t> src, std::pair<uint32_t,uint32_t> dst) const;
};
// 2x2 mesh: (0,0)→(1,1) = (1+1) × hops_latency
```

**验收**：`./build/bin/cpptlm_tests "[gpu][noc]"` PASS

**Commit**：`feat(tlm/gpu): GpuNoC mesh XY routing (Phase 8.A Task 3)`

---

### Task 4: KernelLaunchTLM AQL 简化

**关键 API**：
```cpp
class KernelLaunchTLM : public ChStreamModuleBase {
    KernelLaunchTLM(const std::string& name, EventQueue* eq);
    void set_kernel_id(uint32_t);
    void set_workgroup_size(uint32_t);
    void set_grid_size(uint32_t);
    void set_kernel_launch_interval(uint32_t cyc);
    void tick() override;  // 按 interval 周期发 KernelDesc
};
```

**验收**：`./build/bin/cpptlm_tests "[gpu][kernel_launch]"` PASS

**Commit**：`feat(tlm/gpu): KernelLaunchTLM AQL simplified (Phase 8.A Task 4)`

---

### Task 5: GpuClusterSharedInterface + 4 级 cluster 改造（**风险最高**）

**前置门 (F12 Gate, Option A 决策)**: 本 Task **必须等待 F12 含 MinimalWarpScheduler 完成后启动**。F12 验证：`grep -rE 'class GpuComputeUnitTLM|class VectorRegFileTLM|class WavefrontTLM|class MinimalWarpScheduler' include/tlm/gpu/` ≥4 匹配。F12 总预算 950-1200 LOC,工期 2-3 周。

**关键 API**：
```cpp
// 新 include/tlm/gpu/gpu_cluster_shared_interface.hh
struct GpuTopology {
    uint32_t num_gpc = 1;
    uint32_t num_tpc_per_gpc = 1;
    uint32_t num_sm_per_tpc = 1;      // 1 for GB202, 2 for H100/B200/GB203
    uint32_t num_subcore_per_sm = 4;  // configurable
    uint32_t warp_size = 32;
    // NEW (FP#4, #6, #10): tensor_core_count, smem/l1/regfile KB, mem_bus_bits, mem_channels, has_nv_hub
    // 详见 design.md §2 + specs REQ-GPU-8A-5
};
class GpuClusterSharedInterface {
    virtual void set_gpu_topology(const GpuTopology&) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
    virtual void tick() = 0;
};

// GpuCluster 改造
class GpuCluster : public SimModule, public GpuClusterSharedInterface {
    // 现有 GpuCluster 代码 + 实现上述虚函数
};
// GpcCluster / TpcCluster / ComputeCluster stub 完善（实现 GpuClusterSharedInterface）
```

**apu_soc 兼容性测试**：
```bash
./build/bin/cpptlm_tests "[gpu][phase7]" --reporter compact
# 必须仍 14+1 cases pass（不破坏 apu_soc Phase 7 已有功能）
```

**验收**：
- `test_gpu_cluster_shared.cc` PASS
- `[gpu][phase7]` 14+1 cases 仍全绿

**Commit**：`feat(cluster): GpuClusterSharedInterface for apu_soc/gpu_soc sharing (Phase 8.A Task 5)`

---

### Task 6: GpuSocTLM 顶层 + REGISTER_MODULE

**关键 API**：
```cpp
class GpuSocTLM : public SimModule {
    GpuSocTLM(const std::string& name, EventQueue* eq);
    GpuCluster* get_gpu_cluster();
    GpuNoC* get_noc();
    MemoryClusterTLM* get_memory_cluster();
    KernelLaunchTLM* get_kernel_launch();
    void tick() override;
};
```

**注册**：`include/modules_cluster.hh` 末尾追加 `REGISTER_MODULE(GpuSocTLM)`

**验收**：`./build/bin/cpptlm_tests "[gpu][soc]"` PASS

**Commit**：`feat(tlm/gpu): GpuSocTLM top-level (Phase 8.A Task 6)`

---

### Task 7: 集成测试 + JSON 配置 (gb203_v1.json)

**前置门 (F12 Gate, Option A 决策)**: 集成测试要求 GpuComputeUnitTLM 已能 dispatch (`requests_completed > 0`)。F12 必须含 MinimalWarpScheduler (Option A 决策),否则循环依赖 (CU 不 dispatch → 无请求 → 测试不通过)。

**Files**:
- Create: `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`
- Create: `test/test_gpu_soc_phase8a.cc`

**关键测试**：
```cpp
TEST_CASE("gpu_soc_phase8a: end-to-end kernel → mem", "[gpu][soc][phase8a]") {
    auto* factory = ModuleFactory::instance();
    factory->loadConfig("configs/templates/gpu_soc/gpu_soc_gb203_v1.json");
    factory->instantiateAll();
    for (int i = 0; i < 1000; ++i) factory->tick();
    REQUIRE(factory->getStats("memory_cluster.requests_completed") > 0);
}
```

**JSON 配置**（参考 OpenSpec change design §5.2）：
- 1 GpuSocTLM 顶层
- 1 KernelLaunchTLM
- 1 GpuComputeUnitTLM
- 1 SharedMemoryTLM (size_kb=64, banks=32)
- 1 GpuNoC (dim=2, hops_latency=2)
- 1 MemoryClusterTLM (channels=4, capacity_gb=8)
- 4 connections（kernel_launch → CU → SMEM → NoC → mem_cluster）

**验收**：`test_gpu_soc_phase8a.cc` PASS

**Commit**：`feat(gpu_soc): Phase 8.A end-to-end test + GB203 minimal config (Task 7)`

---

### Task 8: 5 个微架构 doc + 性能 M1 验收 + docs_sync

**Files**: 5 个微架构 doc（每 ~150-300 行）

**验收清单（全部勾选才算 M1 通过）**：
- [ ] `gpu_soc_phase8a.json` 端到端跑通（Task 7）
- [ ] SharedMemory bank conflict 测试 pass
- [ ] MemoryCluster 通道分配测试 pass
- [ ] 性能：1 SM × 1M cycles < 5 秒
- [ ] `docs_sync_check.sh --strict` 0 missing
- [ ] `format.sh --check` clean
- [ ] 现有 `[gpu]` (14 cases) + `[phase7]` (1 case) + `[apu_soc]` 全绿（不破坏 apu_soc）

**Commit**：`docs(gpu_soc): Phase 8.A microarchitecture docs + roadmap update + M1 verification (Task 8)`

---

## 验收节点 1：Oracle 审查

### Task 9: Oracle 审查（**新增**）

> **目标**：在归档 OpenSpec change 前，用 Oracle subagent 审查 Phase 8.A 全部 commit + 验收结果，确认无重大问题后才归档。

**Files:**
- Create: `docs/validation/phase8a_oracle_review.md`（审查报告）

- [ ] **Step 1: 收集证据**

```bash
# 收集所有 Phase 8.A 相关 commit
git log --oneline | grep -i "phase 8\|8\.A\|gpu_soc" | head -20

# 跑全量测试收集证据
./build/bin/cpptlm_tests --reporter compact > /tmp/phase8a_test_output.txt 2>&1
tail -5 /tmp/phase8a_test_output.txt
# 期望: "All tests passed (N assertions in M test cases)" 其中 M ≥ 703

# 性能数据
time ./build/bin/cpptlm_tests "[gpu][soc][phase8a]" --reporter compact 2>&1 | tail -3
# 期望: < 5 秒

# 文档同步
./scripts/test/docs_sync_check.sh --strict 2>&1 | tail -5
# 期望: 0 missing

# 格式检查
./scripts/build/format.sh --check 2>&1 | tail -3
# 期望: clean
```

- [ ] **Step 2: 调用 Oracle subagent**

```
调用 subagent_type="oracle" 提供以下信息:

任务: 审查 Phase 8.A 实施质量（对应 openspec change 2026-06-24-gpu-soc-phase8a-infra）

参考文档:
- Spec: docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md §5.1
- OpenSpec change: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/
- 本 plan: docs/superpowers/plans/2026-06-24-gpu-soc-phase8a.md
- ADR: docs/adr/ADR-NV-01-gpu-soc-architecture-target.md

待审查 commit: (从 Step 1 收集的 8 个 commit)
待审查文件: (从 file structure 列出的 7 新 + 4 修改)
测试基线: 703/703 (期望 ≥ 703)
性能: 1 SM × 1M cycles < 5s

请审查以下 5 个维度:
1. **代码质量**: 新 4 个核心模块是否遵循 CppTLM 现有风格 (ChStreamModuleBase 派生、bundle_base 兼容)？
2. **架构一致性**: GpuClusterSharedInterface 是否破坏 apu_soc 已有 14+1 测试？是否有循环依赖？
3. **测试覆盖**: 每个新模块是否有单元测试？集成测试是否覆盖端到端 5 类 microbenchmark？
4. **性能**: 1 SM × 1M cycles 是否 < 5 秒？是否有性能瓶颈？
5. **文档同步**: 5 个微架构 doc + AGENTS.md + roadmap 是否同步？

请输出:
- ✅/❌ 5 维度评估
- 🚨 任何阻塞问题（必须修复才能归档）
- 💡 建议改进（不阻塞归档）
- 📊 整体评价: APPROVED / NEEDS_FIX
```

- [ ] **Step 3: 记录 Oracle 审查结果**

写入 `docs/validation/phase8a_oracle_review.md`:
```markdown
# Phase 8.A Oracle 审查报告

**日期**: YYYY-MM-DD
**审查者**: Oracle subagent
**OpenSpec change**: 2026-06-24-gpu-soc-phase8a-infra

## 5 维度评估

| 维度 | 评估 | 备注 |
|------|------|------|
| 1. 代码质量 | ✅/❌ | ... |
| 2. 架构一致性 | ✅/❌ | ... |
| 3. 测试覆盖 | ✅/❌ | ... |
| 4. 性能 | ✅/❌ | ... |
| 5. 文档同步 | ✅/❌ | ... |

## 阻塞问题
（若无则写 "无"）

## 改进建议
...

## 整体评价
**APPROVED** / **NEEDS_FIX** (若 NEEDS_FIX，回到 Task 1-8 修复后再审查)
```

- [ ] **Step 4: 验证 Oracle 评价**

- 若评价为 **APPROVED**：继续 Task 10 (归档)
- 若评价为 **NEEDS_FIX**：回到 Task 1-8 修复阻塞问题，重新 Step 1-3

**Commit**: `docs(validation): Phase 8.A Oracle review report (Task 9)`

---

## 验收节点 2：归档 OpenSpec Change

### Task 10: 归档 OpenSpec Change

> **目标**：Oracle 批准后，把 OpenSpec change 从 `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/` 移到 `openspec/changes/archive/2026-06-24-gpu-soc-phase8a-infra/`，并更新相关文档。

**Files:**
- Move: `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/` → `openspec/changes/archive/2026-06-24-gpu-soc-phase8a-infra/`
- Modify: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`（追加 Phase 8 状态）

- [ ] **Step 1: 验证 Task 9 Oracle 评价为 APPROVED**

```bash
# 读取 Oracle 审查报告
grep "整体评价" docs/validation/phase8a_oracle_review.md
# 必须显示 **APPROVED**
```

若未 APPROVED，**不要继续**——回到 Task 9 修复后重新审查。

- [ ] **Step 2: 归档 OpenSpec change**

```bash
# 标准 OpenSpec 归档命令（参考项目根 openspec/ 目录结构）
git mv openspec/changes/2026-06-24-gpu-soc-phase8a-infra openspec/changes/archive/2026-06-24-gpu-soc-phase8a-infra

# 验证归档成功
ls -la openspec/changes/archive/ | grep gpu-soc-phase8a
# 期望: 看到 2026-06-24-gpu-soc-phase8a-infra 目录
```

- [ ] **Step 3: 更新 roadmap 状态**

修改 `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`：
- 在 §1 待办索引把 F12 移至 §7 已完成历史
- 在 §0 状态摘要追加 Phase 8.A 行
- 引用新 plan + OpenSpec change 路径

具体 diff（参考 §7 既有 Phase A 风格）：
```markdown
| **Phase A — F12** Phase 8.A 基础设施 (4 模块 + GpuClusterSharedInterface + GpuSocTLM + 5 microarch doc) | ⏳ 进行中 → ✅ 已归档 | 8 commits, M1 验收 |
```

- [ ] **Step 4: 提交 + 推送**

```bash
git add openspec/changes/ docs/superpowers/plans/2026-06-20-future-work-roadmap.md docs/validation/phase8a_oracle_review.md
git commit -m "chore(openspec): archive gpu-soc-phase8a-infra + update roadmap

OpenSpec change 2026-06-24-gpu-soc-phase8a-infra 已完成:
- 8 个实施任务 + Oracle 审查 (APPROVED) + 归档
- 4 个新核心模块 (MemoryCluster / SharedMemory / GpuNoC / KernelLaunch)
- GpuClusterSharedInterface 共享层 (apu_soc/gpu_soc)
- GpuSocTLM 顶层 + 5 微架构 doc
- M1 验收: 1 SM × 1M < 5s, apu_soc 兼容, 703/703 测试通过

后续依赖: 2026-06-24-gpu-soc-phase8b-core (8.B 核心仿真, 6 周)"
git push origin main
```

- [ ] **Step 5: 验证归档 + 推送成功**

```bash
# 验证
ls openspec/changes/ | grep gpu-soc-phase8a-infra
# 期望: 无输出（已不在 changes/ 目录）

ls openspec/changes/archive/ | grep gpu-soc-phase8a-infra
# 期望: 2026-06-24-gpu-soc-phase8a-infra

git log --oneline -5
# 期望: 最新 commit 显示 "chore(openspec): archive gpu-soc-phase8a-infra"

git status
# 期望: Your branch is up to date with 'origin/main'
```

---

## 整体验收 Gates

- [ ] **G1 单元测试**: `[gpu][smem][memcluster][noc][kernel_launch][cluster_shared][soc]` 全 pass
- [ ] **G2 apu_soc 兼容**: `[gpu]` 14 cases + `[phase7]` 1 case 全 pass
- [ ] **G3 端到端**: `test_gpu_soc_phase8a.cc` pass
- [ ] **G4 性能 M1**: 1 SM × 1M cycles < 5s
- [ ] **G4+ 性能 M1+**: 4 SM × 100K cycles < 5s (multi-SM contention check, 验证 O(N²) 不会引入性能瓶颈)
- [ ] **G5 文档**: 5 个微架构 doc + docs_sync 0 missing
- [ ] **G6 格式**: format.sh --check clean
- [ ] **G7 Oracle 审查**: docs/validation/phase8a_oracle_review.md 显示 APPROVED
- [ ] **G8 OpenSpec 归档**: 2026-06-24-gpu-soc-phase8a-infra 在 archive/ 目录

## 执行时间线

| Task | 周 | 累计 |
|------|:---:|:---:|
| Task 1-4 (4 独立模块) | 1.5 | 1.5 |
| Task 5 (GpuClusterSharedInterface) | 1 | 2.5 |
| Task 6 (GpuSocTLM) | 0.5 | 3 |
| Task 7 (集成测试) | 0.5 | 3.5 |
| Task 8 (5 microarch doc) | 0.5 | 4 |
| **Task 9 (Oracle 审查)** | 0.1 | 4.1 |
| **Task 10 (归档)** | 0.1 | **4.2 周** |

并行加速：Task 1-4 可 4 人并行 → 关键路径 ~1.5 周 + 1（Task 5）= 2.5 周单人/4人

## 关联文档

- **OpenSpec change**: `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/`
- **Spec**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` §5.1
- **主 plan**: `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.A
- **ADR**: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`
- **roadmap 父文档**: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`
