# dgpu-4ip-port-contract: 4-IP Topology 端口契约 (chip-internal wire-protocol 静态检查)

> **所属 change**: [`dgpu-realip-pin-contract`](../proposal.md)
> **范围**: 4-IP topology chip-internal 端口契约代码化 (`Fabric2VramCtrlBundle` + `BUNDLE_FIELD_WIDTH` 编译期检查 + programmatic bridge) — Change A + Change B
> **关联 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`, `dgpu-driver-visible-minimal-soc`
> **前置**: `cpptlm-dgpu-realip-refactor` ✅ (Phase C, tag `dgpu-realip-refactor-completion` 落 2026-10-07) + Change A ✅ (tag `dgpu-realip-pin-contract-change-a-completion` 落 2026-10-08)

---

## Purpose

4-IP topology (`PcieControllerIP` / `GpuInternalFabric` / `VramControllerTLM` / `DGpuSoc`) 完成 chip-internal 端口契约代码化:

1. **`Fabric2VramCtrlBundle`** chip-internal wire-protocol (fabric→vram_ctrl 数据路径, 之前 handoff §9.2 #9 列为 D3/D4 deferred work)
2. **`port_contract_asserts.hh` `BUNDLE_FIELD_WIDTH` macro** 编译期检查字段宽度 (per Momus 2026-10-08 修订: 删 `PORT_IS_INITIATOR`/`PORT_IS_TARGET` 因 chstream 端口非模板类)
3. **programmatic bridge** via `GpuInternalFabric::set_vram_target(VramControllerTLM*)` + `VramControllerTLM::set_fabric_source(GpuInternalFabric*)` setter (per Phase 8 HB/RC tick 转发先例)
4. **(Change B) Facade Production Wiring** — `DGpuBoard::init()` 公共路径接 `ctrl->attach_endpoint(ep)` + `set_pcie_controller_ip(ctrl)`,激活 Strangler gate;`match_bar` whole-BAR 签名锁定

让 RTL team 1:1 对照 chip-internal 连线 (`Fabric2VramCtrlBundle` 字段 1:1 → 硅片 wire),DGpuBoard facade 接管数据路径,Strangler gate 保留作 facade verdict 与 expected mirror 一致性的 std::abort 保护。

---

## ADDED Requirements

### Requirement: fabric-vram-ctrl-port-contract

`GpuInternalFabric` 与 `VramControllerTLM` SHALL 通过 `Fabric2VramCtrlBundle` 严格匹配 (per Change A 2026-10-08 resplit):
- **字段对齐**: `Fabric2VramCtrlBundle` 8 字段 (`address[64]` / `size[8]` / `is_write` / `data[64]` / `transaction_id[32]` / `resp_data[64]` / `error_code[8]` / `chip_select[8]`) 必须 `BUNDLE_FIELD_WIDTH` 编译期校验与 chip-internal wire-protocol 1:1 对应
- **程序化桥接**: 不依赖 JSON 声明式端口接线 (per Phase 8 HB/RC tick 转发先例 + cpptlm-p2-integration-unblock design D7),`GpuInternalFabric::set_vram_target(VramControllerTLM*)` setter 在 JSON 装配后程序化桥接
- **Momus 2026-10-08 修订**: 删除原 `PORT_IS_INITIATOR` / `PORT_IS_TARGET` macro (基于不存在的 `chstream::ChStreamInitiatorPort<Bundle>` 模板 — 实际 `cpptlm::ChStreamInitiatorPort` 是非模板类);改用 `BUNDLE_FIELD_WIDTH` 编译期字段宽度校验 + setter-only 桥接 + 运行时 round-trip 测试

#### Scenario: GpuInternalFabric.set_vram_target setter 桥接

- **WHEN** `GpuInternalFabric::set_vram_target(VramControllerTLM*)` 在 JSON 装配后被调
- **THEN** `vram_ctrl_` 指针被缓存,`tick()` 时如果 `vram_ctrl_out.valid()`,转发到 `vram_ctrl_->fabric_in.req_in`
- **AND** 不依赖 JSON 声明式 `fabric.vram_ctrl_out → vram_ctrl.fabric_in` 端口连接 (避免 19 §14.2.3 静默丢弃)
- **AND** 当 `vram_ctrl_ == nullptr` (未桥接) 时,`tick()` 静默跳过 fabric_out 转发 (deferred D3/D4, 无 silent failure)

#### Scenario: VramControllerTLM.set_fabric_source setter 桥接

- **WHEN** `VramControllerTLM::set_fabric_source(GpuInternalFabric*)` 在 JSON 装配后被调
- **THEN** `fabric_` 指针被缓存,响应经 `Fabric2VramCtrlBundle::resp_data/error_code/transaction_id` 回填到 `fabric_in::resp_out` 端口
- **AND** 与 `GpuInternalFabric::set_vram_target` 双向配对 (`DGpuBoard::bind_memory_backings()` 中通过 `soc_->getInternalInstance("fabric")` + `"vram_ctrl"` 装配)

#### Scenario: Fabric2VramCtrlBundle 字段定义编译期锁定

- **WHEN** 编译 `include/bundles/dgpu_fabric_bundles_tlm.hh`
- **THEN** 8 字段 (`address[64]`/`size[8]`/`is_write`/`data[64]`/`transaction_id[32]`/`resp_data[64]`/`error_code[8]`/`chip_select[8]`) `BUNDLE_FIELD_WIDTH` 宽对齐
- **AND** `chip_select[8]` 字段对齐 VramControllerTLM `row_hit(addr) noexcept override` 计算 (per vram_controller_tlm.hh:47-52 4K row buffer)
- **AND** `error_code[8]` 0=OK, 1=DECERR, 2=SLVERR, 3=EXOKAY 字段语义文档化

---

### Requirement: dgpu-4ip-port-contract-test-coverage (Change A, 双标签 `[realip][pin-contract]`)

2 个测试文件**双标签** `[realip][pin-contract]` SHALL 覆盖 4-IP 端口契约 (Change A scope):
- `test/test_realip_pin_contract.cc` — `Fabric2VramCtrlBundle` 字段定义 + `BUNDLE_FIELD_WIDTH` 编译期检查 + `set_vram_target` / `set_fabric_source` 程序化桥接 round-trip (3 TEST_CASEs)
- `test/test_dgpu_board_no_legacy_flags.cc` — DGpuBoard public API 编译期不含已删除 4 routing flag + PciePath + attach_profile (Change C 终态 ACTIVE 验证, ctest grep gate 补充)

**双标签**: `[realip][pin-contract]` (per handoff §9.2 #10 vacuous tag 风险, 空格 = AND, 逗号 = OR, per cpptlm-p2-integration-unblock proposal G9)

#### Scenario: 双标签 + T28 gate 防 vacuous pass

- **WHEN** 跑 `./build/bin/cpptlm_tests "[realip][pin-contract]"`
- **THEN** 至少 5 个 test cases PASS (Change A 3 cases + Change A XFAIL-probe 2 cases, 共 ≥5)
- **AND** 跑 `./build/bin/cpptlm_tests "[pin-contract]"` → 同样 ≥5 cases PASS (确保 `[pin-contract]` 独立 tag 有命中)
- **AND** 跑 `./build/bin/cpptlm_tests "[realip]"` → **零回归** (Phase C 后基线 433/36682 cases PASS)
- **AND** Strangler gate 字符串在 Release build = 0 (Change C 终态, per AC-C3); DEBUG build 保留 assert 字符串

#### Scenario: test_realip_pin_contract 端口契约覆盖 (3 TEST_CASEs)

- **WHEN** 跑 `test/test_realip_pin_contract.cc`
- **THEN** TEST_CASE #1: `Fabric2VramCtrlBundle` 8 字段宽对齐 (`BUNDLE_FIELD_WIDTH` 编译期通过)
- **AND** TEST_CASE #2: `GpuInternalFabric::set_vram_target` setter 缓存 `vram_ctrl_` 指针 + `vram_ctrl()` accessor 返回缓存指针
- **AND** TEST_CASE #3: `VramControllerTLM::set_fabric_source` setter 缓存 `fabric_` 指针 + `fabric()` accessor 返回缓存指针 + 手动 setter round-trip

#### Scenario: test_dgpu_board_no_legacy_flags 编译期保护

- **WHEN** 编译 `test/test_dgpu_board_no_legacy_flags.cc`
- **THEN** `static_assert` 检测 `DGpuBoard` 不含 `pcie_path_*` 5 字段 (编译期 SFINAE)
- **AND** `static_assert` 检测 `DGpuBoard` 不含 5 个 setter 方法
- **AND** Release build `std::abort` 路径已删,DEBUG-only assert 保留 (防止 `controller->ep_` vs `board->pcie_ep()` 不对称静默漂移)
- **AND** **零运行时开销**: 全部 `static_assert` 编译期完成,无运行时分支

---

### Requirement: dgpu-board-facade-production-wiring (Change B, 2026-10-08)

`DGpuBoard::init()` 公共路径 SHALL 装配 `PcieControllerIP` facade,激活 Strangler gate 接管数据路径 (Change B scope, per Oracle `bg_14b6dd83` APPROVE-WITH-CHANGES):

- **最小接线** (per Oracle AD-B1): `ctrl->attach_endpoint(ep)` + `set_pcie_controller_ip(ctrl)`;**不**用 `set_endpoint_attachment(this)` (全仓不存在,无消费者,YAGNI)
- **接线点** (per Oracle AD-B2): 入 `DGpuBoard::init()` 公共路径,早于 `simulation_mode` timing/functional 分支 (timing 模式也覆盖,消除原 `init_timing_mode` 不接 facade 的盲区)
- **`match_bar` 签名零变化** (per Oracle AD-B3 + 前序 Oracle bg_c60653e0 §2.1): 保持 4 枚举 (`Bar0Mmio` / `Bar1FbFastPath` / `Bar2VramAperture` / `Unknown`),whole-BAR 锁定,不做 controller-side offset 子分发
- **空 guard 模式** (per Oracle AD-B4): null-guards + DPRINTF + `~DGpuBoard()` 清 `controller_ip_` + `gmmu_` + `sdma_engine_` + `cycle_advance_modules_` 4 个 raw pointer 防悬空

**前置 (硬约束)**: cfg/admin-visible configs (`dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` / `dgpu_soc_with_pcie_ip.json` / `dgpu_soc_v2_realip_p2.json`) 必须同时含 `pcie_ctrl` + `pcie_ep` 节点 (per Change B.0 config 前置)

#### Scenario: DGpuBoard::init wires PcieControllerIP facade in production path

- **WHEN** `DGpuBoard::init()` runs after `bind_memory_backings()`
- **AND** `soc_->getInternalInstance("pcie_ctrl")` returns non-null `PcieControllerIP*`
- **AND** `pcie_ep()` returns non-null `PcieEndpointIP*`
- **THEN** `ctrl->attach_endpoint(ep)` is called
- **AND** `set_pcie_controller_ip(ctrl)` is called (Strangler gate activated)
- **AND** mirror verdict in `mmio_read/write` matches `match_bar` verdict (no `std::abort`)

#### Scenario: Dormant-guard: pcie_ctrl without pcie_ep collapses match_bar to Unknown gracefully

- **WHEN** `pcie_ctrl` 在 JSON 中存在但 `pcie_ep()` 返回 nullptr (eg. DGpuSoc 只有 pcie_ctrl 模块)
- **THEN** `attach_endpoint()` skipped
- **AND** `match_bar(bar, offset, out_target, out_sub_offset)` collapses to `out_target=Unknown + matched=false` (不 abort)
- **AND** 此 case regression-lock 静默塌陷路径 — Change C 改 mmio_read/write 时不会突然 abort

#### Scenario: Strangler gate 终态 (Change C 删 + DEBUG-only assert 保留)

- **WHEN** `DGpuBoard::mmio_read/write` is called with `controller_ip_ != nullptr`
- **THEN** facade verdict `match_bar(bar, offset, ...)` 与 expected mirror 比对 (`dgpu_board_shell.cc:344,457`)
- **AND** 一致时正常返回,不一致时调 `std::abort`
- **AND** **Strangler gate 是 observation-only** (per Metis bg_03094fec 修订): gate 不分发也不路由,仅观测 — 分发仍走 legacy routing flag (`display_routing_enabled_` / `gmmu_routing_enabled_` / `memory_routing_enabled_` 等);Change C 删除 legacy flag 时, **不可**把 gate verdict 当作分发依据,必须先把分发路径从 legacy flag 迁到 facade verdict,再删 flag

#### Scenario: 4 raw pointer clear in destroy() 防悬空 (per Metis bg_03094fec 修订)

- **WHEN** `DGpuBoard::destroy()` is called (or `~DGpuBoard()` triggers destroy)
- **AND** `soc_.reset()` 已执行
- **THEN** `controller_ip_ = nullptr;` (P2 PR-A facade non-owning ptr)
- **AND** `gmmu_ = nullptr;` (bind_memory_backings 缓存)
- **AND** `sdma_engine_ = nullptr;` (bind_memory_backings 缓存)
- **AND** `cycle_advance_modules_.clear();` (init_timing_mode 累积 vector,防止 re-init 悬空解引用)
- **AND** 4 个 raw pointer 同时清零,保证 destroy() 后 mmio_read/write UAF-safe

#### Scenario: Characterization tests pin facade 激活前后 BAR0/1/2 行为逐位一致 (regression lock)

- **WHEN** `mmio_read(0, 0, 4)` (BAR0 GMMU register window), `mmio_read(1, 0x10, 4)` (BAR1 FB fast-path), `mmio_read(2, 0x0, 4)` (BAR2 VRAM aperture) 调用前后
- **THEN** facade 激活前后 BAR0/1/2 行为逐位一致 (regression lock, byte-baseline constant 记录已验证行为)
- **AND** **不引入** controller-side offset 子分发 (whole-BAR 锁定)
- **AND** 4KB BAR0 子窗口: GMMU regs (0x00-0x1F) vs display regs (0x00-0xF0) 不可 offset 区分,在任何单一 config 中从不共存

---

## 关联

- **proposal**: [`../proposal.md`](../proposal.md)
- **design**: [`../design.md`](../design.md)
- **tasks**: [`../tasks.md`](../tasks.md)
- **前置**: `cpptlm-dgpu-realip-refactor` (archived, Phase C 完成 2026-10-07) + Change A ✅ (2026-10-08)
- **并行**: `cpptlm-p2-integration-unblock` (已 Proposed, 异步推进)
- **ADR**:
  - `ADR-DGPU-05-vram-storage-ownership.md` (framebuffer_ 单一真源)
  - `ADR-DGPU-06-axi-mem-bundle-boundary.md` (AxiMemBundle vs PcieTlpBundle 边界)
  - `ADR-DGPU-07-minimal-soc-evolution-seam.md` (D3 seam 插入点)
  - `ADR-DGPU-08-abi-freeze-policy.md` (23 ABI 冻结)
  - `ADR-DGPU-10` (legacy flag 防劫持 root cause 4 锁定)
- **架构文档**: `docs/designs/dgpu-soc/architecture.md` §2.4 (4-IP 边界表) + §2.4.2 (Change B facade wiring, 待新增) + §7.3 (JSON 层级约束)