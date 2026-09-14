# Tasks: cpptlm-stage-1-4-2-1

> **工期**: 1.5 周 + 0.5-1 周 LTSSM 残余 = **2.0-2.5 周** | TDD 5 步

## §0 残余任务（从 cpptlm-pcie-ep-foundation 迁移）

> **Oracle R-E 修订 2026-09-10**：1.1.5 LTSSM 与 1.4 ASPM 链路状态同域（PCIe Link 层 LTSSM 与 ASPM L0s/L1 共享状态机），并入本 change 实施更合理。

### 任务 0.1：PCIe Link 管理 LTSSM（基础任务 1.1.5）
- [x] **Status**: 设计就绪 (2027-02-09). 现有 LTSSM 11 态已实现在 `PciePhyDigitalCtrl` (`src/tlm/pcie/pcie_phy_digital_ctrl_tlm.cc:88-163`), L0s/L1/L2 enter/exit API 存在 (`enter_l0s/enter_l1/exit_low_power`).
- [ ] **Modify**: `src/tlm/pcie/pcie_phy_digital_ctrl_tlm.{hh,cc}`
  - 新增 `enable_aspm(AspmLevel level)` + `aspm_level_` 成员
  - 新增 `last_traffic_time_` + `enter_state_time_` 计时字段
  - `tick()` 内加 ASPM idle detection (L0 → L0s/L1 转换) + exit-latency countdown (L0s → L0)
- [ ] **Write test**: `test/test_aspm.cc::test_l0s_entry_after_idle_threshold`
- [ ] **Verify fail → implement → Verify pass**: aspm_test 全 PASS
- [ ] **Commit (CppTLM)**: `feat(cpptlm): ASPM idle-timer L0s/L1 自动进出 (基础 1.1.5)`

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

> **Oracle 复审位置（Metis M6 修订 2026-09-10）**：实施 commit 后、docs mirror commit **前**进行。复审发现问题需追加 commit 而非 amend docs。

- [ ] 电源 + P2P 实施质量

## §5 总计

- **CppTLM commits**: 4（2 修复 + 2 entry sync mirror）
- **工期**: 1.5 周
- **Oracle**: 1 次轻量
- **下游**: UE `ue-stage-1-4-2-1-extensions` 集成 + 5.5.9
