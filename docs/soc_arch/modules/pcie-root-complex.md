# pcie-root-complex 微架构文档

> **类别**: PCIe > Root Complex · **状态**: 🔵 Implemented (per Phase 7 + ADR-SOC-12)
> **Header**: `include/tlm/pcie/pcie_root_complex_tlm.hh` (141 行)
> **源实现**: `src/tlm/pcie/pcie_root_complex_tlm.cc` (Phase 8 M1 真实数据路径接线)
> **类**: `tlm::pcie::PcieRootComplexTLM`（非 ChStreamModuleBase / 非 SimModule，独立辅助类）
> **结构体**: `tlm::pcie::DiscoveredDevice`（PCIe 设备/功能枚举结果）
> **蓝图来源**: 自研 RC 模型，镜像 PcieEndpointIP；PF0-only 简化（Oracle M2 标注）
> **关联 ADR**:
> - [`ADR-SOC-12-host-bypass-and-rc.md`](../adr/ADR-SOC-12-host-bypass-and-rc.md) D2/D3 — 自研 RC 模型 + PF0-only 简化 + M1 修复
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](../adr/ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 17 ports 整合模块
> - [`ADR-SOC-13-axi-stream-adapter-mapper.md`](../adr/ADR-SOC-13-axi-stream-adapter-mapper.md) D2 — Axi4StreamAdapter 三端口
> **关联 OpenSpec**: [`openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/`](../../../openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/)
> **首版 commit**: `2027-01-19` T-RC-1 + `429327d` T-P8-1 (Phase 8 M1 修复) · **最近更新**: 2027-02-09 (Phase 8 整合 + ADR-SOC-12 同步)
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 配对组件: [`host-bypass.md`](./host-bypass.md)（软件 bring-up 路径）
> - 下游整合模块: [`dgpu-soc-pcie-slice.md`](./dgpu-soc-pcie-slice.md) (Phase 4-8 演进 + 17 ports PcieEndpointIP)
> - L1 Host Interface 子系统架构: [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md)

---

## 1. 设计目标

`tlm::pcie::PcieRootComplexTLM` 是 **自研 Root Complex 镜像模型**，执行 PCIe 枚举（PF0-only）+ Config Space 读写 + BAR 分配 + BAR 访问路由。

**核心特征**:
- **PCIe 枚举**: 发现 PcieEndpointIP 设备/功能（**PF0-only 简化**,Oracle M2 标注;VF 经 `stream_id` 直访非枚举发现）
- **Config Space 读写**: 路由到 EP 的 PcieConfigSpace（per-port）
- **BAR 分配**: 在 EP 配置空间写入 BAR 基址/属性
- **BAR 访问路由**: 经 AXI master 通道将 BAR 空间读写转发到 EP
- **三端口 AXI**: 内部持有 [`Axi4StreamAdapter`](./axi4-stream-adapter.md) 暴露 `axi_master_out / axi_slave_in / cfg_slave_in`

---

## 2. 架构概览

```
       PcieRootComplexTLM
       ┌────────────────────────────┐
       │ enumerate()                │ ◄──── PCIe 枚举:发现 PF0 + VF0..VF15
       │ config_space_read/write()  │ ◄──── Config Space 访问
       │ bar_alloc()                │ ◄──── BAR 分配
       │ bar_route()                │ ◄──── BAR 访问路由(经 axi_master_out)
       │ tick()                     │ ◄──── 周期推进(per Phase 8 M1 修复)
       │ attach_to_endpoint(ep)     │ ◄──── 挂接 PcieEndpointIP 引用
       └────────────┬───────────────┘
                    │
                    ▼ Axi4StreamAdapter 三端口
                    │
           PcieEndpointIP (17 ports)
```

---

## 3. 关键接口

### 3.1 DiscoveredDevice 结构体

```cpp
struct DiscoveredDevice {
    uint16_t device_id;       // 设备号
    uint16_t function;        // 功能号 (0..7)
    uint16_t vendor_id;       // 来自 EP 配置空间
    uint16_t device_id_reg;   // device_id 寄存器
    uint32_t class_code;      // 类别代码
    uint8_t  revision_id;     // 修订号
};
```

### 3.2 PcieRootComplexTLM 接口

| 接口 | 签名 | 作用 |
|------|------|------|
| 构造 | `PcieRootComplexTLM(const std::string& name, EventQueue* eq)` | 名称 + 事件队列 |
| 初始化 | `void init()` | 幂等初始化 |
| 挂接 EP | `void attach_to_endpoint(PcieEndpointIP* ep) noexcept` | 挂接 PcieEndpointIP 引用 |
| 周期推进 | `void tick()` | 自动转发 4 方向 AXI 通道（per Phase 8 M1 `429327d`） |
| PCIe 枚举 | `enumerate()` | 发现 PF0（**VF 简化**,Oracle M2 标注） |
| Config Space 访问 | `config_space_read/write()` | 路由到 EP 的 PcieConfigSpace |
| BAR 分配 | `bar_alloc()` | 在 EP 配置空间写入 BAR 基址/属性 |
| BAR 访问路由 | `bar_route()` | 经 axi_master_out 转发 BAR 空间读写 |

---

## 4. PF0-only 简化说明

**Oracle M2 标注** (`docs/architecture/14-pcie-ip-microarchitecture.md` L939):
- **VF 经 `stream_id` 直访**,非枚举发现(简化假设,生产 RC 需遍历完整 PCIe 拓扑)
- **PF0-only**: 假设 EP 仅暴露 1 个 PF + 16 个 VF,真实 RC 需支持 multi-PF(per PcieEndpointIP 17 ports 定义)
- **BAR 分配**: v1.0 MVP 仅支持静态 BAR 分配,v1.1 完整需支持 dynamic BAR resize

---

## 5. 测试与验证

**全链路 E2E** (`test/test_pcie_endpoint_ip_full_e2e.cc`, per Phase 8):
- 3 TEST_CASE: `{config, bar, rc}` 全 PASS
- `rc` TEST_CASE: PcieRootComplexTLM 枚举 + BAR 分配 + BAR 访问路由 PASS

---

## 6. 已知限制

- **PF0-only 简化**: 不支持 multi-PF（per Oracle M2 标注）;VF 通过 `stream_id` 直访非枚举发现
- **静态 BAR 分配**: v1.0 MVP;v1.1 完整需 dynamic BAR resize
- **Phase 8 M1 Cfg 地址编码简化**: 当前用 `awaddr` 当 offset（per ADR-SOC-13 D5）;需未来细化按 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset`
