# io-pci 微架构文档

> **类别**: io > pci · **状态**: 🟡 规划中 + 📋 v1.0 dGPU SoC 战略补充(per [`dgpu-soc-pcie-slice.md`](./dgpu-soc-pcie-slice.md))
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/pci_device_tlm.hh` + `pci_host_tlm.hh`
> **蓝图来源**: gem5 `src/dev/pci/device.hh` + `src/dev/pci/host.hh`
> **首版 commit**: 蓝图（来自调研 §2.5 + Phase 7 备选 dGPU）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-pio.md](./io-pio.md) | [io-dma.md](./io-dma.md) | [coherence-bridge.md](./coherence-bridge.md) (dGPU 桥接)

---

## 1. 设计目标（蓝图）

`tlm::PciDeviceTLM` + `tlm::PciHostTLM` 是 CppTLM Phase 7 备选 dGPU 规划的 **PCI 设备 + 主机桥接**——配置空间 + BAR 寻址 + MSI 中断。**与 gem5 对位**: `gem5::PciDevice`（~400 行，config space + BAR + DMA）+ `PciHost`（PCIe 物理层 + 中断）。

**核心特征**：
- **PCI 配置空间**（256 B 兼容 / 4 KB PCIe）
- **BAR 寻址**（典型 6 个 BAR 寄存器）
- **MSI / MSI-X 中断**（v0 简化：可选）
- **DMA 传输**（继承 DmaDeviceTLM）
- **PCIe 物理层**（PciHost，链路训练 + 带宽限流）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                  PciDeviceTLM 继承 DmaDeviceTLM              │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  Config Space (256 B)                             │     │
│  │    - 0x00: Vendor ID / Device ID                  │     │
│  │    - 0x04: Command / Status                       │     │
│  │    - 0x10-0x24: BAR0-BAR5 (6 个)                 │     │
│  │    - 0x3C: Interrupt Line / Pin                   │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  MSI Capability (v0 简化)                          │     │
│  │    - MSI Address / Data 寄存器                    │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  DMA Port (继承)                                  │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                  PciHostTLM (CPU 侧)                        │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  PCI Config Space Access                          │     │
│  │    - PIO 0xCF8/0xCFC (传统机制)                  │     │
│  │    - MMIO 0xE0000000+ (PCIe ECAM)                │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  BAR Router (CPU 内存访问 → PCI 设备)              │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  MSI Interrupt Injector (向 CPU 投递中断)         │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 PciDevice 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `config_req_in_` | `InputStreamAdapter<PciConfigBundle>` | 接收 config read/write |
| `config_resp_out_` | `OutputStreamAdapter<PciConfigBundle>` | 发送 config 响应 |
| `bar_req_in_[6]` | `InputStreamAdapter<PioReqBundle>` | 接收 BAR 访问（6 个 BAR） |
| `bar_resp_out_[6]` | `OutputStreamAdapter<PioRespBundle>` | 发送 BAR 响应 |
| `msi_out_` | `OutputStreamAdapter<MsiBundle>` | 发送 MSI 中断 |
| `dma_port_master_` | `OutputStreamAdapter<CacheReqBundle>` | DMA（继承） |
| `dma_port_slave_` | `InputStreamAdapter<CacheRespBundle>` | 接收内存响应（继承） |

### 2.2 PciHost 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `config_req_out_` | `OutputStreamAdapter<PciConfigBundle>` | 转发 config 到设备 |
| `config_resp_in_` | `InputStreamAdapter<PciConfigBundle>` | 接收 config 响应 |
| `bar_master_req_in_` | `InputStreamAdapter<PioReqBundle>` | CPU PIO 访问 PCI 设备 |
| `bar_master_resp_out_` | `OutputStreamAdapter<PioRespBundle>` | 响应 CPU PIO |
| `msi_in_` | `InputStreamAdapter<MsiBundle>` | 接收 MSI 中断 |
| `irq_cpu_out_` | `OutputStreamAdapter<IrqBundle>` | 向 CPU 投递中断 |

## 3. 接口（规划）

```cpp
namespace tlm {

// === PciConfigBundle ===
struct PciConfigBundle {
    uint8_t bus, device, function;
    uint16_t offset;
    bool is_write;
    uint32_t data;
    uint64_t transaction_id;
};

struct MsiBundle {
    uint64_t addr;     // MSI address
    uint16_t data;     // MSI data
    uint8_t vector;    // 中断向量
};

// === PciDeviceTLM ===
class PciDeviceTLM : public DmaDeviceTLM {
public:
    static constexpr uint8_t NUM_BARS = 6;
    static constexpr uint16_t VENDOR_ID_AMD = 0x1002;  // 用于 dGPU
    static constexpr uint16_t DEVICE_ID_RADEON = 0x73BF;

    struct BarConfig {
        uint64_t base_addr;
        uint64_t size;
        bool is_64bit;
        bool is_prefetchable;
    };

    explicit PciDeviceTLM(const std::string& name, EventQueue* eq,
                          uint16_t vendor_id, uint16_t device_id,
                          uint8_t bus = 0, uint8_t device = 0,
                          uint8_t function = 0);

    std::string get_module_type() const override { return "PciDeviceTLM"; }

    void on_config_loaded() override;
    void set_bar(uint8_t bar_idx, uint64_t size, bool is_64bit = false,
                 bool is_prefetchable = false);
    void enable_msi(uint64_t addr, uint16_t data) { msi_enabled_ = true; ... }

    // 设备触发 MSI 中断
    void raise_msi(uint8_t vector);

    // PioDevice 风格（继承自 DmaDevice）
    void on_dma_complete(uint64_t txn_id, bool success) override;

    // 端口
    cpptlm::InputStreamAdapter<bundles::PciConfigBundle>& config_req_in();
    cpptlm::OutputStreamAdapter<bundles::PciConfigBundle>& config_resp_out();
    cpptlm::InputStreamAdapter<bundles::PioReqBundle>& bar_req_in(uint8_t idx);
    cpptlm::OutputStreamAdapter<bundles::PioRespBundle>& bar_resp_out(uint8_t idx);
    cpptlm::OutputStreamAdapter<bundles::MsiBundle>& msi_out();

    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    uint16_t vendor_id_, device_id_;
    uint8_t bus_, device_, function_;
    uint32_t config_space_[64];  // 256 B
    BarConfig bars_[NUM_BARS];
    bool msi_enabled_;
    uint64_t msi_addr_;
    uint16_t msi_data_;
    // 端口数组
    std::array<InputStreamAdapter<PioReqBundle>, NUM_BARS> bar_req_ins_;
    std::array<OutputStreamAdapter<PioRespBundle>, NUM_BARS> bar_resp_outs_;

    // 统计
    tlm_stats::Scalar config_reads_;
    tlm_stats::Scalar config_writes_;
    tlm_stats::Scalar bar_writes_;
    tlm_stats::Scalar msi_raised_;
};

// === PciHostTLM ===
class PciHostTLM : public PioDeviceTLM {
public:
    static constexpr uint64_t DEFAULT_ECAM_BASE = 0xE0000000;
    static constexpr uint64_t DEFAULT_ECAM_SIZE = 256 * 1024 * 1024;  // 256 MB

    explicit PciHostTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "PciHostTLM"; }

    void on_config_loaded() override;
    void register_pci_device(uint8_t bus, uint8_t device,
                             PciDeviceTLM* dev);

    // PioDevice 抽象实现（CPU 通过 PIO/MMIO 访问 config）
    uint64_t read(uint64_t offset, unsigned size_bytes) override;
    void write(uint64_t offset, uint64_t data, unsigned size_bytes) override;

    // 端口
    cpptlm::OutputStreamAdapter<bundles::PciConfigBundle>& config_req_out();
    cpptlm::InputStreamAdapter<bundles::PciConfigBundle>& config_resp_in();
    cpptlm::InputStreamAdapter<bundles::MsiBundle>& msi_in();
    cpptlm::OutputStreamAdapter<bundles::IrqBundle>& irq_cpu_out();

    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    std::map<uint32_t, PciDeviceTLM*> device_table_;  // (bus,dev,fn) → device
    std::map<uint8_t, std::map<uint64_t, PciDeviceTLM*>> bar_routing_;  // bus → (addr → device)

    // 统计
    tlm_stats::Scalar config_accesses_;
    tlm_stats::Scalar bar_routes_;
    tlm_stats::Scalar msi_forwarded_;
};

}  // namespace tlm
```

## 4. 行为流程

```cpp
// PciDevice tick: 处理 config + BAR + MSI
void PciDeviceTLM::tick() {
    // 1. Config read/write
    if (config_req_in_.valid() && config_req_in_.ready()) {
        const auto& cfg = config_req_in_.data();
        uint16_t reg = cfg.offset / 4;
        if (cfg.is_write) {
            config_space_[reg] = cfg.data;
            // BAR 写入处理
            if (reg >= 0x10 && reg <= 0x24) {
                uint8_t bar_idx = (reg - 0x10) / 4;
                bars_[bar_idx].base_addr = cfg.data & ~0xF;
                // 64-bit BAR 扩展...
            }
            ++config_writes_;
        } else {
            PciConfigBundle resp;
            resp.data = config_space_[reg];
            resp.transaction_id = cfg.transaction_id;
            config_resp_out_.write(resp);
            ++config_reads_;
        }
        config_req_in_.consume();
    }

    // 2. BAR 访问（继承 PioDevice 风格）
    for (uint8_t i = 0; i < NUM_BARS; ++i) {
        if (bar_req_ins_[i].valid() && bar_req_ins_[i].ready()) {
            const auto& req = bar_req_ins_[i].data();
            handle_bar_access(i, req);
            bar_req_ins_[i].consume();
        }
    }

    if (adapter_) adapter_->tick();
}

void PciDeviceTLM::raise_msi(uint8_t vector) {
    if (!msi_enabled_) return;
    MsiBundle msi;
    msi.addr = msi_addr_;
    msi.data = msi_data_ + vector;
    msi.vector = vector;
    msi_out_.write(msi);
    ++msi_raised_;
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/pci/device.hh` PciDevice | `tlm::PciDeviceTLM` | 同语义 |
| `PciDevice::configSpace` | `config_space_` | 同语义 |
| `PciDevice::BAR0-BAR5` | `bars_[6]` | 同语义 |
| `src/dev/pci/host.hh` PciHost | `tlm::PciHostTLM` | 同语义 |
| `PciHost::configAddress` | `PciConfigBundle.addr` | 同语义 |
| `PciHost::intrPost` | `raise_msi` | 同语义 |
| `PciDevice::Interrupt` | `MsiBundle` | 同语义 |
| `src/dev/pci/pci_host.hh` GenPciHost | (Phase 7 备选) | Intel/AMD 特定 |

## 6. 实施路径

### Phase 7 备选 dGPU 步骤

1. 新建 `PciConfigBundle` / `MsiBundle` 在 `include/bundles/pci_bundles_tlm.hh`
2. 新建 `PciDeviceTLM`（~350 行）继承 DmaDeviceTLM
3. 新建 `PciHostTLM`（~250 行）继承 PioDeviceTLM
4. 加 Catch2 测试

**估计工作量**: 3-4 周（基础版 + dGPU 集成）

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **PCIe 规范复杂度**——4KB ECAM、MSI-X 等 | 高 | 中 | v0 仅实现 256B 兼容 config + MSI（基本功能） |
| R2 | **BAR 大小探测**——CPU 通过写入 0xFFFFFFFF 探测 | 中 | 中 | 实现标准探测序列 |
| R3 | **多 function**——单 device 多 function | 中 | 中 | function 字段支持（v0 简化：单 function） |
| R4 | **MSI 路由**——多设备 MSI 地址冲突 | 中 | 中 | PciHost 路由表（device → vector） |

## 8. 决策点

### D1 PCIe vs PCI
- **Q**: 默认 PCIe 还是传统 PCI？
- **建议**: PCIe（Phase 7 备选 dGPU 必需）
- **依赖**: dGPU 需求

### D2 MSI vs 传统中断
- **Q**: v0 MSI 还是传统 IRQ？
- **建议**: MSI（更简单 + 必备）
- **依赖**: 中断模型

### D3 dGPU Vendor/Device ID
- **Q**: 默认 vendor/device ID 多少？
- **建议**: 0x1002 / 0x73BF (AMD Radeon 仿造)
- **依赖**: 仿真目的

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7 备选 dGPU (未来)**: 基础版实施（PciDevice + PciHost）
