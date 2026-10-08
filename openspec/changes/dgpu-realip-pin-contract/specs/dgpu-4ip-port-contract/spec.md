# dgpu-4ip-port-contract: 4-IP Topology 端口契约 (chip-internal wire-protocol 静态检查)

> **所属 change**: [`dgpu-realip-pin-contract`](../proposal.md)
> **范围**: 4-IP topology chip-internal 端口契约代码化 (`Fabric2VramCtrlBundle` + `static_assert` 编译期检查 + DGpuBoard legacy flag 删除)
> **关联 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`, `dgpu-driver-visible-minimal-soc`
> **前置**: `cpptlm-dgpu-realip-refactor` ✅ (Phase C, tag `dgpu-realip-refactor-completion` 落 2026-10-07)

---

## Purpose

4-IP topology (`PcieControllerIP` / `GpuInternalFabric` / `VramControllerTLM` / `DGpuSoc`) 完成 chip-internal 端口契约代码化:
1. **`Fabric2VramCtrlBundle`** chip-internal wire-protocol (fabric→vram_ctrl 数据路径, 之前 handoff §9.2 #9 列为 D3/D4 deferred work)
2. **`port_contract_asserts.hh` `static_assert`** 编译期检查 4-IP 端口名 + 类型 + 方向 (`ChInitiatorPort`/`ChTargetPort`/`MasterPort`/`SlavePort`)
3. **`DGpuBoard` 7 个 legacy routing flag 删除** (`PciePath` enum + 5 routing_enabled flags + `mmio_self_drain_enabled`),JSON schema 顶层 4 个 `routing_enabled` 字段同步退役

让 RTL team 1:1 对照 chip-internal 连线 (Fabric2VramCtrlBundle 字段 1:1 → 硅片 wire),DGpuBoard 瘦身 (mmio_read/write 从 ~120 行 → ~40 行),Strangler gate 保留 (per dgpu_board_shell.cc:336-343 facade verdict 与 expected mirror)。

---

## ADDED Requirements

### Requirement: fabric-vram-ctrl-port-contract

`GpuInternalFabric::vram_ctrl_out` (egress ChInitiatorPort) 与 `VramControllerTLM::fabric_in` (ingress ChTargetPort) SHALL 通过 `Fabric2VramCtrlBundle` 严格匹配:
- **字段对齐**: `Fabric2VramCtrlBundle` 8 字段 (`address[64]` / `size[8]` / `is_write` / `data[64]` / `transaction_id[32]` / `resp_data[64]` / `error_code[8]` / `chip_select[8]`) 必须 `static_assert` 与 chip-internal wire-protocol 1:1 对应
- **方向约束**: `GpuInternalFabric::vram_ctrl_out` 是 `ChInitiatorPort<Fabric2VramCtrlBundle>` (egress),`VramControllerTLM::fabric_in` 是 `ChTargetPort<Fabric2VramCtrlBundle>` (ingress)
- **程序化桥接**: 不依赖 JSON 声明式端口接线 (per Phase 8 HB/RC tick 转发先例 + cpptlm-p2-integration-unblock design D7),`GpuInternalFabric::set_vram_target(VramControllerTLM*)` setter 在 JSON 装配后程序化桥接

#### Scenario: GpuInternalFabric.vram_ctrl_out 端口契约静态检查

- **WHEN** 编译 `include/tlm/cluster/gpu_internal_fabric.hh`
- **THEN** `static_assert` 验证 `vram_ctrl_out` 是 `ChStreamInitiatorPort<Fabric2VramCtrlBundle>` (egress)
- **AND** 编译期字段对齐 `Fabric2VramCtrlBundle::address[64]` 与 `VramControllerTLM::tick()` 期望输入
- **AND** 编译期 `chip_select[8]` 字段宽匹配 VramControllerTLM::row_address 路由 (per `vram_controller_tlm.hh:47-52` 4K row buffer)

#### Scenario: VramControllerTLM.fabric_in 端口契约静态检查

- **WHEN** 编译 `include/tlm/vram_controller_tlm.hh`
- **THEN** `static_assert` 验证 `fabric_in` 是 `ChStreamTargetPort<Fabric2VramCtrlBundle>` (ingress)
- **AND** `tick()` flag-gated 检查: 仅 `fabric_in` valid 时消费 (per flag-gated 默认 disable 纪律)
- **AND** 响应经 `Fabric2VramCtrlBundle::resp_data/error_code/transaction_id` 回填到 `fabric_in::resp_out` 端口

#### Scenario: Fabric2VramCtrlBundle 字段定义编译期锁定

- **WHEN** 编译 `include/bundles/dgpu_fabric_bundles_tlm.hh`
- **THEN** 8 字段 (`address[64]`/`size[8]`/`is_write`/`data[64]`/`transaction_id[32]`/`resp_data[64]`/`error_code[8]`/`chip_select[8]`) `static_assert` 宽对齐
- **AND** `chip_select[8]` 字段对齐 VramControllerTLM `row_hit(addr) noexcept override` 计算 (per vram_controller_tlm.hh:47-52 4K row buffer)
- **AND** `error_code[8]` 0=OK, 1=DECERR, 2=SLVERR, 3=EXOKAY 字段语义文档化

#### Scenario: set_vram_target 程序化桥接

- **WHEN** `GpuInternalFabric::set_vram_target(VramControllerTLM*)` 在 JSON 装配后被调
- **THEN** `vram_ctrl_` 指针被缓存,`tick()` 时如果 `vram_ctrl_out.valid()`,转发到 `vram_ctrl_->fabric_in.req_in`
- **AND** 不依赖 JSON 声明式 `fabric.vram_ctrl_out → vram_ctrl.fabric_in` 端口连接 (避免 19 §14.2.3 静默丢弃)
- **AND** 当 `vram_ctrl_ == nullptr` (未桥接) 时,`tick()` 静默跳过 fabric_out 转发 (deferred D3/D4, 无 silent failure)

---

### Requirement: dgpu-board-legacy-flag-retirement

`DGpuBoard` SHALL 删除 7 个 legacy routing flag + setter + accessor + enum,Strangler gate 保留:

**删除清单** (per `dgpu_board_shell.hh:75-117,317-348`):
1. `PciePath` enum (Legacy/Tlp/AxiBypass/Mock, 4 values)
2. `pcie_path_` 成员 (`PciePath` 类型, default=Legacy)
3. `pcie_path()` accessor
4. `display_routing_enabled_` 成员 (`bool`, default=false)
5. `set_display_routing_enabled(bool)` setter + `display_routing_enabled()` accessor
6. `memory_routing_enabled_` 成员 + `set_memory_routing_enabled(bool)` setter + `memory_routing_enabled()` accessor
7. `storage_routing_enabled_` 成员 + `set_storage_routing_enabled(bool)` setter + `storage_routing_enabled()` accessor
8. `gmmu_routing_enabled_` 成员 + `set_gmmu_routing_enabled(bool)` setter + `gmmu_routing_enabled()` accessor
9. `mmio_self_drain_enabled` 成员 (`bool{true}`) — **保留** (drain 优化,不接 legacy path)

**保留**:
- `controller_ip_` 成员 + `set_pcie_controller_ip(PcieControllerIP*)` setter + `controller_ip()` accessor (P2 PR-A facade 必要)
- `mmio_self_drain_enabled` 成员 (drain 优化,不属 legacy routing)

**Strangler gate 保留**:
- `dgpu_board_shell.cc:336-343` (mmio_read) + `:433-466` (mmio_write) facade verdict divergence 检查保留
- `controller_ip_->match_bar()` 与 expected mirror 保留 (P2 PR-A facade 锁定)

#### Scenario: DGpuBoard public API 编译期不含已删除 flag

- **WHEN** 编译 `include/tlm/gpu/dgpu_board_shell.hh` 与 `test/test_dgpu_board_no_legacy_flags.cc`
- **THEN** `static_assert(sizeof(DGpuBoard) < X)` (X 由 PostDelete sizeof 估算, ~600 bytes vs 当前 ~750 bytes)
- **AND** `static_assert` 验证 `DGpuBoard` 类不含 `pcie_path_` / `display_routing_enabled_` / `memory_routing_enabled_` / `storage_routing_enabled_` / `gmmu_routing_enabled_` 5 个成员 (编译期 SFINAE 探测)
- **AND** `static_assert` 验证 `DGpuBoard` 不含 `set_pcie_path_*` / `set_display_routing_enabled` / `set_memory_routing_enabled` / `set_storage_routing_enabled` / `set_gmmu_routing_enabled` 5 个 setter

#### Scenario: DGpuBoard::mmio_read 简化回归

- **WHEN** `mmio_read(bar=0, offset=0x123, buf, len=4)` 被调
- **THEN** 走 `controller_ip_->match_bar()` Strangler gate (per dgpu_board_shell.cc:336-343)
- **AND** 走 `pcie_ep()->display_device().mmio_read()` (per D1 v1.1.1 fast-path, **不依赖** `display_routing_enabled_` flag)
- **AND** 走 `pcie_ep()->memory_device().memory_read()` (per D2 v1.1 fast-path, **不依赖** `memory_routing_enabled_` flag)
- **AND** 走 BAR1 framebuffer window 直接 memcpy (per Phase 8 B2 fast-path, **不依赖** `storage_routing_enabled_` flag)
- **AND** **不再走** `mmio_regs_` legacy path (`std::map<pair<bar, offset>, vector<uint8_t>>`, 后续 pin-contract 二次清理)

#### Scenario: DGpuBoard::mmio_self_drain_enabled 保留

- **WHEN** `mmio_read/write` 在 `inject_q_` 有 pending req
- **THEN** `mmio_self_drain_enabled` flag 保留 (per `dgpu_board_shell.cc:387`),`drain_injection_queue()` 在调用线程自 drain
- **AND** flag 保留是 drain 性能优化,不是 legacy path 选择,**不属本 change 删除范围**
- **AND** 默认值 `true` 保留 (向后兼容)

---

### Requirement: dgpu-4ip-port-contract-test-coverage

2 个新测试文件**双标签** `[realip][pin-contract]` SHALL 覆盖 4-IP 端口契约 + legacy flag 删除回归:

- `test/test_realip_pin_contract.cc` — `Fabric2VramCtrlBundle` 字段定义 + `static_assert` 编译期检查 + `set_vram_target` 程序化桥接 round-trip
- `test/test_dgpu_board_no_legacy_flags.cc` — DGpuBoard public API 编译期不含已删除 5 routing flag + Strangler gate 字符串保留 (`strings build/bin/cpptlm_tests | grep -c "Strangler gate" == 2`)

**双标签**: `[realip][pin-contract]` (per handoff §9.2 #10 vacuous tag 风险, 空格 = AND, 逗号 = OR, per cpptlm-p2-integration-unblock proposal G9)

#### Scenario: 双标签 + T28 gate 防 vacuous pass

- **WHEN** 跑 `./build/bin/cpptlm_tests "[realip][pin-contract]"`
- **THEN** 至少 2 个 test cases PASS (1 文件 ≥1 case, 共 ≥2)
- **AND** 跑 `./build/bin/cpptlm_tests "[pin-contract]"` → 同样 ≥2 cases PASS (确保 `[pin-contract]` 独立 tag 有命中)
- **AND** 跑 `./build/bin/cpptlm_tests "[realip]"` → **零回归** (Phase C 后基线 433/36682 cases PASS)
- **AND** Strangler gate 字符串仍存在 (`strings build/bin/cpptlm_tests | grep -c "Strangler gate" == 2`)

#### Scenario: test_realip_pin_contract 端口契约覆盖

- **WHEN** 跑 `test/test_realip_pin_contract.cc`
- **THEN** TEST_CASE #1: `Fabric2VramCtrlBundle` 8 字段宽对齐 (`static_assert` 编译期通过)
- **AND** TEST_CASE #2: `GpuInternalFabric::vram_ctrl_out` 是 ChInitiatorPort
- **AND** TEST_CASE #3: `VramControllerTLM::fabric_in` 是 ChTargetPort
- **AND** TEST_CASE #4: `set_vram_target` 程序化桥接: 经 JSON 装配 + set_vram_target + tick, fabric→vram_ctrl 数据路径 round-trip

#### Scenario: test_dgpu_board_no_legacy_flags 编译期保护

- **WHEN** 编译 `test/test_dgpu_board_no_legacy_flags.cc`
- **THEN** `static_assert` 检测 `DGpuBoard` 不含 `pcie_path_*` 5 字段 (编译期 SFINAE)
- **AND** `static_assert` 检测 `DGpuBoard` 不含 5 个 setter 方法
- **AND** 编译期 Strangler gate 字符串存在 (per `dgpu_board_shell.cc:336-343` source grep)
- **AND** **零运行时开销**: 全部 `static_assert` 编译期完成,无运行时分支

---

### Requirement: json-routing-enabled-retirement

`configs/dgpu_soc_v2_realip_p2.json` 顶层 4 个 `routing_enabled` 字段 SHALL 退役:
- `display_routing_enabled`
- `memory_routing_enabled`
- `storage_routing_enabled`
- `gmmu_routing_enabled`

#### Scenario: JSON 顶层 routing_enabled 字段移除

- **WHEN** 读 `configs/dgpu_soc_v2_realip_p2.json` (Phase C 后基线)
- **THEN** 顶层 `routing_enabled` 字段全部不存在
- **AND** `validateConfig` 通过 (无 routing_enabled 校验,因为 DGpuBoard 不再消费)
- **AND** `_note` 字段说明字段退役 + 引用 ADR-DGPU-05/06 边界契约
- **AND** `dgpu_soc_v2_realip_p2.json` `version` 字段从 `"2-realip-p2-b"` → `"2-realip-p3-pin"` (per Phase C → Phase D 版本递增)

#### Scenario: Strangler gate 保留 (per ADR-DGPU-10)

- **WHEN** `DGpuBoard::mmio_read/write` 被调
- **THEN** Strangler gate (`dgpu_board_shell.cc:336-343` / `:433-466`) 保留
- **AND** `controller_ip_->match_bar()` 与 expected mirror 比对 (P2 PR-A facade 锁定)
- **AND** **不依赖**任何 routing_enabled flag
- **AND** 编译期字符串 `"Strangler gate"` 仍存在于 `dgpu_board_shell.cc` (per `strings build/bin/cpptlm_tests | grep -c "Strangler gate" == 2`)

---

## 关联

- **proposal**: [`../proposal.md`](../proposal.md)
- **design**: [`../design.md`](../design.md)
- **tasks**: [`../tasks.md`](../tasks.md)
- **前置**: `cpptlm-dgpu-realip-refactor` (archived, Phase C 完成 2026-10-07)
- **并行**: `cpptlm-p2-integration-unblock` (已 Proposed, 异步推进)
- **ADR**:
  - `ADR-DGPU-05-vram-storage-ownership.md` (framebuffer_ 单一真源)
  - `ADR-DGPU-06-axi-mem-bundle-boundary.md` (AxiMemBundle vs PcieTlpBundle 边界)
  - `ADR-DGPU-07-minimal-soc-evolution-seam.md` (D3 seam 插入点)
  - `ADR-DGPU-08-abi-freeze-policy.md` (23 ABI 冻结)
  - `ADR-DGPU-10` (legacy flag 防劫持 root cause 4 锁定)
- **架构文档**: `docs/designs/dgpu-soc/architecture.md` §2.4 (4-IP 边界表) + §7.3 (JSON 层级约束)
