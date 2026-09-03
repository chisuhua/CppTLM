# ADR-SOC-13: AXI Stream Adapter + AXI4Mapper 集成 — PCIe↔AXI 边界 + OOO 关联

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: AXI4 ↔ PCIe TLP 边界 + outstanding 跟踪 + OOO completion
> **类别**: SoC 架构 / Phase 5-6 PCIe EP 子链路
> **关联文档**:
> - [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md)（§6.2/6.3/6.4 详细 AXI Bundle 集成）
> - [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md) §8/§9/§10（AXI Stream Adapter + Mapper + AXI↔PCIe Adapter）
> **关联 OpenSpec**: [`2026-11-03-cpptlm-dgpu-axi-stream-adapter/`](../../../openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/)、[`2026-12-22-cpptlm-dgpu-axi4-mapper/`](../../../openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/)
> **关联真实代码**:
> - `include/framework/axi4_stream_adapter.{hh,cc}`（Phase 5）
> - `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc`（Phase 6）
> - `include/tlm/pcie/pcie_axi_adapter_tlm.{hh,cc}`（Phase 5+8 M1）

---

## 1. Context（背景）

### 1.1 PCIe ↔ AXI 边界需求

dGPU SoC v1.0 需要在 PcieEndpointIP 与 SoC 内部模块之间建立：
- **AXI4 ↔ PCIe TLP** 边界（事务类型转换）
- **outstanding 跟踪**：AXI4 master/slave 独立 outstanding 计数
- **OOO completion**：AXI4 响应可乱序（per `outstanding_id`）

### 1.2 Phase 5 关键修复（M1/M2/M3）

| 修复 | 描述 | commit |
|------|------|--------|
| **M1** | `ch_uint<512>` 实为 64-bit 存储（per `include/bundles/cpphdl_types.hh`） | `c5b58ac` |
| **M2** | master_resp 读分支 `if (resp.rlast.read())` 才清除 outstanding | `c5b58ac` |
| **M3** | `PcieAxiAdapter::set_endpoint()` + `PcieEndpointIP::attach_composition` 独立 axi_adapter 分支绑定 `this`，避免 composition 路径丢失 EP 指针 | `c5b58ac` |

### 1.3 Phase 6 AXI4Mapper 关键设计

- **outstanding 跟踪**：AXI4 多事务并行，按 `outstanding_id` 路由
- **OOO completion 关联**：`rdata`/`bresp` 可乱序，但 `rid` 与 `arid` 关联
- **可独立复用**：与 PcieEndpointIP 解耦，可被 CrossbarTLM / CacheTLM 复用
- **JSON 可选注入**：`axi4_mapper_inject: true` 时启用（per `examples/dgpu_soc_with_pcie_ip.json`）

---

## 2. Decision（决策）

### D1. Axi4Bundle 17 字段 + Axi4LiteBundle 9 字段

✅ **Bundle 字段定义**（per `include/bundles/axi4_bundles_tlm.hh`）：

**Axi4Bundle**(17 字段):
- `transaction_id`, `parent_id`, `address`, `size`, `is_write`, `data`
- `aw*`(awvalid/awready/awaddr/awlen/awsize/awburst/awcache/awprot/awregion/awqos)
- `w*`(wvalid/wready/wdata/wstrb/wlast)
- `b*`(bvalid/bready/bresp)
- `ar*`(arvalid/arready/araddr/arlen/arsize/arburst/arcache/arprot/arregion/arqos)
- `r*`(rvalid/rready/rdata/rresp/rlast)

**Axi4LiteBundle**(9 字段):
- `transaction_id`, `address`, `size`, `data`
- `aw*`(awvalid/awready/awaddr/awprot)
- `w*`(wvalid/wready/wdata/wstrb)
- `b*`(bvalid/bready/bresp)
- `ar*`(arvalid/arready/araddr/arprot)
- `r*`(rvalid/rready/rdata/rresp)

### D2. Axi4StreamAdapter 三端口（master_out / slave_in / cfg_slave_in）

✅ **三端口设计**：

| 端口 | 类型 | 用途 |
|------|------|------|
| `axi_master_out` | AXI4 master | SoC → Host 主动访问（如 GPU → Host BAR） |
| `axi_slave_in` | AXI4 slave | Host → SoC 事务入口（如 Host 写入 PushBuffer） |
| `cfg_slave_in` | AXI4-Lite slave | 配置空间访问（AXI4-Lite 子集） |

**Outstanding 跟踪 + OOO completion**：
- AXI4 master/slave 独立 outstanding 计数
- OOO completion 按 `outstanding_id` 路由
- `if (resp.rlast.read())` 才清除 outstanding（per Phase 5 M2 修复）

### D3. Axi4Mapper 独立模块 + JSON 可选注入

✅ **Axi4Mapper 独立模块**（per `include/framework/axi4_mapper.hh`）：

- 与 PcieEndpointIP 解耦
- 可被 CrossbarTLM / CacheTLM 复用
- JSON 可选注入：`axi4_mapper_inject: true`

### D4. 512-bit 数据宽度已知限制

✅ **`ch_uint<512>` 实为 64-bit 存储**：

- per `include/bundles/cpphdl_types.hh`
- 真实 PCIe PHY 兼容通过多次 burst 拼接
- 已 Phase 5 M1 文档化（spec.md）

### D5. PCIe Cfg 地址编码修复（v1.1 实现完成）

✅ **Phase 8 M1 简化 → v1.1 修复**：

- Phase 8 M1：awaddr 直接当 offset（per commit `429327d`）
- v1.1：按 PCIe 规范解码 — `cfg_byte_off = awaddr & ~0x3`，低 2 bit [1:0] 为对齐保留位
- 范围判定：`awaddr < config_size` 区分 cfg 与 BAR 路径
- 测试覆盖：见 `[cfg-encoding]` 标签（`test/test_pcie_cfg_address_encoding.cc`）

---

## 3. Consequences（后果）

### 3.1 正面影响

- **PCIe↔AXI 标准化边界**：事务类型转换统一
- **outstanding + OOO 性能**：AXI4 高效吞吐
- **Mapper 独立复用**：Crossbar/Cache 也能用

### 3.2 负面影响

- **512-bit 限制**：需多次 burst 拼接
- **PCIe Cfg 地址编码简化**：Minor 已知问题

### 3.3 兼容性保证

- **Phase 5 M1/M2/M3 修复**：Oracle 复评 PASS
- **Phase 6 PASS**：所有测试通过
- **Phase 8 M1 修复**：`test_pcie_endpoint_ip_full_e2e.cc` 3 TEST_CASE PASS

---

## 4. Implementation（实施）

### 4.1 已实施（Phase 5-6 + Phase 8 M1）

| 模块 | 路径 | commit |
|------|------|--------|
| Axi4StreamAdapter | `include/framework/axi4_stream_adapter.{hh,cc}` | `6223534` + `c5b58ac` + `b181170`（backpressure handshake） |
| Axi4Mapper | `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc` | `f2540e8` |
| PcieAxiAdapter | `include/tlm/pcie/pcie_axi_adapter_tlm.{hh,cc}` | Phase 5 + `429327d` |
| 测试 | `test_*.cc`（多个）| ✅ PASS |

### 4.2 关键修复（M1/M2/M3）

**Phase 5 M1**（per `c5b58ac`）：
- `ch_uint<512>` 实为 64-bit 存储
- 已写入 `spec.md`（per `docs-archived/adr/ADR-X.16` 关联）

**Phase 5 M2**（per `c5b58ac`）：
- `master_resp` 读分支 `if (resp.rlast.read())` 才清除 outstanding
- 修正 read 路径早清 outstanding 的 bug

**Phase 5 M3**（per `c5b58ac`）：
- `PcieAxiAdapter::set_endpoint()` + `PcieEndpointIP::attach_composition` 独立 axi_adapter 分支绑定 `this`
- 避免 composition 路径丢失 EP 指针

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | `ch_uint<512>` 实为 64-bit 存储限制 | 🟢 低 | 已 Phase 5 M1 文档化；多次 burst 拼接 |
| **R2** | PCIe Cfg 地址编码简化(Minor) | 🟡 中 | 已 Phase 8 M1 修复(简化)；v1.1 完整版按 PCIe 规范细化 |
| **R3** | outstanding 跟踪错误(M2) | 🟢 低 | 已 Phase 5 M2 修复(`if (resp.rlast.read())`)|

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-06 D5 | CommandProcessor + PcieAxiAdapter 集成 |
| ADR-SOC-11 | PcieEndpointIP 17 ports（含 PcieAxiAdapter） |
| ADR-SOC-12 | HostBypassTLM ↔ PcieEndpointIP（AXI 桥接目标） |

### 6.2 关联 OpenSpec

| Change | 关联 |
|--------|------|
| `2026-11-03-cpptlm-dgpu-axi-stream-adapter` | Phase 5 AXI Stream Adapter 实施 |
| `2026-12-22-cpptlm-dgpu-axi4-mapper` | Phase 6 AXI4Mapper 实施 |
| `2027-02-09-cpptlm-dgpu-pcie-ip-integration` | Phase 8 整合（含 M1 修复） |

### 6.3 关联真实代码

| 文件 | 角色 |
|------|------|
| `include/framework/axi4_stream_adapter.{hh,cc}` | AXI↔TLP 边界 |
| `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc` | AXI4 OOO Mapper |
| `include/tlm/pcie/pcie_axi_adapter_tlm.{hh,cc}` | PcieAxiAdapter |
| `include/bundles/axi4_bundles_tlm.hh` | Axi4Bundle + Axi4LiteBundle |
| `include/bundles/cpphdl_types.hh` | ch_uint<512> 定义（实为 64-bit） |

---

## Status Update

- **2027-02-09**: 📋 Proposed。Phase 5 + Phase 6 + Phase 8 M1 修复已落地（HEAD `429327d`）；Oracle 评审 PASS（含 M1/M2/M3 修复 + 复评）。