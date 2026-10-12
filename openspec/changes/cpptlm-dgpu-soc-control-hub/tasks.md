# Tasks — cpptlm-dgpu-soc-control-hub (P0 架构级, TDD 5-step)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/dgpu-soc-control-hub/spec.md`](specs/dgpu-soc-control-hub/spec.md)
> **目标 tag**: `cpptlm-dgpu-soc-control-hub-completion`
> **估时**: **21-30d** (T1-T5 改造 + T6 adapter + T7 0 回归 + T8 实施笔记)
> **依赖**: 全部 archived changes 已是 ✅

---

## 总体时间线 (8 sub-task, ~25d 中位数)

| 阶段 | 内容 | 估时 |
|------|------|:----:|
| Phase 1 | TDD RED (20 failing cases) | 3d |
| Phase 2 | GREEN: Chub + SdmaEngineTLM 切型 | 10-14d |
| Phase 3 | GREEN: GmmuTLM + VramControllerTLM 切型 | 5-7d |
| Phase 4 | PcieMemoryDevice 移除 + 0 回归锁 | 3-5d |
| Phase 5 | 文档 + archive | 1d |

---

## Phase 1: TDD RED — 20 failing cases (3d)

### Task 1.1: Write failing tests for Chub (RED)

**Files**: Create `test/test_control_hub.cc`

- [ ] T1: Chub 4 BAR decode 路由 (BAR0 → CrBundle, BAR1 → DoorbellBundle, BAR2 → NoCFlitBundle, BAR3 → Unknown)
- [ ] T2: Chub `axi_slave_in` 接收 + `route_bar0_cr()` 转换 Axi4 → CrBundle
- [ ] T3: Chub `doorbell_out` 发 DoorbellBundle (BAR1, per 设计)
- [ ] T4: Chub `noc_master_out` 发 NoCFlitBundle (BAR2, per 设计)
- [ ] T5: Chub `msix_in` 接收 MsixBundle (Phase 2 准备)
- [ ] T6: Chub edge cases — BAR boundary / offset 越界 / 未对齐

**验证**: `cmake --build build` RED
**Commit**: `test(chub): RED 6 cases (Phase 1.1)`

### Task 1.2: Write failing tests for TranslateReqBundle + TranslateRespBundle (RED)

**Files**: Create `test/test_translate_bundles.cc`

- [ ] T7: TranslateReqBundle 字段序列化 (iova/size/access_type/task_id/request_id)
- [ ] T8: TranslateRespBundle 字段序列化 (paddr/status/task_id/request_id)
- [ ] T9: TranslateReq → TranslateResp 字段映射 (task_id/request_id 透传)
- [ ] T10: bundle 字段宽度锁 (BUNDLE_FIELD_WIDTH 编译期 assert)
- [ ] T11: 错误码语义 (OK=0 / PERMISSION_FAULT=1 / PAGE_FAULT=2)

**验证**: `cmake --build build` RED
**Commit**: `test(translate): RED 5 cases (Phase 1.2)`

### Task 1.3: Write failing tests for SdmaEngineTLM 8 端口切型 (RED)

**Files**: `test/test_sdma_engine_tlm.cc` (append) + `test/test_minimal_dgpu_soc_e2e.cc` (append)

- [ ] T12: SdmaEngineTLM 8 端口类型断言 (axi_master_out + mem_in/out + translate_req/resp + doorbell_in)
- [ ] T13: SDMA translate 直连 (JSON `sdma.translate_req → gmmu.translate_in` 走通, 无 programmatic bridge)
- [ ] T14: SDMA H2D bulk DMA 经 `sdma.mem_out → soc_noc.1 → soc_noc.3 → vram_ctrl.req_in` (替代 PcieMemoryDevice 路径)

**验证**: `cmake --build build` RED
**Commit**: `test(sdma): RED 3 cases 8 端口切型 (Phase 1.3)`

### Task 1.4: Write failing tests for GmmuTLM 5 端口切型 (RED)

**Files**: `test/test_gmmu_tlm.cc` (append) + `test/test_minimal_dgpu_soc_e2e.cc` (append)

- [ ] T15: GmmuTLM 5 端口类型断言 (translate_in/resp + req_out/resp_in + cr_in)
- [ ] T16: GMMU PTE fetch 经 `gmmu.req_out → soc_noc.2 → soc_noc.3 → vram_ctrl.req_in` (替代 programmatic bridge)
- [ ] T17: GMMU translate slave (SDMA→GMMU 直连, TranslateReq → TranslateResp)

**验证**: `cmake --build build` RED
**Commit**: `test(gmmu): RED 3 cases 5 端口切型 (Phase 1.4)`

### Task 1.5: Write failing E2E BAR2 直连 case (RED)

**Files**: `test/test_minimal_dgpu_soc_e2e.cc` (append) + 新 `test/test_pcie_bar2.cc` (替换 [pcie-memory])

- [ ] T18: `[pcie][bar2]` driver BAR2 写 vram 路径 (per 提议 5, 经 Chub→NoC→VramController)
- [ ] T19: `[soc_noc][bar2]` BAR2 经 SoC NoC 路径
- [ ] T20: `[chub][bar2]` Chub BAR2 decode + 路由
- [ ] T21: `[pcie-memory] → [pcie][bar2]` 迁移 case 验证 8 cases 全过 (替代旧 17 cases)

**验证**: `cmake --build build` RED
**Commit**: `test(bar2): RED 4 cases E2E BAR2 直连 (Phase 1.5)`

---

## Phase 2: GREEN — Chub + SdmaEngineTLM (10-14d)

### Task 2.1: Chub 模块新建 (5-7d)

**Files**: `include/tlm/cluster/control_hub.{hh,cc}` + `src/tlm/cluster/control_hub.cc`

- [ ] Step 1: 头文件 + 类骨架 (基类 SimModule, 6 端口常量)
- [ ] Step 2: 6 类 ChStreamPort 字段 (axi_slave_in + cfg_slave_in + cr_master_out[] + doorbell_out + noc_master_out + msix_in)
- [ ] Step 3: `set_stream_adapter()` override (单端口 + 多端口 overload)
- [ ] Step 4: `num_ports() const override { return 6; }`
- [ ] Step 5: 4 BAR decode 路由 (per design.md §2.3)
- [ ] Step 6: 3 类内部转换 adapter (Chub_Axi4_To_Cr / _To_Doorbell / _To_Noc)
- [ ] Step 7: `tick()` 实现 + 状态机
- [ ] Step 8: `modules_cluster.hh` 加 `REGISTER_MODULE(ControlHub)`
- [ ] Step 9: 单元测试 T1-T6 GREEN
- [ ] Step 10: 回归 [pcie] 430 cases 0 失败
- [ ] Step 11: commit: `feat(chub): 新建 ControlHub 模块 6 端口 + 4 BAR decode (Phase 2.1)`

### Task 2.2: TranslateReqBundle + TranslateRespBundle 新建 (1-2d)

**Files**: `include/bundles/translate_bundles_tlm.hh`

- [ ] Step 1: TranslateReqBundle 5 字段定义 (iova[64] + size[32] + access_type[8] + task_id[8] + request_id[16])
- [ ] Step 2: TranslateRespBundle 4 字段定义 (paddr[64] + status[8] + task_id[8] + request_id[16])
- [ ] Step 3: 编译期 `BUNDLE_FIELD_WIDTH` 锁 (per `include/utils/port_contract_asserts.hh`)
- [ ] Step 4: 错误码常量 (OK=0 / PERMISSION_FAULT=1 / PAGE_FAULT=2)
- [ ] Step 5: 单元测试 T7-T11 GREEN
- [ ] Step 6: commit: `feat(bundles): TranslateReqBundle + TranslateRespBundle 新建 (Phase 2.2)`

### Task 2.3: SdmaEngineTLM 端口切型 5 → 8 端口 (3-4d)

**Files**: `include/tlm/gpu/sdma_engine_tlm.{hh,cc}`

- [ ] Step 1: 删除 desc_in/host_out/done_out 旧 PcieTlpBundle 端口
- [ ] Step 2: 加 `axi_master_out` (Axi4Bundle, port 0)
- [ ] Step 3: mem_in/mem_out 改 NoCFlitBundle (per physical-ip-port-boundary)
- [ ] Step 4: 加 translate_req/resp (TranslateReqBundle/RespBundle, port 3/4)
- [ ] Step 5: 加 doorbell_in (DoorbellBundle, port 5)
- [ ] Step 6: 改 `set_stream_adapter()` overload — 8 端口 MultiPort adapter
- [ ] Step 7: 内部状态机改造 (per design.md §3.3)
- [ ] Step 8: 删除 `set_translate_cb`/`set_translate_target` programmatic API (保留 deprecated 标记)
- [ ] Step 9: 单元测试 T12-T14 GREEN
- [ ] Step 10: 回归 [sdma] 68 cases 0 失败
- [ ] Step 11: commit: `feat(sdma): 端口切型 5 → 8 (AXI + NoC + translate + doorbell) (Phase 2.3)`

### Task 2.4: 4 个新 wire-format adapter 实施 (3-5d, 与 2.1-2.3 并行)

**Files**: Create `include/framework/{sdma,vram_ctrl,chub}_noc_pcie_adapter.{hh,cc}` (6 文件)

- [ ] Step 1: `SDMA_Axi_Master_Adapter` (Axi4Bundle ↔ DMA descriptor, axi_master_out 端口)
- [ ] Step 2: `VramCtrl_Noc_To_AxiAdapter` (NoCFlitBundle → CacheReqBundle, VramController 入口侧)
- [ ] Step 3: `VramCtrl_Axi_To_NocAdapter` (CacheRespBundle → NoCFlitBundle, VramController 出口侧)
- [ ] Step 4: `Chub_Axi4_To_CrAdapter` (Axi4Bundle → CrBundle, Chub BAR0 内部)
- [ ] Step 5: `Chub_Axi4_To_DoorbellAdapter` (Axi4Bundle → DoorbellBundle, Chub BAR1 内部)
- [ ] Step 6: `Chub_Axi4_To_NocAdapter` (Axi4Bundle → NoCFlitBundle, Chub BAR2 内部)
- [ ] Step 7: 注册到 `ChStreamAdapterFactory`
- [ ] Step 8: 单元测试 GREEN
- [ ] Step 9: commit: `feat(adapters): 4 + 3 个 wire-format adapter (Phase 2.4)`

### Task 2.5: 单元测试 T1-T14 GREEN (与 2.1-2.4 并行)

- [ ] T1-T6 (Chub): GREEN
- [ ] T7-T11 (TranslateBundle): GREEN
- [ ] T12-T14 (SDMA): GREEN
- [ ] commit: `test(chub+sdma+translate): GREEN 14 cases (Phase 2.5)`

---

## Phase 3: GREEN — GmmuTLM + VramControllerTLM (5-7d)

### Task 3.1: GmmuTLM 端口切型 2 → 5 端口 (1-2d)

**Files**: `include/tlm/gpu/gmmu_tlm.{hh,cc}`

- [ ] Step 1: 加 translate_in/resp (TranslateReqBundle/RespBundle, port 0/1)
- [ ] Step 2: req_out/resp_in 改 NoCFlitBundle (per physical-ip-port-boundary)
- [ ] Step 3: 加 cr_in (CrBundle, port 4)
- [ ] Step 4: 改 `set_stream_adapter()` overload — 5 端口 MultiPort adapter
- [ ] Step 5: 内部 translate() 状态机改造 (per design.md §4.2)
- [ ] Step 6: 保留 `mem_view_/translate_timing` (timing-mode H-C6)
- [ ] Step 7: 删除 `set_translate_target/set_vram_target` programmatic API (保留 deprecated 标记)
- [ ] Step 8: 单元测试 T15-T17 GREEN
- [ ] Step 9: 回归 [gmmu] 25 cases 0 失败
- [ ] Step 10: commit: `feat(gmmu): 端口切型 2 → 5 (translate + NoC + CrBundle) (Phase 3.1)`

### Task 3.2: VramControllerTLM 端口切型 (3-4d)

**Files**: `include/tlm/vram_controller_tlm.{hh,cc}`

- [ ] Step 1: 上游 `req_in_/req_out_/resp_in_/resp_out_` 改 `NoCFlitBundle` (替换继承的 CacheReqBundle/CacheRespBundle)
- [ ] Step 2: 内部 `req_internal/resp_internal` 保留 `CacheReqBundle/CacheRespBundle` (MemoryTLM 协议真实)
- [ ] Step 3: 内部转换函数 `noc_flit_to_cache_req()` + `cache_resp_to_noc_flit()`
- [ ] Step 4: 改 `set_stream_adapter()` override
- [ ] Step 5: 保留 `handle_functional_request()` + `set_backing_view()` + `set_mode()`
- [ ] Step 6: 单元测试 GREEN
- [ ] Step 7: 回归 [pcie-memory] 17 cases 0 失败 (本 Phase 暂不迁移, Phase 4 移除 PcieMemoryDevice 时迁移)
- [ ] Step 8: 回归 [dgpu_soc_timing] 37 cases 0 失败 (timing-mode 不动)
- [ ] Step 9: commit: `feat(vram-controller): 上游 NoC 化 (Phase 3.2)`

### Task 3.3: SocInternalNoc ChStream 化 (per physical-ip-port-boundary, 1-2d)

**Files**: `include/tlm/cluster/soc_internal_noc.{hh,cc}`

- [ ] Step 1: 加 N 组 ChStreamPort 字段 (NoCFlitBundle 类型)
- [ ] Step 2: 保留 `route_flit()` mesh 路由 + `send_request/response` + `vc_counts_[4]` + `flits_dispatched_`
- [ ] Step 3: `set_stream_adapter()` override
- [ ] Step 4: `num_ports() const override` → NUM_PORTS
- [ ] Step 5: 取消 4 个 `set_*_target` programmatic bridge (保留作 fallback)
- [ ] Step 6: 单元测试 T18-T20 GREEN
- [ ] Step 7: 回归 [noc] 64 cases 0 失败
- [ ] Step 8: commit: `feat(soc-noc): ChStream 化 (Phase 3.3)`

### Task 3.4: 单元测试 T15-T17 GREEN (与 3.1-3.3 并行)

- [ ] T15-T17 (GMMU): GREEN
- [ ] commit: `test(gmmu): GREEN 3 cases (Phase 3.4)`

---

## Phase 4: PcieMemoryDevice 移除 + 0 回归锁 (3-5d)

### Task 4.1: PcieMemoryDevice 模块完全删除 (1d)

**Files**: `include/tlm/gpu/pcie_memory_device.{hh,cc}` + `src/tlm/gpu/pcie_memory_device.cc` + 相关引用

- [ ] Step 1: 删除头文件 + 实现文件
- [ ] Step 2: 删除 `chstream_register.hh` 中 `PcieMemoryDevice` 注册
- [ ] Step 3: 删除 `dgpu_soc_minimal_v1.json` 中 `pcie_memory` 节点
- [ ] Step 4: 删除 `dgpu_board_shell.cc::bind_memory_backings()` 中 `pcie_mem->set_vram_controller()` 调用
- [ ] Step 5: 验证 driver BAR2 路径经 Chub→NoC→VRAM 走通
- [ ] Step 6: commit: `refactor: 移除 PcieMemoryDevice 模块 (Phase 4.1)`

### Task 4.2: [pcie-memory] 17 cases 迁移到 3 个新标签 (1-2d)

**Files**: `test/test_pcie_memory_device.cc` (拆分到多个新文件)

- [ ] Step 1: `[pcie][bar2]` 新建 8 cases (driver BAR2 路径)
- [ ] Step 2: `[soc_noc][bar2]` 新建 4 cases (NoC 路径)
- [ ] Step 3: `[chub][bar2]` 新建 5 cases (Chub BAR2 decode)
- [ ] Step 4: 删除 `test/test_pcie_memory_device.cc` (旧 17 cases)
- [ ] Step 5: 验证 17 cases 拆分后 0 回归
- [ ] Step 6: commit: `test(bar2): [pcie-memory] 迁移到 3 个新标签 (Phase 4.2)`

### Task 4.3: 13 条 JSON connection E2E 直连 (1-2d)

**Files**: `configs/dgpu_soc_minimal_v1.json` (替换 1 条 connection → 13 条)

- [ ] Step 1: 删除 `pcie_memory` 节点 + 旧 `sdma.2 → pcie_memory.0` connection
- [ ] Step 2: 加 13 条新 connection (per design.md §7.1)
- [ ] Step 3: 加 `chub` 模块声明
- [ ] Step 4: 单元测试 T18-T21 GREEN
- [ ] Step 5: 验证 ModuleFactory::instantiateAll 解析 13 条 connection 成功
- [ ] Step 6: commit: `feat(config): 13 条 JSON connection 完整拓扑 (Phase 4.3)`

### Task 4.4: 0 回归锁

- [ ] `[pcie]` 430/36672 0 回归
- [ ] `[gmmu]` 25/5273 0 回归
- [ ] `[noc]` 64/304 0 回归
- [ ] `[sdma]` 68/12945 0 回归
- [ ] `[minimal_dgpu_soc]` 24→32/170→220 (新增 8 E2E 直连 cases) 0 回归
- [ ] `[dgpu_soc_timing]` 37/12024 0 回归 (timing-mode 不动)
- [ ] `[driver_visible]` 4/40 0 回归
- [ ] `[pcie-memory]` 删除 (迁移到 [pcie][bar2] + [soc_noc][bar2] + [chub][bar2])
- [ ] `[pcie-ep-soc-bridge]` 8 cases 0 回归 (per p2-integration-unblock)
- [ ] `[control_hub]` 6+ cases / 30+ assertions 全过
- [ ] `[translate]` 4+ cases / 20+ assertions 全过
- [ ] `[port-typing]` 6 cases / 30 assertions 全过
- [ ] `[soc_noc][chstream]` 4 cases / 20 assertions 全过
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
- [ ] 同步更新 `AGENTS.md` 路径 (本 change 已修订)
- [ ] commit: `chore(docs): sync post-change 验证 (Phase 4.6)`

### Task 4.7: DGpuBoard::bind_memory_backings 简化

**Files**: `src/tlm/gpu/dgpu_board_shell.cc`

- [ ] Step 1: 删除 4 个 `soc_noc->set_*_target()` 调用
- [ ] Step 2: 删除 `pcie_mem->set_vram_controller()` 调用
- [ ] Step 3: 删除 `fabric->set_vram_target()` + `vram_ctrl->set_fabric_source()` 调用
- [ ] Step 4: 删除 `gmmu_->set_vram_target()` 调用
- [ ] Step 5: 保留 `vram_ctrl->set_backing_view()` (backdoor 装配, 必需)
- [ ] Step 6: 保留 LwcBusHub `register_*_target` (LWC 总线 programmatic, 保留)
- [ ] Step 7: 验证 `bind_memory_backings()` 从 ~30 行简化到 ~3 行
- [ ] Step 8: commit: `refactor(board): bind_memory_backings 简化 30 → 3 行 (Phase 4.7)`

---

## Phase 5: 文档 + archive (1d)

### Task 5.1: 实施笔记

**Files**: `docs/implementation/2026-XX-XX-dgpu-soc-control-hub.md`

- [ ] Step 1: Pre-flight 验证 (Gap L/M + D3 演进状态 + Chub 设计 + 13 条 JSON 拓扑 + 0 处 programmatic bridge)
- [ ] Step 2: Post-flight 验证 (commit 表 + 0 回归锁 + frozen headers 0 diff)
- [ ] Step 3: Deviation 记录 (per design.md §6 4 处 deviation 实证)
- [ ] Step 4: commit: `docs(implementation): T-X — 实施笔记 + post-flight 验证 (Phase 5.1)`

### Task 5.2: spec 落盘 + archive

- [ ] `openspec/specs/dgpu-soc-control-hub/spec.md` 已起草 (5 ADDED Requirements)
- [ ] 同步 spec 修改: `sdma-engine-tlm` / `gmmu-pte-wf` / `vram-controller-functional-mode` / `soc-internal-noc` / `lwc-bus-hub-mvp` / `dgpu-soc-physical-ip-port-boundary` (合并 superset)
- [ ] 提交: `openspec archive cpptlm-dgpu-soc-control-hub`
- [ ] 跟踪 proposal.md 从 index 移除
- [ ] 同步 archive `cpptlm-dgpu-soc-physical-ip-port-boundary` (per 范围超集关系)
- [ ] KPI: 1 slot 释放

### Task 5.3: ADR Status Update

- [ ] ADR-DGPU-05 (vram-storage-ownership): Status Update 2026-10-13 追加, 标记"PcieMemoryDevice 移除"
- [ ] ADR-DGPU-06 (axi-mem-bundle-boundary): Status Update 2026-10-13 追加, 标记"已落地"
- [ ] ADR-DGPU-09 (driver-visible-minimal-soc-scope): Status Update 2026-10-13 追加, 标记"BAR2 路径改走 Chub→NoC"
- [ ] commit: `docs(adr): ADR-DGPU-05/06/09 Status Update (Phase 5.3)`

---

## 验证清单 (T-X 完整实施后)

- [ ] 0 回归锁: 11+ 标签 1200+ cases 0 失败
- [ ] frozen headers 0 diff + 23 ABI 0 变更
- [ ] JSON 拓扑 1 条 → 13 条
- [ ] DGpuBoard::bind_memory_backings() 简化 ~30 行 → ~3 行
- [ ] 7 模块 (新增 Chub) + 0 PcieMemoryDevice
- [ ] 5 模块端口切型完成 (SDMA 8 + GMMU 5 + VramCtrl + SocNoc + Fabric)
- [ ] 4 + 3 个 wire-format adapter 实施 + 注册
- [ ] 2 个新 Bundle (TranslateReq/Resp) 新建
- [ ] [pcie-memory] 17 cases 全部迁移到 3 个新标签
- [ ] 实施笔记完整 + post-flight 验证
- [ ] spec 落盘 + 6 个关联 spec 同步
- [ ] 3 个 ADR Status Update 追加
- [ ] 1 KPI slot 释放 (给 Change F backlog)
- [ ] 0 frozen headers 改动 (per ADR-DGPU-08 §2.3 严格遵守)
- [ ] 0 programmatic bridge (提议 6 完全 JSON 化)
