# cpptlm-dgpu-pcie-phy-digital-ctrl: PHY Digital Ctrl + Bypass Mux 3态

> **状态**: 📋 Proposed — 2026-10-06 · **工期**: 4 周(W5-W8,与 P2 并行)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置**: Phase 1 (11 commits) + Phase 2 (3 commits) 已完成
> **Phase 2 评审发现**: 3 个 Critical Issues(Rate switch 未集成、Rx wire-busy advance bug、GEN1=2 精度)— 本 change 必须先解决 #1 #2,#3 留 Phase 7

## Why

Phase 1+2 实现 Link Layer + 编码延迟建模,但链路建立/训练/热插拔/速率切换完全缺失。本 Phase 实现:
- **§1 PHY Digital Control**: LTSSM 11 主状态机(Detect/Polling/Configuration/Recovery/L0/L0s/L1/L2/Disabled/Loopback/Hot_Reset)+ Gen3+ 均衡协商(TS1/TS2 + 8 Preset)
- **§5 Bypass Mux**: 3 态模式切换(Full/Bypass/Partial)— DRAIN_OR_ABORT 策略
- **修复 Phase 2 评审 #1 (Rate switch 集成)**: PcieLinkLayer 加 `trigger_rate_switch(Rate from, Rate to)` API,PHY Digital Ctrl 调用
- **修复 Phase 2 评审 #2 (Rx wire-busy advance bug)**: rx_tlp_from_host 先 FC check 再 advance busy

## What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh` + `.cc` | **新**: LTSSM FSM + Gen3+ 均衡 + 速率切换 + 热插拔 |
| `src/tlm/pcie/pcie_link_layer_tlm.cc` | **改**: `trigger_rate_switch` + Rx wire-busy 修复 |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | **改**: wire BypassMux 注入 composition |
| `include/tlm/pcie/pcie_bypass_mux.hh` + `.cc` | **新**: 3 态 Bypass Mux + DrainPolicy |
| `test/test_pcie_phy_digital_ltssm.cc` | **新**: LTSSM 11 主状态转换 |
| `test/test_pcie_phy_digital_equalization.cc` | **新**: Gen3+ 均衡协商 |
| `test/test_pcie_phy_digital_hotplug.cc` | **新**: 热插拔 + Surprise Removal (Q14) |
| `test/test_pcie_phy_digital_rate_switch.cc` | **新**: Rate switch 触发 → link 不可用 + 延迟 |
| `test/test_pcie_bypass_mux.cc` | **新**: Bypass 3 态 + apply_mode 状态清理 |
| `test/test_pcie_link_layer_rx_no_double_charge.cc` | **新**: Rx FC-reject 不双倍计费 |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 注册新源 + 6 个 ctest |

### Bypass Mux 3 态(per Oracle Top-7 + Q7)

- `mode=Full`: §1 PHY → §2 Link Layer → §3 TL → §6 AXI(完整 PCIe 链路)
- `mode=Bypass`: §3 TL → §6 AXI(跳过 PHY+LinkLayer,快速软件 bring-up)
- `mode=Partial`: §2 Link Layer → §3 TL → §6 AXI(跳过 §1 PHY,保留 LinkLayerFC/DLLP)

**apply_mode 清理**(10 步,per Oracle Top-7):
1. 通知对端模式切换
2. 暂停 DLLP/TLP 传输
3. 处理 in-flight TLP(DRAIN 或 ABORT)
4. Retry Buffer 清到 ack seq
5. seq# 计数器重置
6. FC Token Bucket 重置
7. Partial 模式守卫
8. MSI-X pending 清理
10. 通知对端切换完成 + 恢复传输

## Acceptance Gate

| Gate | 验证 |
|---|---|
| **P3-G1** | LTSSM 11 主状态转换测试 PASS |
| **P3-G2** | Gen3+ 均衡协商测试 PASS |
| **P3-G3** | 热插拔信号 + Surprise Removal 测试 PASS |
| **P3-G4** | Bypass Mux 3 态切换 + 状态清理 PASS |
| **P3-G5** | Rate switch 集成:触发 → link down 期间 wire 不可用 + 延迟注入 |
| **P3-G6** | Rx wire-busy 修复:FC reject 不双倍计费 |
| **P3-G7** | 全量 ctest 无回归(P1+P2 11+19=30 测试仍 PASS) |

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: LTSSM 异步 vs TLM 同步 | EventQueue 内部状态机 + tick() 驱动 |
| R2: Bypass 状态泄漏 | apply_mode 10 步强制清理 |
| R3: 23 ABI 破坏 | 不修改 `include/tlm/gpu/pcie_endpoint_tlm.h` / `include/abi/cpptlm_emulator.h` |
| R4: 旧类成员布局变化 | Phase 1 已经修改过 .hh 私有成员,继续加私有不破坏 ABI(23 ABI 是 C extern) |