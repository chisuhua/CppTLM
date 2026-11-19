# GMMU PTE Bus 修正 + SocInternalNoc DataNoC 接线 (cpptlm-gmmu-pte-bus-correction)

> **Change**: `cpptlm-gmmu-pte-bus-correction` (Phase 1, Change C, v0.5.2 范围缩减)
> **修复 Gap**: I (GMMU page-walk wire-format 错配) + G2 (SDMA→VRAM 经 NOC) + G1 (删 `translate_cb_` lambda)
> **依赖**: Change A (vram-controller-functional-integration, ✅ archive) + Change B (lwc-bus-hub-mvp, ✅ archive) + e1 v1.1 (SocInternalNoc, ✅ archive, commit `13af5d51`)
> **Owner**: CppTLM Team (Sisyphus)
> **目标 tag**: `cpptlm-gmmu-pte-bus-correction-completion`

---

## 1. 范围 (v0.5.2 缩减后)

本 change **不重写 NoC** (`soc_internal_noc.hh:114-168` 已实现: `route_flit()` 实际推送 + `vc_counts_[4]` + `send_request/response`)。仅做 2 件事:

1. **GMMU dedicated client 接线**: GMMU PTE fetch 直连 VramControllerTLM functional port (`handle_functional_request(pte_addr, 8, false, nullptr)`), **不经 mesh NoC** (per Gap I 裁决 + NVIDIA H100 GMMU dedicated memory client 模式)
2. **删 `translate_cb_` 旧路径**: `dgpu_board_shell.cc` 的 `sdma->set_translate_cb(lambda)` 删除, SDMA translate 改经 GMMU programmatic bridge

Gap G2 (SDMA→VRAM 经 NOC) 由本 change 实施化 (`sdma.2` connection 走 soc_internal_noc → pcie_memory)。

## 2. 数据路径 (实施后)

```
SDMA H2D/D2H bulk DMA (经 NoC, Gap G2):
    sdma.mem_out → soc_internal_noc (JSON connection) → route_flit(dst=VRAM)
        → vram_accept_fn (VramNocAdapter 或 vram_ctrl 数据委托) → handle_functional_request

GMMU PTE fetch (直连 VramControllerTLM, 不经 mesh NoC, Gap I):
    gmmu.translate(iova) → vram_ctrl_->handle_functional_request(pte_addr, 8, false, nullptr)
        → 直 memcpy pte_buf_ → COMPLETE → paddr

SDMA translate (Gap G1 修复, 无 lambda):
    sdma.process_h2d → translate_target_->translate(iova, size, phys)   [direct GMMU pointer]
```

## 3. TDD 5-step + Commits 索引

| T | commit | 内容 |
|---|--------|------|
| T0 | (见 git log) | 实施笔记骨架 + pre-flight 验证 |
| T1 | | RED: GMMU PTE fetch 走 VramController functional port |
| T2 | | GREEN: gmmu_tlm.hh set_vram_target + translate() 走 handle_functional_request |
| T3 | | RED: soc_internal_noc SDMA→pcie_memory 接线 |
| T4 | | GREEN: minimal_v1.json 重写 + bind soc_noc accept_fn |
| T5 | | RED: 删 translate_cb_ 验证 SDMA 新路径 |
| T6 | | GREEN: board 删 set_translate_cb lambda + 装配 |
| T7 | | E2E GREEN: GMMU bypass NOC + SDMA 闭环 |
| T8 | | 实施笔记完成 + post-flight 验证 |

## 4. Pre-flight 验证 (verbatim)

```bash
# 4件套
openspec/changes/cpptlm-gmmu-pte-bus-correction/{proposal,design,tasks}.md + specs/gmmu-pte-wf/spec.md 已读

# NOC 路由已实现 (e1 v1.1 commit 13af5d51)
include/tlm/cluster/soc_internal_noc.hh:114 route_flit() 实际推送
include/tlm/cluster/soc_internal_noc.hh:143-154 send_request/send_response API
include/tlm/cluster/soc_internal_noc.hh:103-106 set_*_accept_fn 注册
include/tlm/cluster/soc_internal_noc.hh:137-140 vc_counts_[flit_category]++

# translate_cb_ 旧路径位置
src/tlm/gpu/dgpu_board_shell.cc:742-746  sdma->set_translate_cb(lambda)
include/tlm/gpu/sdma_engine_tlm.hh:211   void set_translate_cb(DmaTranslateCb cb)
include/tlm/gpu/sdma_engine_tlm.hh:393   DmaTranslateCb translate_cb_;
src/tlm/gpu/sdma_engine_tlm.cc:233-256  process_h2d translate 调用

# GMMU PTE fetch 现状
include/tlm/gpu/gmmu_tlm.hh:131  translate() — mem_view_ sync / req_out_ async (PcieTlpBundle)
include/tlm/gpu/gmmu_tlm.hh:285  tick() — WAIT + resp_in_ CPLD → COMPLETE

# VramControllerTLM::handle_functional_request (Change A T4)
include/tlm/vram_controller_tlm.hh:85  FunctionalResult handle_functional_request(addr, size, is_write, data_ptr)

# 现有测试
test/test_soc_internal_noc_data_flow.cc (7 cases, [noc] tag)
test/test_gmmu_tlm.cc (21 cases, [gmmu] tag)

# minimal_v1 GMMU connection (Gap I 目标)
configs/dgpu_soc_minimal_v1.json:73-74
  { "src": "sdma.2", "dst": "pcie_memory.0" }   ← SDMA 直连 (Gap G2: 改走 soc_internal_noc)
  { "src": "gmmu.0", "dst": "pcie_memory.1" }   ← GMMU 直连 pcie_memory (Gap I: 删除, 改走 vram_ctrl programmatic)

# baseline (2026-10-12)
[gmmu] 21 / 5267 | [noc] 62 / 283 | [minimal_dgpu_soc] 21 / 145 | [pcie-memory] 17 / 48
[driver_visible] 4 / 40 | [dgpu_soc_timing] 37 / 12024 | [realip] 51 / 366 | [sdma] 67 / 12941
```

## 5. Post-flight 验证

```
# 构建: exit 0 (cmake --build build -j)

# 套件 (Change C 后)
[gmmu]              25 / 5273      (baseline 21/5267, +4 cases = T1/T2 4 cases)
[noc]               64 / 304       (baseline 62/283, +2 cases = T3 + T7)
[minimal_dgpu_soc]  24 / 170       (baseline 21/145, +3 cases = T3/T5/T7)
[pcie-memory]       17 / 48        (0 回归)
[dgpu_soc_timing]   37 / 12024     (0 回归, Inv-6 契约重联见 Deviation)
[driver_visible]    4 / 40         (0 回归)
[realip]            51 / 366       (0 回归)
[sdma]              68 / 12945     (baseline 67/12941 +1 case, 0 回归)

# docs_sync_check --strict: exit 0
# frozen headers (pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh / cpptlm_emulator.h): 0 diff
# 23 ABI (include/abi/): 0 diff
# KPI: 3 (Change C 未 archive)

# Gap grep 对比
Gap I — configs/dgpu_soc_minimal_v1.json "gmmu.*pcie_memory": 0 处 (baseline 1 处)
Gap I — gmmu->set_vram_target(vram_ctrl) 装配: dgpu_board_shell.cc:796 (functional-only)
Gap G2 — configs/dgpu_soc_minimal_v1.json soc_noc 节点 (SocInternalNoc): 存在
Gap G1 — src/tlm/gpu/dgpu_board_shell.cc "set_translate_cb": 0 处 (baseline 1 处)
```

## 6. Deviation

1. **`T3/T4 接线为 programmatic 而非 JSON ChStream connection`**: 设计文档 v0.5.2 的 JSON 片段示意 `gmmu.0 → vram_ctrl.0`，但 `GMMU req_out_` 是 `PcieTlpBundle`，而 `VramControllerTLM`（继承 `MemoryTLM`）端口是 `CacheReqBundle`，类型不兼容，JSON ChStream 连接会被 StreamAdapter 层拒绝。实际采用 design.md v0.5.1 明确的 **programmatic bridge**（`set_vram_target`），与 `fabric↔vram_ctrl` 既有装配模式一致。`soc_noc` 同理（SimModule，accept_fn callback 机制，无 ChStream 端口）。

2. **`translate()` 优先级调整**: board 无条件注入 `gmmu_->set_mem_view()`，原 `translate()` 先查 `mem_view_`，会使 `vram_ctrl_` dedicated-client 路径永不触发。已将 `vram_ctrl_` 检查前置于 `mem_view_`（functional-only 装配，timing-mode 不注入 vram_ctrl_，保持 `mem_view_`/`translate_timing` TLB 路径 → H-C6）。

3. **`Inv-6` timing 测试重联** (`test_dgpu_soc_timing_inv.cc`): 原断言 `sdma->get_translate_cb() != nullptr` 验证旧 lambda 注入契约。Gap G1 后 board 改注入 `translate_target_`，该断言 stale。更新为 `sdma->translate_target() == gmmu`（spec 授权的 "既有测试重联"）。此为 Phase 4 timing-mvp 测试，非 Phase 0 产物。

4. **`set_translate_cb` API 保留**: 未删除 `SdmaEngineTLM::set_translate_cb`（8 个既有 `[sdma]` 单元测试依赖 fake translate_cb）。Gap G1 仅移除 **board 的 lambda 注入**，改走 `translate_target_`（Gap G1 = 消除 board 侧 callback 注入，符合 §0.1 原则 3.1）。
