# Tasks: cpptlm-stage-1-4-2-1

> **工期**: 1.5 周 | TDD 5 步

## §1 阶段 1.4 电源管理（0.5 周）
- [ ] **写失败测试**: `test_pm_capability` + `test_power_state_transition` + `test_aspm`
- [ ] **实施**: `pcie_config_space_per_vf_tlm.cc` PM Cap
- [ ] **改造**: `pcie_endpoint_ip.cc` + `dgpu_soc.cc` D0/D3 状态机 + ASPM
- [ ] **Verify**: pm_capability_test + power_state_transition_test + aspm_test 全 PASS
- [ ] **Commit**: `feat(cpptlm): PM D0/D3 + ASPM (stage 1.4)`

## §2 阶段 2.1 P2P + Resizable BAR（1 周）
- [ ] **写失败测试**: `test_p2p_dma` + `test_acs_capability` + `test_resizable_bar`
- [ ] **实施**: `pcie_bypass_mux.cc` + `pcie_ari_router_tlm.cc` P2P 路由
- [ ] **改造**: `pcie_config_space_per_vf_tlm.cc` Resizable BAR Cap
- [ ] **Verify**: p2p_dma_test + acs_test + resizable_bar_test 全 PASS
- [ ] **Commit**: `feat(cpptlm): P2P + Resizable BAR (stage 2.1)`

## §3 双仓 entry sync
- [ ] **UE entry §12** v0.10/v0.11
- [ ] **CppTLM 18-doc mirror** 同步

## §4 Oracle 复审（1 次轻量）
- [ ] 电源 + P2P 实施质量

## §5 总计

- **CppTLM commits**: 4（2 修复 + 2 entry sync mirror）
- **工期**: 1.5 周
- **Oracle**: 1 次轻量
- **下游**: UE `ue-stage-1-4-2-1-extensions` 集成 + 5.5.9
