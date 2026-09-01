# cpptlm-dgpu-pcie-sriov-vf-pool: SR-IOV VF Pool (新类 PcieEndpointIP, 17 端口)

> **状态**: 📋 Proposed — 2026-10-13 · **工期**: 4 周(W9-W12)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置**: Phase 1+2+3 完成(14 commits)

---

## Why

Phase 1-3 实现 Link Layer + FC + Encoding + PHY + BypassMux,但都是单 PF(Physical Function)。GPGPU 主流(Nvidia H100 / AMD MI300)标配 SR-IOV:1 PF + 16 VF,要求:
- per-VF Config Space / MSI-X / BAR / FC / Retry / seq# 独立
- ARI(Alternative Routing-ID Interpretation)支持
- 简化 FLR(Function Level Reset,per Q10)
- Completion tracking(per Q12)
- 新类 `PcieEndpointIP`(不破坏 `PcieEndpointTLM` 的 4 端口冻结布局)

## What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_endpoint_ip.hh` + `.cc` | **新**: `PcieEndpointIP` 类(17 端口 PF+VF0..15) |
| `include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh` + `.cc` | **新**: VF Pool 路由表 + per-VF state |
| `include/tlm/pcie/pcie_msix_per_vf_tlm.hh` + `.cc` | **新**: per-VF MSI-X table |
| `include/tlm/pcie/pcie_config_space_per_vf_tlm.hh` + `.cc` | **新**: per-VF 4KB Config Space + ARI |
| `test/test_pcie_sriov_vf_pool_routing.cc` | **新**: 17 端口 stream_id 路由 |
| `test/test_pcie_sriov_per_vf_config.cc` | **新**: per-VF Config 独立 |
| `test/test_pcie_sriov_per_vf_msix.cc` | **新**: per-VF MSI-X 独立 |
| `test/test_pcie_sriov_ari_capability.cc` | **新**: ARI 紧凑 routing-id |
| `test/test_pcie_sriov_flr.cc` | **新**: Q10 FLR(PF=全,Vx=仅对应状态) |
| `test/test_pcie_sriov_completion_tracking.cc` | **新**: Q12 completion 匹配(trans_id) |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 注册新源 + 6 ctest |

### 17 端口(per Q6 VF Pool + Oracle Top-3 修订)

- `PcieEndpointIP` 独立类(不动 `PcieEndpointTLM`)
- 17 端口:`port[0] = PF`, `port[1..16] = VF0..VF15`
- 内部 `stream_id → PF/VF 路由表`
- `pcie_endpoint_tlm.cc` composition 静态注册表升级:同时支持旧 `PcieEndpointTLM`(4 端口冻结)和新 `PcieEndpointIP`(17 端口)

### FLR(per Q10)

- `flr_pf()`: 全状态复位(PF + 16 VF)
- `flr_vf(uint16_t vf_id)`: 仅对应 VF 状态复位(PF + 其他 VF 不变)
- 触发后续重配置(per-VF Config Space + MSI-X table 清空,FC token bucket reset,Retry buffer clear,seq# 重置)

### Completion Tracking(per Q12)

- `trans_id` 关联 NP 请求 ↔ CplD
- `complete(trans_id, cpl_data)`: 在 CplD 到达时匹配 NP 请求
- 溢出策略:NP 发出后 1s 超时丢弃(简化:outstanding map 容量上限)

### ARI(per design §8)

- 8-bit Function Number(替代 16-bit BDF)
- Function 0 = PF, Function 1..16 = VF0..15
- 紧凑 routing-id 解析

## Acceptance Gate

| Gate | 验证 |
|---|---|
| **P4-G1** | VF Pool 路由:17 端口 stream_id 正确分发 |
| **P4-G2** | per-VF Config Space 独立(VF0 写不影响 VF1/PF0) |
| **P4-G3** | per-VF MSI-X 独立 |
| **P4-G4** | ARI 8-bit Function number 紧凑 routing-id 正确 |
| **P4-G5** | FLR:PF=全复位,VFx=仅对应状态 |
| **P4-G6** | Completion tracking:trans_id 匹配 NP↔CplD |
| **P4-G7** | 全量 ctest 无回归(P1+P2+P3 11+19+19=49 个测试仍 PASS) |
| **P4-G8** | `PcieEndpointTLM` 4 端口冻结布局零破坏(per Oracle Top-3) |

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: 17 端口 MultiPortStreamAdapter<N=17> 编译开销 | 用 .hh 私有成员 + 不依赖模板 N(动态路由) |
| R2: `PcieEndpointTLM` 4 端口被错误扩展 | 新类 `PcieEndpointIP` 独立,**不修改**旧类成员 |
| R3: per-VF state 内存膨胀 | 16 VF × state 字段 ≤ KB 级,远小于 retry buffer |
| R4: ARI 兼容旧 BDF | 同时支持 8-bit 和 16-bit routing-id(ARI bit 判定) |