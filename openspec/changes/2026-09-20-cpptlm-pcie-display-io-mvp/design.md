# D1 Design — Display IO 设备 MVP 详细设计（v1.1 返工版）

> **配套**: [proposal.md](proposal.md) · [tasks.md](tasks.md) · [specs/display-io-mvp/spec.md](specs/display-io-mvp/spec.md)
> **v1.0 返工原因**: Oracle `ses_f37ee373fffeFvaUmM12lCFZ0O` + Metis `ses_f37ee3587ffexJ089actUHbn4c` 指出设计含 4 处事实错误（路径、API、不存在的 ABI 函数）。本版基于实际代码现状重写。

---

## §1 系统拓扑（v1.1）

```
┌────────────────────────────────────────────────────────┐
│                   Host (UsrLinuxEmu)                   │
│                                                        │
│  ┌──────────────┐    ┌─────────────────────────┐     │
│  │ DRM Stub     │ →  │ CpptlmBridge (bridge.h) │     │
│  │ Driver       │    │ 15 ABI fn wrappers      │     │
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
│   │  │ MsiXTable  (msix() accessor)│◄───┼── VBLANK   │
│   │  ├─────────────────────────────┤    │   trigger  │
│   │  │ PcieBarRouter (BAR0 only)  │    │            │
│   │  │  32-bit register-level     │    │            │
│   │  └─────────────────────────────┘    │            │
│   │  ┌─────────────────────────────┐    │            │
│   │  │ PcieDisplayDevice (D1 新)   │◄──┼── tick()   │
│   │  │   registers_[4KB]  (BAR 0)  │    │   advance  │
│   │  │   framebuffer_[32MB] (BAR 1) │    │            │
│   │  └─────────────────────────────┘    │            │
│   └─────────────────────────────────────┘            │
│             ▲                                           │
│             │ via soc_->getInternalInstance("pcie_ep")
│   ┌─────────────────────────────────────┐            │
│   │  DGpuBoard (dgpu_board_shell)       │            │
│   │   mmio_read/write(BAR 0/1)          │            │
│   │     ↓ D1 路由变更                    │            │
│   │   → PcieDisplayDevice              │            │
│   │   backdoor_read/write(BAR 1)       │            │
│   │     ↓ D1 路由变更                    │            │
│   │   → PcieDisplayDevice.framebuffer_ │            │
│   └─────────────────────────────────────┘            │
│                                                        │
└────────────────────────────────────────────────────────┘
```

## §2 PcieDisplayDevice 内部结构（v1.1 修正路径与字段）

```cpp
// include/tlm/gpu/pcie_display_device.hh
namespace tlm::gpu {

class PcieDisplayDevice {
public:
    // BAR 0: 4KB MMIO registers (vendor/device/control/fb_info/status/int/scratch)
    static constexpr size_t kRegSize = 4096;
    std::array<uint8_t, kRegSize> registers_{};

    // BAR 1: 32MB framebuffer backing (模拟 VRAM)
    static constexpr size_t kFbSize = 32 * 1024 * 1024;
    std::vector<uint8_t> framebuffer_;  // 延迟 init（首次 write 时分配）

    // VBLANK timer（由 PcieEndpointIP::tick() 推进）
    uint64_t cycle_counter_ = 0;
    static constexpr uint64_t kVblankInterval = 1024;  // ≈60Hz @ 60kHz
    uint32_t vblank_count_ = 0;

    // Framebuffer info（驱动配置）
    struct FramebufferInfo {
        uint64_t base_addr = 0;  // 主机可见地址（D1 用 identity mapping）
        uint32_t size = 0;       // 默认 32MB
        uint32_t pitch = 0;      // 每行字节数
    } fb_info_;

    // MMIO read/write (BAR 0 寄存器字节级访问 — 与 PcieBarRouter 的 32-bit
    // register-level 不同，本接口支持 1/2/4 字节访问 + unaligned)
    int mmio_read(uint64_t offset, void* buf, size_t len);
    int mmio_write(uint64_t offset, const void* buf, size_t len);

    // Backdoor read/write (BAR 1 framebuffer)
    int backdoor_read(uint64_t offset, void* buf, size_t len);
    int backdoor_write(uint64_t offset, const void* buf, size_t len);

    // Tick（由 PcieEndpointIP::tick() 调用，注入 VBLANK 推进）
    void tick();

    // 寄存器布局常量
    static constexpr uint32_t kRegDeviceIdentity = 0x00;   // RO: vendor+device+revision
    static constexpr uint32_t kRegDisplayControl  = 0x10;   // RW: mode/format
    static constexpr uint32_t kRegFramebufferInfo = 0x20;   // RW: base/size/pitch
    static constexpr uint32_t kRegStatus          = 0x30;   // R/W1C: VBLANK pending
    static constexpr uint32_t kRegStatusClear     = 0x34;   // W: W1C for VBLANK
    static constexpr uint32_t kRegInterruptMask   = 0x40;   // RW
    static constexpr uint32_t kRegInterruptStatus = 0x44;   // R
    static constexpr uint32_t kRegScratch         = 0xF0;   // RW: 16 字节测试寄存器

private:
    // VBLANK 触发 helper（写 STATUS bit0 + 通知 MsiXTable）
    void trigger_vblank(MsiXTable& msix);
};

}  // namespace tlm::gpu
```

## §3 寄存器布局（D1 范围）

| Offset | Size | Name | Access | Description |
|--------|------|------|--------|-------------|
| 0x00-0x0F | 16 | DEVICE_IDENTITY | R | vendor_id (0x1002) + device_id (0x0001) + revision 镜像 `ep->config_space()` |
| 0x10 | 4 | DISPLAY_MODE | RW | 0=off, 1=1920x1080, 2=2560x1440, 3=3840x2160 |
| 0x14 | 4 | PIXEL_FORMAT | RW | 0=RGB888, 1=RGBA8888, 2=NV12 |
| 0x20 | 8 | FB_BASE_LO | RW | Framebuffer 主机可见基地址（D1: identity） |
| 0x28 | 4 | FB_SIZE | RW | 默认 32MB |
| 0x2C | 4 | FB_PITCH | RW | Bytes per scan line |
| 0x30 | 4 | STATUS | R/W1C | bit 0 = VBLANK pending (write-1-to-clear) |
| 0x34 | 4 | STATUS_CLEAR | W | W1C for VBLANK pending |
| 0x40 | 4 | INT_MASK | RW | bit 0 = VBLANK mask (0=masked, 1=enabled) |
| 0x44 | 4 | INT_STATUS | R | 镜像 STATUS bit 0 |
| 0xF0 | 16 | SCRATCH[0..3] | RW | 4 × uint32_t 驱动 round-trip 验证 |

## §4 v1.0 设计错误修正（Oracle + Metis 审查后）

### 4.1 路径修正

| v1.0 声称 | v1.1 修正 |
|---------|---------|
| `include/tlm/pcie/pcie_bar_router_mvp.hh` | `include/tlm/gpu/pcie_bar_router_mvp.hh` |
| `src/tlm/pcie/pcie_bar_router_mvp.cc` | `src/tlm/gpu/pcie_bar_router_mvp.cc` |
| `include/tlm/pcie/pcie_display_device.hh` | `include/tlm/gpu/pcie_display_device.hh` |
| `src/tlm/pcie/pcie_display_device.cc` | `src/tlm/gpu/pcie_display_device.cc` |
| `src/core/cpptlm_emulator_api.cc` | **不需要修改**（ABI wrapper 已正确） |

### 4.2 API 名修正

| v1.0 声称 | v1.1 修正（实际 API） |
|---------|-------------------|
| `PcieEndpointIP::pcie_config_read/write` | **不存在**。实际：`config_space().read/write` 访问器（line 137-148） |
| `ep->bar_router_->dispatch_read(bar, offset, buf, len)` | **不存在**。`PcieBarRouter` 是 32-bit register-level，仅处理 BAR 0 doorbell |
| `PcieBarRouter::dispatch_read` | **不存在**。实际：`mmio_read(uint32_t offset) → uint32_t` |
| `cpptlm_emulator_backdoor_read/write` | **不存在于 ABI**（15 函数无 backdoor）。backdoor 走 board 内部方法 |

### 4.3 注入层次修正

v1.0 模糊在 EP vs Board 之间。v1.1 明确：

```
PcieDisplayDevice 持有者: PcieEndpointIP
                       (与 config_space / msix 一致 — 物理上 EP 持有设备)
                       ↓
                  tick() 调用 display_device_->tick() (VBLANK 推进)
                       ↓
DGpuBoard 访问路径: soc_->getInternalInstance("pcie_ep")
                       → ep->display_device() (新 accessor)
                       → 路由 BAR 0/1 到 device
```

## §5 DGpuBoard 路由层修改（v1.1 精确路径）

### 5.1 `DGpuBoard::mmio_read` 修改（line 223-282）

```cpp
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;
    if (is_mmio_gated()) return -EIO;

    // D1 新增: 路由 BAR 0/1 到 PcieDisplayDevice（替代 shell-local mmio_regs_ 路径）
    if (soc_ && (bar == 0 || bar == 1)) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            // 现有 inject_q + self-drain 路径保留（驱动协议不变）
            PendingReq req;
            req.bar = bar;
            req.offset = offset;
            req.data.resize(len);
            req.trans_id = next_trans_id_++;
            req.is_mmio_read = true;
            req.target = PendingReq::Target::DISPLAY_DEVICE;  // D1 新增字段
            // ... (保持原有 self-drain + wait_for 逻辑不变)
            // 不同之处: drain 时调 ep->display_device()->mmio_read()
            // 而非从 mmio_regs_[bar,offset] 读
        }
    }

    // 现有 shell-local 路径（向后兼容 SDMA doorbell 等不接 device 的场景）
    // ... (line 235-281 逻辑保留)
}
```

### 5.2 `DGpuBoard::mmio_write` 修改（line 284-348）

```cpp
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;

    const bool is_doorbell = (bar == 1 && offset == kBar1DoorbellOffset);
    if (!is_doorbell && is_mmio_gated()) return -EIO;

    // D1 新增: BAR 0 → PcieDisplayDevice::mmio_write
    // 注: BAR 1 doorbell 走原 SDMA 路径，不路由 device（D1 范围）
    if (soc_ && bar == 0 && !is_doorbell) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            // D1: 写入 device 而非 shell-local mmio_regs_
            // 保留 mmio_regs_ 作为 fallback 用于非 device BAR（如 SDMA BAR 1）
            std::vector<uint8_t> payload(static_cast<const uint8_t*>(buf),
                                         static_cast<const uint8_t*>(buf) + len);
            {
                std::lock_guard<std::mutex> lock(inject_mu_);
                mmio_regs_[std::make_pair(bar, offset)] = payload;
            }
            // 仍调用 dispatch_mmio_to_pcie（保持 SDMA doorbell 路径）
            dispatch_mmio_to_pcie(bar, offset, buf, len);
            // 注: BAR 0 不通过 inject_q_ push（device 直读路径，drain 时跳过）
            // 简化: BAR 0 write 是同步的，return 0
            return 0;
        }
    }

    // 现有路径（BAR 1 doorbell / BAR 1 fallback 等保留）
    // ... (line 296-347 逻辑)
}
```

### 5.3 `DGpuBoard::backdoor_read/write` 修改（line 383-440）

```cpp
int DGpuBoard::backdoor_read(uint64_t vram_offset, void* buf, size_t len) {
    if (buf == nullptr) return -EINVAL;

    // D1 新增: 优先路由到 PcieDisplayDevice::backdoor_read
    if (soc_) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            return ep->display_device()->backdoor_read(vram_offset, buf, len);
        }
    }

    // 现有 shell-local vram_segments_ 路径（向后兼容，无 device 时）
    // ... (line 398-410 逻辑保留)
}

int DGpuBoard::backdoor_write(uint64_t vram_offset, const void* buf, size_t len) {
    // D1 新增: 同上，路由到 PcieDisplayDevice::backdoor_write
    if (soc_) {
        auto* ep = dynamic_cast<tlm::pcie::PcieEndpointIP*>(
            soc_->getInternalInstance("pcie_ep"));
        if (ep && ep->has_display_device()) {
            return ep->display_device()->backdoor_write(vram_offset, buf, len);
        }
    }

    // 现有路径保留
    // ... (line 423-440 逻辑)
}
```

## §6 PcieEndpointIP 注入（v1.1）

### 6.1 新增成员（include/tlm/pcie/pcie_endpoint_ip.hh）

```cpp
class PcieEndpointIP : public SimModule {
    // ... 既有成员 (line 100-260 不变) ...

    // D1 新增: PcieDisplayDevice 持有与访问
public:
    [[nodiscard]] bool has_display_device() const noexcept {
        return display_device_ != nullptr;
    }
    tlm::gpu::PcieDisplayDevice& display_device() noexcept {
        return *display_device_;
    }
    const tlm::gpu::PcieDisplayDevice& display_device() const noexcept {
        return *display_device_;
    }

private:
    std::unique_ptr<tlm::gpu::PcieDisplayDevice> display_device_;
};
```

### 6.2 构造与 tick（src/tlm/pcie/pcie_endpoint_ip.cc）

```cpp
// 构造（D1 改动）
PcieEndpointIP::PcieEndpointIP(const std::string& name, EventQueue* eq)
    : SimModule(name, eq, "pcie_ep", 17)
    , display_device_(std::make_unique<tlm::gpu::PcieDisplayDevice>())  // D1
{
    // ... 既有初始化
}

// tick（D1 改动：推进 VBLANK）
void PcieEndpointIP::tick() {
    // ... 既有 tick 逻辑 ...

    // D1: 推进显示设备 VBLANK timer（传递 msix 引用，方案 A — 见 §6.3）
    if (display_device_) {
        display_device_->tick(msix());
    }
}

// PcieDisplayDevice::tick() 内部（D1 设计，方案 A：接受 MsiXTable& 参数）
void PcieDisplayDevice::tick(MsiXTable& msix) {
    cycle_counter_++;
    if (cycle_counter_ >= kVblankInterval) {
        cycle_counter_ = 0;
        vblank_count_++;
        // 写 STATUS.VBLANK_PENDING (offset 0x30, bit 0)
        registers_[kRegStatus / 4] |= 0x1u;
        // 触发 MSI-X vector 0 (如果 INT_MASK 未屏蔽)
        if (registers_[kRegInterruptMask / 4] & 0x1u) {
            msix.update_pending(0);  // MSI-X vector 0 = VBLANK
        }
    }
}
```

### 6.3 PcieDisplayDevice ↔ MsiXTable 耦合设计

由于 `PcieDisplayDevice` 由 EP 持有，但 `tick()` 需要调 `MsiXTable::update_pending(0)`，设计选择：

**方案 A（推荐）**：PcieDisplayDevice::tick() 接受 `MsiXTable&` 参数
- 优点：解耦 device 与 EP（device 可独立单元测试）
- 缺点：每次 EP tick() 要传引用

**方案 C（备选）**：PcieDisplayDevice 持有 `MsiXTable*` 指针（EP init 时注入）
- 优点：tick() 无参
- 缺点：device 与 EP 紧耦合（测试时需 mock MsiXTable）

**选择 A**：

```cpp
// PcieDisplayDevice 不持有 MsiXTable
void PcieDisplayDevice::tick(MsiXTable& msix) {
    cycle_counter_++;
    if (cycle_counter_ >= kVblankInterval) {
        cycle_counter_ = 0;
        vblank_count_++;
        registers_[kRegStatus / 4] |= 0x1u;
        if (registers_[kRegInterruptMask / 4] & 0x1u) {
            msix.update_pending(0);  // MSI-X vector 0 = VBLANK
        }
    }
}

// EP::tick 调用
void PcieEndpointIP::tick() {
    // ... 既有 ...
    if (display_device_) {
        display_device_->tick(msix());  // 传引用
    }
}
```

## §7 ABI 影响分析（v1.1）

### 7.1 ABI 冻结验证

```bash
# D1 实施前后必须验证
$ git diff HEAD -- include/abi/cpptlm_emulator.h
# (空输出 = ABI 冻结)

# 头文件函数签名
$ grep -c "^uint32_t cpptlm_emulator_\|^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" \
    include/abi/cpptlm_emulator.h
# 期望: 15 (不变) — 必须包含 uint32_t 模式，因 cpptlm_emulator_get_device_count 是 uint32_t 返回类型 (line 69)

# Callback typedef
$ grep -c "typedef.*cpptlm_.*_cb_t" include/abi/cpptlm_emulator.h
# 期望: 4 (不变)
```

### 7.2 0 个新 ABI 函数

D1 不新增 `cpptlm_emulator_*` 函数：
- `cpptlm_emulator_backdoor_read/write` **不存在**（也不应新增）
- 所有 D1 路由变更在 `DGpuBoard` 内部方法中（非 ABI）

### 7.3 0 个 ABI 签名变更

现有 15 函数 + 4 callback typedef 字节级一致。

## §8 冻结面遵守矩阵

| 冻结面 | D1 是否触碰 | 理由 |
|--------|------------|------|
| `include/abi/cpptlm_emulator.h` | ❌ 不碰 | ABI 冻结 |
| `include/tlm/gpu/pcie_endpoint_tlm.h` | ❌ 不碰 | PcieEndpointTLM 4 端口冻结 |
| `src/abi/cpptlm_emulator.cc` | ❌ 不碰 | ABI wrapper 已正确 |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | ⚠️ **仅添加** | 新增 `display_device()` accessor + `display_device_` unique_ptr 成员（不删除/修改现有） |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | ✅ 修改 | tick() 注入 VBLANK + ctor 构造 |
| `include/tlm/gpu/pcie_bar_router_mvp.hh` | ❌ 不碰 | 32-bit register-level 与 BAR 0/1 device 路由正交，不复用 |
| `include/tlm/gpu/dgpu_board_shell.hh` | ⚠️ 可选 | 若 board 通过 EP 间接访问 device，不需修改 |
| `src/tlm/gpu/dgpu_board_shell.cc` | ✅ 修改 | mmio/backdoor 路由层加 if-else 分支 |

## §9 测试设计（v1.1 修正数量）

### T0 Characterization Test（**T1 步骤先写**，防回归）

```cpp
// test/test_pcie_board_routing_characterization.cc
TEST_CASE("DGpuBoard mmio_read/write 当前 shell-local 行为") {
    // 不接 SOC，验证 board 在 has_display_device()=false 时走
    // 现有 mmio_regs_ / vram_segments_ 路径（保持现有 PASS 测试基线）
}

TEST_CASE("DGpuBoard backdoor_read miss 返 -ENOENT (修复 #6 已存在)") {
    // 锁定现有 miss 行为：D1 后 miss 仍返 -ENOENT
}
```

### T1-T5 设备级测试（5 个）

| 测试 | 标签 | 覆盖 |
|------|------|------|
| `test_pcie_display_device_basic` | `[pcie][display]` | BAR 0 mmio_read/write round-trip |
| `test_pcie_display_device_backdoor` | `[pcie][display][backdoor]` | BAR 1 framebuffer round-trip |
| `test_pcie_display_device_vblank` | `[pcie][display][msix]` | tick() → VBLANK → MSI-X pending |
| `test_pcie_display_device_e2e` | `[pcie][display][e2e]` | init → config → enumerate → mmio → VBLANK |
| `test_pcie_board_routing_characterization` | `[pcie][display][regression]` | 锁定现有 board 行为 |

（v1.0 提的 9 个测试拆为 4 个设备级 + 1 个 characterization = 5 个，避免冗余）

## §10 数据流验证（D1 E2E）

```
1. 驱动 cpptlm_emulator_pcie_config_read(0x00) → DGpuBoard → ep->config_space().read(0)
   → 返回 vendor_id=0x1002 + device_id=0x0001 (来自 config space)

2. 驱动 cpptlm_emulator_mmio_read(bar=0, off=0x00) → DGpuBoard.mmio_read
   → 命中 BAR 0 路由 → ep->display_device()->mmio_read(0x00)
   → 返回 DEVICE_IDENTITY (从 config space 镜像)

3. 驱动 cpptlm_emulator_mmio_write(bar=0, off=0x10, mode=1)
   → DGpuBoard.mmio_write → ep->display_device()->mmio_write(0x10, ...)
   → 写 registers_[0x10/4] = 1

4. 驱动 cpptlm_emulator_mmio_read(bar=0, off=0x10)
   → 返回 1（device 真实状态，round-trip 验证通过）

5. EP::tick() 推进 1024 cycle → display_device_->tick(msix())
   → 写 STATUS.VBLANK_PENDING (registers_[0x30/4] |= 1)
   → msix.update_pending(0) → 真实调 trigger_irq_async

6. 驱动 cpptlm_emulator_msix_update_pending(emu, 0)（或读 INT_STATUS）
   → 验证 MSI-X vector 0 pending=1
```

## §11 不在 D1 范围（与 proposal §5 一致）

D2（memory device MVP）/ D3（GMMU PoC）/ 物理层 / Power management 等。

---

**v1.1 设计修正依据**:
- Oracle session `ses_f37ee373fffeFvaUmM12lCFZ0O`（§B BLOCKING-1/2）
- Metis session `ses_f37ee3587ffexJ089actUHbn4c`（§B B1/B2/B3）
- 实测验证（`dgpu_board_shell.cc` line 223-440, `pcie_endpoint_ip.hh` line 100-160, `pcie_bar_router_mvp.hh` line 70-110）