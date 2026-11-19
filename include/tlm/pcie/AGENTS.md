# include/tlm/pcie/ — PCIe EP 微架构（7 阶段）

**状态**: v1（2026-11-19 建档）· **最后更新**: 2026-11-19
**域**: PCIe Endpoint 微架构头文件（26 个 `.hh`；实现 `.cc` 在 `src/tlm/pcie/`，18 个）

## 职责

按 **Phase 1→8** 分层的 EP 模型：链路层 → 编码 → PHY 数字控制 → 旁路开关 → SR-IOV VF → AXI 适配 → Host Bypass/RC → 整合。
接口语义以 ArchForge 规格为准；本仓是**硬件建模面**实现。

## 文件索引（按阶段）

| 阶段 | 文件 | 说明 |
|---|---|---|
| **1 链路层 + FC** | `pcie_link_layer_tlm.hh` · `pcie_flow_control_token_bucket.hh` | DLLP / FC credit |
| **1/2 复合** | `pcie_link_phy_mux_tlm.hh` · `pcie_link_phy_mux_tlm_fwd.hh` | LL+PHY+Mux（17 端口） |
| **2 编码** | `pcie_encoding_latency_model.hh` | 128b/130b 延迟模型 |
| **3 PHY 数字** | `pcie_phy_digital_ctrl_tlm.hh` · `pcie_bypass_mux.hh` · `pcie_bypass_mux_p2p.hh` | LTSSM 11 态 / 3 态模式切换 |
| **4 SR-IOV** | `pcie_sriov_vf_pool_tlm.hh` · `pcie_config_space_per_vf_tlm.hh` · `pcie_msix_per_vf_tlm.hh` · `pcie_completion_tracker_tlm.hh` · `pcie_ari_router_tlm.hh` | 1 PF + 16 VF |
| **4 整合** | `pcie_endpoint_ip.hh` | ★ EP 本体（`REGISTER_MODULE`） |
| **5 AXI 边界** | `pcie_axi_adapter_tlm.hh` · `pcie_requester_engine.hh` · `pcie_completer_engine.hh` | TLP ↔ AXI |
| **6/7 主机侧** | `host_bypass_tlm.hh` · `pcie_root_complex_tlm.hh` | 旁路 bring-up / 自研 RC |
| **facade** | `pcie_controller_ip.hh` · `pcie_axi_bridge_ip.hh` | 4-IP 顶层 / EP composite |
| **编解码与能力** | `pcie_tlp_codec.hh` · `pcie_resizable_bar.hh` · `pcie_acs_extended_cap.hh` | TLP / BAR / ACS |
| **测试支撑** | `host_pcie_test_stub.hh` · `pcie_mock_ip.hh` | 仅测试用，⛔ 勿在产品路径依赖 |

## 本目录铁律

1. **唯一 EP 顶层 = `PcieEndpointIP`**（`REGISTER_MODULE`，可从 JSON 顶层实例化）。
   `PcieAxiBridgeIP` 是 **composite**：`REGISTER_MODULE` 已移除 ⇒ ⛔ 不可在 JSON 里当顶层类型（会实例化失败）。
2. **`PcieControllerIP` 是 facade 不是 EP 本体**：它做 4 BAR decode（Bar0 MMIO / Bar1 FB fast-path / Bar2 VRAM aperture / Unknown），EP 通过 `attach_endpoint()` 注入 —— ⛔ 不要把 EP 逻辑写进 facade。
3. **跨层改动按阶段验证**：改第 N 层必须跑该阶段的测试标签（见 `test/AGENTS.md`），⛔ 不要只跑 `[e2e]` 就认为没破坏中间层。
4. **ABI 零改动**：23 个 ABI 头冻结（`include/abi/`）；`include/tlm/gpu/pcie_endpoint_tlm.h` 为 `[[deprecated]]` 冻结头，新代码一律用 `PcieEndpointIP`。
5. **测试标签**：`[pcie]`；全链路用 `test/test_pcie_endpoint_ip_full_e2e.cc`。

## 常见坑

- `pcie_link_phy_mux_tlm_fwd.hh` 是**前向声明**，只为打破循环依赖 —— ⛔ 不要把实现写进去。
- `pcie_mock_ip.hh` / `host_pcie_test_stub.hh` 依赖测试语义，产品配置里出现即错。
- 修改 BAR 大小来源：`configs/*.json` 的 `pcie_ep.params.bar_sizes`，`DGpuBoard` 会据此派生 framebuffer 尺寸。

## 关联

`src/tlm/pcie/`（18 个 `.cc`）· `test/test_pcie_*.cc` · `examples/dgpu_soc_with_pcie_ip.json`（完整配置）· `docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md`（chip-internal vs board-level 边界）
