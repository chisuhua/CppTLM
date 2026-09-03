# phy-digital-ctrl Specification

## Purpose
TBD - created by archiving change 2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl. Update Purpose after archive.
## Requirements
### Requirement: phy-digital-control

PciePhyDigitalCtrl **SHALL** 建模 LTSSM 11 主状态机训练序列 (Detect → Polling → Configuration → L0),**NOT** 训练直跳 L0。Hot_Reset 作为 Recovery 内子状态 (Recovery + PERST# assert 进入);Disabled / Loopback 提供显式进入 API。

理由:Oracle C3 评审发现训练直跳 L0、11 态仅 6 态可进入,状态机无真实转换表。

范围 (Oracle C3/C4 修订):
- `start_link_training()` 分步: Detect → Polling → Configuration → L0 (每 tick 一状态)
- `advance_training()` 显式推进训练序列 (返回是否到达 L0)
- `enter_disabled()` / `enter_loopback()` / `enter_hot_reset()` 显式 API
- Hot_Reset 进入路径: Recovery 内 PERST# assert (signal_perst 期间)
- 低功耗 L0s/L1/L2 与既有语义一致 (exit_low_power 恢复)

#### Scenario: LTSSM 训练序列 (Detect → Polling → Configuration → L0)

- **WHEN** 调用 `start_link_training()`
- **THEN** state == Detect (非直跳 L0) 且 link_up == false
- **AND** 每次 `advance_training()` 推进一状态: Polling → Configuration → L0
- **AND** 到达 L0 时 link_up == true,返回 true

#### Scenario: Hot_Reset 可达 (Recovery 内 PERST#)

- **WHEN** 链路在 Recovery (速率切换中) 且 PERST# assert
- **THEN** state == Hot_Reset,link_up == false,in_recovery == true

#### Scenario: Disabled / Loopback 显式可达

- **WHEN** 调用 `enter_disabled()` / `enter_loopback()`
- **THEN** state == Disabled / Loopback 且 link_up == false

### Requirement: bypass-mux-3-mode

PcieEndpointTLM TLP 入口 **SHALL** 按 Bypass Mux mode 分派数据路径: Full → 过 PcieLinkLayer (FC/DLLP/rate-switch); Bypass → 短路链路层直送事务层; Partial → 保留 LL FC/DLLP 但跳过 PHY 阶段。

理由:Oracle C1 评审发现 mode() 零消费——Bypass 声称"跳过 PHY+LL"但 TLP 无条件走 PcieLinkLayer,三态对仿真行为零影响。

范围 (Oracle C1 修订):
- `src/tlm/gpu/pcie_endpoint_tlm.cc` TLP 入口按 `mux->mode()` 分派
- Full / Partial / 无 mux: 走 `ll->rx_tlp_from_host(req)` (FC 门控 + Rx ACK)
- Bypass: 短路 LL,TLP 直接送事务层

#### Scenario: Full 模式 TLP 经 LL (FC 反压)

- **WHEN** mode == Full 且 FC 容量 = 1,连续注入 2 个 MMIO_WRITE
- **THEN** 第 1 个消费 (FC 1→0),第 2 个反压 (req 保持 pending)
- **AND** 证明走了 LL 的 FC 门控

#### Scenario: Bypass 模式短路 LL (FC = 0 仍接受)

- **WHEN** mode == Bypass 且 FC 容量 = 0,注入 MMIO_WRITE
- **THEN** TLP 立即被接受并落写 (绕过 LL FC 检查)

#### Scenario: Partial 模式 LL 仍工作

- **WHEN** mode == Partial 且 FC 容量 = 1
- **THEN** 与 Full 一致: FC 反压生效 (LL 保留)

#### Scenario: 模式切换立即生效

- **WHEN** Full 模式下 TLP 因 FC 反压 pending,随后 `apply_mode(Bypass)`
- **THEN** 下一 tick pending TLP 被接受 (无需重建/重注入)

### Requirement: rate-switch-integration

速率切换完成后 PcieLinkLayer 编码延迟模型 **SHALL** 同步到新速率,后续 TLP 延迟按新 Gen 计费。

理由:Oracle C2 评审发现 `trigger_rate_switch` 只设 rate_switching_+ready_ns,从不调用编码延迟更新——切换完成后 enc_rate_ 仍是旧速率,"Rate Switch" 实质是"速率盲区"。

范围 (Oracle C2 修订):
- `PcieLinkLayer::on_rate_switch_complete(Rate)` 在编码已启用时更新 enc_rate_
- `PciePhyDigitalCtrl::tick()` 完成 Recovery 后调用 (每 tick 检测 ready)

#### Scenario: 切换完成后编码延迟按新速率

- **WHEN** 编码在 GEN3 启用 (16 lanes = 8ns/128B),随后 GEN3→GEN5 切换完成
- **THEN** `ll.encoding_rate() == GEN5` 且后续 TLP block_latency 按 GEN5 (2ns @16 lanes) 计费

#### Scenario: 未启用编码时切换不意外开启

- **WHEN** 编码未启用 (enc_enabled_ == false) 且速率切换完成
- **THEN** `encoding_latency_enabled()` 仍为 false (仅更新,不启用)

