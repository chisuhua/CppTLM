# cpptlm-dgpu-pcie-link-layer-and-fc: PcieLinkLayer + FcTokenBucket + DLLP Bundle

> **状态**: 📋 Proposed — 2026-09-08 · **日期**: 2026-09-08 · **Owner**: CppTLM Team (Sisyphus)
> **性质**: Phase 1 子 change(7 阶段路线图第一步)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **关联**: ADR-SOC-07 D2 + Oracle `bg_455593fe` + `bg_87d368a6` 修订

---

## Why

`PcieEndpointTLM`(2026-08-26 archive)链路层 + Flow Control 完全缺失(Board shell 注入 TLP 直接绕过),无法建模:
- **FC 反压**:Host 可无限制推送 TLP,淹没 SoC 模块
- **ACK/NAK 重传**:无法模拟链路错误与重传
- **DLLP 协议**:链路层协议完全缺失
- **双向链路层**(per Oracle Q17):EP 收方向解析 + ACK 生成

Oracle 修订路线图:**先 FC 后 LL**——Flow Control Token Bucket 是 backpressure 基础,所有后续阶段(130b 编码 / PHY 数字 / SR-IOV / AXI 适配)依赖它。

## What Changes

| 文件 | 用途 |
|---|---|
| `include/bundles/pcie_dllp_bundles_tlm.hh` | **新**: `PcieDllpBundle`(6 种 kind,3 组 credit)+ `PciePhyConfig` |
| `include/tlm/pcie/pcie_flow_control_token_bucket.hh` + `.cc` | **新**: FcTokenBucket 类(无自动 refill,per Q2) |
| `include/tlm/pcie/pcie_link_layer_tlm.hh` + `.cc` | **新**: PcieLinkLayer 模块(DLLP gen/parse + 双向 Rx + error injector) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | **新**: 加 `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏(空头,声明意图) |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | **改**: composition 集成 PcieLinkLayer(不修改旧类布局) |
| `test/test_pcie_dllp_bundle.cc` | **新**: PcieDllpBundle 字段序列化 |
| `test/test_pcie_link_layer_fc_token_bucket.cc` | **新**: FC Token Bucket 单元测试 |
| `test/test_pcie_link_layer_dllp.cc` | **新**: DLLP gen/parse/dispatch |
| `test/test_pcie_link_layer_error_injector.cc` | **新**: Q15 错误注入接口 |
| `test/test_pcie_link_layer_downstream_rx.cc` | **新**: Q17 下行 Rx 路径 |
| `test/test_pcie_link_layer_ack_nak.cc` | **新**: ACK/NAK 重传(累积确认) |
| `test/test_pcie_endpoint_with_ll.cc` | **新**: 集成测试(无回归) |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 注册新源文件 + ctest |

### 不可改(Oracle 修订)

- ❌ `include/tlm/gpu/pcie_endpoint_tlm.h` 不动(归档 spec 冻结 `num_ports()==4`,23 ABI 兼容)
- ❌ 不给旧类加 `[[deprecated]]`(推迟到 Phase 4 PcieEndpointIP 可用后)
- ❌ 不修改 `include/abi/cpptlm_emulator.h`(23 ABI C 符号冻结)

## Acceptance Gate

| Gate | 验证 |
|---|---|
| **P1-G1** | `cmake --build build` 通过 + **7 个新测试 PASS**(Oracle C7 修订) |
| **P1-G2** | DLLP gen/parse 测试 PASS |
| **P1-G3** | FC Token Bucket 测试 PASS(无 refill 测试,per Q2) |
| **P1-G4** | ACK/NAK 重传测试 PASS |
| **P1-G4b** | Error Injector 测试 PASS(Q15) |
| **P1-G4c** | 下行 Rx 测试 PASS(Q17) |
| **P1-G5** | 集成测试 PASS(无回归) |
| **P1-G6** | 23 ABI 边界:`cpptlm_emulator.h` 未修改 + `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏 + 类方法纪律 |
| **P1-G7** | 全量 `ctest -j$(nproc)` 无回归 |
| **P1-G8** | `openspec validate` PASS |

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: 23 ABI 破坏 | 不修改 `pcie_endpoint_tlm.h` 现有类成员;不修改 `cpptlm_emulator.h` |
| R2: FC 反压作废 | 仅 UpdateFC 补充,无自动 refill(per Q2) |
| R3: 旧类加 [[deprecated]] 铺警告 | 不加,推迟到 Phase 4(per Top-9) |
| R4: 编译时间爆炸(MultiPortStreamAdapter N 模板) | Phase 1 仍用 4 端口旧类,17 端口 P4 才引入 |
