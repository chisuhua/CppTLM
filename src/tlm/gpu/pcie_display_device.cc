// src/tlm/gpu/pcie_display_device.cc
// PcieDisplayDevice 实现 (D3a-2 注入式重构)
//
// 作者: CppTLM Team / 日期: 2026-09-20 (D3a-2: 2026-10-04)
// 参考: openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/design.md §2-§6
//       openspec/changes/cpptlm-dgpu-d3a1-display-injection-refactor/design.md §3.2.2
#include "tlm/gpu/pcie_display_device.hh"

#include <cerrno>
#include <cstring>

namespace tlm::gpu {

    PcieDisplayDevice::PcieDisplayDevice() {
        // registers_ zero-initialized by std::array default constructor
        // backing_view_ = nullptr / backing_view_size_ = 0 (non-owning, per D3a-2)
        // D1 v1.1.1: DEVICE_IDENTITY 默认初始化（spec "DEVICE_IDENTITY read"）
        // registers_[0..1] = kVendorId (0x1002 LE), [2..3] = kDeviceId (0x0001 LE)
        uint16_t vid = kVendorId;
        uint16_t did = kDeviceId;
        std::memcpy(&registers_[0], &vid, sizeof(vid));
        std::memcpy(&registers_[2], &did, sizeof(did));
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
        // D1 v1.1.1: 统一返回 0 契约（Oracle R7 冻结裁决 + ABI/board 一致）
        return 0;
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
    // D3a-2: view-only —— 无注入 backing 时返 -ENODEV (无 silent lazy alloc)
    // bounds check 使用 backing_view_size_ (从注入 size 派生, 非常量)

    int PcieDisplayDevice::backdoor_read(uint64_t offset, void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (!backing_view_) {
            return -ENODEV;
        }
        if (offset >= backing_view_size_ || len > backing_view_size_ - offset) {
            return -EINVAL;
        }
        std::memcpy(buf, backing_view_ + offset, len);
        // D1 v1.1.1: 统一返回 0 契约（与 mmio_read 对齐；Oracle R7 冻结裁决）
        return 0;
    }

    int PcieDisplayDevice::backdoor_write(uint64_t offset, const void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (!backing_view_) {
            return -ENODEV;
        }
        if (offset >= backing_view_size_ || len > backing_view_size_ - offset) {
            return -EINVAL;
        }
        std::memcpy(backing_view_ + offset, buf, len);
        // D1 v1.1.1: 统一返回 0 契约（与 mmio_write 对齐）
        return 0;
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

} // namespace tlm::gpu