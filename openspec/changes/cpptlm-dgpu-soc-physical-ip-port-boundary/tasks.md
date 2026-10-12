# Tasks — cpptlm-dgpu-soc-physical-ip-port-boundary (Phase 0+ 重构级, TDD 5-step)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/dgpu-soc-physical-ip-port-boundary/spec.md`](specs/dgpu-soc-physical-ip-port-boundary/spec.md)
> **目标 tag**: `cpptlm-dgpu-soc-physical-ip-port-boundary-completion`
> **估时**: **14-22d** (T1-T5 各模块切型 + T6 adapter + T7 0 回归 + T8 实施笔记)
> **依赖**: 全部 archived changes 已是 ✅

---

## 总体时间线 (5 阶段 + 2 协同, ~18d 中位数)

| 阶段 | 内容 | 估时 |
|------|------|:----:|
| Phase 1 | TDD RED (15 failing cases) | 2d |
| Phase 2 | GREEN: 4 个 adapter + 5 模块端口切型 | 10-14d |
| Phase 3 | GREEN: SocInternalNoc + GpuInternalFabric ChStream 化 | 3-5d |
| Phase 4 | 0 回归锁 + E2E 直连 case | 2-3d |
| Phase 5 | 文档 + archive | 1d |

---

## Phase 1: TDD RED — 15 failing cases (2d)

### Task 1.1: Write failing tests for 4 个 adapter (RED)

**Files**: Create `test/test_port_typing_adapters.cc`

- [ ] T1: `SDMA_PcieTlp_To_NocAdapter` 双向 — PcieTlpBundle(mem_write) → NoCFlitBundle(REQ) 字段映射
- [ ] T2: `SDMA_Noc_To_PcieTlpAdapter` 双向 — NoCFlitBundle(RESP) → PcieTlpBundle(MEM_READ_RESP) 字段映射
- [ ] T3: `VramCtrl_Noc_To_AxiAdapter` 双向 — NoCFlitBundle(REQ) → CacheReqBundle 字段映射
- [ ] T4: `VramCtrl_Axi_To_NocAdapter` 双向 — CacheRespBundle → NoCFlitBundle(RESP) 字段映射
- [ ] T5: edge cases — empty payload / max size / multi-beat split (per AXI4-like multi-beat)
- [ ] T6: bundle 字段宽度锁 (per BUNDLE_FIELD_WIDTH 编译期 assert)

**验证**: `cmake --build build` RED
**Commit**: `test(port-typing): RED 6 cases 4 个 adapter (Phase 1.1)`

### Task 1.2: Write failing tests for 5 模块端口切型 (RED)

**Files**: Create `test/test_dgpu_soc_port_typing.cc`

- [ ] T7: SdmaEngineTLM 端口类型断言 (mem_in/mem_out = NoCFlitBundle, desc_in/host_out/done_out = PcieTlpBundle)
- [ ] T8: VramControllerTLM 端口类型断言 (req_* = NoCFlitBundle, 内部 CacheReqBundle)
- [ ] T9: GmmuTLM 端口类型断言 (req_out_/resp_in_ = NoCFlitBundle)
- [ ] T10: GpuInternalFabric ChStream 化断言 (NUM_PORTS > 0 + set_stream_adapter override)
- [ ] T11: SocInternalNoc ChStream 化断言 (NUM_PORTS > 0 + set_stream_adapter override)

**验证**: `cmake --build build` RED
**Commit**: `test(port-typing): RED 5 cases 5 模块 (Phase 1.2)`

### Task 1.3: Write failing E2E 直连 case (RED)

**Files**: `test/test_minimal_dgpu_soc_e2e.cc` (append)

- [ ] T12: `[minimal_dgpu_soc][noc-direct]` SDMA H2D 经 `sdma.2 → soc_noc.0 → soc_noc.1 → vram_ctrl.0` 直连 (无 PcieMemoryDevice 中介)
- [ ] T13: `[minimal_dgpu_soc][gmmu-direct]` GMMU PTE fetch 经 `gmmu.req_out → soc_noc.3 → soc_noc.1 → vram_ctrl.0` 直连 (替代 programmatic bridge)
- [ ] T14: `[minimal_dgpu_soc][fabric-chstream]` `fabric.0 → soc_noc.2` JSON 直连走通
- [ ] T15: `[minimal_dgpu_soc][vram-noc-bridge]` VramController 内部 CacheReqBundle ↔ 上游 NoCFlitBundle 双向转换

**验证**: `cmake --build build` RED
**Commit**: `test(minimal-dgpu-soc): RED 4 cases E2E 直连 (Phase 1.3)`

---

## Phase 2: GREEN — 5 模块端口切型 (10-14d)

### Task 2.1: SdmaEngineTLM 端口切型 (3-4d)

**Files**: `include/tlm/gpu/sdma_engine_tlm.{hh,cc}` + `include/bundles/noc_bundles_tlm.hh` (确认 16 字段)

- [ ] Step 1: 加 `NoCFlitBundle` 端口字段 (PORT_MEM_IN + PORT_MEM_OUT, port 1 + 2)
- [ ] Step 2: 保留 PcieTlpBundle 端口字段 (PORT_DESC_IN + PORT_HOST_OUT + PORT_DONE_OUT, port 0 + 3 + 4)
- [ ] Step 3: 内部转换函数 `pcie_tlp_to_noc_flit()` (port 2 出口) + `noc_flit_to_pcie_tlp()` (port 1 入口)
- [ ] Step 4: 改 `set_stream_adapter()` overload — MultiPort adapter 类型参数化
- [ ] Step 5: 单元测试 T1-T2 GREEN
- [ ] Step 6: 回归 [sdma] 68 cases 0 失败
- [ ] Step 7: commit: `feat(sdma): 端口切型 3 端口 NoC + 2 端口 PCIe (Phase 2.1)`

### Task 2.2: VramControllerTLM 端口切型 (3-4d)

**Files**: `include/tlm/vram_controller_tlm.{hh,cc}`

- [ ] Step 1: 上游 `req_in_/req_out_/resp_in_/resp_out_` 改 `NoCFlitBundle` (替换继承的 CacheReqBundle/CacheRespBundle)
- [ ] Step 2: 内部 `req_internal/resp_internal` 保留 `CacheReqBundle/CacheRespBundle` (MemoryTLM 协议真实)
- [ ] Step 3: 内部转换函数 `noc_flit_to_cache_req()` + `cache_resp_to_noc_flit()`
- [ ] Step 4: 改 `set_stream_adapter()` override
- [ ] Step 5: 保留 `handle_functional_request()` (per Change A) + `set_backing_view()` (per ADR-DGPU-10) + `set_mode()` (per ADR-DGPU-11)
- [ ] Step 6: 单元测试 T3-T4 GREEN
- [ ] Step 7: 回归 [pcie-memory] 17 cases + [vram_controller] (新) 0 失败
- [ ] Step 8: commit: `feat(vram-controller): 上游 NoC 化 (Phase 2.2)`

### Task 2.3: GmmuTLM 端口切型 (1-2d)

**Files**: `include/tlm/gpu/gmmu_tlm.{hh,cc}`

- [ ] Step 1: `req_out_` 改 `NoCFlitBundle` (PTE fetch 出口)
- [ ] Step 2: `resp_in_` 改 `NoCFlitBundle` (PTE fetch 响应入口)
- [ ] Step 3: 保留 `mem_view_/translate_timing` (timing-mode H-C6) + `translate_target_` (Gap G1 修复)
- [ ] Step 4: 内部 `translate()` 走 `vram_ctrl_->handle_functional_request()` (per Change C T2)
- [ ] Step 5: 单元测试 T9 GREEN
- [ ] Step 6: 回归 [gmmu] 25 cases 0 失败
- [ ] Step 7: commit: `feat(gmmu): 端口切型 NoC 化 (Phase 2.3)`

### Task 2.4: 4 个新 adapter 实施 (3-5d, 与 2.1-2.3 并行)

**Files**: Create `include/framework/{sdma,vram_ctrl}_noc_pcie_adapter.{hh,cc}` (4 文件)

- [ ] Step 1: `SDMA_PcieTlp_To_NocAdapter` 实现 + `set_stream_adapter()` override
- [ ] Step 2: `SDMA_Noc_To_PcieTlpAdapter` 实现
- [ ] Step 3: `VramCtrl_Noc_To_AxiAdapter` 实现
- [ ] Step 4: `VramCtrl_Axi_To_NocAdapter` 实现
- [ ] Step 5: 注册到 `ChStreamAdapterFactory` (per `include/chstream_register.hh`)
- [ ] Step 6: 单元测试 T1-T4 + T5-T6 GREEN
- [ ] Step 7: commit: `feat(adapters): 4 个 wire-format adapter (Phase 2.4)`

### Task 2.5: 单元测试 T7-T8 GREEN (与 2.1-2.4 并行)

- [ ] SdmaEngineTLM 端口类型断言 (T7)
- [ ] VramControllerTLM 端口类型断言 (T8)
- [ ] GmmuTLM 端口类型断言 (T9)
- [ ] commit: `test(port-typing): GREEN 3 cases 3 模块 (Phase 2.5)`

---

## Phase 3: GREEN — SimModule ChStream 化 (3-5d)

### Task 3.1: GpuInternalFabric ChStream 化 (2-3d)

**Files**: `include/tlm/cluster/gpu_internal_fabric.{hh,cc}`

- [ ] Step 1: 加 4 组 ChStreamPort 字段 (req_in + req_out + resp_in + resp_out)
- [ ] Step 2: `Fabric2VramCtrlBundle` 8 字段**保留** (内部 IP-间 wire, 不动 BUNDLE_FIELD_WIDTH)
- [ ] Step 3: `set_stream_adapter()` override (单端口 + 多端口 overload)
- [ ] Step 4: `num_ports()` override → 4
- [ ] Step 5: 取消 `set_vram_target/set_fabric_source` programmatic bridge (保留作 fallback)
- [ ] Step 6: 单元测试 T10 GREEN
- [ ] Step 7: 回归 [realip][fabric] 0 失败
- [ ] Step 8: commit: `feat(fabric): ChStream 化 (Phase 3.1)`

### Task 3.2: SocInternalNoc ChStream 化 (3-4d)

**Files**: `include/tlm/cluster/soc_internal_noc.{hh,cc}`

- [ ] Step 1: 加 N 组 ChStreamPort 字段 (NoCFlitBundle 类型, 与内部 mesh 协议一致)
- [ ] Step 2: 保留 `route_flit()` mesh 路由 + `send_request/response` API + `vc_counts_[4]` + `flits_dispatched_`
- [ ] Step 3: `set_stream_adapter()` override
- [ ] Step 4: `num_ports()` override → NUM_PORTS
- [ ] Step 5: 取消 4 个 `set_*_target` programmatic bridge (保留作 fallback)
- [ ] Step 6: 单元测试 T11 GREEN
- [ ] Step 7: 回归 [noc] 64 cases 0 失败
- [ ] Step 8: commit: `feat(soc-noc): ChStream 化 (Phase 3.2)`

### Task 3.3: 单元测试 T10-T11 GREEN (与 3.1-3.2 并行)

- [ ] GpuInternalFabric ChStream 化断言 (T10)
- [ ] SocInternalNoc ChStream 化断言 (T11)
- [ ] commit: `test(port-typing): GREEN 2 cases 2 SimModule (Phase 3.3)`

---

## Phase 4: 0 回归锁 + E2E 直连 case (2-3d)

### Task 4.1: JSON 拓扑补全

**Files**: `configs/dgpu_soc_minimal_v1.json` (替换 1 条 connection → 4-5 条)

- [ ] Step 1: 删除 `sdma.2 → pcie_memory.0`
- [ ] Step 2: 加 `sdma.2 → soc_noc.0`
- [ ] Step 3: 加 `soc_noc.1 → vram_ctrl.0`
- [ ] Step 4: 加 `fabric.0 → soc_noc.2`
- [ ] Step 5: 加 `gmmu.0 → soc_noc.3` (GMMU dedicated client)
- [ ] Step 6: 加 `soc_noc.4 → pcie_memory.0` (backward compat 保留, PcieMemoryDevice 作 PCIe 外观层)

### Task 4.2: DGpuBoard::bind_memory_backings 简化

**Files**: `src/tlm/gpu/dgpu_board_shell.cc`

- [ ] Step 1: 删除 4 个 `soc_noc->set_*_target()` 调用
- [ ] Step 2: 删除 `fabric->set_vram_target()` + `vram_ctrl->set_fabric_source()` 调用
- [ ] Step 3: 保留 `pcie_mem->set_vram_controller()` (PcieMemoryDevice 仍作 PCIe 外观层)
- [ ] Step 4: 保留 `gmmu_->set_vram_target()` (programmatic fallback, Gap I 兜底)
- [ ] Step 5: 验证 `bind_memory_backings()` 从 ~30 行简化到 ~5 行

### Task 4.3: E2E GREEN case

- [ ] T12: SDMA H2D 经 `sdma.2 → soc_noc.0 → soc_noc.1 → vram_ctrl.0` 直连
- [ ] T13: GMMU PTE fetch 经 `gmmu.req_out → soc_noc.3 → soc_noc.1 → vram_ctrl.0` 直连
- [ ] T14: `fabric.0 → soc_noc.2` JSON 直连
- [ ] T15: VramController 内部 CacheReqBundle ↔ 上游 NoCFlitBundle 双向转换
- [ ] commit: `test(minimal-dgpu-soc): GREEN 4 cases E2E 直连 (Phase 4.3)`

### Task 4.4: 0 回归锁

- [ ] `[pcie]` 430/36672 0 回归
- [ ] `[gmmu]` 25/5273 0 回归
- [ ] `[noc]` 64/304 0 回归
- [ ] `[sdma]` 68/12945 0 回归
- [ ] `[minimal_dgpu_soc]` 24→28/170→190 (新增 4 cases) 0 回归
- [ ] `[dgpu_soc_timing]` 37/12024 0 回归 (timing-mode 不动)
- [ ] `[driver_visible]` 4/40 0 回归
- [ ] `[pcie-memory]` 17/48 0 回归
- [ ] `[port-typing]` 6 cases / 30 assertions 全过
- [ ] `[soc_noc][chstream]` 4 cases / 20 assertions 全过
- [ ] `[gpu_fabric][chstream]` 4 cases / 20 assertions 全过
- [ ] `[sdma][noc-bridge]` 3 cases / 15 assertions 全过
- [ ] `[vram][noc-bridge]` 3 cases / 15 assertions 全过
- [ ] `[gmmu][noc-bridge]` 2 cases / 10 assertions 全过

### Task 4.5: frozen headers + 23 ABI 0 diff 锁

- [ ] `git diff include/tlm/gpu/pcie_endpoint_tlm.h` → 0 行
- [ ] `git diff include/tlm/gpu/pcie_display_device.hh` → 0 行
- [ ] `git diff include/bundles/pcie_bundles_tlm.hh` → 0 行
- [ ] `git diff include/abi/cpptlm_emulator.h` → 0 行
- [ ] 23 ABI 函数签名 0 diff

### Task 4.6: docs_sync_check --strict

- [ ] `scripts/test/docs_sync_check.sh --strict` → exit 0
- [ ] 同步更新 `AGENTS.md` 路径(本 change 已修订)
- [ ] commit: `chore(docs): sync post-change 验证 (Phase 4.6)`

---

## Phase 5: 文档 + archive (1d)

### Task 5.1: 实施笔记

**Files**: `docs/implementation/2026-XX-XX-dgpu-soc-physical-ip-port-boundary.md`

- [ ] Step 1: Pre-flight 验证 (Gap I + D3 演进状态 + SocInternalNoc 既有 + Change A/B/C 装配记录)
- [ ] Step 2: Post-flight 验证 (commit 表 + 0 回归锁 + frozen headers 0 diff)
- [ ] Step 3: Deviation 记录 (per design.md §6 5 处 deviation 实证)
- [ ] Step 4: commit: `docs(implementation): T-X — 实施笔记 + post-flight 验证 (Phase 5.1)`

### Task 5.2: spec 落盘 + archive

- [ ] `openspec/specs/dgpu-soc-physical-ip-port-boundary/spec.md` 已起草 (5 ADDED Requirements)
- [ ] 同步 spec 修改: `sdma-engine-tlm` / `vram-controller-functional-mode` / `gmmu-pte-wf` / `soc-internal-noc`
- [ ] 提交: `openspec archive cpptlm-dgpu-soc-physical-ip-port-boundary`
- [ ] 跟踪 proposal.md 从 index 移除
- [ ] KPI: 1 slot 释放

### Task 5.3: ADR Status Update

- [ ] ADR-DGPU-06 (axi-mem-bundle-boundary) Status Update 2026-10-12 追加, 标记"已落地"
- [ ] commit: `docs(adr): ADR-DGPU-06 Status Update 落地 (Phase 5.3)`

---

## 验证清单 (T-X 完整实施后)

- [ ] 0 回归锁: 8 标签 1100+ cases 0 失败
- [ ] frozen headers 0 diff + 23 ABI 0 变更
- [ ] JSON 拓扑 1 条 → 4-5 条
- [ ] DGpuBoard::bind_memory_backings() 简化 ~30 行 → ~5 行
- [ ] 5 模块端口切型完成 (SDMA + VramCtrl + Gmmu + Fabric + SocNoc)
- [ ] 4 个新 adapter 实施 + 注册
- [ ] 实施笔记完整 + post-flight 验证
- [ ] spec 落盘 + 4 个关联 spec 同步
- [ ] ADR-DGPU-06 Status Update 追加
- [ ] 1 KPI slot 释放 (给 Change F backlog)
- [ ] 0 frozen headers 改动 (per ADR-DGPU-08 §2.3 严格遵守)
