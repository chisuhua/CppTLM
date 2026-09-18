# ADR-SOC-17: PcieMockIP — 独立 PCIe Mock IP（gem5 风格简化端点）

> **状态**: ✅ Accepted
> **日期**: 2027-09-17
> **影响**: Phase 9 新增独立组件，profile `"pcie_path": "mock"` 启用；不影响既有 PcieEndpointIP
> **类别**: SoC 架构 / PCIe 端点仿真
> **实施**: OpenSpec change `2026-09-16-cpptlm-pcie-tlp-wire-datapath` T-P9-3
> **关联**: spec [`openspec/changes/.../specs/pcie-tlp-wire-datapath/spec.md`](../../../openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md) §pcie-mock-ip；设计 `docs/architecture/14-pcie-ip-microarchitecture.md` §Phase 9

---

## 1. 背景

### 1.1 问题

既有 PcieEndpointIP 是完整的 17 端口 SR-IOV 模型（1 PF + 16 VF），包含：
- PcieLinkLayer（FC Token Bucket、retry buffer、ACK-NAK）
- PciePhyDigitalCtrl（LTSSM 11 态）
- PcieBypassMux（3 态模式切换）
- PcieAxiAdapter + AXI4Mapper
- CompletionTracker（NP↔CplD trans_id 关联）
- PcieSriovVfPool（per-VF Config Space / MSI-X / FC / seq#）

合计 ~5000+ 行，17 端口状态机。当用户仅需要基本的 BAR 空间模拟 + MSI-X + D3hot gate 时（如 PTX-EMU functional-only 集成、profile `"pcie_path": "mock"` 场景），完整 PcieEndpointIP 的 **TLP 协议栈负担和 17 端口 SR-IOV 状态机完全不必要**。

### 1.2 既有 bypass 方案不足

Phase 7 的 HostBypassTLM 路径（`bar_write`）仍依赖 PcieEndpointIP::tick() 内部 BAR 处理，仍有完整 SR-IOV 状态机。

### 1.3 需求

用户目标（per spec.md §pcie-mock-ip）：
> "如果配置是跳过 TLP 链，那么就跳过 PCIe EP 的内部逻辑，只需要基本的 bar 空间模拟并支持创建 AXI payload 发到 AXI 接口"

需要 **独立、轻量（~500 行目标，800 行上限）**的简化 PCIe 端点组件。

---

## 2. 决策

✅ **创建独立 PcieMockIP 组件**（gem5 风格简化端点），profile `"pcie_path": "mock"` 启用。

### 2.1 架构定位

| 维度 | 决策 | 理由 |
|------|------|------|
| **独立组件** | 不继承 SimModule/SimObject，无 TLM 协议栈依赖 | 保持轻量（~500 行），避免框架层耦合 |
| **BAR 空间** | 复用 PcieBarRouter 数据驱动机制（JSON bar0_registers） | 与 PcieEndpointIP 等价语义，同一 key 命中相同值 |
| **MSI-X** | 直接调 cpptlm_intr_deliver_cb_t ABI 回调 | 不经过 TLP 编码/投递链，简化时序验证 |
| **D3hot gate** | 对齐 PcieEndpointIP AXI slave 路径 | 所有 BAR 写 DECERR（-EIO），无 doorbell 例外（V-5 决议） |
| **AXI payload** | 内部 Axi4StreamAdapter（device-side only） | EP 内部设备发起 SOC 输出时直接构造 Axi4Bundle |
| **不模拟** | TLP 编码/FC/ACK-NAK/PHY/LL/Config Space | 目标仿真场景不需要协议栈精度 |
| **不依赖** | PcieEndpointIP / PcieSriovVfPool / 17 端口 SR-IOV | 独立组件，零耦合 |

### 2.2 Mock IP 边界（明确不做）

- ❌ TLP 编码/解码
- ❌ FC token bucket
- ❌ ACK-NAK DLLP
- ❌ LTSSM 11 态
- ❌ 17 端口 SR-IOV
- ❌ PcieAxiAdapter 依赖（用内部 Axi4StreamAdapter）
- ❌ Config Space 完整语义（仅 PMCSR D3hot gate）
- ❌ TLP sink 注册
- ❌ backdoor ABI

### 2.3 行为对齐矩阵

| 行为 | PcieEndpointIP AXI 路径 | PcieMockIP | 对齐状态 |
|------|------------------------|------------|----------|
| BAR0 读写 | `bar_store_[key]` | `bar_regs_[(bar, offset)]` | ✅ 等价语义 |
| D3hot gate | 全 BAR DECERR（bresp=3） | 全 BAR -EIO（-5） | ✅ 对齐 |
| D3hot doorbell | 被 gate（无例外） | 被 gate（无例外） | ✅ V-5 决议 |
| MSI-X | dispatch_msix → tx_tlp → 投递 | msix_update_pending → ABI 回调 | 🟡 路径不同 |

---

## 3. 实施

### 3.1 文件清单

| 文件 | 行数 | 状态 |
|------|------|------|
| `include/tlm/pcie/pcie_mock_ip.hh` | ~145 | ✅ 新 |
| `src/tlm/pcie/pcie_mock_ip.cc` | ~200 | ✅ 新 |
| `test/test_pcie_mock_ip_basic.cc` | ~192 | ✅ 新 |

### 3.2 API 设计

```cpp
namespace cpptlm::pcie {
class PcieMockIP {
public:
    // 配置
    void attach_composition(const nlohmann::json& cfg);

    // BAR 空间
    int mmio_write(uint8_t bar, uint64_t offset, const void* data, std::size_t len);
    int mmio_read(uint8_t bar, uint64_t offset, void* buf, std::size_t len);

    // PCIe Config（仅 PMCSR D3hot gate）
    int pcie_config_write(uint16_t offset, uint8_t width, uint32_t value);
    int pcie_config_read(uint16_t offset, uint8_t width, uint32_t& value);

    // MSI-X（直接 ABI 回调）
    int msix_init(uint32_t table_size, uint32_t mask);
    int msix_update_pending(uint32_t vector);
    int msix_clear_pending(uint32_t vector);
    void register_msi_callback(std::function<void(uint32_t, uint32_t)> cb);

    // Device-side AXI
    void device_axi_write(uint64_t addr, const void* data, std::size_t len);

    // 查询
    bool is_mmio_gated() const noexcept;
    bool axi_master_req_valid() const;
    const bundles::Axi4Bundle& axi_master_req_data() const;
    bool msix_tlp_pending() const noexcept;  // 永远 false
};
}
```

### 3.3 注册机制

PcieMockIP 是独立 C++ 类（非 SimModule/SimObject），由测试直接构造，不注册到 ModuleFactory。依赖的 src 文件注册到 `src/CMakeLists.txt` CORE_SOURCES。

---

## 4. 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| Mock IP 与 PcieEndpointIP 行为漂移 | 测试误判 | 行为对齐矩阵定期验证，6 个 TEST_CASE 持续监控 |
| 行数膨胀超 800 行 | 违背轻量设计 | 编译期约束 + CODE SMELLS 250 行检查 |
| PMCSR 语义覆盖不全 | 门控逻辑出错 | 仅支持 D0/D3hot 切换，D1/D2 忽略（对齐 EP） |

---

## 5. 参考文献

- [spec.md §pcie-mock-ip](../../../openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md) (lines 175-233)
- [ADR-SOC-11-pcie-endpoint-ip.md](./ADR-SOC-11-pcie-endpoint-ip.md) — PcieEndpointIP 架构决策
- [14-pcie-ip-microarchitecture.md](../../architecture/14-pcie-ip-microarchitecture.md) §Phase 9 — PCIe EP 微架构设计
- `include/tlm/pcie/pcie_mock_ip.hh` — Mock IP 头文件
- `src/tlm/pcie/pcie_mock_ip.cc` — Mock IP 实现
- `test/test_pcie_mock_ip_basic.cc` — 6 个 TEST_CASE（29 assertions）

---

## Status Update

No updates yet (initial version, 2027-09-17).

### 2027-09-17 — 文档迁移（per Phase 9 P2 摸底， git mv 保留历史）

**关联文档路径迁移**: `docs/architecture/14-pcie-ip-microarchitecture.md` → `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`（git mv 保留历史, PcieEndpointIP 内部微架构 SSOT 位置迁移）。正文链接保留旧路径以符合 ADR 不可变原则，新读者请移步新路径查阅。
