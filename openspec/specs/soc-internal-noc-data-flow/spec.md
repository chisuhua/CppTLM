# soc-internal-noc-data-flow Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-soc-internal-noc-e1. Update Purpose after archive.
## Requirements
### Requirement: e1-datanoc-scope (v1.1 新增)

E.1 SHALL 仅承载 **DataNoC** (SDMA bulk DMA 路径), 显式排除 PT_BASE / doorbell / page walk 流量。

#### Scenario: DataNoC scope declaration (per AC-E1-6.1)

- **GIVEN** E.1 实施完成 (Phase 1-6 全绿)
- **THEN** DataNoC 仅承载 SDMA bulk DMA 流量: SDMA submit → SocInternalNoc → VRAM
- **AND** PT_BASE / doorbell 走 `DGpuBoard::mmio_read/write` 直调 (per `dgpu_board_shell.cc:467`), **不进 SocInternalNoc**
- **AND** GMMU page walk 走独立 PcieTlpBundle MasterPort (per `gmmu_tlm.hh:105-117`), **不进 SocInternalNoc**
- **AND** ConfigBus / TranslationBus / PTE_FETCH deferred to E.2 (multi-VC NoC in ArchForge)
- **NOTE**: per CRITICAL C1/C2 修订; E.1 不实现全 SoC NoC 观测

---

### Requirement: e1-soc-internal-noc-route-flit-actual-dispatch (v1.0 保留)

`SocInternalNoc::route_flit()` SHALL 实际推 flit 到 `target->accept(flit)` (per E0 stub 升级, per AC-E1-3.1).

#### Scenario: dst=VRAM 路由 + accept (per AC-E1-3.1)

- **GIVEN** soc_noc 已 `set_vram_target(&vram)`, vram 实现 `accept_noc_request(const NoCFlitBundle&)` 接口
- **AND** flit.dst_node = `NODE_ID_VRAM` (=3)
- **WHEN** `soc_noc.route_flit(flit)` 调用
- **THEN** vram 收到 flit via `vram->accept_noc_request(flit)`
- **AND** `soc_noc.flits_dispatched_()` 计数 +1 (v1.0 是 `flits_routed_`, v1.1 改为 `flits_dispatched_` 更准确)
- **NOTE**: **NODE_ID_PCIE 作为 dst 在 E.1 中本期无入向流量** (per v1.1 scope 缩减: PCIe→SoC 走 AXI4 路径). 保留 NODE_ID_PCIE=0 是为对称性 + E.2 重编号便利.

#### Scenario: nullptr target + 路由 → 0 crash (per AC-E1-3.2)

- **GIVEN** soc_noc 4 target 全 nullptr
- **WHEN** route_flit 触发 4 个 dst_node
- **THEN** 0 crash, `flits_dispatched_` 增 4
- **NOTE**: 路由骨架不检查 target 非空 (per E0 design §2.3 决策)

#### Scenario: 未知 dst_node → graceful skip (per AC-E1-3.3)

- **GIVEN** soc_noc 4 target 均 set
- **WHEN** `route_flit(flit_with_dst_99)` 调用
- **THEN** `unknown_dst_count_++`, 0 crash
- **AND** 4 target 都不收到 (per D.2 优雅降级)

---

### Requirement: e1-vc-counter (v1.1 新增, per MAJOR M2)

`SocInternalNoc` SHALL 维护 `vc_counts_[4]` 计数器按 `flit_category` 分桶 (per AC-E1-3.5, per ARM CHI 4 VC 物理对应).

#### Scenario: VC 计数器按 flit_category 分桶 (per AC-E1-3.5)

- **GIVEN** soc_noc 4 target 已 set
- **WHEN** route_flit 触发 4 个不同 `flit_category`:
  - REQ flit (category=0) → vc_counts_[0]++
  - RESP flit (category=1) → vc_counts_[1]++
  - DAT flit (category=2) → vc_counts_[2]++
  - CREDIT flit (category=3) → vc_counts_[3]++
- **THEN** 4 个 vc_counts_ 分别 +1
- **AND** `vc_count(unsigned vc_id)` 返回对应值
- **NOTE**: per ARM CMN-700 TRM §CHI-interface-signals, 4 个独立 VC (VC_REQ/VC_RSP/VC_DAT/VC_SNP) 是真实 CHI 实现; v1.1 软件抽象以 `flit_category` 字段作分桶依据, 为 E.2 multi-VC 铺设基础

---

### Requirement: e1-send-request-response-api (v1.0 保留)

`SocInternalNoc` SHALL 提供 `send_request(NoCFlitBundle&)` + `send_response(NoCRespFlitBundle&)` 双 API (per AC-E1-3.4).

#### Scenario: send_request 调用 (per AC-E1-3.4)

- **GIVEN** soc_noc 已装配 4 target
- **WHEN** `soc_noc.send_request(req_flit)` 调用 (req_flit.dst_node = `NODE_ID_VRAM`)
- **THEN** 路由到 vram_target 的 NoC 入口 (per `accept_noc_request`)

#### Scenario: send_response 调用 (per AC-E1-3.4)

- **GIVEN** SDMA 提交 task tag=42
- **WHEN** `soc_noc.send_response(resp_flit)` 调用 (resp_flit.dst_node=`NODE_ID_SDMA`, resp_flit.transaction_id=42)
- **THEN** 路由回 SDMA target (per `accept_noc_response`)
- **AND** SDMA task_complete(42) 触发 (per `sdma_engine_tlm.cc`)

---

### Requirement: e1-sdma-noc-adapter (v1.0 保留, 增强)

`SDMANocAdapter` SHALL 在 `include/tlm/gpu/sdma_noc_adapter.hh` 提供 PcieTlpBundle ↔ NoCFlitBundle 双向转换 (per AC-E1-2.8).

#### Scenario: PcieTlpBundle → NoCFlitBundle 转换 (per AC-E1-2.8 T1)

- **GIVEN** SDMA submit_descriptor with `op=READ, vram_addr=0x1000, size=64, tag=42`
- **WHEN** `SDMANocAdapter::emit_noc_request(op, addr, size, tag)` 调用
- **THEN** 返回 `NoCFlitBundle` 含 `transaction_id=42, is_write=false, address=0x1000, size=64, src_node=NODE_ID_SDMA=1, dst_node=NODE_ID_VRAM=3, flit_category=CATEGORY_REQUEST`
- **AND** tag 保留 (per `sdma_engine_tlm.cc:683-713` `h2d_pending_` tag-based outstanding)

#### Scenario: NoCFlitBundle → PcieTlpBundle 转换 (per AC-E1-2.8 T2)

- **GIVEN** `NoCFlitBundle` with `transaction_id=42, is_ok=true, flit_category=CATEGORY_RESPONSE`
- **WHEN** `SDMANocAdapter::accept_noc_response(flit)` 调用
- **THEN** SDMA 内部 `task_complete(42, OK)` 触发

#### Scenario: 5 端口 backward compat (per AC-E1-5.1)

- **WHEN** SDMA 现有 PcieTlpBundle 端口用既有测试用例 (per `[minimal_dgpu_soc]`)
- **THEN** 行为不变, 0 回归
- **NOTE**: PcieTlpBundle 端口标 `[[deprecated("use NoCFlitBundle via SDMANocAdapter")]]` 但仍工作

---

### Requirement: e1-vram-noc-adapter-axi4-mapping (v1.1 新增, per MAJOR M4)

`VramNocAdapter` SHALL 在 `include/tlm/vram_noc_adapter.hh` 提供 NoCFlitBundle → Fabric2VramCtrlBundle + AXI4-like 字段映射 (per AC-E1-2.7).

#### Scenario: VramNocAdapter thin wrapper (per AC-E1-2.7 T1)

- **GIVEN** `NoCFlitBundle` with `address=0x1000, size=8, is_write=true, src_node=NODE_ID_SDMA=1, vc_id=0`
- **WHEN** `VramNocAdapter::emit_noc_request(flit)` 调用
- **THEN** 返回 `Fabric2VramCtrlBundle` 含:
  - `address=0x1000`, `size=8`, `is_write=true`, `data=0` (待填充)
  - `transaction_id=truncated_64_to_32`
  - `chip_select=vc_id<<8 | src_node` (multi-chip select encoding per HCS topology)
  - `error_code=0` (OK)
- **NOTE**: Fabric2VramCtrlBridge 保留 (per Change A frozen), 两条路径并存

#### Scenario: wrap_response (per AC-E1-2.7 T2)

- **GIVEN** `Fabric2VramCtrlBundle` with `resp_data=0xDEADBEEF, transaction_id=42`
- **WHEN** `VramNocAdapter::wrap_response(fabric)` 调用
- **THEN** 返回 `NoCFlitBundle` 含:
  - `data=0xDEADBEEF`, `transaction_id=42`
  - `src_node=NODE_ID_VRAM=3`, `dst_node=NODE_ID_SDMA=1`
  - `flit_category=CATEGORY_RESPONSE`, `is_ok=true`

#### Scenario: multi-beat split (per AC-E1-2.7 T3, AXI4 burst split 简化)

- **GIVEN** `NoCFlitBundle` with `flit_count=4, flit_index=0..3, size=128` (AXI4 burst)
- **WHEN** `VramNocAdapter::emit_noc_request(flit)` 调用
- **THEN** 拆为 4 个 `Fabric2VramCtrlBundle` (每拍 32B) 提交到 Fabric2VramCtrlBridge
- **NOTE**: 真实 HBM3 Controller AXI4 burst split (per AMD PG276 §AXI4 protocol); functional-mode 简化

#### Scenario: chip-internal AXI4-like 行为 (per MAJOR M4)

- **GIVEN** VramControllerTLM 接受 NoCReqFlitBundle 经 VramNocAdapter 转换
- **THEN** `vram_storage_` backing 写入 (per Change A 既已闭环)
- **NOTE**: per Oracle RTL 调研, 真实 HBM3 Controller 是 AXI4 slave; VramNocAdapter 做 NoC→AXI4-like 字段映射; functional-mode 无 wait/ready 握手, 0 cycle 完成

---

### Requirement: e1-datanoc-e2e (v1.1 限定 DataNoC, v1.0 是 5 E2E)

4 E2E scenarios SHALL 验证 DataNoC 路径真实可观测 (per AC-E1-4, v1.1 缩减为 4 scenarios).

#### Scenario: SDMA bulk DMA E2E (per AC-E1-4.2, v1.1 主 E2E)

- **GIVEN** 完整 dGPU SoC 仿真运行 (per `dgpu_soc_v2_realip_p2.json`)
- **WHEN** driver 写 BAR1 doorbell (SDMA descriptor: op=READ, vram_addr=0x1000, size=64, tag=42)
- **THEN** SDMA 解析 descriptor → SDMANocAdapter → SocInternalNoc → VramNocAdapter → VramControllerTLM → Fabric2VramCtrlBridge → vram_storage_
- **AND** response flit 经 VramNocAdapter wrap → SocInternalNoc → SDMA → `task_complete(42, OK)`
- **AND** driver 收到 MSI-X vector (per 既已闭环链路 `sdma_engine_tlm.cc:738 → dgpu_board_shell.cc:1125`)
- **AND** `[noc][e2e][datanoc]` 套件扩展

#### Scenario: SDMA fence→MSI-X 联动 E2E (per AC-E1-4.4 verification-only, v1.1 改写)

- **GIVEN** 1 SDMA task 提交
- **WHEN** SDMA 完成 copy
- **THEN** `process_fence_queue()` → `dgpu_board_->sdma_fence_complete()` → `msix_update_pending(kSdmaFenceVector)` 递增
- **AND** driver 收到 MSI-X vector 中断
- **NOTE**: **per CRITICAL C3 / 既已闭环**, 不调 `PcieEndpointIP::trigger_msix` (该 API 不存在); verification-only 测既有链路

#### Scenario: SDMA outstanding verification-only (v1.1 新增, per 既已实现 `h2d_pending_`)

- **GIVEN** 2 SDMA tasks 并发 (tag=0, tag=1)
- **WHEN** submit → process → response (in-flight 期间可乱序)
- **THEN** 2 task 各自独立完成
- **AND** fence 投递顺序与 tag 顺序一致 (per `sdma_engine_tlm.cc:738` 既有)
- **NOTE**: per 既已实现 `h2d_pending_: std::unordered_map<uint16_t, DmaDescriptor>` (per `sdma_engine_tlm.hh:475`); v1.1 不新增 `pending_tasks_` 数据结构

#### Scenario: GMMU TLB verification-only (v1.1 新增, per 既已实现 `translate_timing`)

- **GIVEN** functional-mode: GMMU TLB 含 (vaddr, paddr) entry (per `gmmu_tlm.hh:147-193`)
- **WHEN** `gmmu.translate(vaddr, paddr)` 调用 (mem_view_ 注入)
- **THEN** TLB hit 直返 paddr (0 page walk)
- **AND** translate_timing() 累加 hits/misses 统计
- **NOTE**: per 既已实现 `translate_timing()` (per `gmmu_tlm.hh:147-193`); v1.1 不新增 `tlb_: std::unordered_map` 数据结构

---

### Requirement: e1-dgpu-board-adapters-wiring (v1.1 缩减为 2 adapter)

`DGpuBoard::bind_memory_backings()` SHALL 装配 2 NoC adapter (SDMA + VRAM, per AC-E1-2 + AC-E1-3, v1.0 是 4 adapter).

#### Scenario: 2 adapter 装配 (per AC-E1-2 + AC-E1-3)

- **GIVEN** DGpuBoard 已 init, SDMA + VRAM 已 inject
- **WHEN** `bind_memory_backings()` 调
- **THEN** 2 adapter 装配:
  - `sdma_noc_adapter → sdma->set_noc_adapter(sdma_noc_adapter)`
  - `vram_noc_adapter → vram_ctrl->set_noc_adapter(vram_noc_adapter)`
- **AND** 2 桥接完成 (1 vram via GpuInternalFabric 既有 + 1 new via SocInternalNoc)
- **AND** **不装配** PcieControllerIP / GMMU adapter (per v1.1 scope 缩减)

#### Scenario: JSON 配置 (per AC-E1-2 + AC-E1-3)

- **GIVEN** `configs/dgpu_soc_v2_realip_p2.json`
- **WHEN** 含 `"soc_noc": {"data_flow": true, "vc_counters_enabled": true}` 节点
- **THEN** SocInternalNoc::route_flit() 实际推 flit (per AC-E1-3.1)
- **NOTE**: data_flow=false 时回退 E0 stub 行为 (仅计数), 0 数据流

---

### Requirement: e1-zero-regression (v1.1 保留)

本 change SHALL 不引入任何现有测试套件回归 (per AC-E1-5.1).

#### Scenario: 12 套件全绿

- **GIVEN** 修复后 build
- **WHEN** 跑 12 套件
- **THEN** 全绿, 0 回归:
  - `[pcie]` 430/430 / 36672 assertions
  - `[pcie-memory]` 17/17 / 48 assertions
  - `[minimal_dgpu_soc]` 12/12 / 66 assertions
  - `[realip]` ≥32 / ≥254 assertions (本 change +3 cases)
  - `[dgpu_soc_timing]` 37/37 / 12024 assertions
  - `[driver_visible]` ≥6 / ≥29 assertions (本 change +4 cases)
  - `[d1]` 3/3 / 6 assertions
  - `[d2]` 4/4 / 6 assertions
  - `[d3]` 4/4 / 19 assertions
  - `[router]` 9/9 / 51 assertions
  - `[phase6]` 9/9 / 69 assertions
  - `[realip][pin-contract][noc]` 5/5 / 31 assertions

#### Scenario: 死锁 fuzz 回归锁 `[noc][deadlock-fuzz]` (per proposal §"已知架构局限")

- **GIVEN** 修复后 build
- **WHEN** 跑 `[noc][deadlock-fuzz]` 套件 (3 cases: T1 SDMA bulk DMA fuzz + T2 SDMA 多任务 tag 压测 + T3 nullptr target 不 crash)
- **THEN** 全绿, 100 种子全部通过, 0 flake:
  - T1: `SocInternalNoc::flits_dispatched_()` 单增, 100 轮执行完毕无超时, 计数 `≤ 注入总数`
  - T2: 2 SDMA tasks (tag 0/1) 并发, 各自独立完成, fence 顺序正确
  - T3: 4 target 全 nullptr 时 100 种子 fuzz 注入 4 dst_node, 0 crash

#### Scenario: 4 frozen headers 零 diff (per AC-E1-1.4 + AC-E1-1.5)

- **WHEN** `git diff cpptlm_emulator.h pcie_endpoint_tlm.h pcie_display_device.hh pcie_bundles_tlm.hh` 跑
- **THEN** 0 行 diff
- **NOTE**: per ADR-DGPU-08 freeze policy, 强制锁

#### Scenario: DataNoC 删除项确认 (v1.1 新增, per AC-E1-1.6 + AC-E1-6.2 + AC-E1-6.3)

- **WHEN** `find include/bundles/dgpu_soc_noc_bundles_tlm.hh` 跑
- **THEN** 0 匹配 (per AC-E1-1.6 dual bundle 删除确认)
- **WHEN** `git grep emit_noc_request` 跑
- **THEN** 0 匹配 (per AC-E1-6.3 PCIe Controller 不输出 NoC bundle)
- **WHEN** `git diff dgpu_board_shell.cc` 该行区域 0x00-0x100 跑
- **THEN** 0 改动 (per AC-E1-6.2 PT_BASE 硬编码 if/else 保留)

#### Scenario: openspec validate

- **WHEN** `openspec validate cpptlm-dgpu-soc-internal-noc-e1 --strict`
- **THEN** 退出码 0, 输出 "Change is valid"

#### Scenario: docs_sync_check

- **WHEN** `./scripts/test/docs_sync_check.sh --strict`
- **THEN** 退出码 0, 输出 "所有路径引用都有效"
- **NOTE**: 新 E.1 design doc 路径在 AGENTS.md / architecture.md 引用

---

