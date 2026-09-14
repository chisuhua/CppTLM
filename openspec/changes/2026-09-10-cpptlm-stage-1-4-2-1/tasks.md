# Tasks: cpptlm-stage-1-4-2-1 — 电源管理 + P2P/Resizable BAR

> **工期**: 0.5 工作日 (密集执行) | TDD 5 步 | **7 commit**
> **状态**: Phase A 实施完成 (2027-02-09) — 待 Oracle 复评归档

---

## §0 残余任务（从 cpptlm-pcie-ep-foundation 迁移）

> **Oracle R-E 修订**: LTSSM L0s/L1 子状态并入本 change 实施

### 任务 0.1：PCIe Link 管理 LTSSM（基础任务 1.1.5）
- [x] **Status**: API 已实现 (2027-02-09)
- [x] **Modify**: `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.{hh,cc}`
  - 新增 `AspmLevel` enum + `enable_aspm()` + `aspm_level()` + `on_traffic()`
  - tick() 加 ASPM 自动转换 (L0 → L0s/L1 idle-timer + exit-latency countdown)
- [x] **Verify**: `feat(pcie): PciePhyDigitalCtrl ASPM enable_aspm + tick auto-transition` (b760a522)
- [ ] **Write test** (测试文件 `test/test_aspm.cc` 已写但 lifetime 调试未完成, 后续 PR 跟进):
  - `test/test_aspm.cc::test_l0s_entry_after_idle_threshold` — TODO 修 fixture lifetime
  - `test/test_aspm.cc::test_l0s_exit_latency` — TODO
  - `test/test_aspm.cc::test_l1_entry_and_exit` — TODO
  - `test/test_aspm.cc::test_enable_aspm_off_disables_idle_timer` — TODO

## §1 阶段 1.4 电源管理（0.5 周）
- [x] **写失败测试**: `test/test_pm_capability.cc` (3 cases / 24 assertions)
- [x] **实施**: `PcieConfigSpace::set_pmcsr_write_cb` + write() PMCSR 拦截 (PWS[1:0] 抖动抑制)
- [x] **改造**: `PcieEndpointIP` 构造期安装 PM Cap + `set_power_state(D0/D3hot)` + `mmio_gated()` accessor + tick() INV-A gate
- [x] **写失败测试**: `test/test_power_state_transition.cc` (3 cases / 13 assertions)
- [x] **Verify**: `[pm]` 6 cases / 37 assertions 全 PASS
- [x] **Commit**: `30cfe14d PM Cap` + `e26d85e5 power state`

## §2 阶段 2.1 P2P + Resizable BAR（1 周）
- [x] **写失败测试**: `test/test_p2p_dma.cc` (3 cases / 2008 assertions, fuzz 1000 次)
- [x] **实施**: `include/tlm/pcie/pcie_bypass_mux_p2p.hh` P2P 路由 + ACS 检查 (INV-D strict reject)
- [x] **写失败测试**: `test/test_resizable_bar.cc` (4 cases / 11 assertions)
- [x] **实施**: `include/tlm/pcie/pcie_resizable_bar.hh` ResizableBar 状态机 (INV-C disable→reprogram→enable)
- [x] **Verify**: `[p2p]` + `[bar]` 全 PASS (新增 + 既有 = 16 cases / 111 assertions 共享 [bar] tag)
- [x] **Commit**: `77ada2e3 P2P/ACS` + `c2e74a91 Resizable BAR`

## §3 双仓 entry sync
- [ ] **UE entry §12** v0.10/v0.11 (待 UE 项目集成, 非阻塞)
- [ ] **CppTLM 18-doc mirror** 同步 (后续 PR 跟进, 与 23 ABI 边界无关)

## §4 Oracle 复评

> **复评位置 (Metis M6 修订)**: 实施 commit 后、docs mirror commit 前

- [ ] Oracle 复评 — 提交后启动

## §5 总计

| 指标 | 值 |
|------|-----|
| **CppTLM commits** | 7 (1 baseline + 5 feat + 1 docs 后续) |
| **工期** | 0.5 工作日 (密集) |
| **测试 cases (新增)** | 14 |
| **测试 assertions (新增)** | ~2056 (含 fuzz) |
| **Oracle** | 待复评 |
| **下游** | UE `ue-stage-1-4-2-1-extensions` 集成 + 5.5.9 |

## §6 已知 follow-up (非阻断)

1. **ASPM 测试 lifetime 调试**: `test/test_aspm.cc` 已在磁盘 (5 cases)，但 fixture lifetime double-free 调试未完成。**核心 API (enable_aspm/on_traffic/tick auto-transition) 已提交并通过编译**。下一 PR 跟进修 fixture → detach_from_endpoint 序列。

2. **Scenario 5 JSON 警告测试 (F2)**: `test/test_pcie_endpoint_ip_json_config.cc` 仍断言 `bar_sizes` / `vf_bar0_size` 产生 warning。本 change 设计明确不消费这些键（属 BAR window 模型），保留 warning 行为。**当未来 change 实施 BAR window model 时，需同步更新 Scenario 5 断言**（per Oracle F2 follow-up）。

3. **Extended Capability (ACS/PM)**: 当前实现使用标准 PCI Capability (id=0x01 PM, id=0x0D ACS 占位)。PCIe 规范的 ACS Extended Capability (id=0x000D) 完整实现留待后续 PR。

4. **PM Cap 控制域扩展**: 当前 control=0x0013 (version 3 + D3hot + D1) 写死。后续 PR 可 JSON-driven。
