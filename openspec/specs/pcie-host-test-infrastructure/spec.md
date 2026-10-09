# pcie-host-test-infrastructure Specification

## Purpose
TBD - created by archiving change cpptlm-pcie-host-test-flr-rebar. Update Purpose after archive.
## Requirements
### Requirement: host-pcie-test-stub-lifecycle (v1.1 统一构造签名)

`HostPcieTestStub` SHALL 提供完整 lifecycle, 仓内 in-process 创建, **不依赖 UsrLinuxEmu 跨仓 driver**。

**v1.1 统一构造签名**（修 v1.0 三处不一致）:
```cpp
HostPcieTestStub(DGpuBoard* board, const std::string& config_path);
```

#### Scenario: 统一构造签名 (per AC G2, v1.1 修复)

- **GIVEN** 已 init 的 `DGpuBoard* board` (per `cpptlm_emulator_create("configs/dgpu_*.json")`)
- **WHEN** `HostPcieTestStub stub(board, "configs/dgpu_soc_v2_realip_p2.json")` 实例化（v1.1 统一签名）
- **THEN** `stub.serializer_` 私有成员就绪 (per `PcieTlpSerializer` 既有)
- **AND** `stub.board_` 私有指针 = injected board
- **AND** `stub.config_path_` = `"configs/dgpu_soc_v2_realip_p2.json"`
- **AND** `stub.current_state()` 返回 `IDLE`
- **NOTE**: **v1.1 修 v1.0 三处文本不一致**（design §3.1 vs spec lifecycle vs tasks T1）

#### Scenario: in-process fixture, no external dep

- **WHEN** 测试用例 teardown
- **THEN** stub 析构自动清理所有错误注入点 + state trigger 状态
- **AND** 不留任何全局状态污染后续测试

---

### Requirement: tlp-packet-round-trip (v1.0 保留)

`PcieTlpSerializer` SHALL 提供 TLP packet 序列化 / 反序列化, 复用 `include/bundles/pcie_bundles_tlm.hh::PcieTlpBundle` 作为 wire format (frozen per `cpptlm-emulator-abi` precedent)。

#### Scenario: TLP serializer round-trip (per AC G2)

- **GIVEN** `PcieTlpBundle` 包含完整 TLP header + data payload
- **WHEN** `serialize(bundle) → bytes` → `deserialize(bytes) → bundle'`
- **THEN** `bundle == bundle'` (round-trip 一致, 0 字段丢失)

#### Scenario: 协议层 TLP 类型覆盖

- **WHEN** 测 3 类 TLP: Config Read/Write (Type 0000/0001) + Memory Read/Write (Type 0000/0001 with 64-bit addr) + Completion (Type 1010)
- **THEN** 全部 round-trip PASS, **0 字段错位** (与 `[pcie]` 430/36672 cases 解码格式一致)

---

### Requirement: state-trigger (v1.1 改写: 直接调既有 C++ 入口)

`HostPcieTestStub` SHALL 提供 state trigger, **直接调既有 DGpuBoard C++ 入口** (per CRITICAL C2: 0 新增 ABI).

#### Scenario: trigger_flr() 调既有入口 (per AC G3, v1.1 改写)

- **GIVEN** `HostPcieTestStub stub(board, config)`
- **WHEN** `stub.trigger_flr()` 调用
- **THEN** 直接调既有 `board->trigger_flr()` → `PcieEndpointIP::flr_pf()` → `pool_.flr_pf()`
- **AND** **0 调用 23 ABI**（v1.1 删除 `cpptlm_emulator_trigger_flr()` 新 ABI）
- **AND** **不重建** `DGpuBoard::eq_` (per Step 0 P0 fix)
- **NOTE**: per 双审查: 既有 `flr_pf()` 已实现完整 FLR 状态机, **不新建 PcieFlrHandler**

#### Scenario: trigger_resize_bar() 调既有入口 (per AC G4, v1.1 改写)

- **GIVEN** `HostPcieTestStub stub(board, config)`
- **WHEN** `stub.trigger_resize_bar(2, 0x100000000ULL)` 调用（v1.1 uint64，**修 v1.0 uint32 溢出**）
- **THEN** 直接调既有 `board->resize_bar(2, 0x100000000)` → `PcieEndpointIP::handle_resize_bar(2, 4GB)` → `ResizableBarRuntime::commit()`
- **AND** **0 调用 23 ABI**（v1.1 删除 `cpptlm_emulator_resize_bar()` 新 ABI）
- **AND** `ResizableBar::new_size_bytes_` (uint64) = 0x100000000 (4GB)

#### Scenario: MSI-X handler registration (per AC G3)

- **GIVEN** `HostPcieTestStub stub(board, config)`
- **WHEN** `stub.register_msix_handler(callback)` 调用
- **THEN** callback 被触发, 携带 MSI-X vector index
- **AND** 订阅既有 `DGpuBoard::msix_update_pending` 路径（既有, per `dgpu_board_shell.cc:1125-1128`）

---

### Requirement: extension-to-existing-3-test-suites (v1.0 保留)

`HostPcieTestStub` SHALL 复用现有 3 类测试作为扩展点 (per 用户决策 "复用 + 扩展现有 3 类测试")。

#### Scenario: `[device_perspective]` host-side TLP sender (per AC G5)

- **WHEN** `test_dgpu_pcie_device_perspective.cc` 新增 ≥2 cases 通过 `HostPcieTestStub` (v1.1 统一签名) 发送 host 侧 TLP
- **THEN** device 侧响应 (config read/write + mmio read/write) 与 `HostPcieTestStub` 期望匹配
- **AND** 0 引入 UsrLinuxEmu 跨仓 driver 依赖

#### Scenario: `[driver_visible]` 协议层扩展 (per AC G6, v1.1 改写)

- **WHEN** `test_minimal_soc_driver_visible_e2e.cc` 扩展 HostPcieTestStub trigger_flr/trigger_resize_bar
- **THEN** ≥2 cases PASS
- **AND** **0 引入新 ABI 函数** (per CRITICAL C2: 23 ABI 冻结 + 0 新增)
- **AND** ABI 闭环验证 (23 既有函数 + 0 新增) 0 回归

#### Scenario: `[pcie]` 测试套件协同 (per AC G7)

- **WHEN** `[pcie]` 430/36672 cases 跑全量
- **THEN** 全绿, 0 回归
- **AND** HostPcieTestStub 接入不破既有 `PcieTlpBundle` 解码协议
- **AND** 新增 `[pcie][flr]` 3 cases + `[pcie][rebar]` 3 cases 全绿

---

