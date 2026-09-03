# axi4-stream-adapter 微架构文档

> **类别**: Framework > Stream Adapter · **状态**: 🔵 Implemented (per Phase 5 + ADR-SOC-13)
> **Header**: `include/framework/axi4_stream_adapter.hh` (147 行)
> **类**: `cpptlm::Axi4StreamAdapter`（独立类,注册到 `ChStreamAdapterFactory`）
> **命名空间**: `cpptlm::`（per `include/AGENTS.md` 约定）
> **蓝图来源**: Phase 5 AXI Stream Adapter（per `openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/`）
> **关联 ADR**:
> - [`ADR-SOC-13-axi-stream-adapter-mapper.md`](../adr/ADR-SOC-13-axi-stream-adapter-mapper.md) D1/D2/D4 — Axi4Bundle 17 字段 + 三端口 + 512-bit 限制
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](../adr/ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 17 ports 持有 Axi4StreamAdapter
> - [`ADR-SOC-12-host-bypass-and-rc.md`](../adr/ADR-SOC-12-host-bypass-and-rc.md) — HostBypassTLM/RC 持有 Axi4StreamAdapter
> **关联 OpenSpec**: [`openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/`](../../../openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/)
> **首版 commit**: `2026-11-03` T-AXI-1 + `6223534..b1811703` (Phase 5 实施,Oracle M1/M2/M3 修复) + Oracle M1/M2/M3 修复 · **最近更新**: 2027-02-09 (Phase 8 整合 + ADR-SOC-13 同步)
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 框架基础: [`include/framework/AGENTS.md`](../../../include/framework/AGENTS.md) (StreamAdapter 接口契约)
> - Bundle 定义: [`include/bundles/axi4_bundles_tlm.hh`](../../../include/bundles/axi4_bundles_tlm.hh) (Axi4Bundle 17 字段 + Axi4LiteBundle 9 字段)
> - 配对组件: [`axi4-mapper.md`](./axi4-mapper.md) (独立模块,outstanding 跟踪 + OOO completion)
> - 使用方: [`host-bypass.md`](./host-bypass.md) + [`pcie-root-complex.md`](./pcie-root-complex.md) + [`dgpu-soc-pcie-slice.md`](./dgpu-soc-pcie-slice.md)

---

## 1. 设计目标

`cpptlm::Axi4StreamAdapter` 是 **AXI4 / AXI4-Lite 流适配器**,为 PcieEndpointIP 向 SoC 暴露的 AXI 事务边界提供 valid/ready 握手语义。

**核心特征**:
- **三端口**: `axi_master_out` (EP→SoC) / `axi_slave_in` (SoC→EP) / `cfg_slave_in` (AXI4-Lite 配置访问)
- **valid/ready 反压**: 不丢事务（通道 empty 且 valid 时仅在 ready=1 推进）
- **outstanding 跟踪**: Phase 6 OOO 预留，master 侧跟踪 awid/arid → bid/rid 关联
- **独立于 PcieEndpointIP**: 可被 HostBypassTLM / PcieRootComplexTLM / 任何 AXI 边界组件复用

---

## 2. 架构概览

```
   axi_master_out（EP→SoC）
   ┌─────────────────────────┐
   │ master_req(req,track_id)│ ◄──── EP 注入 AXI 请求(valid=1)
   │ set_master_ready(ready) │ ◄──── SoC 侧 ready 信号
   │ master_req_valid()      │
   │ master_req_data()       │
   │ consume_master_req()    │ ◄──── ready=1 且 tick() 后清 valid
   │ master_resp(resp)       │ ◄──── SoC 返回响应
   │ master_resp_valid()     │
   └─────────────────────────┘
   
   axi_slave_in（SoC→EP）+ cfg_slave_in（AXI4-Lite）
   ┌─────────────────────────┐
   │ slave_req(req)          │ ◄──── SoC 注入请求
   │ set_slave_ready()       │
   │ consume_slave_req()     │
   │ slave_resp(resp)        │ ◄──── EP 返回响应
   │ cfg_req / cfg_resp 同理  │
   └─────────────────────────┘
```

---

## 3. 三端口语义（per spec.md）

| 端口 | 方向 | 职责 |
|------|------|------|
| **axi_master_out** | EP (master) → SoC | EP 发起对 SoC 的读/写访问 |
| **axi_slave_in** | SoC (master) → EP | SoC 发起进入 Endpoint 的事务 |
| **cfg_slave_in** | SoC → EP (AXI4-Lite) | AXI4-Lite 配置空间访问 |

---

## 4. 关键接口（节选）

### 4.1 axi_master_out

| 接口 | 签名 | 作用 |
|------|------|------|
| `master_req(req, track_id=true)` | `bool` | EP 注入 AXI4 请求（写或读）;返回 true 表示接受(valid 置位),false 表示 backpressure |
| `set_master_ready(ready)` | `void` | 下游 ready 信号(SoC 侧) |
| `master_req_valid()` / `master_req_data()` | query | 请求通道状态 |
| `consume_master_req()` | `void` | ready=1 且 tick() 后清 valid |
| `master_resp(resp)` | `void` | 下游注入响应 |
| `master_resp_valid()` / `consume_master_resp()` | query/clear | 响应通道状态 |

### 4.2 axi_slave_in + cfg_slave_in

类似 `axi_master_out`,但语义对称(SoC 发起 / EP 消费)。
- `cfg_*` 是 AXI4-Lite 版本,字段宽度较小。

---

## 5. 握手规则（valid/ready backpressure）

1. 每个方向的请求/响应通道有独立 valid/ready 信号
2. 通道 empty 且 valid 时,tick() 仅在 ready=1 才推进(不丢事务)
3. 生产者调用 `master_req()/slave_req()/cfg_req()` 设置 valid
4. 消费者在 ready=1 且 tick() 后调用 `consume_*_req()` 清 valid

---

## 6. ID 关联（Phase 6 OOO 预留）

- **master 侧跟踪** outstanding 写/读请求 ID(`awid`/`arid`)
- `master_resp()` 返回时按 `bid`/`rid` 匹配并移除对应 outstanding 条目
- 完整 OOO 调度由 [`Axi4Mapper`](./axi4-mapper.md) 提供

---

## 7. 已知限制

- **512-bit 数据宽度**（per ADR-SOC-13 D4）: `ch_uint<512>` 实为 64-bit 存储（per `include/bundles/cpphdl_types.hh`）
- **PCIe Cfg 地址编码简化**（per ADR-SOC-13 D5）: Phase 8 M1 用 `awaddr` 当 offset;完整 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset` 待 v1.1 细化
- **当前 outstanding 跟踪在 master 侧**;slave 侧 OOO 由 Axi4Mapper 独立提供

---

## 8. 测试覆盖

- `test/test_pcie_axi_adapter_*.cc` (Phase 5): 三端口 + valid/ready 握手 + outstanding
- `test/test_pcie_endpoint_ip_full_e2e.cc` (Phase 8): 3 TEST_CASE 全链路 PASS
