# gpu_soc Phase 8.C Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced` change —— 4 个高级模块（Tcc / Tma / Dsm / PowerModel）+ 3 个 Python 子包（cpptlm.nvidia / cpptlm.gpu_workload / cpptlm.gpu_soc）+ 与 apu_soc Phase 7.F 端到端集成 + 完整验证报告；M3 验收（5 类场景 gpgpu-sim 数值对照带宽 ±15% + 延迟 ±20%）。

**Architecture:** Phase-accurate gpu_soc 收官。TccTLM 用 DualPortStreamAdapter 写合并；TmaTLM 异步拷贝（按 SM_120 paper §9.1：Load 488cyc, Store 33cyc）；DsmTLM inter-SM shmem（230cyc read, 36cyc write）；PowerModelTLM 经验模型 P = 80W + 1W/SM。3 个 Python 子包提供 `gpu_topology()` / `WorkloadGenerator` / `simulate()` API。

**Tech Stack:**
- C++17（CppTLM 核心）
- Catch2 v3.7.0（C++ 测试）
- Python pytest + Pybind11（Python 子包）
- pre-commit + clang-format
- Pybind11（Python ↔ C++ 桥接）

---

## File Structure (新增/修改)

### 新增头文件（4 个）
```
include/tlm/gpu/
├── tcc_tlm.hh                  (Task 19)
├── tma_tlm.hh                  (Task 20)
├── dsm_tlm.hh                  (Task 21)
└── power_model_tlm.hh         (Task 22)
```

### 新增 C++ 实现
```
src/tlm/gpu/{tcc,tma,dsm,power_model}_tlm.cc
```

### 新增测试（5 个）
```
test/test_{tcc,tma,dsm,power_model}_tlm.cc          (Task 19-22)
test/test_gpu_soc_phase8c.cc                          (Task 26 apu_soc 集成)
```

### 新增 Python 子包（3 个 + 10 个文件）
```
cpptlm/cpptlm/
├── nvidia/
│   ├── __init__.py
│   ├── topology.py
│   ├── blueprint.py
│   ├── export.py
│   └── sku_library.py
├── gpu_workload/
│   ├── __init__.py
│   ├── generator.py
│   ├── replay.py
│   └── patterns.py
└── gpu_soc/
    ├── __init__.py
    ├── simulate.py
    └── report.py
```

### 新增 Python 测试（3 个文件，~15 用例）
```
test/python/test_cpptlm_nvidia.py
test/python/test_cpptlm_gpu_workload.py
test/python/test_cpptlm_gpu_soc.py
```

### 新增配置
```
configs/templates/gpu_soc/gpu_soc_phase8c.json
```

### 新增微架构 doc（4 个）
```
docs/soc_arch/modules/gpu-{tcc,tma,dsm,power-model}.md
```

### 新增验证报告（2 个）
```
docs/validation/phase8c_verification_report.md
docs/validation/gpgpu_sim_baseline.csv
```

### 修改文件
```
include/chstream_register.hh                 (+4 行注册)
include/tlm/cluster/gpu_cluster.hh           (集成 TccTLM + TmaTLM + PowerModelTLM)
AGENTS.md                                    (STRUCTURE 节加 cpptlm/{nvidia,gpu_workload,gpu_soc})
docs/superpowers/plans/2026-06-20-future-work-roadmap.md  (Phase 8 完成状态)
```

---

## 实施任务（9 个 + 2 个验收节点 = 11 个任务）

### Task 19: TccTLM write coalescing 单元测试

**关键 API**：
```cpp
class TccTLM : public ChStreamModuleBase {
    TccTLM(const std::string& name, EventQueue* eq);
    void submit_write(uint64_t addr, uint32_t size, const uint8_t* data);
    void flush();
    uint32_t transactions_issued() const;
};
// 4 个连续 16B 写 → 1 个 64B MemoryTLM transaction (合并比 ≥ 4×)
```

**验收**：`./build/bin/cpptlm_tests "[gpu][tcc]"` PASS

**Commit**：`feat(tlm/gpu): TccTLM write coalescing (Phase 8.C Task 19)`

---

### Task 20: TmaTLM async copy + mbarrier

**关键 API**：
```cpp
class TmaTLM : public ChStreamModuleBase {
    TmaTLM(const std::string& name, EventQueue* eq);
    uint32_t load_latency(uint32_t size_b, bool swizzle) const;
    uint32_t store_latency(uint32_t size_b) const;
};
// SM_120 paper §9.1: Load 1D 1024B = 488 cyc, Load 2D 1024B = 620 cyc, Store 2D 1024B = 33 cyc
```

**验收**：`./build/bin/cpptlm_tests "[gpu][tma]"` PASS

**Commit**：`feat(tlm/gpu): TmaTLM async copy (Phase 8.C Task 20)`

---

### Task 21: DsmTLM inter-SM shmem

**关键 API**：
```cpp
class DsmTLM : public ChStreamModuleBase {
    DsmTLM(const std::string& name, EventQueue* eq);
    uint32_t remote_read_latency() const { return 230; }   // SM_120 paper
    uint32_t remote_write_latency() const { return 36; }
};
```

**验收**：`./build/bin/cpptlm_tests "[gpu][dsm]"` PASS

**Commit**：`feat(tlm/gpu): DsmTLM inter-SM shmem (Phase 8.C Task 21)`

---

### Task 22: PowerModelTLM 80W + 1W/SM 经验模型

**关键 API**：
```cpp
class PowerModelTLM : public ChStreamModuleBase {
    PowerModelTLM(const std::string& name, EventQueue* eq, uint32_t num_sm);
    double compute_power(bool tc_saturated) const;
};
// 简化模型（按 SM_120 paper §11.3）: P = 80 + N × 1.0 (+ 30 if TC saturated)
```

**验收**：`./build/bin/cpptlm_tests "[gpu][power]"` PASS

**Commit**：`feat(tlm/gpu): PowerModelTLM 80W+1W/SM (Phase 8.C Task 22)`

---

### Task 23: cpptlm.nvidia 拓扑生成 + 4 SKU 预置

**Files**: 5 个 .py 文件
- `cpptlm/cpptlm/nvidia/__init__.py` — 导出 `gpu_topology` / `gb203_consumer` / `gb200_datacenter` / `gh100_hopper` / `ga100_ampere`
- `topology.py` — `GpuTopology` dataclass（22 个参数）+ `gpu_topology(**kwargs)` factory
- `blueprint.py` — `NvidiaGPUBlueprint` + Variant 模式
- `export.py` — `to_json()` / `to_dot()` / `validate()`
- `sku_library.py` — 4 个 SKU 预置函数

**Test**: `test/python/test_cpptlm_nvidia.py` (~5 用例)

**Commit**：`feat(cpptlm): nvidia topology subpackage + 4 SKU presets (Phase 8.C Task 23)`

---

### Task 24: cpptlm.gpu_workload + 5 pattern 构造器

**Files**: 4 个 .py 文件
- `generator.py` — `WorkloadGenerator` class
- `replay.py` — `TraceReplay` class（NCU `.ncu-rep` + CuPTI `.cupti-trace` 双格式）
- `patterns.py` — 5 个 pattern: `GEMM` / `FlashAttention` / `VectorAdd` / `Stencil3D` / `SparseSpMV`

**Test**: `test/python/test_cpptlm_gpu_workload.py` (~5 用例)

**Commit**：`feat(cpptlm): gpu_workload subpackage + 5 patterns (Phase 8.C Task 24)`

---

### Task 25: cpptlm.gpu_soc 顶层 simulate + report

**Files**: 3 个 .py 文件
- `simulate.py` — `simulate(topo, workload, duration_cycles, metrics) -> SimulationResult`
- `report.py` — Markdown 报告生成 + gpgpu-sim 对照表
- `__init__.py` — 导出顶层 API

**Python ↔ C++ 接口**：Pybind11 桥接（或 subprocess + JSON 备选）

**Test**: `test/python/test_cpptlm_gpu_soc.py` (~5 用例)

**Commit**：`feat(cpptlm): gpu_soc top-level + report generator (Phase 8.C Task 25)`

---

### Task 26: apu_soc 集成测试 + 完整验证报告

**Files**:
- Create: `test/test_gpu_soc_phase8c.cc`（apu_soc 集成测试）
- Create: `docs/validation/phase8c_verification_report.md`
- Create: `docs/validation/gpgpu_sim_baseline.csv`

**apu_soc 集成测试**（`test_gpu_soc_phase8c.cc`）：
```cpp
TEST_CASE("gpu_soc 与 apu_soc 共享 GpuCluster 集成", "[gpu][apu][phase8c]") {
    auto* factory = ModuleFactory::instance();
    factory->loadConfig("configs/apu_soc_phase7f.json");
    factory->loadConfig("configs/templates/gpu_soc/gpu_soc_phase8c.json");
    factory->instantiateAll();
    auto* cluster_in_apu = factory->get<GpuCluster>("gpu_cluster");
    auto* cluster_in_gpu = factory->get<GpuCluster>("gpu_cluster");
    REQUIRE(cluster_in_apu == cluster_in_gpu);  // 同一实例
    for (int i = 0; i < 100; ++i) factory->tick();
    REQUIRE(cluster_in_apu->getCycleCount() == 100);
}
```

**完整验证报告**（`docs/validation/phase8c_verification_report.md`）：
| 场景 | 测量带宽 | gpgpu-sim baseline | 误差% | 是否通过(±15%) |
|------|---------|-------------------|:-----:|:---:|
| GEMM | TBD | 700 GB/s | TBD% | ✓/✗ |
| FlashAttn | TBD | 470 GB/s | TBD% | ✓/✗ |
| vector_add | TBD | 1176 GB/s | TBD% | ✓/✗ |
| stencil | TBD | 940 GB/s | TBD% | ✓/✗ |
| sparse SpMV | TBD | 230 GB/s | TBD% | ✓/✗ |

+ 延迟 p99（±20%）+ Cache 命中率（±5pp）+ PowerModel 输出

**Commit**：`feat(gpu_soc): Phase 8.C 完整验证报告 + apu_soc 集成 (Task 26)`

---

### Task 27: 4 个微架构 doc + AGENTS.md + roadmap 最终化

**Files**:
- Create: 4 个微架构 doc（每 ~150-300 行）
- Modify: `AGENTS.md`（STRUCTURE 节加 3 个 Python 子包）
- Modify: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`（Phase 8 完成状态）

**Commit**：`docs(gpu_soc): Phase 8.C microarch docs + AGENTS.md + roadmap finalization (Task 27)`

---

## 验收节点 1：Oracle 审查

### Task 28: Oracle 审查（**新增**）

> **目标**：在归档 OpenSpec change 前，用 Oracle subagent 审查 Phase 8.C 全部 commit + 验证结果。

**Files:**
- Create: `docs/validation/phase8c_oracle_review.md`

- [ ] **Step 1: 收集证据**

```bash
# 收集所有 Phase 8.C 相关 commit
git log --oneline | grep -i "phase 8\|8\.C\|gpu_soc\|cpptlm" | head -20

# 跑全量测试
./build/bin/cpptlm_tests --reporter compact > /tmp/phase8c_test_output.txt 2>&1
tail -5 /tmp/phase8c_test_output.txt
# 期望: "All tests passed (N assertions in M test cases)" 其中 M ≥ 703 + 8.A + 8.B + 8.C 新增

# 跑全量 Python 测试
python -m pytest test/python/ -v > /tmp/phase8c_pytest_output.txt 2>&1
tail -10 /tmp/phase8c_pytest_output.txt
# 期望: 222 + ~15 (8.B) + ~15 (8.C) = 252/252 passed

# 文档同步
./scripts/test/docs_sync_check.sh --strict 2>&1 | tail -3
# 期望: 0 missing

# 格式
./scripts/build/format.sh --check 2>&1 | tail -3
# 期望: clean

# 验证报告
ls -la docs/validation/
cat docs/validation/phase8c_verification_report.md | head -30
```

- [ ] **Step 2: 调用 Oracle subagent**

```
调用 subagent_type="oracle" 提供以下信息:

任务: 审查 Phase 8.C 实施质量（最终 phase, 整个 gpu_soc 收官）
对应 openspec change: 2026-06-24-gpu-soc-phase8c-advanced

参考文档:
- Spec: docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md §5.3
- OpenSpec change: openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/
- 本 plan: docs/superpowers/plans/2026-06-24-gpu-soc-phase8c.md
- 依赖: openspec/changes/2026-06-24-gpu-soc-phase8b-core/ (M2 已完成)
- ADR: docs/adr/ADR-NV-01-gpu-soc-architecture-target.md

待审查 commit: (从 Step 1 收集的 9 个 commit)
测试基线: 703 + 8.A(5) + 8.B(7) + 8.C(4) = 719/719 C++ + 222 + 8.B(15) + 8.C(15) = 252/252 Python
验证报告: docs/validation/phase8c_verification_report.md

请审查 7 维度:
1. **代码质量**: 4 个新高级模块是否遵循 CppTLM 风格?
2. **架构一致性**: Tcc/Tma/Dsm/PowerModel 集成到 GpuCluster 是否破坏 Phase 8.A/8.B?
3. **Python 子包**: cpptlm.{nvidia,gpu_workload,gpu_soc} API 易用性 + 5 pattern 完整?
4. **apu_soc 集成**: 两者引用同一 GpuCluster 实例测试通过?
5. **验证报告**: 5 类场景带宽 ±15% + 延迟 ±20% + 命中率 ±5pp?
6. **性能**: 完整仿真可跑通, 性能在合理范围?
7. **文档同步**: 4 microarch doc + AGENTS.md + roadmap + ADR-NV-01 全部同步?

请输出:
- ✅/❌ 7 维度评估
- 🚨 阻塞问题
- 💡 改进建议
- 📊 整体评价: APPROVED / NEEDS_FIX
- 🎯 特别关注: Phase 8 整体收官是否达标? gpu_soc 路径是否完整可用?
```

- [ ] **Step 3: 记录 Oracle 审查结果**

写入 `docs/validation/phase8c_oracle_review.md`:
```markdown
# Phase 8.C Oracle 审查报告 (gpu_soc 收官)

**日期**: YYYY-MM-DD
**审查者**: Oracle subagent
**OpenSpec change**: 2026-06-24-gpu-soc-phase8c-advanced

## 7 维度评估
| 维度 | 评估 | 备注 |
|------|------|------|
| 1. 代码质量 | ✅/❌ | |
| 2. 架构一致性 | ✅/❌ | |
| 3. Python 子包 | ✅/❌ | |
| 4. apu_soc 集成 | ✅/❌ | |
| 5. 验证报告 | ✅/❌ | |
| 6. 性能 | ✅/❌ | |
| 7. 文档同步 | ✅/❌ | |

## Phase 8 整体收官评估
- Phase 8.A 基础设施: ✅ Done
- Phase 8.B 核心仿真: ✅ Done
- Phase 8.C 高级特性: ⏳ Reviewing
- 14 个新模块全部落地: ✅/❌
- 3 个 Python 子包完整: ✅/❌
- 与 apu_soc 共享 GpuCluster: ✅/❌
- gpgpu-sim 数值对照: 5 类 ±15%: ✅/❌

## 阻塞问题
(若无则 "无")

## 改进建议

## 整体评价
**APPROVED** / **NEEDS_FIX**
```

- [ ] **Step 4: 验证 Oracle 评价**

- **APPROVED** → 继续 Task 29
- **NEEDS_FIX** → 回到 Task 19-27 修复后重新 Step 1-3

**Commit**: `docs(validation): Phase 8.C Oracle review report (Task 28)`

---

## 验收节点 2：归档 OpenSpec Change

### Task 29: 归档 OpenSpec Change

> **目标**：Oracle 批准后，Phase 8.C 是**最后阶段**——归档后整个 Phase 8 (gpu_soc 路径) 完整收官。

**Files:**
- Move: `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/` → `openspec/changes/archive/2026-06-24-gpu-soc-phase8c-advanced/`
- Modify: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`（Phase 8 完整收官状态）

- [ ] **Step 1: 验证 Task 28 Oracle 评价为 APPROVED**

```bash
grep "整体评价" docs/validation/phase8c_oracle_review.md
# 必须: **APPROVED**
```

若未 APPROVED，**不要继续**——回到 Task 28 修复后重新审查。

- [ ] **Step 2: 归档 OpenSpec change**

```bash
git mv openspec/changes/2026-06-24-gpu-soc-phase8c-advanced openspec/changes/archive/2026-06-24-gpu-soc-phase8c-advanced
ls -la openspec/changes/archive/ | grep gpu-soc
# 期望: 看到全部 3 个 (8.A, 8.B, 8.C)
```

- [ ] **Step 3: 更新 roadmap 状态 (Phase 8 完整收官)**

在 §0 状态摘要表追加 Phase 8.C row（**最终状态**）:
```markdown
| **Phase 8.C 高级特性** | ⏳ 进行中 → ✅ 已归档 | ✅ Done | 目标: 715 → 719/719 + 252/252 Python |
```

在 §7 已完成历史追加 Phase 8 段:
```markdown
| **Phase 8 — gpu_soc 完整收官** | 3 commits (`X` `Y` `Z`) | 2026-MM-DD | 14 新模块 + 3 Python 子包 + 15 microarch doc |
| Phase 8.A 基础设施 | (commit) | (date) | 4 模块 + GpuClusterSharedInterface + GpuSocTLM + M1 |
| Phase 8.B 核心仿真 | (commit) | (date) | 6 模块 + 5 类 microbenchmark + gpgpu-sim ±15% + M2 |
| Phase 8.C 高级特性 | (commit) | (date) | 4 模块 + 3 Python 子包 + apu_soc 集成 + M3 |
```

- [ ] **Step 4: 提交 + 推送**

```bash
git add openspec/changes/ docs/superpowers/plans/2026-06-20-future-work-roadmap.md docs/validation/phase8c_oracle_review.md
git commit -m "chore(openspec): archive gpu-soc-phase8c-advanced + Phase 8 完整收官

OpenSpec change 2026-06-24-gpu-soc-phase8c-advanced 已完成 (Phase 8 收官):
- 4 个高级模块 (Tcc / Tma / Dsm / PowerModel)
- 3 个 Python 子包 (cpptlm.nvidia / gpu_workload / gpu_soc)
- apu_soc Phase 7.F 端到端集成 (共享 GpuCluster)
- 完整 5 类场景验证报告 (带宽 ±15%, 延迟 ±20%)
- Oracle 审查 APPROVED

Phase 8 完整收官:
- 总: 14 新模块 + 3 Python 子包 + 15 microarch doc + 3 OpenSpec changes (全部 archived)
- 测试基线: 703 → 719/719 C++ + 222 → 252/252 Python
- 与 apu_soc 共享: GpuCluster + 4 子模块 (P5 stub → 完整)

后续: gpu_soc 路径已完整可用; apu_soc 可享用 Phase 8 高级仿真能力"
git push origin main
```

- [ ] **Step 5: 验证归档 + 推送成功**

```bash
# 验证全部 3 个 OpenSpec changes 都在 archive/
ls openspec/changes/ | grep gpu-soc
# 期望: 无输出（全部 archived）

ls openspec/changes/archive/ | grep gpu-soc
# 期望: 2026-06-24-gpu-soc-phase8a-infra
# 期望: 2026-06-24-gpu-soc-phase8b-core
# 期望: 2026-06-24-gpu-soc-phase8c-advanced

git log --oneline -5
# 期望: "chore(openspec): archive gpu-soc-phase8c-advanced"

git status
# 期望: Your branch is up to date with 'origin/main'
```

---

## 整体验收 Gates

- [ ] **G1 单元测试**: `[gpu][tcc][tma][dsm][power]` 全 pass
- [ ] **G2 Python 子包**: `test_cpptlm_{nvidia,gpu_workload,gpu_soc}.py` 15+ 用例全 pass
- [ ] **G3 apu_soc 集成**: `test_gpu_soc_phase8c.cc` 共享 GpuCluster 测试 pass
- [ ] **G4 验证报告**: `phase8c_verification_report.md` 5 类场景带宽 ±15%, 延迟 ±20%
- [ ] **G5 性能 M3**: 完整仿真可跑通（无具体性能目标）
- [ ] **G6 文档**: 4 个微架构 doc + AGENTS.md 更新 + docs_sync 0 missing
- [ ] **G7 兼容**: Phase 8.A + 8.B + apu_soc 全绿（不破坏）
- [ ] **G8 Oracle 审查**: docs/validation/phase8c_oracle_review.md 显示 APPROVED
- [ ] **G9 OpenSpec 归档**: 2026-06-24-gpu-soc-phase8c-advanced 在 archive/ 目录
- [ ] **G10 Phase 8 整体收官**: 全部 3 个 OpenSpec changes 归档 + 测试基线 719/719 + 252/252

## 执行时间线

| Task | 周 | 累计 |
|------|:---:|:---:|
| Task 19 (Tcc) | 0.5 | 0.5 |
| Task 20 (Tma) | 0.5 | 1 |
| Task 21 (Dsm) | 0.3 | 1.3 |
| Task 22 (PowerModel) | 0.3 | 1.6 |
| Task 23 (cpptlm.nvidia) | 0.5 | 2.1 |
| Task 24 (cpptlm.gpu_workload) | 0.4 | 2.5 |
| Task 25 (cpptlm.gpu_soc) | 0.3 | 2.8 |
| Task 26 (apu_soc 集成 + 验证报告) | 0.5 | 3.3 |
| Task 27 (4 microarch doc + AGENTS) | 0.3 | 3.6 |
| **Task 28 (Oracle 审查)** | 0.1 | 3.7 |
| **Task 29 (归档)** | 0.1 | **3.8 周** |

并行加速：Task 19-22 (4 高级模块) + Task 23-25 (3 Python 子包) 都可并行 → 关键路径 ~2.5 周

## 关联文档

- **OpenSpec change**: `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/`
- **依赖**: `openspec/changes/2026-06-24-gpu-soc-phase8b-core/`（M2 必须先完成）
- **Spec**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` §5.3
- **主 plan**: `docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md` Phase 8.C
- **ADR**: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`
- **roadmap 父文档**: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`
- **最终交付物** (Phase 8 整体):
  - 14 个新 C++ 模块 (3650 LOC impl + 650 tests)
  - 3 个 Python 子包 (800 LOC)
  - 15 个微架构 doc
  - 3 个 OpenSpec changes (全部 archived)
  - 3 个 per-phase plans (含 Oracle + 归档)
  - 1 个 ADR-NV-01
  - 1 个 spec (gpu-soc-architecture.md)
  - 1 个主 plan (gpu-soc-roadmap.md)
  - 1 个验证报告 (phase8c_verification_report.md)
