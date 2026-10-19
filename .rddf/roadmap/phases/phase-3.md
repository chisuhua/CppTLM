---
id: phase-3
kind: phase
status: completed
phase_refs: []
主题: PCIe EP 链路层 + 编码 + PHY (Phase 1-3 微架构)
---

## phase-3: PCIe EP 链路层 + 编码 + PHY (Phase 1-3 微架构)

> **真源**: [`docs/architecture/19-pcie-ip-microarchitecture.md`](../../docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md) (**已迁 ArchForge** 独立仓, per AGENTS.md §PHASE STATE)
> **OpenSpec 已交付**: [Phase 1-3 sub-specs](../../openspec/specs/) (link-layer-and-fc + 130b-encoding + phy-digital-ctrl)

### Phase 1: PCIe Link Layer + FC Token Bucket

- **完成日期**: 2026-06 (Oracle PASS)
- **关键模块**: `include/tlm/pcie/pcie_link_layer_tlm.{hh,cc}`
- **关键能力**: 链路层 + DLLP + FC Token Bucket (Posted/Non-Posted/Completion 3 类)
- **关联 commit**: `e9b1b..`

### Phase 2: 128b/130b Encoding Latency Model

- **完成日期**: 2026-08 (Oracle PASS)
- **关键模块**: `include/tlm/pcie/pcie_encoding_latency_model.hh` + `include/tlm/pcie/pcie_link_phy_mux_tlm.{hh,cc}` (★ Phase 1/2 LL+PHY+Mux composite, 17 端口)
- **关键能力**: 128b/130b 编码延迟模型 + 17 端口 composite
- **关联 commit**: `8d1f1d5` `b21d290`

### Phase 3: PHY Digital Ctrl + Bypass Mux

- **完成日期**: 2026-09 (Oracle 9/10 复评 PASS)
- **关键模块**: `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.{hh,cc}` + `include/tlm/pcie/pcie_bypass_mux.{hh,cc}` (3 态模式切换: Full/Bypass/Partial)
- **关键能力**: PHY 数字控制 (LTSSM 11 态) + 3 态 Bypass Mux
- **关联 commit**: `05be913..bac9267`

### 测试覆盖

- `[pcie]` 标签下 Phase 1-3 子测试 (`test_pcie_link_layer_*.cc` + `test_pcie_phy_digital_*.cc` + `test_pcie_bypass_*.cc`)
- 当前 baseline (实测 per AGENTS.md): 1241 cases / 47634 assertions / 2 known fail