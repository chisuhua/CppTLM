# D1 Design — Display IO 设备 MVP 详细设计

## §1 系统拓扑

```
┌────────────────────────────────────────────────────────┐
│                   Host (UsrLinuxEmu)                   │
│                                                        │
│  ┌──────────────┐    ┌─────────────────────────┐     │
│  │ DRM Stub     │ →  │ CpptlmBridge (bridge.h) │     │
│  │ Driver       │    │ mmio_read/write/config  │     │
│  └──────────────┘    └──────────┬──────────────┘     │
│                                 │ ABI C extern         │
└────────────────────────────────┬┘─────────────────────┘
                                 │ 22 ABI fn
┌────────────────────────────────┼─────────────────────┐
│                    CppTLM (实现仓)                     │
│                                ▼                        │
│   ┌────────────┐    ┌─────────────────────┐          │
│   │ Host       │ →  │ PCIe Root Complex   │          │
│   │ Bypass     │    │ (pcie_root_complex) │          │
│   │ (Phase 7)  │    └──────────┬──────────┘          │
│   └────────────┘               │ PCIe TLP            │
│                                ▼                        │
│   ┌─────────────────────────────────────┐            │
│   │  PcieEndpointIP (Phase 4, 已交付)    │            │
│   │  ┌─────────────────────────────┐    │            │
│   │  │ PcieBarRouter (扩展)        │    │            │
│   │  │  BAR 0 → PcieDisplayDevice  │    │            │
│   │  │  BAR 1 → Display Framebuffer│    │            │
│   │  └─────────────────────────────┘    │            │
│   │  ┌─────────────────────────────┐    │            │
│   │  │ MSIX Table (1 vector)       │    │            │
│   │  └─────────────────────────────┘    │            │
│   │  ┌─────────────────────────────┐    │            │
│   │  │ PcieConfigSpace (修 #3)     │    │            │
│   │  └─────────────────────────────┘    │            │
│   │  ┌─────────────────────────────┐    │            │
│   │  │ PcieDisplayDevice (新)      │◄──┼── tick()   │
│   │  │   registers[4KB]            │    │   推进     │
│   │  │   framebuffer[32MB]         │    │   VBLANK   │
│   │  └─────────────────────────────┘    │            │
│   └─────────────────────────────────────┘            │
└────────────────────────────────────────────────────────┘
```

## §2 PcieDisplayDevice 内部结构

```cpp
class PcieDisplayDevice : public ChStreamModuleBase {
public:
    // 4KB MMIO registers (BAR 0)
    static constexpr size_t kRegSize = 4096;
    uint8_t registers_[kRegSize];

    // 32MB framebuffer backing (BAR 1)
    static constexpr size_t kFbSize = 32 * 1024 * 1024;
    std::vector<uint8_t> framebuffer_;

    // VBLANK timer
    uint64_t cycle_counter_ = 0;
    static constexpr uint64_t kVblankInterval = 1024;  // 60Hz @ 60kHz
    uint32_t vblank_count_ = 0;

    // Framebuffer info
    struct FramebufferInfo {
        uint64_t base_addr;  // 主机可见地址（identity mapping for D1）
        uint32_t size;
        uint32_t pitch;
    } fb_info_;

    // Register map (offsets + access flags)
    static const RegisterDesc kRegMap[];

    // Read/write from BAR router
    int mmio_read(uint64_t offset, void* buf, size_t len);
    int mmio_write(uint64_t offset, const void* buf, size_t len);

    // BAR 1 (backdoor)
    int backdoor_read(uint64_t offset, void* buf, size_t len);
    int backdoor_write(uint64_t offset, const void* buf, size_t len);

    // Tick (called by PcieEndpointIP)
    void tick();

    // MSI-X trigger (called by tick on VBLANK)
    void trigger_vblank_msix();
};
```

## §3 寄存器布局（D1 范围）

| Offset | Size | Name | Access | Description |
|--------|------|------|--------|-------------|
| 0x00 | 4 | DEVICE_ID | R | Vendor ID + Device ID（镜像 config space） |
| 0x04 | 4 | REVISION | R | Revision ID |
| 0x10 | 4 | DISPLAY_MODE | RW | 0=off, 1=1920x1080, 2=2560x1440, 3=3840x2160 |
| 0x14 | 4 | PIXEL_FORMAT | RW | 0=RGB888, 1=RGBA8888, 2=NV12 |
| 0x20 | 8 | FB_BASE_LO | RW | Framebuffer 基地址（identity mapping） |
| 0x28 | 4 | FB_SIZE | RW | Framebuffer size（默认 32MB） |
| 0x2C | 4 | FB_PITCH | RW | Bytes per scan line |
| 0x30 | 4 | STATUS | R | bit 0=VBLANK pending |
| 0x34 | 4 | STATUS_CLEAR | W | W1C（write-1-to-clear）VBLANK pending |
| 0x40 | 4 | INT_MASK | RW | 0=VBLANK masked, 1=VBLANK enabled |
| 0x44 | 4 | INT_STATUS | R | 当前中断状态（镜像 STATUS） |
| 0xF0 | 16 | SCRATCH | RW | 8 x uint32_t 往返验证（驱动测试用） |

## §4 ABI 修复路径

### 4.1.1 `cpptlm_emulator_pcie_config_read/write`（修 #3）

**当前实现**：`return -ENOSYS`

**D1 修复**：直接路由到 `PcieEndpointIP::pcie_config_read/write`（在 `src/tlm/pcie/pcie_endpoint_ip.cc` 已有但未暴露）

```cpp
int cpptlm_emulator_pcie_config_read(cpptlm_emulator_t* emu, uint16_t offset,
                                     uint8_t width, uint32_t* val) {
    auto* ep = static_cast<PcieEndpointIP*>(emu);
    return ep->pcie_config_read(offset, width, val);
}
```

### 4.1.2 `cpptlm_emulator_mmio_read/write`（修 #5/#7）

**当前实现**：`return 0; *((uint8_t*)buf) = 0;` 或 `return 0; (no-op)`

**修复**：路由到 `PcieEndpointIP::bar_router_->dispatch()`

```cpp
int cpptlm_emulator_mmio_read(cpptlm_emulator_t* emu, uint8_t bar,
                              uint64_t offset, void* buf, size_t len) {
    auto* ep = static_cast<PcieEndpointIP*>(emu);
    return ep->bar_router_->dispatch_read(bar, offset, buf, len);
}
```

### 4.1.3 `cpptlm_emulator_backdoor_read/write`（修 #6）

**当前实现**：未命中 vram_segments_ 返 `len`

**修复**：注入 PcieDisplayDevice 内部 framebuffer

```cpp
int cpptlm_emulator_backdoor_read(cpptlm_emulator_t* emu, uint8_t bar,
                                  uint64_t offset, void* buf, size_t len) {
    auto* dev = static_cast<PcieDisplayDevice*>(emu);
    if (bar == 1 && offset + len <= dev->framebuffer_.size()) {
        memcpy(buf, dev->framebuffer_.data() + offset, len);
        return len;
    }
    return -EINVAL;
}
```

## §5 VBLANK 中断链路（D1 仅实现 MSI-X 触发，intr_cb 留给 D2）

每 1024 cycle 触发一次 VBLANK：

```cpp
void PcieEndpointIP::tick() {
    // 现有 PCIe 逻辑 ...
    display_device_->tick();  // 推进 VBLANK counter
}

void PcieDisplayDevice::tick() {
    cycle_counter_++;
    if (cycle_counter_ >= kVblankInterval) {
        cycle_counter_ = 0;
        vblank_count_++;
        registers_[0x30 / 4] |= 1;  // set STATUS.VBLANK_PENDING
        // 触发 MSI-X (D1: 仅记录 pending；D2/D3: 调 intr_cb)
        msix_->update_pending(0);
    }
}
```

## §6 数据流验证（D1 E2E 测试）

```
1. 驱动发起 Config Read offset=0 → PcieEndpointIP::pcie_config_read
2. 驱动发起 BAR Enumerate (write to BAR 0 探测) → PcieDisplayDevice::mmio_write
3. 驱动发起 MMIO Read DEVICE_ID offset=0x00 → PcieDisplayDevice::mmio_read
4. 驱动发起 MMIO Write DISPLAY_MODE=1 → PcieDisplayDevice::mmio_write
5. 驱动发起 MMIO Read DISPLAY_MODE → 返回 1（验证 round-trip）
6. tick() 推进 1024 cycle → VBLANK pending
7. 驱动发起 MSI-X Read Status → pending=1
8. 驱动发起 Status Clear → pending=0
```

## §8 冻结面遵守

| 冻结头 | D1 是否触碰 |
|--------|------------|
| `include/abi/cpptlm_emulator.h` | ❌ 不碰（22 ABI 签名不变） |
| `include/tlm/gpu/pcie_endpoint_tlm.h` | ❌ 不碰（PcieEndpointTLM 4 端口冻结） |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | ✅ 修改（仅 `.cc` 实现，非冻结头） |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | ⚠️ 添加方法（不删除/修改现有方法） |

**不破坏 ABI 兼容**：所有变更在实现层，公开接口签名不变。