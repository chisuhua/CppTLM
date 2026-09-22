# D2 Design — Memory 设备 MVP 详细设计

> **配套**: [proposal.md](proposal.md) · [tasks.md](tasks.md) · [specs/memory-device-mvp/spec.md](specs/memory-device-mvp/spec.md)
> **基于**: D1 Display IO 设备设计（`2026-09-20-cpptlm-pcie-display-io-mvp`）

---

## §1 系统拓扑（D2）

```
┌────────────────────────────────────────────────────────┐
│                   Host (UsrLinuxEmu)                   │
│                                                        │
│  ┌──────────────┐    ┌─────────────────────────┐     │
│  │ NVMe-like   │ →  │ CpptlmBridge (bridge.h) │     │
│  │ Stub Driver │    │ 15 ABI fn wrappers      │     │
│  └──────────────┘    └──────────┬──────────────┘     │
│                                 │ C ABI call          │
└────────────────────────────────┼─────────────────────┘
                                 │ (15 ABI fns)
┌────────────────────────────────┼─────────────────────┐
│                    CppTLM (实现仓)                 │
│                                ▼                        │
│   ┌────────────┐    ┌─────────────────────┐          │
│   │ Host       │ →  │ PCIe Root Complex   │          │
│   │ Bypass     │    │                     │          │
│   │ (Phase 7)  │    └──────────┬──────────┘          │
│   └────────────┘               │ PCIe TLP            │
│                                ▼                        │
│   ┌─────────────────────────────────────┐            │
│   │  PcieEndpointIP (Phase 4, 已交付)    │            │
│   │  ┌─────────────────────────────┐    │            │
│   │  │ PcieConfigSpace            │◄──┼── vendor_id │  │
│   │  │  (config_space() accessor) │    │   device_id │
│   │  ├─────────────────────────────┤    │            │
│   │  │ MsiXTable  (msix() accessor)│   │            │
│   │  ├─────────────────────────────┤    │            │
│   │  │ PcieDisplayDevice (D1)     │◄──┼── BAR 0/1  │
│   │  ├─────────────────────────────┤    │            │
│   │  │ PcieMemoryDevice (D2 新)   │◄──┼── BAR 0/2  │
│   │  │   registers_[4KB]  (BAR 0)  │    │            │
│   │  │   memory_backing_[8GB](BAR 2)│   │            │
│   │  └─────────────────────────────┘    │            │
│   └─────────────────────────────────────┘            │
│             ▲                                           │
│             │ via soc_->getInternalInstance("pcie_ep")
│   ┌─────────────────────────────────────┐            │
│   │  DGpuBoard (dgpu_board_shell)       │            │
│   │   mmio_read/write(BAR 0/1/2)        │            │
│   │     ↓ D2 路由变更                    │            │
│   │   → PcieMemoryDevice              │            │
│   │   backdoor_read/write(BAR 2)      │            │
│   │     ↓ D2 路由变更                    │            │
│   │   → PcieMemoryDevice.memory_backing_ │            │
│   └─────────────────────────────────────┘            │
│                                                        │
└────────────────────────────────────────────────────────┘
```

## §2 PcieMemoryDevice 内部结构

```cpp
// include/tlm/gpu/pcie_memory_device.hh
namespace tlm::gpu {

class PcieMemoryDevice {
public:
    // BAR 0: 4KB MMIO registers
    static constexpr size_t kRegSize = 4096;
    std::array<uint8_t, kRegSize> registers_{};

    // BAR 2: 8GB memory backing (lazy alloc)
    static constexpr uint64_t kDefaultMemSize = 8ULL * 1024 * 1024 * 1024;  // 8GB
    std::vector<uint8_t> memory_backing_;

    // MMIO read/write (BAR 0 寄存器字节级访问)
    int mmio_read(uint64_t offset, void* buf, size_t len);
    int mmio_write(uint64_t offset, const void* buf, size_t len);

    // Memory backing read/write (BAR 2)
    int memory_read(uint64_t offset, void* buf, size_t len);
    int memory_write(uint64_t offset, const void* buf, size_t len);

    // Tick（推进 cycle counter，无中断）
    void tick();

    // 寄存器布局常量
    static constexpr uint32_t kRegDeviceIdentity = 0x00;   // RO: vendor_id=0x1002 + device_id=0x0002
    static constexpr uint32_t kRegMemSizeLo       = 0x10;   // RW: 容量低 32-bit
    static constexpr uint32_t kRegMemSizeHi       = 0x14;   // RW: 容量高 32-bit
    static constexpr uint32_t kRegMemBaseLo      = 0x18;   // RW: 主机可见基地址低 32-bit
    static constexpr uint32_t kRegMemBaseHi       = 0x1C;   // RW: 主机可见基地址高 32-bit
    static constexpr uint32_t kRegStatus          = 0x20;   // RW: bit 0=ready, bit 1=error
    static constexpr uint32_t kRegScratch         = 0xF0;   // RW: 16 字节测试寄存器

    // 辅助方法
    [[nodiscard]] bool has_memory_backing() const noexcept { return !memory_backing_.empty(); }
    void ensure_memory_backing_allocated();

private:
    uint64_t cycle_counter_ = 0;
};

}  // namespace tlm::gpu
```

## §3 寄存器布局（D2 范围）

| Offset | Size | Name | Access | Description |
|--------|------|------|--------|-------------|
| 0x00-0x0F | 16 | DEVICE_IDENTITY | R | vendor_id (0x1002) + device_id (0x0002) + revision |
| 0x10 | 4 | MEM_SIZE_LO | RW | 容量低 32-bit（默认 0x00000000 = 8GB 的低 32-bit） |
| 0x14 | 4 | MEM_SIZE_HI | RW | 容量高 32-bit（默认 0x00000002 = 8GB 的高 32-bit） |
| 0x18 | 4 | MEM_BASE_LO | RW | 主机可见基地址低 32-bit（D2: identity，0x0） |
| 0x1C | 4 | MEM_BASE_HI | RW | 主机可见基地址高 32-bit |
| 0x20 | 4 | STATUS | RW | bit 0=ready (RW), bit 1=error (RW1C) |
| 0xF0-0xFF | 16 | SCRATCH[0..3] | RW | 4 × uint32_t 驱动 round-trip 验证 |

**与 D1 的关键区别**：
- D1 device_id=0x0001，D2 device_id=0x0002
- D1 有 VBLANK/MSI-X，D2 无中断
- D1 BAR 1 是 32MB FB，D2 BAR 2 是 8GB memory backing
- D1 有 DISPLAY_CONTROL/FB_INFO 等显示相关寄存器，D2 有 MEM_SIZE/MEM_BASE 等内存相关寄存器

## §4 PcieEndpointIP 注入（D2）

### 4.1 新增成员（include/tlm/pcie/pcie_endpoint_ip.hh）

```cpp
class PcieEndpointIP : public SimModule {
    // ... 既有成员不变 ...

    // D2 新增: PcieMemoryDevice 持有与访问
public:
    [[nodiscard]] bool has_memory_device() const noexcept {
        return memory_device_ != nullptr;
    }
    tlm::gpu::PcieMemoryDevice& memory_device() noexcept {
        return *memory_device_;
    }
    const tlm::gpu::PcieMemoryDevice& memory_device() const noexcept {
        return *memory_device_;
    }

private:
    std::unique_ptr<tlm::gpu::PcieMemoryDevice> memory_device_;  // D2 新增
};
```

### 4.2 构造与 tick（src/tlm/pcie/pcie_endpoint_ip.cc）

```cpp
// 构造（D2 改动）
PcieEndpointIP::PcieEndpointIP(const std::string& name, EventQueue* eq)
    : SimModule(name, eq, "pcie_ep", 17)
    , display_device_(std::make_unique<tlm::gpu::PcieDisplayDevice>())  // D1
    , memory_device_(std::make_unique<tlm::gpu::PcieMemoryDevice>())    // D2
{
    // ... 既有初始化
}

// tick（D2 改动：推进 memory device cycle counter）
void PcieEndpointIP::tick() {
    // ... 既有 tick 逻辑 ...

    // D2: 推进 memory device（无 MSI-X，仅 cycle counter）
    if (memory_device_) {
        memory_device_->tick();
    }
}

// PcieMemoryDevice::tick() 内部（D2 设计）
void PcieMemoryDevice::tick() {
    cycle_counter_++;
    // 无 VBLANK，无 MSI-X，仅推进 cycle counter
}
```

## §5 DGpuBoard 路由层修改（D2）

### 5.1 `DGpuBoard::mmio_read` 修改

```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;
    if (is_mmio_gated()) return -EIO;

    // D1: BAR 0/1 → PcieDisplayDevice
    if (soc_ && (bar == 0 || bar == 1)) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            // ... D1 路由逻辑不变 ...
        }
    }

    // D2 新增: BAR 2 → PcieMemoryDevice（无 MSI-X，仅 MMIO 读写）
    if (soc_ && bar == 2) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_memory_device()) {
            // D2: memory device MMIO（BAR 2 的 MMIO 访问，不是 memory backing 本身）
            // memory device 的 MMIO 寄存器空间在 BAR 0，所以 BAR 2 MMIO 通常返回 0
            // 但我们允许通过 BAR 2 访问 device 寄存器（某些 NVMe 风格设计）
            std::memset(buf, 0, len);
            return 0;
        }
    }

    // 现有 shell-local 路径（向后兼容）
    // ...
}
```

### 5.2 `DGpuBoard::mmio_write` 修改

```cpp
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;

    // D1: BAR 0 doorbell 路径保留
    const bool is_doorbell = (bar == 1 && offset == kBar1DoorbellOffset);
    if (!is_doorbell && is_mmio_gated()) return -EIO;

    // D1: BAR 0/1 → PcieDisplayDevice
    if (soc_ && bar == 0 && !is_doorbell) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            // ... D1 路由逻辑 ...
        }
    }

    // D2 新增: BAR 2 → PcieMemoryDevice
    if (soc_ && bar == 2) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_memory_device()) {
            // D2: 允许通过 BAR 2 写 device MMIO（NVMe 风格）
            // 实际 memory backing 读写走 backdoor_* 路径
            return 0;  // BAR 2 MMIO 写 device 寄存器（无实际效果）
        }
    }

    // 现有路径保留
    // ...
}
```

### 5.3 `DGpuBoard::backdoor_read/write` 修改

```cpp
int DGpuBoard::backdoor_read(uint64_t offset, void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;

    // D1: 优先路由到 PcieDisplayDevice::backdoor_read
    if (soc_) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            // D1 framebuffer 路由（bar=1）
            return ep->display_device()->backdoor_read(offset, buf, len);
        }
        // D2 新增: 路由 BAR 2 到 PcieMemoryDevice::memory_backing_
        if (ep && ep->has_memory_device()) {
            return ep->memory_device()->memory_read(offset, buf, len);
        }
    }

    // 现有 shell-local vram_segments_ 路径
    // ...
}

int DGpuBoard::backdoor_write(uint64_t offset, const void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;

    // D1: 路由到 PcieDisplayDevice::backdoor_write
    if (soc_) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            return ep->display_device()->backdoor_write(offset, buf, len);
        }
        // D2 新增: 路由 BAR 2 到 PcieMemoryDevice::memory_backing_
        if (ep && ep->has_memory_device()) {
            return ep->memory_device()->memory_write(offset, buf, len);
        }
    }

    // 现有路径保留
    // ...
}
```

## §6 数据流验证（D2 E2E）

```
1. 驱动 cpptlm_emulator_pcie_config_read(0x00) → DGpuBoard → ep->config_space().read(0)
   → 返回 vendor_id=0x1002 + device_id=0x0002 (来自 memory device config)

2. 驱动 cpptlm_emulator_mmio_write(bar=0, off=0x10, size_lo=0x00000000, 4)
   → DGpuBoard.mmio_write → ep->memory_device()->mmio_write(0x10, ...)
   → 写 registers_[kRegMemSizeLo] = 0x00000000

3. 驱动 cpptlm_emulator_mmio_read(bar=0, off=0x10, buf, 4)
   → 返回 0x00000000（memory device 真实状态，round-trip 验证通过）

4. 驱动 cpptlm_emulator_mmio_write(bar=2, off=0x1000, data, 8)
   → DGpuBoard.backdoor_write → ep->memory_device()->memory_write(0x1000, ...)
   → 写入 memory_backing_[0x1000..0x1007]

5. 驱动 cpptlm_emulator_mmio_read(bar=2, off=0x1000, buf, 8)
   → DGpuBoard.backdoor_read → ep->memory_device()->memory_read(0x1000, ...)
   → 返回之前写入的 data（memory backing 持久性验证通过）

6. EP::tick() 推进 cycle_counter_（无 MSI-X 触发）
```

## §7 BAR 路由 Priority（D1 vs D2 共存）

由于 D1 和 D2 都使用 BAR 0，需要明确路由优先级：

| BAR | 访问类型 | D1 优先级 | D2 优先级 |
|-----|---------|---------|---------|
| BAR 0 | mmio_read/write | **D1 display_device 优先** | D2 memory_device 其后 |
| BAR 1 | mmio_read/write | D1 display_device | N/A |
| BAR 1 | backdoor | D1 display_device | N/A |
| BAR 2 | mmio_read/write | N/A | D2 memory_device |
| BAR 2 | backdoor | N/A | D2 memory_device |

**路由决策**：
```cpp
// mmio_read/write BAR 0
if (bar == 0) {
    if (ep && ep->has_display_device()) {
        // D1 优先
        return ep->display_device()->mmio_read/write(...);
    }
    if (ep && ep->has_memory_device()) {
        // D2 兜底（display 不存在时）
        return ep->memory_device()->mmio_read/write(...);
    }
}
```

## §8 ABI 影响分析（D2）

### 8.1 ABI 冻结验证

```bash
# D2 实施前后必须验证
$ git diff HEAD -- include/abi/cpptlm_emulator.h
# (空输出 = ABI 冻结)

# 函数签名计数
$ grep -c "^uint32_t cpptlm_emulator_\|^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" \
    include/abi/cpptlm_emulator.h
# 期望: 15 (不变)
```

### 8.2 0 个新 ABI 函数

D2 不新增 `cpptlm_emulator_*` 函数。所有 D2 路由变更在 `DGpuBoard` 内部方法中（非 ABI）。

## §9 测试设计（D2）

| 测试 | 标签 | 覆盖 |
|------|------|------|
| `test_pcie_memory_device_routing_characterization` | `[pcie][memory][regression]` | 锁定现有 board BAR 2 未映射行为 |
| `test_pcie_memory_device_basic` | `[pcie][memory]` | BAR 0 mmio_read/write round-trip |
| `test_pcie_memory_device_backing` | `[pcie][memory][backing]` | BAR 2 memory_read/write round-trip |
| `test_pcie_memory_device_e2e` | `[pcie][memory][e2e]` | init → config → enumerate → mmio → memory（端到端） |

---

**D2 设计依据**:
- D1 设计（`2026-09-20-cpptlm-pcie-display-io-mvp`）
- 实测验证（`dgpu_board_shell.cc` line 223-440, `pcie_endpoint_ip.hh`）
