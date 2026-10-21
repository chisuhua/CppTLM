# Design — cpptlm-msix-generator-ip v0.5.0 (Phase 2 Change E)

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/msix-generator/spec.md`](specs/msix-generator/spec.md)
> **目标 tag**: `cpptlm-msix-generator-ip-completion`
> **估时**: **5-7d** (TDD 5-step + LWC 集成 + TLP 链路)
> **Frozen surface**: 23 ABI 零 diff + **4 frozen headers 零 diff** (per AGENTS.md H3 + ADR-DGPU-08 §2.3)
> **依赖**: Change A ✅ archived + Change B ✅ archived + Change C ✅ archived (Phase 1 完成)

---

## 总体设计

### 目标

修复 §0.1 Gap L: 消除 `DGpuBoard::trigger_irq_async()` 直调, 建立真实 MSI-X Generator IP 模块, 生成 MSI-X TLP (MWr 64-bit) 经 `PcieEndpointIP::submit_tlp()` 链路上报 host.

### 核心思路 (修复 Gap L 4 件事)

1. **MSI-X Generator IP 模块** (`tlm::pcie::MsixGeneratorIP`): PBA + Vector table per-VF/per-PF, 接收 `trigger_msix(vector, addr, data, masked)` 调用
2. **per-VF/per-PF MSI-X PBA + Vector table**: per PCIe spec §6.8, 每个 PF + 16 VF 有独立 MSI-X Table (64 entries default) + PBA (Pending Bit Array)
3. **`trigger_msix(uint16_t vector, uint64_t addr, uint32_t data, bool masked)` API**: C++ 入口在 `DGpuBoard` 内部 (不暴露 ABI), 用于 device IP (SDMA/DMA/等) 触发 MSI-X 中断
4. **PcieEndpointIP 集成**: 接收 MSI-X 请求, 发 MSI-X TLP (MWr 64-bit with addr + data) 至 host (复用 Change D `submit_tlp()` API)

---

## 数据路径 (实施后)

```
device IP (e.g., SDMA 完成中断):
    │
    ▼
DGpuBoard::trigger_msix(vector, addr, data, masked)
    │
    ▼ (C++ API, 不进 ABI)
MsixGeneratorIP::trigger_msix(vector, addr, data, masked)
    │
    │ Step 1: PBA[vector] bit 检查 (masked?)
    │ Step 2: MSI-X Vector Table[vector] lookup:
    │           - addr_lo = table[vector].addr_lo
    │           - addr_hi = table[vector].addr_hi
    │           - data   = table[vector].data
    │ Step 3: 生成 PcieTlpBundle (MWr 64-bit, per PCIe spec §6.8.2)
    │
    ▼
LWC hub 路由 (Change B 设施, MsixBundle)
    │
    ▼
PcieEndpointIP::submit_tlp(tlp_bundle)
    │
    ▼
PCIe link (MWr 64-bit TLP, addr + data)
    │
    ▼
host (MSI-X 中断处理)

MSI-X Table 结构 (per PCIe spec §6.8):
    ┌────────────────────────────────────────────┐
    │ MSI-X Table (BAR 指向的 memory)           │
    │ 每 entry:                                  │
    │   - MsgAddr[63:2] (64-bit, 4-byte aligned) │
    │   - MsgData (32-bit)                       │
    │   - Vector Control (1 bit: Masked)          │
    └────────────────────────────────────────────┘
    PBA (Pending Bit Array):
    ┌────────────────────────────────────────────┐
    │ 每 bit = 1 vector pending interrupt         │
    └────────────────────────────────────────────┘
```

---

## 核心模块设计

### MsixGeneratorIP (`include/tlm/pcie/msix_generator_ip.hh`)

```cpp
namespace tlm::pcie {

// MSI-X Generator IP: 仿真 PCIe EP MSI-X Generator 硬件行为
// - PBA (Pending Bit Array) per vector
// - MSI-X Vector Table per vector (addr_lo, addr_hi, data, masked)
// - trigger_msix() → MSI-X TLP (MWr 64-bit)
class MsixGeneratorIP : public ChStreamModuleBase {
public:
    // LWC SlavePort in: 接收 MsixBundle interrupt 请求 (per Change B)
    cpptlm::InputStreamAdapter<bundles::MsixBundle>& msix_in() { return msix_in_; }

    // C++ API: trigger MSI-X interrupt (from DGpuBoard/device IP)
    void trigger_msix(uint16_t vector, uint64_t addr, uint32_t data, bool masked);

    // MSI-X table access (per-VF/per-PF)
    void set_table_entry(uint16_t vector, uint64_t addr_lo, uint64_t addr_hi,
                          uint32_t data, bool masked);
    void set_pba_bit(uint16_t vector, bool pending);

    // TLP out: submit to PcieEndpointIP via submit_tlp()
    void submit_tlp(const PcieTlpBundle& tlp);

private:
    // MSI-X Vector Table (64 entries default per PF/VF)
    struct MsixVectorEntry {
        uint64_t addr_lo;
        uint64_t addr_hi;
        uint32_t data;
        bool     masked;
    };
    std::vector<MsixVectorEntry> vector_table_;  // size = 64 * 17 (1 PF + 16 VF)

    // Pending Bit Array
    std::vector<bool> pba_;  // size = 64 * 17

    // LWC in port (MsixBundle)
    cpptlm::InputStreamAdapter<bundles::MsixBundle> msix_in_;

    // TLP submit (to PcieEndpointIP)
    void submit_msix_tlp(uint16_t vector, uint64_t addr, uint32_t data);
};

}  // namespace tlm::pcie
```

### MsixTable (`include/tlm/pcie/msix_table.hh`)

```cpp
namespace tlm::pcie {

// MSI-X Table manager: per-VF/per-PF MSI-X table + PBA
// 每个 PF + 16 VF 有独立 table: 17 × 64 = 1088 entries
class MsixTable {
public:
    static constexpr uint16_t kVectorsPerPf = 64;
    static constexpr uint8_t  kMaxVfCount  = 16;
    static constexpr uint16_t kTotalVectors = kVectorsPerPf * (1 + kMaxVfCount);

    MsixTable();

    // Per-vector access
    void set_entry(uint16_t vector, uint64_t addr_lo, uint64_t addr_hi,
                   uint32_t data, bool masked);
    bool is_masked(uint16_t vector) const;
    uint64_t get_addr_lo(uint16_t vector) const;
    uint64_t get_addr_hi(uint16_t vector) const;
    uint32_t get_data(uint16_t vector) const;

    // PBA
    void set_pending(uint16_t vector, bool pending);
    bool get_pending(uint16_t vector) const;

    // Per-VF/PF index
    static uint16_t pf_vf_index(uint8_t pf, uint8_t vf) {
        return pf * kVectorsPerPf + vf * kVectorsPerPf;
    }

private:
    std::vector<MsixVectorEntry> table_;  // kTotalVectors entries
    std::vector<bool> pba_;               // kTotalVectors bits
};

}  // namespace tlm::pcie
```

### MSI-X TLP 生成 (per PCIe spec §6.8.2)

```cpp
// MSI-X TLP: MWr 64-bit
// PCIe spec §6.8.2: MSI-X is posted write, no completion
// Format: TLP Header (4DW) + Data (4B)
//   - Fmt[2:0] = 110 (MWr 64-bit)
//   - Type[4:0] = 00000 (Memory Write)
//   - TC[2:0] = 0
//   - Attr[1:0] = 00
//   - Length[9:0] = 1 (1 DW)
//   - Address[63:2] = MsgAddr[63:2] (4-byte aligned)
//   - Data[31:0] = MsgData

void MsixGeneratorIP::submit_msix_tlp(uint16_t vector, uint64_t addr, uint32_t data) {
    PcieTlpBundle tlp;
    tlp.kind = PcieTlpBundle::KIND_MWR_64;        // MWr 64-bit
    tlp.fmt  = 0b110;
    tlp.type = 0b00000;
    tlp.tc   = 0;
    tlp.attr = 0b00;
    tlp.td   = 0;
    tlp.ep   = 0;
    tlp.length = 1;                                // 1 DW
    tlp.addr_hi = (addr >> 32) & 0xFFFFFFFF;
    tlp.addr_lo = addr & 0xFFFFFFFF;               // low 32-bit (addr[31:2] << 2)
    tlp.data_be = 0b1111;                          // 4B all valid
    tlp.data.resize(4);
    tlp.data[0] = data & 0xFF;
    tlp.data[1] = (data >> 8) & 0xFF;
    tlp.data[2] = (data >> 16) & 0xFF;
    tlp.data[3] = (data >> 24) & 0xFF;

    submit_tlp(tlp);  // → PcieEndpointIP::submit_tlp()
}
```

---

## LWC 装配 (per Change B 设施)

```
LWC hub (已有, per Change B):
    msix_generator_in (SlavePort, MsixBundle) ← device IP (SDMA/DMA)
```

**DGpuBoard::bind_memory_backings()** 装配:
```cpp
// msix_generator setup
msix_generator_ = std::make_unique<MsixGeneratorIP>();
msix_generator_->set_pcie_ep(ep_);

// LWC hub wiring (per Change B)
lwc_hub_->set_target(PortIndex::MSIX, msix_generator_.get());
```

**注意**: `MsixBundle` 已在 Change B T8 落地 (`include/bundles/msix_bundles_tlm.hh`), **不重写**.

---

## 边界 (per arch-truth-cleanup + AGENTS.md 4 层模型)

### chip-internal (MsixBundle + LWC)

- `MsixGeneratorIP` 内部: `MsixBundle` (4 fields: vector + addr_lo/hi + data + masked)
- `MsixGeneratorIP` → `PcieEndpointIP::submit_tlp()`: `PcieTlpBundle` (board-level TLP)
- **路径**: device IP → LWC hub → MsixGeneratorIP → PcieTlpBundle → submit_tlp()

### host↔board (PcieTlpBundle, 冻结)

- `PcieEndpointIP::submit_tlp()` 接收 `PcieTlpBundle`
- `PcieEndpointIP` → PCIe link: MSI-X TLP (MWr 64-bit)
- **不碰** frozen headers (`cpptlm_emulator.h` 零新增 ABI)

### 禁建约束 (per ADR-DGPU-08 §2.3)

- ❌ **不建** `cpptlm_emulator_msix.h` (ABI 头文件)
- ❌ **不建** `trigger_msix` ABI 函数
- ✅ `trigger_msix` C++ 入口在 `DGpuBoard` 内部

---

## Frozen Surface 锁

| 文件 | 约束 |
|------|------|
| `include/abi/cpptlm_emulator.h` | 零新增 ABI 函数 (per ADR-088 §D5) |
| `include/tlm/gpu/pcie_endpoint_tlm.h` | 零修改 (deprecated, frozen) |
| `include/tlm/gpu/pcie_display_device.hh` | 零修改 (frozen) |
| `include/tlm/pcie/pcie_bundles_tlm.hh` | 零修改 (frozen) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 准 frozen (仅 additive `submit_tlp()` per Change D) |

---

## 协同依赖

| 协同 Change | 复用内容 |
|------------|---------|
| Change B (`lwc-bus-hub-mvp`) | LWC hub 设施 + MsixBundle (4 fields: vector + addr_lo/hi + data + masked) |
| Change D (`pcie-tlp-entry-layer`) | `PcieTlpSerializer` (MSI-X TLP 序列化) + `PcieEndpointIP::submit_tlp()` API |
| Change A (`vram-controller-functional-integration`) | 0 关联 (独立路径) |
| Change C (`gmmu-pte-bus-correction`) | 0 关联 (独立路径) |

---

## 实施注意事项

### TLP 序列化 (复用 Change D v0.5.2)

`MsixGeneratorIP` 内部调用 `PcieTlpSerializer::encode_msix_tlp(vector, addr, data)`, 复用 `include/utils/pcie_tlp_serializer.hh` (host_pcie_test_stub 已在用).

### MSI-X Table 初始化

MSI-X Table (BAR 指向的 memory) 由 host software 配置. 本 change 仅建模 table 行为 (软件写入 table entry 后, `trigger_msix` 查表生成 TLP).

### per-VF MSI-X

每个 VF 有独立 MSI-X Table (64 entries). VF MSI-X table base 从 PF MSI-X table base + offset 计算 (per PCIe spec §6.8.2).

### masked bit 语义

`Vector Control[0]` = 1 时, 该 vector 被 mask, `trigger_msix` 查 PBA 但不发 TLP. 解除 mask 后, PBA bit 仍 pending 的 vector 会触发 (per PCIe spec).
