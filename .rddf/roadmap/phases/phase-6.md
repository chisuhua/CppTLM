---
id: phase-6
kind: phase
status: completed
phase_refs: []
主题: PCIe EP Host Bypass + RC + 整合交付 (Phase 7-8 微架构)
---

## phase-6: PCIe EP Host Bypass + RC + 整合交付 (Phase 7-8 微架构)

> **真源**: [`docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`](../../docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md) (**已迁 ArchForge**)
> **OpenSpec 已交付**: [host-bypass-and-rc](../../openspec/specs/host-bypass-and-rc/)

### Phase 7: Host Bypass + 自研 RC

- **完成日期**: 2026-11 (Oracle 有条件 PASS M1/M2)
- **关键模块**:
  - `include/tlm/pcie/host_bypass_tlm.{hh,cc}` — 软件 bring-up 跳过 RC BFM
  - `include/tlm/pcie/pcie_root_complex_tlm.{hh,cc}` — 自研 RC 模型 (枚举 PF0-only)
- **关键能力**: Host Bypass (跳过 RC BFM, 软件 bring-up) + 自研 RC (PF0-only 枚举)
- **关联 commit**: `ce50b05..45763fa`
- **M1 (待修复)**: Phase 7 阶段未解决 — 由 Phase 8 M1 修复 (见下)

### Phase 8: 整合交付 (W24 末)

- **完成日期**: 2027-02-09 (整合完成, HEAD `429327d`)
- **关键交付**:
  - `examples/dgpu_soc_with_pcie_ip.json` 完整 dGPU SoC + PCIe EP 配置
  - `test/test_pcie_endpoint_ip_full_e2e.cc` 全链路 E2E (3 TEST_CASE, solve Phase 7 M1)
  - `PcieEndpointTLM` 加 `[[deprecated]]` 标注 (迁移提示, ABI 不动)
  - **M1 桥接修复 (`429327d`)**: HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道, 让 EP 真实消费请求
- **关联 commit**: `e29defd..429327d`

### 配置 + 测试

- `configs/dgpu_soc_with_pcie_ip.json` — 完整 dGPU SoC + PCIe EP 配置
- `test/test_pcie_endpoint_ip_full_e2e.cc` — ★ Phase 8 全链路 E2E
- `examples/demo_pcie_full_e2e.{cc,py}` — Phase 8 E2E demo

### 已知问题 (Minor, 不阻断)

- `test_pcie_endpoint_ip_full_e2e.cc:_config` + `_bar` — Phase 8 e2e 已知 Minor, AXI↔PCIe Cfg 地址编码简化 (直接用 `awaddr` 当 offset, 需未来细化按 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset`)
- `ch_uint<512>` 数据宽度实为 64-bit 存储 (向后兼容 shim, P1/P2 迁移到 `slice<>/set_slice<>` 后可移除)