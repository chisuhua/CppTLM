# Tasks — cpptlm-dgpu-soc-internal-noc-minimal (TDD 5 步)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/soc-internal-noc/spec.md`](specs/soc-internal-noc/spec.md)
> **起点**: `dgpu-realip-pin-contract-change-d-completion` tag
> **目标 tag**: `cpptlm-dgpu-soc-internal-noc-minimal-completion`
> **估时**: 4-5h

---

## Phase 1: Header 骨架 (TDD RED → GREEN, 1h)

### Task 1.1: Write failing test stub (RED)

**Files:**
- Create: `test/test_soc_internal_noc.cc`
- Include: `catch_amalgamated.hpp`, `tlm/cluster/soc_internal_noc.hh`

**步骤**:
- [ ] Step 1: 写 5 路由场景 + 4 setter/accessor 测试 stub (TEST_CASE #1-#5 pending, NOT YET COMPILE)
- [ ] Step 2: 期望编译失败 (SocInternalNoc 类未定义)

**验证**: `cmake --build build` 退出码非 0 (`'SocInternalNoc' was not declared`)

**Commit**: `test(soc-internal-noc): add 5 routing scenarios + 4 setter/accessor RED stub`

---

### Task 1.2: Create header (GREEN)

**Files:**
- Create: `include/tlm/cluster/soc_internal_noc.hh` (~80 行)

**步骤**:
- [ ] Step 1: 定义 `class SocInternalNoc : public SimModule` (namespace `tlm::cluster`)
- [ ] Step 2: 4 setter + 4 accessor (`pcie_/sdma_/gmmu_/vram_` 私有成员, raw 指针)
- [ ] Step 3: `static constexpr uint32_t NODE_ID_{PCIE,SDMA,GMMU,VRAM}` (mesh 寻址)
- [ ] Step 4: `void route_flit(const NoCFlitBundle& flit)` 入口
- [ ] Step 5: 统计成员 `flits_routed_` + `unknown_dst_count_`
- [ ] Step 6: forward-decl 4 target 类, 不 #include (避免循环)

**验证**: `cmake --build build` 退出码 0, TEST_CASE #1-#5 通过

**Commit**: `feat(soc-internal-noc): add SocInternalNoc SimModule with 4-target routing (GREEN)`

---

## Phase 2: 实现 (TDD GREEN, 1h)

### Task 2.1: Implement route_flit() 核心逻辑

**Files:**
- Create: `src/tlm/cluster/soc_internal_noc.cc` (~100 行)

**步骤**:
- [ ] Step 1: ctor 调 `SimModule(name, eq)` 基类
- [ ] Step 2: `route_flit` 读 `flit.dst_node`, switch 4 cases, default 增 `unknown_dst_count_`
- [ ] Step 3: nullptr target 不 crash (D.2 优雅降级)
- [ ] Step 4: 加 4 target 的 `set_target_*` 实际路由: 推到 target 内部 queue (per Phase 1 桩)

**验证**: TEST_CASE #1-#5 仍 PASS, route_flit 走通 4 dst_node 路径

**Commit**: `feat(soc-internal-noc): implement route_flit() with 4-target routing + nullptr safety`

---

## Phase 3: 装配集成 (1h)

### Task 3.1: REGISTER_MODULE 短名注册

**Files:**
- Modify: `include/modules_cluster.hh` (加 2 行)

**步骤**:
- [ ] Step 1: 加 `#include "tlm/cluster/soc_internal_noc.hh"`
- [ ] Step 2: 加 `const bool _reg_socinternalnoc = (REGISTER_MODULE(tlm::cluster::SocInternalNoc), true);`
- [ ] Step 3: 加短名 (仿 PcieControllerIP precedent): `const bool _reg_socinternalnoc_short = (ModuleFactory::registerModule<tlm::cluster::SocInternalNoc>("SocInternalNoc"), true);`

**验证**: compile 退出码 0, ModuleFactory::getModuleRegistry().count("SocInternalNoc") == 1

**Commit**: `feat(soc-internal-noc): register SocInternalNoc in modules_cluster (FQ + 短名)`

---

### Task 3.2: bind_memory_backings 装配 4 桥接

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()` (加 ~15 行)
- Add forward-decl: `class SocInternalNoc;` in tlm::cluster namespace

**步骤**:
- [ ] Step 1: 在现有 vram bridge (`GpuInternalFabric::set_vram_target`) 之后, 加 SocInternalNoc 装配段
- [ ] Step 2: 通过 `soc_->getInternalInstance("soc_noc")` 找 SocInternalNoc, dynamic_cast 验证
- [ ] Step 3: 4 setters: pcie_ctrl (PcieControllerIP*), sdma (SdmaEngineTLM*), gmmu (GmmuTLM*), vram (VramControllerTLM*)
- [ ] Step 4: 加 comments 解释 5 桥接 (1 vram via GpuInternalFabric + 4 new via SocInternalNoc)

**验证**: `[minimal_dgpu_soc]` 12/12 + `[pcie-memory]` 17/17 全绿 (0 回归)

**Commit**: `feat(soc-internal-noc): wire 4 setters in bind_memory_backings (5 bridges total)`

---

## Phase 4: 测试覆盖 (1h)

### Task 4.1: 完整测试用例 (Phase 1 stub 补充)

**Files:**
- Modify: `test/test_soc_internal_noc.cc` (补全 5 TEST_CASE)

**步骤**:
- [ ] Step 1: TEST_CASE #1 - 4 setter + accessor 双向 (set nullptr 验证 graceful)
- [ ] Step 2: TEST_CASE #2 - dst=PCIE 路由到 pcie_target
- [ ] Step 3: TEST_CASE #3 - dst=SDMA/GMMU/VRAM 同样 (3 sub-cases)
- [ ] Step 4: TEST_CASE #4 - dst=99 未知 → unknown_dst_count++
- [ ] Step 5: TEST_CASE #5 - nullptr target + 路由触发 → 不 crash, flits_routed 不增

**验证**: 5 TEST_CASE / ≥10 assertions 全绿, `[realip][pin-contract][noc]` sub-tag 命中

**Commit**: `test(soc-internal-noc): 5 routing scenarios + 4 setter/accessor (full coverage)`

---

## Phase 5: 文档同步 (1h)

### Task 5.1: architecture.md §2.4 更新

**Files:**
- Modify: `docs/designs/dgpu-soc/architecture.md`

**步骤**:
- [ ] Step 1: §2.4 4-IP 表格新增 `SocInternalNoc` 行 (映射 Arteris FlexNoC / Custom Mesh)
- [ ] Step 2: §2.4.1 字段表后加 §2.4.3 "SocInternalNoc Functional Routing" 段
- [ ] Step 3: 引用 NoCFlitBundle 18 字段表 (per #4 commit `3fba7a61`)

**Commit**: `docs(soc-internal-noc): architecture.md §2.4 - add SocInternalNoc 4-IP row`

---

### Task 5.2: AGENTS.md KEY INVARIANTS

**Files:**
- Modify: `AGENTS.md`

**步骤**:
- [ ] Step 1: KEY INVARIANTS 加 "SocInternalNoc functional routing" 条目
- [ ] Step 2: 引用本 change + design.md + 4 NODE_ID
- [ ] Step 3: OpenSpec baseline 更新 (3/3 Proposed — 2 现有 + 1 新)

**Commit**: `docs(AGENTS): add SocInternalNoc KEY INVARIANT + OpenSpec baseline 2 → 3`

---

### Task 5.3: 设计文档

**Files:**
- Create: `docs/designs/dgpu-soc/soc-internal-noc-design.md`

**步骤**:
- [ ] Step 1: 与 `design.md` 内容对齐, 加图示 + 路由表
- [ ] Step 2: 引用 AGENTS.md "仿真框架 vs 应用设计" 范围治理
- [ ] Step 3: 引用 ADR-DGPU-07 演进 seam

**Commit**: `docs(soc-internal-noc): create design doc at designs/dgpu-soc/`

---

## Phase 6: 验证 gates (30min)

### Task 6.1: 全面验证

**步骤**:
- [ ] `cmake --build build` 退出码 0 (无编译错误)
- [ ] `openspec validate cpptlm-dgpu-soc-internal-noc-minimal --strict` PASS
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS (新 design doc 路径引用)
- [ ] 7 核心套件全绿, 0 回归:
  - [pcie] 430/430
  - [pcie-memory] 17/17
  - [minimal_dgpu_soc] 12/12 (新装配路径)
  - [realip] 24/24 (含 [realip][pin-contract][noc] 新 sub-tag)
  - [dgpu_soc_timing] 37/37
  - [driver_visible] 2/2
  - [tmu] 11/11

**Commit**: `docs(plan): add completion report + gate verification`

---

## Phase 7: 收口 (15min)

### Task 7.1: 标签归档

**步骤**:
- [ ] Git tag: `cpptlm-dgpu-soc-internal-noc-minimal-completion`
- [ ] OpenSpec status: `openspec archive cpptlm-dgpu-soc-internal-noc-minimal`
- [ ] 准备 PR / merge 到 main

---

## 实施时间线（修订, 2026-10-08）

| Phase | 估时 | 累计 |
|-------|------|------|
| Phase 1 header (RED→GREEN) | 1h | 1h |
| Phase 2 impl | 1h | 2h |
| Phase 3 装配 | 1h | 3h |
| Phase 4 tests | 1h | 4h |
| Phase 5 docs | 1h | 5h |
| Phase 6 verify | 30min | 5.5h |
| Phase 7 closeout | 15min | **5.75h** |
