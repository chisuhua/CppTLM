# host-bypass 微架构文档

> **类别**: PCIe > Host Bypass · **状态**: 🔵 Implemented (per Phase 7 + ADR-SOC-12)
> **Header**: `include/tlm/pcie/host_bypass_tlm.hh` (131 行)
> **源实现**: `src/tlm/pcie/host_bypass_tlm.cc` (Phase 8 M1 真实数据路径接线)
> **类**: `tlm::pcie::HostBypassTLM`（非 ChStreamModuleBase / 非 SimModule，独立辅助类）
> **蓝图来源**: 软件 bring-up 跳过 PCIe Root Complex BFM，Host 侧直接 AXI 桥接到 PcieEndpointIP
> **关联 ADR**:
> - [`ADR-SOC-12-host-bypass-and-rc.md`](../adr/ADR-SOC-12-host-bypass-and-rc.md) D1/D3 — Host Bypass 软件 bring-up 路径决策 + M1 自动转发修复
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](../adr/ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 17 ports 整合模块
> - [`ADR-SOC-13-axi-stream-adapter-mapper.md`](../adr/ADR-SOC-13-axi-stream-adapter-mapper.md) D2 — Axi4StreamAdapter 三端口 (axi_master_out / axi_slave_in / cfg_slave_in)
> **关联 OpenSpec**: [`openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/`](../../../openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/)
> **首版 commit**: `2027-01-19` T-HB-1 + `429327d` T-P8-1 (Phase 8 M1 修复) · **最近更新**: 2027-02-09 (Phase 8 整合 + ADR-SOC-12 同步)
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 配对组件: [`pcie-root-complex.md`](./pcie-root-complex.md)（RC 镜像,可选）
> - 下游整合模块: [`dgpu-soc-pcie-slice.md`](./dgpu-soc-pcie-slice.md) (Phase 4-8 演进 + 17 ports PcieEndpointIP)
> - L1 Host Interface 子系统架构: [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md)
> - 上游 StreamAdapter: [`axi4-stream-adapter.md`](./axi4-stream-adapter.md) (axi_master_out / axi_slave_in / cfg_slave_in)
> - 框架基础: [`include/framework/AGENTS.md`](../../../include/framework/AGENTS.md) (StreamAdapter 接口契约)

---

## 1. 设计目标

`tlm::pcie::HostBypassTLM` 是 **软件 bring-up 阶段**的 Host 侧 AXI 桥接组件，**跳过 PCIe Root Complex BFM** 直接在 Host 侧与 PcieEndpointIP 之间建立 AXI 事务边界。

**核心特征**:
- **职责单一**: 仅负责 Host ↔ PcieEndpointIP 双向 AXI 桥接，**不模拟** PCIe 枚举/Config Space/BAR 分配（这些由 [`PcieRootComplexTLM`](./pcie-root-complex.md) 负责）
- **三端口 AXI**: 内部持有 [`Axi4StreamAdapter`](./axi4-stream-adapter.md) 暴露 `axi_master_out / axi_slave_in / cfg_slave_in`
- **valid/ready 反压**: 不丢事务（通道 empty 且 valid 时仅在 ready=1 推进）
- **tick() 转发**: 自动将 4 方向 AXI 通道（master_out↔slave_in + slave_resp↔master_resp）转发至底层 Axi4StreamAdapter（per Phase 8 M1 修复 `429327d`）

---

## 2. 架构概览

```
       Host 侧 (软件 / UsrLinuxEmu)
                  │
                  ▼
   ┌────────────────────────────┐
   │  HostBypassTLM             │
   │  - attach_to_endpoint(ep)  │ ◄──── 挂接 PcieEndpointIP 引用
   │  - init()                  │
   │  - tick()                  │ ◄──── EventQueue 周期推进
   │  - axi_master_out (req/resp)│
   │  - axi_slave_in   (req/resp)│
   │  - cfg_slave_in   (req/resp)│
   └────────────┬───────────────┘
                │
                ▼ Axi4StreamAdapter 三端口
                │
       PcieEndpointIP (17 ports)
       req_in[17] / resp_out[17]
```

---

## 3. 关键接口

| 接口 | 签名 | 作用 |
|------|------|------|
| 构造 | `HostBypassTLM(const std::string& name, EventQueue* eq)` | 名称 + 事件队列 |
| 初始化 | `void init()` | 幂等初始化（内部 Axi4StreamAdapter） |
| 挂接 EP | `void attach_to_endpoint(PcieEndpointIP* ep) noexcept` | 挂接 PcieEndpointIP 引用 |
| 解绑 | `void detach() noexcept` | 解绑 EP 引用 |
| 周期推进 | `void tick()` | 自动转发 4 方向 AXI 通道（per Phase 8 M1 `429327d`） |
| EP 状态 | `bool is_attached() / PcieEndpointIP* endpoint()` | 查询挂接状态 |

**底层访问**:
- `axi_master_out()` / `axi_slave_in()` / `cfg_slave_in()`: 返回底层 [`Axi4StreamAdapter`](./axi4-stream-adapter.md) 三端口引用

---

## 4. 与 PcieRootComplexTLM 的关系

| 维度 | HostBypassTLM | PcieRootComplexTLM |
|------|---------------|---------------------|
| 用途 | 软件 bring-up 跳过 RC BFM | 镜像 RC 行为（枚举 + Config Space） |
| PCIe 枚举 | ❌ | ✅ (发现 PF0 + VF0..VF15) |
| Config Space 读写 | ❌ | ✅ |
| BAR 分配 | ❌ | ✅ |
| AXI 桥接 | ✅ | ✅ |
| 使用场景 | 单测 / 软件驱动集成 | 完整 PCIe 枚举验证 |
| 可同时使用 | ✅ | ✅ |

---

## 5. 测试与验证

**全链路 E2E** (`test/test_pcie_endpoint_ip_full_e2e.cc`, per Phase 8):
- 3 TEST_CASE: `{config, bar, rc}` 全 PASS
- 桥接: HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道，让 EP 真实消费请求

---

## 6. 已知限制

- 当前 Phase 8 M1 用 `awaddr` 直接当 PCIe Cfg offset（per ADR-SOC-13 D5）;完整 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset` 待 v1.1 细化
- `ch_uint<512>` 数据宽度实际为 64-bit 存储（per `include/bundles/cpphdl_types.hh`）
