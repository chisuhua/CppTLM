# ADR-SOC-12: Host Bypass 软件 Bring-up 路径 + 自研 RC PF0-only 简化

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: 软件 bring-up 跳过 PCIe RC BFM + 自研 RC + M1 修复 + 23 ABI 兼容性
> **类别**: SoC 架构 / Phase 7 PCIe EP 子链路
> **关联文档**:
> - [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md) L939（Oracle M2 标注）
> - [`docs/soc_arch/architecture/01-host-interface.md`](../architecture/01-host-interface.md) §11/§12（HostBypassTLM + PcieRootComplexTLM 详细设计）
> **关联 OpenSpec**: [`2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/`](../../../openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/)
> **关联 Phase 8 整合交付证据**: git commit `429327d`（M1 修复）

---

## 1. Context（背景）

### 1.1 软件 bring-up 阶段需求

dGPU SoC v1.0 在软件 bring-up 阶段需要：
- **跳过 PCIe RC BFM**(Root Complex Bus Functional Model)
- **直接桥接 AXI 接口** → PcieEndpointIP
- **节省 PCIe PHY/Link Layer 初始化时间**（数小时 → 数秒）
- **快速验证 SoC 内部模块**（不依赖 PCIe PHY 状态机）

### 1.2 可选自研 RC 模型

- **可选**组件(per `2027-01-19` 范围："可选自研 RC 模型（若不用外部 VIP/QEMU）")
- **镜像 PcieEndpointIP**(简化 enumeration)
- **PF0-only** 简化(per Oracle M2)

### 1.3 Phase 8 M1 修复

`HostBypassTLM::tick()` **自动转发 4 方向 AXI 通道**：
- `master_out ↔ slave_in`(双向)
- `slave_resp ↔ master_resp`(双向)

**修复目的**：让 PcieEndpointIP **真实消费** AXI 请求，而非"接而不消费"。

### 1.4 测试覆盖

| 测试 | 场景 |
|------|------|
| `test_host_bypass_basic.cc` | 基本桥接功能 |
| `test_host_bypass_software_bringup.cc` | 软件 bring-up 路径 |
| `test_pcie_root_complex_enumeration.cc` | 与 RC 协同 |
| `test/test_pcie_endpoint_ip_full_e2e.cc` | Phase 8 全链路 E2E（config/bar/rc 3 TEST_CASE） |

---

## 2. Decision（决策）

### D1. 软件 bring-up 跳过 PCIe RC BFM

✅ **HostBypassTLM 直接桥接 AXI**（per `2027-01-19` OpenSpec）：

- 持有 `Axi4StreamAdapter` 三端口（`axi_master_out` / `axi_slave_in` / `cfg_slave_in`）
- `tick()` 转发至底层 Axi4StreamAdapter（valid/ready 反压，不丢事务）
- 软件 bring-up 阶段跳过 PCIe PHY/Link Layer 初始化

### D2. PcieRootComplexTLM 枚举 PF0-only

✅ **自研 RC PF0-only 简化**（per Oracle M2 标注）：

- **枚举只报告 PF0**（device 0, function 0）
- **VF（VF0..VF15）的发现**需要 ARI capability + SR-IOV capability 的完整枚举流程
- **当前模型简化**：VF 通过 `stream_id` 直接访问配置空间 (`config_read/write(device, function, offset, stream_id)`)
- **已知边界**：不构成缺陷，但使用 `PcieRootComplexTLM` 进行枚举测试时必须注意此限制

### D3. M1 修复（Phase 8 整合交付 `429327d`）

✅ **HostBypassTLM/RC::tick() 4 方向 AXI 桥接**：

- `master_out ↔ slave_in`：Host ↔ SoC 双向
- `slave_resp ↔ master_resp`：响应双向

**修复目的**：让 PcieEndpointIP 真实消费 AXI 请求并返回响应。

---

## 3. Consequences（后果）

### 3.1 正面影响

- **节省仿真时间**：跳过 PCIe PHY/Link Layer 初始化（数小时 → 数秒）
- **快速验证**：bring-up 阶段快速迭代
- **可选 RC**：无需外部 VIP/QEMU 即可完成 PCIe 枚举验证

### 3.2 负面影响

- **PF0-only 限制**：VF 枚举不完整（需 ARI + SR-IOV capability）
- **可选组件**：RC 简化模型不替代真实 PCIe 枚举
- **边界明确**：Oracle M2 文档化已知边界

### 3.3 兼容性保证

- **真实 PCIe 路径**：Bypass 模式仅供软件 bring-up 使用，不可用于真实 PCIe PHY 验证
- **23 ABI 不变**：HostBypassTLM/RC 不暴露 23 ABI 外部契约

---

## 4. Implementation（实施）

### 4.1 HostBypassTLM 真实实现（per `include/tlm/pcie/host_bypass_tlm.hh`）

```cpp
class HostBypassTLM {
public:
    void init();
    void attach_to_endpoint(PcieEndpointIP* ep) noexcept;
    void detach() noexcept;
    void tick();  // Phase 8 M1 修复:4 方向 AXI 桥接
    
    // axi_master_out (Host 发起 SoC 访问)
    void set_axi_master_ready(bool r);
    void axi_master_req_consume();
    void axi_master_resp_consume();
    
    // axi_slave_in (Host 发起进入 Endpoint 的事务)
    void set_axi_slave_ready(bool r);
    void axi_slave_req_consume();
    void axi_slave_resp_consume();
    
    // cfg_slave_in (AXI4-Lite 配置访问)
    void set_axi_cfg_ready(bool r);
    void axi_cfg_req_consume();
    void axi_cfg_resp_consume();
    
    void reset();
};
```

### 4.2 PcieRootComplexTLM 真实实现（per `include/tlm/pcie/pcie_root_complex_tlm.hh`）

```cpp
struct DiscoveredDevice {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus_num;
    uint8_t device_num;
    uint8_t function_num;
};

class PcieRootComplexTLM {
public:
    void init();
    void attach_to_endpoint(PcieEndpointIP* ep) noexcept;
    void detach() noexcept;
    bool enumerate();  // PCIe 枚举:发现 EP 设备/功能(PF0-only 简化)
    
    // 已发现的设备列表(enumerate 后填充)
    std::vector<DiscoveredDevice> discovered_devices_;
    
    void tick();
    void reset();
};
```

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 软件 bring-up 与真实 PCIe PHY 不一致 | 🟢 低 | Bypass 模式明确文档化不可用于真实 PHY 验证 |
| **R2** | PcieRootComplexTLM PF0-only 限制 | 🟡 中 | Oracle M2 已文档化;v1.1 追加 ARI/SR-IOV capability |
| **R3** | M1 修复回归风险 | 🟢 低 | `test/test_pcie_endpoint_ip_full_e2e.cc` 3 TEST_CASE PASS |

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-11 | PcieEndpointIP 17 ports（HostBypassTLM 桥接目标） |
| ADR-SOC-14 | v5.5+ 系统级硬件仿真集成（含 live migration） |

### 6.2 关联 OpenSpec

| Change | 关联 |
|--------|------|
| `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc` | Phase 7 Host Bypass + RC 实施 |

### 6.3 关联真实代码

| 文件 | 角色 |
|------|------|
| `include/tlm/pcie/host_bypass_tlm.hh` | HostBypassTLM 真实定义 |
| `include/tlm/pcie/pcie_root_complex_tlm.hh` | PcieRootComplexTLM 真实定义 |

---

## Status Update

- **2027-02-09**: 📋 Proposed。Phase 7 + Phase 8 已完成（HEAD `429327d`）；Oracle M1/M2 条件已修复/文档化；测试 `test/test_pcie_endpoint_ip_full_e2e.cc` PASS。