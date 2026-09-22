// src/tlm/gpu/pcie_display_device.cc
// PcieDisplayDevice 实现
//
// 作者: CppTLM Team / 日期: 2026-09-20
// 参考: openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/design.md §2-§6
#include "tlm/gpu/pcie_display_device.hh"

#include <cerrno>
#include <cstring>

namespace tlm::gpu {

    PcieDisplayDevice::PcieDisplayDevice() {
        // registers_ zero-initialized by std::array default constructor
        // framebuffer_ empty (lazy alloc)
    }

    // ── MMIO read/write (BAR 0) ──

    int PcieDisplayDevice::mmio_read(uint64_t offset, void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        // Bounds check: offset+len must fit within BAR 0 (4KB)
        if (offset >= kRegSize || len > kRegSize - offset) {
            return -EINVAL;
        }
        std::memcpy(buf, &registers_[offset], len);
        return static_cast<int>(len);
    }

    int PcieDisplayDevice::mmio_write(uint64_t offset, const void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (offset >= kRegSize || len > kRegSize - offset) {
            return -EINVAL;
        }

        // RO registers: DEVICE_IDENTITY (0x00-0x0F), STATUS (0x30), INT_STATUS (0x44)
        // Writes to RO regions are silently ignored (return 0)
        if (offset <= 0x0F) {
            // kRegDeviceIdentity region (0x00-0x0F) - RO
            return 0;
        }
        if (offset == kRegStatus) {
            // RO register - write ignored
            return 0;
        }
        if (offset == kRegIntStatus) {
            // RO register - write ignored
            return 0;
        }

        // W1C register: STATUS_CLEAR (0x34) - clear VBLANK pending bit
        if (offset == kRegStatusClear) {
            // W1C: write 1 to bit 0 to clear VBLANK pending
            uint8_t clear_byte = 0;
            std::memcpy(&clear_byte, buf, (len < 1) ? 1 : 1);
            if (clear_byte & kStatusVblankPending) {
                registers_[kRegStatus] &= ~kStatusVblankPending;
            }
            return 0;
        }

        // RW registers: copy data
        std::memcpy(&registers_[offset], buf, len);
        return 0;
    }

    // ── Backdoor read/write (BAR 1 framebuffer) ──

    int PcieDisplayDevice::backdoor_read(uint64_t offset, void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        ensure_framebuffer_allocated();
        if (offset >= framebuffer_.size() || len > framebuffer_.size() - offset) {
            return -EINVAL;
        }
        std::memcpy(buf, &framebuffer_[offset], len);
        return static_cast<int>(len);
    }

    int PcieDisplayDevice::backdoor_write(uint64_t offset, const void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        ensure_framebuffer_allocated();
        if (offset >= framebuffer_.size() || len > framebuffer_.size() - offset) {
            return -EINVAL;
        }
        std::memcpy(&framebuffer_[offset], buf, len);
        return static_cast<int>(len);
    }

    // ── Tick (VBLANK 推进) ──

    void PcieDisplayDevice::tick(MsiXTable& msix) {
        cycle_counter_++;
        if (cycle_counter_ >= kVblankInterval) {
            cycle_counter_ = 0;
            vblank_count_++;
            trigger_vblank(msix);
        }
    }

    void PcieDisplayDevice::trigger_vblank(MsiXTable& msix) {
        // Set STATUS.VBLANK_PENDING bit (R/W1C)
        registers_[kRegStatus] |= kStatusVblankPending;
        // Trigger MSI-X vector 0 if VBLANK interrupt enabled (INT_MASK bit 0 = 1)
        if (registers_[kRegIntMask] & kIntMaskVblankEnable) {
            msix.update_pending(0);
        }
    }

    // ── Helpers ──

    void PcieDisplayDevice::ensure_framebuffer_allocated() {
        if (framebuffer_.empty()) {
            framebuffer_.resize(kFbSize, 0);
        }
    }

}  // namespace tlm::gpu