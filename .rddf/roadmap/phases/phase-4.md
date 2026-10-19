---
id: phase-4
kind: phase
status: completed
phase_refs: []
主题: PCIe EP SR-IOV VF Pool (Phase 4 微架构)
---

## phase-4: PCIe EP SR-IOV VF Pool (Phase 4 微架构)

> **真源**: [`docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`](../../docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md) (**已迁 ArchForge**)
> **OpenSpec 已交付**: [sriov-vf-pool](../../openspec/specs/sriov-vf-pool/)

### Phase 4: SR-IOV VF Pool + PcieEndpointIP

- **完成日期**: 2026-10 (Oracle PASS)
- **关键模块**:
  - `include/tlm/pcie/pcie_sriov_vf_pool_tlm.{hh,cc}` — 17 端口 PcieEndpointIP (1 PF + 16 VF)
  - `include/tlm/pcie/pcie_endpoint_ip.{hh,cc}` — 整合模块; Phase 2 基类 SimModule (REGISTER_MODULE)
  - `include/tlm/pcie/pcie_config_space_per_vf_tlm.hh` — per-VF Config Space
  - `include/tlm/pcie/pcie_msix_per_vf_tlm.hh` — per-VF MSI-X
  - `include/tlm/pcie/pcie_completion_tracker_tlm.hh` — NP↔CplD trans_id 关联
  - `include/tlm/pcie/pcie_ari_router_tlm.hh` — ARI 路由
- **关键能力**: 1 PF + 16 VF + per-VF Config/MSI-X + ARI 路由 + NP↔CplD 追踪
- **关联 commit**: `4e9564e` `478cdd9` `a442b65` `536dbfc` `6ebbd7d`

### 测试覆盖

- `[pcie]` + `[sriov]` 标签下 Phase 4 子测试 (`test_pcie_sriov_*.cc` + `test_pcie_endpoint_ip_*.cc`)
- 当前 baseline: 1241 cases / 47634 assertions / 2 known fail