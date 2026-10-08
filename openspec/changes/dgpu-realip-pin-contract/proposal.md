# dgpu-realip-pin-contract: Legacy Flag 删除 + 端口契约代码化 (Phase D 边界收紧)

> **状态**: 📋 Proposed — 2026-10-07
> **目标**: 删除 `DGpuBoard` 7 个 legacy routing flag + 端口契约代码化 (chip-internal wire-protocol 静态检查), 让 4-IP topology (PcieControllerIP / GpuInternalFabric / VramControllerTLM / DGpuSoc) 从"行规 flag 拼装"切到"端口契约 + compile-time port map"
> **前置**: `cpptlm-dgpu-realip-refactor` ✅ archived 2026-10-07 (Phase C tag `dgpu-realip-refactor-completion` 落)
> **Oracle 评分卡起点**: Phase C 后平均 ~6.6/10 (GpuInternalFabric 6, VramControllerTLM 7.5, PcieControllerIP 7, DGpuBoard 5.5, PcieBarRouter 7.5)
> **预期跃升**: 平均 ~7.5/10 (DGpuBoard 5.5→7.5 [+2.0], PcieControllerIP 7→7.5 [+0.5], 端口契约解锁 RTL team 1:1 对照)

---

## Why

Phase C 完成 4-IP topology 装配 (DGpuSoc + PcieControllerIP + GpuInternalFabric + VramControllerTLM),但 **DGpuBoard::mmio_read/write 仍依赖 6+ 个 legacy routing flag 拼装** (per `dgpu_board_shell.cc:30-38` + `108-126`):
- `PciePath` enum (Legacy/Tlp/AxiBypass/Mock)
- `display_routing_enabled_` / `memory_routing_enabled_` / `storage_routing_enabled_` / `gmmu_routing_enabled_`
- `mmio_self_drain_enabled`

这些 flag 是**接缝补丁** (per D1 v1.1.1/D2 v1.1 锁定 "防劫持 root cause 4" + Phase 8 M1 Strangler gate),**实际数据路径已被 PcieControllerIP::match_bar + DGpuBoard::set_pcie_controller_ip 取代**(per PR-B Phase Gate B1/B2/B3 全部 RESOLVED)。Flag 保留仅是为"向后兼容 legacy path"测试,但 legacy path 不再被任何 JSON 引用。

同时**端口契约没有代码化**:`dgpu_soc_v2_realip_p2.json` 中 fabric 节点加 inner CrossbarTLM child 时(T25A.1)需要 `connections: []` 字段才能过 `validateConfig` (per `module_factory_validate.cc:210-216`);`PcieControllerIP.match_bar` 与 `PcieEndpointIP.bar_store_` 的协议契约只在 T25B 测试 + dgpu_board_shell.cc:336-343 Strangler gate 中体现,无 compile-time 端口契约保护。

**主因(按 ROI 排序)**:

1. **DGpuBoard 7 个 legacy flag 占据 ~200 行代码** (`dgpu_board_shell.cc:108-126` 加载 + `:308-422` mmio_read 6 个 if 分支 + `:433-466` mmio_write 6 个 if 分支),**RTL team 1:1 对照时需要翻译每行** (DGpuBoard.h 实际只有 ~75 行有效业务)
2. **PcieControllerIP facade verdict 与 expected mirror 重复实现** (`dgpu_board_shell.cc:320-343` + `:433-466` 重新展开 BAR decode 逻辑)— controller facade 已是 single source of truth,重复浪费 ~50 行
3. **fabric→vram_ctrl 端到端数据路径未代码化** (per handoff §9.2 #9): 4-IP 拓扑已装配,但**只有 Step 1-3 实例化**,**Step 4 数据流(vram_ctrl→fabric→pcie_ctrl)未覆盖**。本 change 落 chip-internal port map (`Fabric2VramCtrlBundle`) 代码化契约
4. **JSON 端口契约仅注释存在**: `dgpu_soc_v2_realip_p2.json:_note` 文字描述 PcieControllerIP + GpuInternalFabric + VramControllerTLM 4-IP 边界,**编译期无校验**,`fabric` 节点误加 `axi_master_out` 端口会被 19 §14.2.3 静默丢弃(per cpptlm-p2-integration-unblock design D7 锁定)

**预期收益**:
- DGpuBoard::mmio_read/write 从 ~120 行 → ~40 行 (移除 6 if 分支,只剩 controller_ip facade + Strangler gate)
- 7 个 routing flag 全部删除 → JSON schema 简化 (顶层 `routing_enabled` 字段退役),**与 PR-B Strangler gate 一致** (per `dgpu_board_shell.cc:336-343` facade verdict 与 expected mirror)
- 端口契约代码化: chip-internal `Fabric2VramCtrlBundle` (新建) + `static_assert` 编译期检查 4-IP 端口名 + 类型 + 方向 (`MasterPort`/`SlavePort`/`InitiatorPort`/`TargetPort`)
- `[pin-contract]` tag 测试覆盖 4-IP 端口契约 + legacy flag 删除回归 (双标签 `[realip][pin-contract]` 防 vacuous pass per handoff §9.2 #10)

---

## What Changes

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/tlm/gpu/dgpu_board_shell.hh` | 改 (删除 7 flag + 3 setter + 1 enum + 2 accessors) | 移除 PciePath enum, 5 routing_enabled flag, mmio_self_drain_enabled, controller_ip setter 保留 (controller_ip_ 是 P2 PR-A 必要) |
| `src/tlm/gpu/dgpu_board_shell.cc` | 改 (mmio_read/write 简化) | 删除 6 个 if 分支 (display/memory/storage/gmmu routing) + pcie_path 加载, 保留 Strangler gate (line 320-343/433-466) |
| `include/bundles/dgpu_fabric_bundles_tlm.hh` | **新** (~60 行) | `Fabric2VramCtrlBundle` (per-module address/data/size/is_write/transaction_id/resp_data/error_code/chip_select, chip-internal 端口契约) |
| `include/chstream_register.hh` | 改 (注册 Fabric2VramCtrlBundle) | `REGISTER_CHSTREAM` 加 bundle registration |
| `include/tlm/cluster/gpu_internal_fabric.hh` | 改 (端口契约 vram_ctrl_out 声明) | 新增 ChPort `vram_ctrl_out` (egress, Fabric2VramCtrlBundle) + 类型契约 static_assert |
| `src/tlm/cluster/gpu_internal_fabric.cc` | 改 (simulate_instantiate 装配 vram_ctrl out) | 遍历 cfg.modules 找 vram_ctrl, 调 `set_vram_target(vram_ctrl)` 程序化桥接 (per Phase 8 HB/RC 先例) |
| `include/tlm/vram_controller_tlm.hh` | 改 (新增 fabric_in 端口 + 契约 static_assert) | 新增 ChPort `fabric_in` (ingress, Fabric2VramCtrlBundle) |
| `include/abi/cpptlm_emulator.h` | **不动** (frozen per ADR-088 §D5, P5 锁定) | 23 ABI 不动 |
| `configs/dgpu_soc_v2_realip_p2.json` | 改 (移除 4 个 routing_enabled 顶层字段) | memory_routing_enabled/storage_routing_enabled/gmmu_routing_enabled/display_routing_enabled 全部删除 (default=false 不再被任何代码消费) |
| `test/test_realip_pin_contract.cc` | **新** | `[realip][pin-contract]` 双标签: 4-IP 端口契约 static_assert + legacy flag 删除回归 + fabric→vram_ctrl 端到端 round-trip |
| `test/test_dgpu_board_no_legacy_flags.cc` | **新** | `[realip][pin-contract]` 双标签: DGpuBoard public API 不含 7 个已删除 flag/setter 编译期验证 + Strangler gate 保留 |
| `test/CMakeLists.txt` | 改 (GLOB 自动收录 2 个新文件) | 无显式修改 (GLOB 自动) |
| `docs/designs/dgpu-soc/architecture.md` §2.4 | 改 (端口契约文档) | 4-IP 边界表新增 chip-internal port map (`fabric.vram_ctrl_out → vram_ctrl.fabric_in`),Fabric2VramCtrlBundle 字段定义 |
| `docs/designs/dgpu-soc/architecture.md` §7.3 | 改 (JSON 层级约束更新) | 顶层 `routing_enabled` 字段退役说明 |
| `AGENTS.md` KEY INVARIANTS | 改 (新增 legacy flag 退役条目) | 锁 4-IP topology 不再依赖 routing flag, 端口契约代码化为 chip-internal 1:1 映射 |

### Frozen surface (0 修改)

- **23 ABI 签名** (`include/abi/cpptlm_emulator.h`) — 0 修改 (frozen per ADR-088 §D5, P5 锁定)
- **4 frozen header** (`pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h`) — 0 diff (本 change 不碰)
- **`PcieControllerIP`/`PcieEndpointIP`/`VramControllerTLM` 类定义** — 仅端口扩展 (新增 ingress/egress),**不动 match_bar/mmio_write/bar_store 行为**
- **GpuInternalFabric.crossbar() 现有行为** (per T22b 锁定 dormant → T25A.1 JSON 自动 attach) — 仅扩展 vram_ctrl_out 端口

---

## Scope

### IN-SCOPE

- **DGpuBoard 7 个 legacy flag 删除**: `PciePath` enum + `pcie_path_` + 5 routing_enabled flags + `mmio_self_drain_enabled` (mmio_self_drain 实际保留,因其是 drain 优化不接 legacy path;但代码注释重新审查确认)
- **`DGpuBoard::mmio_read/write` 简化**: 删除 6 if 分支,保留 `controller_ip_->match_bar` Strangler gate (per dgpu_board_shell.cc:336-343/433-466) + drain 逻辑
- **`Fabric2VramCtrlBundle` 端口契约**: chip-internal wire-protocol (fabric.vram_ctrl_out ↔ vram_ctrl.fabric_in),字段: `address[64]`/`size[8]`/`is_write`/`data[64]`/`transaction_id[32]`/`resp_data[64]`/`error_code[8]`/`chip_select[8]`
- **`GpuInternalFabric::set_vram_target(VramControllerTLM*)` setter**: 程序化桥接 (per Phase 8 HB/RC tick 转发先例 + cpptlm-p2-integration-unblock design D7),JSON 不声明端口 (避免 19 §14.2.3 静默丢弃)
- **`static_assert` 编译期端口契约** (新 `include/utils/port_contract_asserts.hh`): `GpuInternalFabric::vram_ctrl_out` 是 ChInitiatorPort + `VramControllerTLM::fabric_in` 是 ChTargetPort + bundle 类型匹配
- **2 个新测试文件**: `[realip][pin-contract]` 双标签 (per handoff §9.2 #10 vacuous tag 风险)
- **JSON 退役字段**: 移除顶层 4 个 `routing_enabled` 字段 (per spec §7.3 JSON 层级约束更新)

### OUT-OF-SCOPE

- **DGpuBoard::init() 重构** — 本 change 只删 flag, init() 流程逻辑不动 (per design §1)
- **真实 RTL 桥接** (CppHDL/HybridCache) — P13 单独 change
- **PcieAxiAdapter/Axi4CacheAdapter 实施** — cpptlm-p2-integration-unblock (已 Proposed, 异步推进)
- **PCIe 协议层增强** (FC, retry buffer) — Phase 1-3 已完成
- **任何新 ABI 函数追加** (P5 冻结, per ADR-088 §D5)
- **真实驱动 (VFIO/IOMMUFD) 验证** — P2-5/6 范围
- **DGpuBoard 其他 setter** (`set_display_fb_size` / `set_dma_translate_callback` / `set_irq_callback` 等) — 不在本 change 范围, 业务保留
- **legacy `framebuffer_size_bytes` 顶层 override** — P0.5-landing 已 ship, 保留
- **Phase 8 M1 HB↔EP 4 方向 AXI 流** — 已闭环 (per cpptlm-p2-integration-unblock Phase 8 base), 不动

---

## Capabilities

### New Capabilities

- `dgpu-4ip-port-contract`: 4-IP topology 端口契约代码化 (`Fabric2VramCtrlBundle` + static_assert 编译期检查 + chip-internal wire-protocol 静态契约), 让 RTL team 1:1 对照芯片内部连线

### Modified Capabilities

无 (`pcie-ip-integration` / `pcie-axi-datapath-hardening` / `sdma-engine-tlm` / `host-bypass-and-rc` / `dgpu-driver-visible-minimal-soc` spec 不修改 — 本 change 仅实施 legacy flag 删除 + 端口契约, 不改既有 requirement set)

---

## Acceptance Gate (8 项)

- [ ] **G1**: `openspec validate dgpu-realip-pin-contract --strict` PASS
- [ ] **G2**: `[pin-contract]` 标签测试 ≥2 cases PASS (2 个新测试文件, **双标签 `[realip][pin-contract]`** per handoff §9.2 #10 防 vacuous pass)
- [ ] **G3**: DGpuBoard 7 个 legacy flag **编译期删除** (新测试 `test_dgpu_board_no_legacy_flags.cc` 静态断言 public API 不含 `pcie_path_*` / `display_routing_enabled_*` / `memory_routing_enabled_*` / `storage_routing_enabled_*` / `gmmu_routing_enabled_*` / `mmio_self_drain_enabled`)
- [ ] **G4**: `Fabric2VramCtrlBundle` 端口契约代码化 (新 `include/utils/port_contract_asserts.hh` 提供 `static_assert`, `GpuInternalFabric::vram_ctrl_out` ↔ `VramControllerTLM::fabric_in` bundle 类型匹配编译期验证)
- [ ] **G5**: 4-IP topology `[realip][p3]` ≥9 cases 0 回归 (per Phase C 后基线, T23B+T23C+T25A+T25A.1+T25B)
- [ ] **G6**: `[pcie]` 全链路 ≥433 / 36682 cases 0 回归 (per Phase C 后基线, `[controller_ip][p2]` 8/34 + `[fabric]` 7/13 + `[dgpu_soc_timing]` 37/12024 + `[minimal_dgpu_soc]` 12/69 + `[pcie-memory]` 24/69 全 PASS)
- [ ] **G7**: Strangler gate = 2 (per `strings build/bin/cpptlm_tests | grep -c "Strangler gate"`, dgpu_board_shell.cc:336-343 Strangler gate 保留)
- [ ] **G8**: 4 frozen header 零 diff + 23 ABI 零新增 (per `git diff` 验证, `cpptlm_emulator.h` 等不动)

---

## Impact

**新增代码** (~250 行):
- `Fabric2VramCtrlBundle` 字段定义 (~60 行)
- `port_contract_asserts.hh` 编译期契约宏 (~80 行)
- `GpuInternalFabric::vram_ctrl_out` 端口 + setter (~30 行)
- `VramControllerTLM::fabric_in` 端口扩展 (~20 行)
- 2 个新测试文件 (~400 行总计)

**删除代码** (~250 行):
- `DGpuBoard::PciePath` enum (4 lines)
- 5 routing_enabled flag 成员 + 5 setter + 5 accessor (~50 lines)
- `mmio_read/write` 6 个 if 分支 (display/memory/storage/gmmu routing) (~150 lines)
- JSON `_note` 段落对 routing flag 描述 (~30 lines)
- `pcie_path_str` 加载逻辑 (~10 lines)

**净代码量**: ~0 行 (新增 ~250 + 删除 ~250),但**可读性 + RTL team 1:1 对照性大幅提升** (DGpuBoard.cc mmio_read/write 从 ~120 行 → ~40 行)

**下游 unblock**:
- RTL team 1:1 对照 chip-internal fabric→vram_ctrl 连线 (Fabric2VramCtrlBundle 字段 1:1 → 硅片 wire)
- DGpuBoard 瘦身 → 后续 `dgpu-realip-pin-contract` 删除 DGpuBoard mmio_regs_ (`std::map<pair<bar, offset>, vector<uint8_t>>`, 占用 ~50 行) 更彻底
- 与 cpptlm-p2-integration-unblock (已 Proposed) 桥接: fabric→vram_ctrl 端到端数据路径落实,为 P2-7 (cp_attach 真实化) 解锁

**测试覆盖** (Phase C 后基线):

| 标签 | 基线 | 新增预期 | 合计 |
|------|-----|----------|------|
| `[realip][p3]` | 9/122 (T23B/T23C/T25A/T25A.1/T25B) | +2 (T26 facade + dormant) | ≥11 |
| `[realip][pin-contract]` | 0 (新标签) | ≥2 (2 新文件双标签) | ≥2 |
| `[pcie]` | 433/36682 | +0 (本 change 不动 pcie 范围) | ≥433 |
| `[dgpu_soc_timing]` | 37/12024 | +0 (MUST NOT 1 锚点不动) | ≥37 |
| `[minimal_dgpu_soc]` | 12/69 | +0 | ≥12 |
| `[pcie-memory]` | 24/69 | +0 | ≥24 |

---

## Risks

| # | 风险 | 等级 | 缓解 |
|---|------|:----:|------|
| 1 | legacy flag 删除破坏 D-AXI v1.4 B12 B13 测试 (framebuffer_size_/vram_storage_) | 🟡 中 | 锁定 `framebuffer_size_`/`vram_size_`/`vram_storage_`/`attach_vram_for_testing` 不删 (per ADR-DGPU-05/10),**仅删 routing flag** |
| 2 | Fabric2VramCtrlBundle 字段定义错误, 与 CrossbarTLM::CacheReqBundle 不一致 | 🟡 中 | TDD 5 步先写测试, 字段对齐 `cache_bundles_tlm.hh:30-46` (复用 `transaction_id[64]`/`address[64]`/`size[8]`/`is_write`/`data[64]` + 新增 `chip_select[8]`) |
| 3 | DGpuBoard::mmio_read/write 简化破坏 Strangler gate (line 336-343) | 🔴 高 | Strangler gate 保留**编译期** + 编译期 invariant 注释, `test_dgpu_board_no_legacy_flags.cc` 静态断言 Strangler gate 字符串存在 |
| 4 | JSON 顶层 `routing_enabled` 字段删除破坏既有 dgpu_soc_with_pcie_ip.json (per cpptlm-p2-integration-unblock base) | 🟡 中 | `routing_enabled` 字段已 **default=false** + D2 v1.1/D1 v1.1.1 锁定 root cause 4 防劫持 — 删除后 JSON 顶层字段未消费, `validateConfig` 不会失败 |
| 5 | `set_vram_target` 程序化桥接失效 (Phase 8 HB/RC 先例不适用 4-IP composite) | 🟡 中 | TDD 5 步: 新测试 `test_realip_pin_contract.cc` round-trip fabric→vram_ctrl (经 JSON 装配 + set_vram_target + tick), 失败可回滚到 `[realip][p3]` GREEN 基线 |
| 6 | static_assert 编译期契约触发 DGpuBoard 编译失败 | 🟡 中 | static_assert 仅在 `port_contract_asserts.hh` include 时启用, 旧测试不 include, 0 回归 |
| 7 | frozen header 触碰 (per AGENTS.md `cpptlm_emulator.h` 冻结) | 🔴 高 | git diff 严格 verify: 4 frozen header 零 diff, 23 ABI 零新增, G8 显式 gate |
| 8 | OpenSpec Proposed 季度 KPI 触顶 (当前 1/3 baseline + 1 新 = 2/3, 仍合规) | 🟢 低 | 季度复审 baseline 1/3, 新 change 2/3, 2026 Q4 复审前合规 |

---

## 关联

- **前置 change** (archived): `cpptlm-dgpu-realip-refactor` (Phase C, 2026-10-07, tag `dgpu-realip-refactor-completion`)
- **session handoff** (archived 2026-10-07): `docs/superpowers/plans/archive/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md`
- **原始 plan**: `docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` (Phase C Status Update 落)
- **架构文档**: `docs/designs/dgpu-soc/architecture.md` §2.4 (4-IP 边界表) + §7.3 (JSON 层级约束) + 端口契约代码化文档追加
- **设计 ADR**:
  - `ADR-DGPU-05-vram-storage-ownership.md` (framebuffer_ 单一真源)
  - `ADR-DGPU-06-axi-mem-bundle-boundary.md` (AxiMemBundle vs PcieTlpBundle 边界)
  - `ADR-DGPU-07-minimal-soc-evolution-seam.md` (D3 seam 插入点)
  - `ADR-DGPU-08-abi-freeze-policy.md` (23 ABI 冻结)
  - `ADR-DGPU-09-driver-visible-minimal-soc-scope.md` (范围)
- **并行 change** (已 Proposed): `cpptlm-p2-integration-unblock` (Phase P2 集成断点修复, 异步推进)
- **AGENTS.md**: KEY INVARIANTS (framebuffer 自动分配条目 + OpenSpec Proposed ≤3 KPI) + DOC HYGIENE (ArchForge 跨仓约束)

---

## 时间线 (修订)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| 1 | `Fabric2VramCtrlBundle` + `port_contract_asserts.hh` + `GpuInternalFabric::vram_ctrl_out` + `VramControllerTLM::fabric_in` + 测试 (TDD 5 步, 4-IP 端口契约代码化) | 3-4 天 |
| 2 | `DGpuBoard` 7 flag 删除 + `mmio_read/write` 简化 + JSON `routing_enabled` 字段退役 + Strangler gate 保留 | 2-3 天 |
| 3 | `test_realip_pin_contract.cc` + `test_dgpu_board_no_legacy_flags.cc` (2 新测试, 双标签) + `[pin-contract]` 标签验证 | 2 天 |
| 4 | 文档同步 (architecture.md §2.4/§7.3 + AGENTS.md KEY INVARIANTS) + openspec validate + archive | 1 天 |
| **总计** | — | **8-12 天 ≈ 2-2.5 周单人** (含 Phase C 后基线 + Strangler gate 保护 + frozen header 0 diff gate) |

---

**Owner**: CppTLM Team (Sisyphus)
**起点**: `dgpu-realip-refactor-completion` tag (`26583da7`)
**目标 tag**: `dgpu-realip-pin-contract-completion` (落 DoD)
