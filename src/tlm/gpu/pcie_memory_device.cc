// src/tlm/gpu/pcie_memory_device.cc
// PcieMemoryDevice 实现
//
// 作者: CppTLM Team / 日期: 2026-09-26
// 参考: openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/design.md §2-§6
#include "tlm/gpu/pcie_memory_device.hh"

#include <cerrno>
#include <cstring>

namespace tlm::gpu {

    PcieMemoryDevice::PcieMemoryDevice() {
        // registers_ zero-initialized by std::array default constructor
        // memory_backing_ empty (lazy alloc)
        // DEVICE_IDENTITY 默认初始化: registers_[0..1] = kVendorId (0x1002 LE), [2..3] = kDeviceId (0x0002 LE)
        uint16_t vid = kVendorId;
        uint16_t did = kDeviceId;
        std::memcpy(&registers_[0], &vid, sizeof(vid));
        std::memcpy(&registers_[2], &did, sizeof(did));
        // STATUS 默认 ready
        registers_[kRegStatus] = kStatusReady;
    }

    // ── MMIO read/write (BAR 0) ──

    int PcieMemoryDevice::mmio_read(uint64_t offset, void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (offset >= kRegSize || len > kRegSize - offset) {
            return -EINVAL;
        }
        std::memcpy(buf, &registers_[offset], len);
        return 0;
    }

    int PcieMemoryDevice::mmio_write(uint64_t offset, const void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (offset >= kRegSize || len > kRegSize - offset) {
            return -EINVAL;
        }

        // RO registers: DEVICE_IDENTITY (0x00-0x0F)
        // Writes to RO regions are silently ignored (return 0)
        if (offset <= 0x0F) {
            return 0;
        }

        // RW registers: copy data
        std::memcpy(&registers_[offset], buf, len);
        return 0;
    }

    // ── Memory backing read/write (BAR 2) ──

    int PcieMemoryDevice::memory_read(uint64_t offset, void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (offset >= kDefaultMemSize || len > kDefaultMemSize - offset) {
            return -EINVAL;
        }
        if (offset + len > memory_backing_.size()) {
            memory_backing_.resize(offset + len, 0);
        }
        std::memcpy(buf, &memory_backing_[offset], len);
        return 0;
    }

    int PcieMemoryDevice::memory_write(uint64_t offset, const void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        if (offset >= kDefaultMemSize || len > kDefaultMemSize - offset) {
            return -EINVAL;
        }
        if (offset + len > memory_backing_.size()) {
            memory_backing_.resize(offset + len, 0);
        }
        std::memcpy(&memory_backing_[offset], buf, len);
        return 0;
    }

    // ── Tick (cycle counter 推进) ──

    void PcieMemoryDevice::tick() {
        cycle_counter_++;
    }

}  // namespace tlm::gpu