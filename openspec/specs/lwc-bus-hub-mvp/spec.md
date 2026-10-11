# lwc-bus-hub-mvp Specification

## Purpose
TBD - created by archiving change cpptlm-lwc-bus-hub-mvp. Update Purpose after archive.
## Requirements
### Requirement: lwc-bus-hub-core (新增)

`LwcBusHub` SHALL 提供统一的轻量级总线中心 (1 Hub + 4-32 IP port, 树形/星形结构), 与既有 `SocInternalNoc` (mesh) 正交分层. Hub SHALL 同时支持 `CrBundle` 与 `DoorbellBundle` 2 种异构 Bundle 类型路由 (per §0.1.1 修复路线图).

#### Scenario: LwcBusHub 创建 + 4 port 默认配置 (per AC-LWC-2.1)

- **GIVEN** `LwcBusHub lwc("lwc", &eq);` 实例化
- **WHEN** 不调 `register_*_target()`
- **THEN** `routed_count_ == 0`, `dropped_count_ == 0`, `cr_targets_.size() == 0`, `db_targets_.size() == 0`
- **NOTE**: 0 port 是合法状态 (test scenario edge case)

#### Scenario: 4 IP port 配置 + routing 正确分发 (per AC-LWC-3.1)

- **GIVEN** 4 个 IP 注册 (gmmu cr + sdma db + cpu0 cr + msi-x cr)
- **WHEN** `lwc.cr_write(tx_to_gmmu)` 调用 (target_name="gmmu")
- **THEN** `lwc.routed_count_ == 1`
- **AND** `gmmu.cr_in().valid() == true` (GMMU 收到 CrBundle)
- **AND** 其他 3 IP `cr_in().valid() == false` (不串扰)

#### Scenario: Unknown target routing → dropped (per AC-LWC-3.2)

- **GIVEN** 4 个 IP 注册, 全部 target
- **WHEN** `lwc.cr_write(tx_to_unknown_ip)` 调用 (target_name="nonexistent")
- **THEN** `lwc.dropped_count_ == 1`
- **AND** `routed_count_ == 0`
- **AND** 任何 IP 的 `cr_in().valid() == false` (per graceful degradation)

#### Scenario: 32 port 上限 + 拒绝 port 33 注册 (per AC-LWC-2.3)

- **GIVEN** `LwcBusHub lwc("lwc", &eq);`
- **WHEN** 注册 32 个 IP 后, 第 33 次 `register_cr_target`
- **THEN** 返回 `false` 或抛 `std::overflow_error` (per design 决定); `cr_targets_.size() == 32`
- **NOTE**: Hard Invariant H-B1 — 限制防止资源耗尽

#### Scenario: 异构 Bundle 路由分发正确 (per AC-LWC-4.1)

- **GIVEN** GMMU 注册 cr + SDMA 注册 doorbell + 1 IP 同时注册 cr + doorbell (异构)
- **WHEN** `lwc.cr_write(tx_to_gmmu)` + `lwc.doorbell_write(tx_to_sdma)` 双发
- **THEN** `gmmu.cr_in().valid() == true` (仅 CrBundle 流入)
- **AND** `sdma.doorbell_in().valid() == true` (仅 DoorbellBundle 流入)
- **AND** 同一 IP (异构注册) 收到对应类型的 Bundle (无串扰)

---

### Requirement: cr-bundle-wire-format (新增)

`CrBundle` SHALL 暴露 8 字段 (`address`/`wdata`/`rdata`/`write`/`ready`/`valid`/`error`/`transaction_id`), 编译期 `BUNDLE_FIELD_WIDTH` 锁 max width 64-bit.

#### Scenario: CrBundle 编译期契约 (per AC-LWC-2.2 + 2026-10-06 P0 fix)

- **GIVEN** `static_assert(BUNDLE_FIELD_WIDTH(CrBundle, address, 32));` 编译
- **THEN** 编译通过 (8 字段宽度锁符合协议)
- **AND** 任何字段 width 偏离 → 编译失败

#### Scenario: CrBundle 字段语义 (per design §2)

- `address` (32-bit): register file byte offset (与 PCI BAR0 4KB 子窗口对齐)
- `wdata` (32-bit): 写数据, 仅 `write==1` 时有效
- `rdata` (32-bit): 读 response, 仅 `write==0` 时有效
- `write` (1-bit): 1=write, 0=read
- `ready` (1-bit): ready (从 IP 返)
- `valid` (1-bit): valid (从 source 发)
- `error` (2-bit): 0=OK, 1=DECERR, 2=SLVERR (per D-AXI v1.4 B9 SLVERR propagation)
- `transaction_id` (8-bit): APB-lite 同步 ready/valid 单 outstanding

---

### Requirement: doorbell-bundle-wire-format (新增)

`DoorbellBundle` SHALL 暴露 4 字段 (`wdata[32]`/`ring_id[8]`/`posted[1]`/`ready[1]`), max width 64-bit.

#### Scenario: DoorbellBundle 4 字段契约 (per design §3)

- **GIVEN** `static_assert(BUNDLE_FIELD_WIDTH(DoorbellBundle, ring_id, 8));` 编译
- **THEN** 编译通过
- **AND** 字段 width 锁定 ≤ 64-bit

#### Scenario: DoorbellBundle posted write 语义 (per design §3)

- `wdata` (32-bit): 4-byte doorbell wptr (PCIe posted write payload, max 4-byte dword)
- `ring_id` (8-bit): doorbell ring ID (0-255, 本 SoC 仅 0 = BAR1 ring 0)
- `posted` (1-bit): 1=posted (PCIe posted write, 不需 completion), 0=non-posted (需 completion TLP)
- `ready` (1-bit): SDMA ring consumer ready

---

> **[v1.0.1 B-H1 修订]**: 原 `pcie-ep-cr-emission (新增)` requirement **整段删除**. `PcieEndpointIP` 在本 change 中**0 修改** (既无新增 API 也无删除). DGpuBoard `mmio_write` 直调 `lwc_hub_->cr_write(tx)`, 经 `pending_cr_queue_` mutex-queue 跨 host↔sim 线程 (per v1.0.1 B-B2). 替代原 design §5 的 `emit_cr_write()` 描述.

**替代 Requirement**: 见 `cpptlm-pcie-tlp-entry-layer` Change D Phase 1.3 (PcieEndpointIP::submit_tlp() 整合 LWC 总线入口, defer to Phase 1).

**冻结面要求 (per v1.0.1 B-H1 + B-H5)**: 本 SHALL 锁定:
- `include/tlm/pcie/pcie_endpoint_ip.hh` 在本 change 实施前后必须 `git diff HEAD -- ...` **输出为空** (0 改动, frozen surface diff 0, 既有 17 端口 signature 全保留)
- 既有 17 端口 signature 零 diff

---

### Requirement: gmmu-cr-in-handling (新增, Gap H 核心)

`GmmuTLM` SHALL 加 `cr_in` SlavePort (ChStreamInputStreamAdapter<CrBundle>), 在 `tick()` 处理 CrBundle transaction, 把寄存器数据 (PT_BASE_LO/HI/ENABLE) 写入内部状态 (替换 4 处 `set_pt_base_*` 直调).

#### Scenario: GMMU 收 CrBundle 写 PT_BASE_LO (per Gap H 修复)

- **GIVEN** `gmmu.cr_in().valid() == true` 且 CrBundle `address=0, wdata=0xDEADBEEF, write=1, valid=1`
- **WHEN** `gmmu.tick()` 调用
- **THEN** `gmmu.pt_base_lo_ == 0xDEADBEEF`
- **AND** `cr_in().consume()` 调用
- **AND** `pt_base() == 0x00000000DEADBEEFULL` (组合 pt_base_lo+hi)
- **NOTE**: 替换 `dgpu_board_shell.cc:467` `gmmu_->set_pt_base_lo(val32)` 直调

#### Scenario: GMMU 收 CrBundle 写 PT_BASE_HI (per Gap H 修复)

- **GIVEN** CrBundle `address=4, wdata=0x12345678, write=1, valid=1`
- **WHEN** `gmmu.tick()` 调用
- **THEN** `gmmu.pt_base_hi_ == 0x12345678`
- **NOTE**: 替换 `dgpu_board_shell.cc:469` `gmmu_->set_pt_base_hi(val32)` 直调

#### Scenario: GMMU 收 CrBundle 写 GMMU_CTRL (per Gap H 修复)

- **GIVEN** CrBundle `address=8, wdata=0x1, write=1, valid=1`
- **WHEN** `gmmu.tick()` 调用
- **THEN** `gmmu.enabled_ == true`
- **NOTE**: 替换 `dgpu_board_shell.cc:471` `gmmu_->set_enabled((val32 & 1u) != 0)` 直调

#### Scenario: 旧 `set_pt_base_*` setter 标记 deprecated (per H-B3 backward compat)

- **GIVEN** 编译时调用 `gmmu.set_pt_base_lo(val32)`
- **THEN** 编译器 emit `[[deprecated]]` warning: "use CrBundle via cr_in()"
- **AND** 行为 fallback 到直接赋值 (per H-B3 兼容窗口)
- **NOTE**: 0.5d 兼容窗口 (per design §backward compat); Phase 3.5 后可正式删除

---

### Requirement: sdma-doorbell-in-handling (新增, Gap J 核心)

`SdmaEngineTLM` SHALL 加 `doorbell_in` SlavePort (ChStreamInputStreamAdapter<DoorbellBundle>), 在 `tick()` 处理 DoorbellBundle transaction, 把 wptr 写入 ring consumer (替换 BAR1 0x10010000 直调).

#### Scenario: SDMA 收 DoorbellBundle 写 ring 0 (per Gap J 修复)

- **GIVEN** `sdma.doorbell_in().valid() == true` 且 DoorbellBundle `wdata=42, ring_id=0, posted=1, ready=1`
- **WHEN** `sdma.tick()` 调用
- **THEN** `sdma.ring_consumer_wptr_ == 42`
- **AND** ring consumer 触发 1 次 wptr 提交
- **NOTE**: 替换 `dgpu_board_shell.cc:537` `sdma_engine_->mmio_write(1, kBar1DoorbellOffset, 42)` 直调

#### Scenario: 旧 `mmio_write` 兼容 (per H-B3)

- **GIVEN** 编译时调用 `sdma.mmio_write(1, kBar1DoorbellOffset, wptr)`
- **THEN** 编译器 emit `[[deprecated]]` warning
- **AND** 行为 fallback 到直接 ring 提交

---

### Requirement: mmio-read-cr-bus-handling (新增, Gap K 核心)

`DGpuBoard::mmio_read()` SHALL 删除 `mmio_regs_` write-mirror + `inject_q_` async drain, 改走同步 CR-Bus AR transaction + ready/valid handshake (50ms = implicit PCIe completion timeout).

#### Scenario: MMIO Read BAR0 → 同步 CR-Bus AR (per Gap K 修复)

- **GIVEN** `mmio_read(0, 0x00, buf, 4)` (BAR0 PT_BASE_LO read, 4 bytes)
- **AND** LWC hub + GMMU cr_in 已注入
- **WHEN** 调用
- **THEN** 内部发 CrBundle `address=0, write=0, valid=1, transaction_id=0` 到 LWC hub
- **AND** 等 LWC hub routing → GMMU.cr_in().data() → GMMU.tick() 处理 → rdata 返
- **AND** ready/valid handshake 同步完成 (≤ 50ms, implicit PCIe completion timeout)
- **AND** `memcpy(buf, &rdata, 4)` 返回 0
- **NOTE**: 替换 `dgpu_board_shell.cc:367-388` async inject_q + `:495-501` write-mirror + `:526-529` roundtrip

#### Scenario: MMIO Read timeout (per R5 缓解)

- **GIVEN** `mmio_read(0, 0x00, buf, 4)`
- **AND** target IP 不响应 (timeout)
- **WHEN** 调用 (内部 wait_for(kMmioWaitTimeout))
- **THEN** 返回 `-ETIMEDOUT` (= implicit PCIe completion timeout)
- **AND** 不挂死 (per H-B8)

#### Scenario: 删 `mmio_regs_` write-mirror (per H-B2 + H-B3 Hard Invariant)

- **GIVEN** `dgpu_board_shell.cc::mmio_regs_` (std::map)
- **WHEN** 本 change 实施完成
- **THEN** `mmio_regs_` **删除** (per Gap K 0 callback Hard Invariant)
- **NOTE**: write-mirror 行为本身违反 §0.1 原则 3.1, 必须删除

---

### Requirement: framework-heterogeneous-adapter (新增, infrastructure)

`framework/stream_adapter.hh` SHALL 加 `MultiPortHeterogeneousBundleAdapter<BundleTs...>` 模板, 支持同 IP 上挂多个不同 Bundle 类型的 port (per D-AXI v1.5 B25 决策沿袭 + §0.1.1 LWC 总线原则).

#### Scenario: 模板支持 N 个异构 Bundle (per AC-LWC-1.1)

- **GIVEN** `MultiPortHeterogeneousBundleAdapter<CrBundle, DoorbellBundle> adapter(
    std::tuple<InputStreamAdapter<CrBundle>*, InputStreamAdapter<DoorbellBundle>*>(cr_in, db_in));`
- **WHEN** `adapter.tick()` 调用
- **THEN** 内部 std::get<0>(inputs_)→InputStreamAdapter<CrBundle> 操作
- **AND** std::get<1>(inputs_)→InputStreamAdapter<DoorbellBundle> 操作
- **AND** 两个 port 独立处理, 无 type erasure overhead

#### Scenario: 编译期 type check (per H-B7)

- **GIVEN** 模板 `MultiPortHeterogeneousBundleAdapter<CrBundle, DoorbellBundle>`
- **WHEN** 编译时
- **THEN** 类型擦除检查 → 编译失败若传入不符合 BundleTs... 元组
- **NOTE**: per H-B8 异构 Bundle adapter 必须 support 异构注入同 port

---

### Requirement: lwc-frozen-surface-zero-diff (新增, Hard Invariant)

本 change SHALL 保持 23 ABI 签名零变更 + 5 frozen headers 零 diff.

#### Scenario: 23 ABI 签名零变更

- **GIVEN** `git diff HEAD -- include/abi/` 检查
- **THEN** output 为空
- **NOTE**: per ADR-DGPU-08 §D5 + AGENTS.md H3

#### Scenario: 5 frozen headers 仅新增 API (per H-B5 + H-B8)

- `include/abi/cpptlm_emulator.h`: 0 diff
- `include/tlm/gpu/pcie_endpoint_tlm.h`: 0 diff
- `include/tlm/gpu/pcie_display_device.hh`: 0 diff
- `include/tlm/pcie/pcie_bundles_tlm.hh`: 0 diff
- `include/tlm/pcie/pcie_endpoint_ip.hh`: 0 改动 (per v1.0.1 B-H1, 整段移除; quasi-frozen 0)

---

### Requirement: lwc-regress-6-suite-zero (新增)

本 change SHALL 保持既有 6 测试套件 0 回归 (per §0.2 DoD).

#### Scenario: 既有 6 套件全绿

- `[minimal_dgpu_soc]` (12 cases / 66 assertions): ✅ Pass (BAR 路由行为不变, 仅 internal path 改变)
- `[driver_visible]` (2 cases / 29 assertions): ✅ Pass (driver 视角不变)
- `[dgpu_soc_timing]` (37 cases / 12024 assertions): ✅ Pass (timing-mode 独立)
- `[pcie-memory]` (17 cases / 48 assertions): ✅ Pass (per Change A 协同, port 1 GMMU 不变)
- `[pcie]` (430 cases / 36672 assertions): ✅ Pass
- `[realip]` (22 cases / 193 assertions): ✅ Pass

#### Scenario: Change A 协同 (Phase 0 并行)

- `[vram_controller][functional]` (Change A 10 cases): ✅ Pass
- **NOTE**: Change A 走 `pcie_memory.handle_slave_port` 数据路径; Change B 走 `DGpuBoard::mmio_*` 寄存器路径; **不同函数, 0 冲突**

---

