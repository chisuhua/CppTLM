# cpptlm-pcie-endpoint-ip-json-config Specification

## Purpose
TBD - created by archiving change 2027-02-09-cpptlm-pcie-endpoint-ip-json-config. Update Purpose after archive.
## Requirements
### Requirement: PcieEndpointIP JSON Configuration Surface

The system **MUST** extend `PcieEndpointIP::attach_composition()` to consume 4 classes of previously-silently-dropped JSON `params` fields, plus emit warnings for unconsumed keys. 系统 **MUST** 支持以下 5 个 Scenario 的行为契约。

#### Scenario: phy_digital presets applied via read-modify-write
- **WHEN** JSON `params.phy_digital` 含 `preset_p=5, preset_np=9, preset_cpl=11, hot_plug_supported=true`
- **AND** `PciePhyDigitalCtrl::config()` 现有 `max_speed == GEN5`（仅测试场景假设）
- **THEN** `attach_composition()` 调用后, `phy->config().preset_P == 5` 且 `max_speed == GEN5`（未被重置）
- **AND** 现有 `max_lanes` / `sr_iov_vf_pool_size` 字段一并保留

#### Scenario: sr_iov.ari_capable enables VF8-15 routing
- **WHEN** JSON `params.sr_iov.ari_capable = true`
- **THEN** `pool_.ari_router().ari_enabled() == true`
- **AND** VF stream_id 8..16 的 TLP 路由不再被 PF 截获（per `pcie_ari_router_tlm.hh:58` 规则）

#### Scenario: msix vectors reconfig per-VF at composition time
- **WHEN** JSON `params.sr_iov.vf_msix_vectors = 4` + `params.transaction_layer.msix_num_vectors = 16`
- **THEN** `pool_.msix_pool().configure_vectors(0, 16)` 被调用（PF slot 0）
- **AND** `pool_.msix_pool().configure_vectors(vf, 4)` 对 vf=1..16 各调用一次
- **AND** 配置完成后 vector index < new_n 可触发 MSI-X, ≥ new_n 静默拒绝

#### Scenario: config_size applies to all 17 config slots with validation
- **WHEN** JSON `params.transaction_layer.config_size = 4096`
- **THEN** `pool_.config_spaces()` 的 17 个 slot 全部 `config_size() == 4096`
- **WHEN** JSON `params.transaction_layer.config_size = 1024`（非法）
- **THEN** warning `"transaction_layer.config_size must be 256 or 4096; got 1024, falling back to 4096"` 记录到 `ep.config_warnings()`
- **AND** 17 slot 落回 4096

#### Scenario: unconsumed JSON keys emit warning to stderr and config_warnings getter
- **WHEN** JSON `params` 含 `"foo_bar": 42`（未知顶层键）或 `"phy_digital": {"pipe_interface": "lightweight_4_signal"}`（未知子键）
- **THEN** `ep.config_warnings()` 含对应 `"unrecognized JSON key '...'"` 消息
- **AND** 同步 `std::cerr` 输出 `"[CPPTLM-WARN] ..."` 行（per LINT005 precedent，`module_factory_validate.cc:249-256`）

---

