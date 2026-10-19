---
id: phase-5
kind: phase
status: completed
phase_refs: []
主题: PCIe EP AXI Stream Adapter + AXI4Mapper (Phase 5-6 微架构)
---

## phase-5: PCIe EP AXI Stream Adapter + AXI4Mapper (Phase 5-6 微架构)

> **真源**: [`docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`](../../docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md) (**已迁 ArchForge**)
> **OpenSpec 已交付**: [axi-stream-adapter](../../openspec/specs/axi-stream-adapter/) + [axi4-mapper](../../openspec/specs/axi4-mapper/)

### Phase 5: AXI Stream Adapter

- **完成日期**: 2026-10 (Oracle M1/M2/M3 修复 + 复评 PASS)
- **关键模块**: `include/tlm/pcie/pcie_axi_adapter_tlm.{hh,cc}` (AXI 事务边界 + PcieAxiAdapter)
- **关键能力**: AXI Stream 协议 ↔ PCIe TLP 边界转换
- **关联 commit**: `710c734..dd8c44a`

### Phase 6: AXI4Mapper

- **完成日期**: 2026-10 (Oracle PASS)
- **关键模块**: `include/framework/axi4_mapper.{hh,cc}` (AXI4↔Bundle Mapper + OOO rid 关联)
- **关键能力**: outstanding + rid 关联, 多请求关联跟踪
- **关联 commit**: `8b92bfb..fe4d745`

### 测试覆盖

- `[axi]` + `[pcie]` 标签下 Phase 5-6 子测试 (`test_pcie_axi_adapter_*.cc` + `test_axi4_*.cc` + `test_axi4_mapper_*.cc`)
- 当前 baseline: 1241 cases / 47634 assertions / 2 known fail

### 已知问题

- `ch_uint<512>` 数据宽度实为 64-bit 存储 (per `include/bundles/cpphdl_types.hh`, P0 fix 2026-10-06)
- `ch_uint<W>64>` 用 `std::array<uint64_t, N>` 多字存储 (`ch_uint_wide`)
- `read()` / `write()` 在 W>64 时退化为低 64-bit 截断 (向后兼容 shim)