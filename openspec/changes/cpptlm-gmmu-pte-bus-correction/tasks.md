# Tasks — cpptlm-gmmu-pte-bus-correction v1.0 (Phase 1 Change C, TDD 5-step)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/gmmu-pte-wf/spec.md`](specs/gmmu-pte-wf/spec.md)
> **目标 tag**: `cpptlm-gmmu-pte-bus-correction-completion`
> **估时**: **~6d** (TDD 5-step + NOC 集成 + GMMU wire-format 修订)
> **依赖**: e1 v1.1 ✅ archived + Change B + Change A 已 Phase 0 完成

---

## 总体时间线 (5 阶段, ~6d)

| 阶段 | 内容 | 估时 |
|------|------|:----:|
| Phase 1 | TDD RED (3+3+3 = 9 cases failing tests) | 0.5d |
| Phase 2 | GREEN: GMMU wire-format + NOC route + bind 修订 | 2.5d |
| Phase 3 | GREEN: JSON + SDMA translate NOC 重连 | 1.5d |
| Phase 4 | 0 回归锁 + 完整测试 | 1d |
| Phase 5 | 文档同步 + archive | 0.5d |

---

## Phase 1: TDD RED — 写 9+ failing tests (0.5d)

### Task 1.1: Write failing tests for NOC 5 cases (RED)

**Files**: Create `test/test_soc_internal_noc_data_flow.cc`

- [ ] T1: route_flit(dst=VRAM) → vram 收到 flit via accept
- [ ] T2: route_flit(NULL target 4 次) → 0 crash, dropped 4
- [ ] T3: route_flit(unknown dst=99) → unknown_dst_count++ (graceful)
- [ ] T4: vc_counts_[4] 按 flit_category 分桶
- [ ] T5: 4 NODE_ID 常量唯一性

**验证**: `cmake --build build` RED
**Commit**: `test(noc): RED 5 cases NOC basic + 4 NODE_ID (Phase 1.1)`

### Task 1.2: Write failing tests for GMMU bypass 5 cases (RED)

- [ ] T6: GMMU PTE fetch 走 VramControllerTLM 专用 functional port (per §0.1 Gap I v0.5 裁决: NVIDIA H100 GMMU dedicated memory client 模式)
- [ ] T7: GMMU 通过 `set_vram_target(VramControllerTLM*)` programmatic bridge 装配 (per design §X v0.5.1 修订)
- [ ] T8: GMMU wire-format AxiMemBundle (头文件 + JSON 一致)
- [ ] T9: GMMU 异步状态机 (IDLE→WAIT→COMPLETE)
- [ ] T10: GMMU 不进 mesh NOC (instrument NOC)

**Commit**: `test(noc): RED 5 cases GMMU bypass (Phase 1.2)`

### Task 1.3: Write failing tests for SDMA bulk 5 cases (RED)

- [ ] T11-T15: 5 cases SDMA→VRAM 经 NOC byte-level match

**Commit**: `test(noc): RED 5 cases SDMA bulk DMA (Phase 1.3)`

---

## Phase 2: GREEN — GMMU wire-format + NOC route + bind 修订 (2.5d)

### Task 2.1: GMMU wire-format 切回 AxiMemBundle (GREEN, Gap I 修复)

**Files**: Modify `include/tlm/gpu/gmmu_tlm.hh` (~+50 行) + `src/tlm/gpu/gmmu_tlm.cc`

- [ ] Step 1: 头文件注释 `wire-format: AxiMemBundle (chip-internal)` 替换 `PcieTlpBundle`
- [ ] Step 2: MasterPort `req_out_ : OutputStreamAdapter<bundles::AxiMemBundle>` (替换 PcieTlpBundle)
- [ ] Step 3: SlavePort `resp_in_ : InputStreamAdapter<bundles::AxiMemBundle>` (替换 PcieTlpBundle)
- [ ] Step 4: tick() 内构造 AxiMemBundle req (vs PcieTlpBundle)
- [ ] Step 5: 保留 `translate_timing()` sync 路径 (timing-mode 兼容, mem_view_ 注入)

**验证**: Task 1.1 + 1.2 全 GREEN
**Commit**: `feat(gmmu): wire-format 切回 AxiMemBundle (Gap I 修复, Phase 2.1)`

### Task 2.2: SocInternalNoc route_flit 实际推送 + vc_counts_[4] (GREEN, 沿用 e1 v1.1)

**Files**: Modify `include/tlm/cluster/soc_internal_noc.hh` (+~80 行 per e1 v1.1)

- [ ] Step 1: `flits_dispatched_` 字段 + accessor
- [ ] Step 2: `vc_counts_[4]` std::array + accessor `vc_count(unsigned)`
- [ ] Step 3: `route_flit()` 升级: 实际推 `target->accept(flit)` + vc_counter++ per flit_category
- [ ] Step 4: `send_request(NoCFlitBundle&)` + `send_response(NoCFlitBundle&)` API
- [ ] Step 5: 4 target 接口 `accept_noc_request` / `accept_noc_response`

**验证**: Task 1.1 5 cases GREEN
**Commit**: `feat(noc): route_flit 实际推送 + vc_counts_[4] (Phase 2.2)`

### Task 2.3: SDMANocAdapter + VramNocAdapter (per e1 v1.1)

- [ ] Step 1: `include/tlm/gpu/sdma_noc_adapter.hh` (~80 行)
- [ ] Step 2: `include/tlm/gpu/vram_noc_adapter.hh` (~120 行): NoC→Fabric2VramCtrlBundle 字段映射

**Commit**: `feat(noc): 2 adapter (SDMA + VRAM, Gap G1+G2 修复, Phase 2.3)`

### Task 2.4: set_translate_cb lambda 删除 (GREEN, Gap G1 修复)

**Files**: Modify `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings` (~20 行)

- [ ] Step 1: 删 `sdma->set_translate_cb([this](...){...})` (4 行)
- [ ] Step 2: 改为 `sdma->set_translate_target(noc)` 注入 NOC (1-2 行)
- [ ] Step 3: SdmaEngineTLM::translate 路径改成 NOC 触发 (新加 translate_target_ 字段 + tick 内 dispatch)
- [ ] Step 4 (v0.5.1 修订): 增加 GMMU programmatic bridge — `gmmu->set_vram_target(vram_ctrl)` (per Oracle B1/B4 修订 + §0.1 Gap I v0.5 裁决 NVIDIA H100 GMMU dedicated memory client 模式); GMMU.translate() 改走 `vram_ctrl->handle_functional_request(addr, size, false, nullptr)`

**验证**: Task 1.3 + grep `set_translate_cb` 0 结果 + GMMU PTE 走 VramController functional port (per Task 1.2 T6/T7)
**Commit**: `feat(board): 删 set_translate_cb lambda, 改 NOC 触发 + GMMU programmatic bridge VramController (Gap G1+Gap I 修复, Phase 2.4)`

### Task 2.5: GREEN tests sync

**验证**: Task 1.1 + 1.2 + 1.3 全 GREEN (~10 cases)
**Commit**: `test(noc): GREEN 10 cases functional (Phase 2.5)`

---

## Phase 3: GREEN — JSON + SDMA translate NOC 重连 (1.5d)

### Task 3.1: dgpu_soc_minimal_v1.json +soc_internal_noc 节点 + 改 connections (Gap G2 修复)

**Files**: Modify `configs/dgpu_soc_minimal_v1.json` (+~20 行)

- [ ] Step 1: `modules.soc.modules` 加 `soc_internal_noc` (per e1 v1.1 schema)
- [ ] Step 2: connections 改: `sdma.2 → soc_internal_noc.0` (vs 直 pcie_memory.0)
- [ ] Step 3: 保留 `gmmu.0 → pcie_memory.1` 直连 (Gap I: GMMU bypass mesh)
- [ ] Step 4: JSON schema validator 加 soc_internal_noc 必填检查 (functional-mode 时)

**验证**: `openspec ... [noc]` 启动 PASS
**Commit**: `feat(json): +soc_internal_noc 节点 + 改 connections (Gap G2 修复, Phase 3.1)`

### Task 3.2: 既有 [sdma] 测试重联 SDMA translate NOC path

**Files**: Modify `test/test_sdma*.cc` + `test/test_dgpu_soc_minimal_v1.cc`

- [ ] Step 1: 跑 `[sdma]` 既有测试, 检查断点
- [ ] Step 2: 加 sim loop pump (1+ ticks) 等 NOC dispatch + GMMU translate
- [ ] Step 3: 验证 byte-level match

**Commit**: `test(sdma): 重联 translate NOC path (Phase 3.2)`

### Task 3.3: `[minimal_dgpu_soc]` + `[dgpu_soc_timing]` 0 回归

- [ ] Step 1: `[minimal_dgpu_soc]` 12 cases / 66 assertions 跑通
- [ ] Step 2: `[dgpu_soc_timing]` 37 cases / 12024 assertions 跑通 (timing-mode 独立)

**Commit**: `test(regress): minimal+timing 0 回归 (Phase 3.3)`

---

## Phase 4: 4 套件 0 回归锁 + 3 cases deadlock-fuzz (1d)

### Task 4.1: [noc][deadlock-fuzz] 3 cases (RED+GREEN)

**Files**: Extend `test/test_soc_internal_noc_data_flow.cc` (+~80 行)

- [ ] T16-T18: random flit injection, ≤ 1000 cycles resolve

**Commit**: `test(noc): RED+GREEN 3 cases deadlock-fuzz (Phase 4.1)`

### Task 4.2: 4 套件 0 回归 + 完整 test 运行

**验证**: `[sdma]` + `[pcie]` + `[minimal_dgpu_soc]` + `[dgpu_soc_timing]` 全绿
**Commit**: `test(regress): 4 套件 0 回归 (Phase 4.2)`

---

## Phase 5: 文档 + openspec validate + archive (0.5d)

### Task 5.1: 实施笔记

**Files**: Create `docs/implementation/2026-10-09-gmmu-pte-bus-correction.md` (~50 行)

**Commit**: `docs(implementation): 实施笔记 (Phase 5.1)`

### Task 5.2: architecture.md §0.2 Gap I/G2/G1 ✅ 修订

- [ ] 修订 Gap I 行 ✅ 已达
- [ ] 修订 Gap G2 行 ✅ 已达
- [ ] 修订 Gap G1 行 ✅ 已达

**Commit**: `docs(architecture): §0.2 Gap I/G2/G1 ✅ 修订 (Phase 5.2)`

### Task 5.3: archive + validate

**Commit**: `chore(openspec): archive cpptlm-gmmu-pte-bus-correction (Phase 5.3)`

---

## Acceptance (per §0.2 DoD)

- [ ] **Gap I 修复**: `gmmu_tlm.hh` wire-format 注释 = `AxiMemBundle`, JSON `gmmu.0 → pcie_memory.1` 直连 (无 soc_internal_noc 中转)
- [ ] **Gap G2 修复**: SocInternalNoc 启用 + SDMA→VRAM 经 mesh NOC + `[minimal_dgpu_soc]` 0 回归
- [ ] **Gap G1 修复**: `dgpu_board_shell.cc` `set_translate_cb` 0 处, SDMA 走 `set_translate_target(noc)` NOC 触发
- [ ] 12 commits 全 GREEN
- [ ] `[noc]` 18 cases 全绿 (5 + 5 + 5 + 3)
- [ ] `[sdma]` / `[pcie]` / `[minimal_dgpu_soc]` / `[dgpu_soc_timing]` 0 回归
- [ ] 23 ABI + **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面)
- [ ] `openspec validate --strict` PASS

### 总 commits 数

预计 **15 commits** (含 12 commits from Phase 1-5 + 2 测试 + 1 archive):

```
1.  test(noc): RED 5 cases NOC basic + 4 NODE_ID
2.  test(noc): RED 5 cases GMMU bypass
3.  test(noc): RED 5 cases SDMA bulk DMA
4.  feat(gmmu): wire-format 切回 AxiMemBundle (Gap I)
5.  feat(noc): route_flit 实际推送 + vc_counts_[4]
6.  feat(noc): 2 adapter (SDMA + VRAM)
7.  feat(board): 删 set_translate_cb lambda (Gap G1)
8.  test(noc): GREEN 10 cases functional
9.  feat(json): +soc_internal_noc 节点 + 改 connections (Gap G2)
10. test(sdma): 重联 translate NOC path
11. test(regress): minimal+timing 0 回归
12. test(noc): RED+GREEN 3 cases deadlock-fuzz
13. test(regress): 4 套件 0 回归
14. docs(implementation): 实施笔记
15. docs(architecture): §0.2 Gap I/G2/G1 ✅
16. chore(openspec): archive
```

实际 16 commits.

---

## 后续动作

- C archive 后 (Phase 5.3), KPI 清 1 槽位 → 给 Phase 1 Change D (cpptlm-pcie-tlp-entry-layer) 留 KPI slot
- Change D 实施时, Gap M 修复 (PcieTlpSerializer + TLP 解码层) 与本 change 的 SocInternalNoc 协同

---

**最后更新**: 2026-10-09 (v1.0 起草)
**Owner**: CppTLM Team (Sisyphus)
