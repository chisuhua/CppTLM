// src/tlm/gpu/pcie_memory_device.cc
// PcieMemoryDevice 实现 — Phase 3 (driver-visible-minimal-soc v1.8)
//
// 作者: CppTLM Team / 日期: 2026-09-26 (Phase 3 2027-02-09)
// 参考: openspec/changes/cpptlm-driver-visible-minimal-soc/design.md §4
#include "tlm/gpu/pcie_memory_device.hh"

#include "bundles/pcie_bundles_tlm.hh"

#include <cerrno>
#include <cstring>

namespace tlm::gpu {

    void PcieMemoryDevice::init_identity_regs() {
        // registers_ zero-initialized by std::array default constructor
        // backing_view_ = nullptr (no lazy alloc; must be injected via set_backing_view)
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
        std::lock_guard<std::mutex> lock(backing_mutex_);
        // v1.4 B12/B27: kRegMemSizeLo/Hi 读 injected backing_view_size_ (非常量)
        if (offset == kRegMemSizeLo && len == 4) {
            const uint32_t lo = static_cast<uint32_t>(backing_view_size_ & 0xFFFFFFFFu);
            std::memcpy(buf, &lo, 4);
            return 0;
        }
        if (offset == kRegMemSizeHi && len == 4) {
            const uint32_t hi = static_cast<uint32_t>((backing_view_size_ >> 32) & 0xFFFFFFFFu);
            std::memcpy(buf, &hi, 4);
            return 0;
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

        std::lock_guard<std::mutex> lock(backing_mutex_);
        // RW registers: copy data
        std::memcpy(&registers_[offset], buf, len);
        return 0;
    }

    // ── Memory backing read/write (BAR 2) ──
    // v1.4 B7 (ADR-DGPU-05): memory_backing_ 删除, 改用 backing_view_ 注入
    // v1.4 B8: bound = backing_view_size_ (非 kDefaultMemSize)
    // v1.4 B12: backing_view_==nullptr 返 -ENODEV
    // v1.4 B10: mutex 保护 host/sim 并发访问

    int PcieMemoryDevice::memory_read(uint64_t offset, void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        std::lock_guard<std::mutex> lock(backing_mutex_);
        if (backing_view_ == nullptr) return -ENODEV;
        if (offset >= backing_view_size_ || len > backing_view_size_ - offset) {
            return -EINVAL;
        }
        std::memcpy(buf, backing_view_ + offset, len);
        return 0;
    }

    int PcieMemoryDevice::memory_write(uint64_t offset, const void* buf, size_t len) {
        if (buf == nullptr || len == 0) {
            return -EINVAL;
        }
        std::lock_guard<std::mutex> lock(backing_mutex_);
        if (backing_view_ == nullptr) return -ENODEV;
        if (offset >= backing_view_size_ || len > backing_view_size_ - offset) {
            return -EINVAL;
        }
        std::memcpy(backing_view_ + offset, buf, len);
        return 0;
    }

    // ── SlavePort 处理 (v1.4 B9: SLVERR 传播) ──

    void PcieMemoryDevice::handle_slave_port(unsigned p) {
        if (p >= NUM_PORTS || !req_in[p].valid() || !req_in[p].ready()) {
            return;
        }
        const auto& req = req_in[p].data();

        bundles::PcieTlpBundle resp;
        resp.trans_id.write(req.trans_id.read());
        const uint64_t off = req.offset.read();
        const uint32_t len = req.size.read();

        // v1.4 B9: 检查 memory_read/write 返回值; 失败置 SLVERR
        //   null_backing → -ENODEV, oob → -EINVAL
        // v1.8 H1: PcieTlpBundle inline data ≤8B; >8B 走 BAR2 backdoor
        const bool null_backing = !has_backing_view();
        const bool oob = (len > 8 || off >= backing_size() ||
                          len > backing_size() - off);
        if (null_backing || oob) {
            resp.kind.write(bundles::PcieTlpBundle::CPLD);  // SLVERR: CPLD + data=0xDEAD
            resp.data.write(0xDEAD);
        } else if (req.is_read()) {
            uint64_t val = 0;
            int r = memory_read(off, &val, len);
            resp.data.write((r == 0) ? val : 0xDEAD);
            resp.kind.write(bundles::PcieTlpBundle::CPLD);  // 读完成: CPLD 带 data
        } else if (req.is_write()) {
            uint64_t val = req.data.read();
            (void)memory_write(off, &val, len);
            resp.kind.write(bundles::PcieTlpBundle::CPLD);  // 写完成: CPLD
            // minimal_v1 CPLD 无额外状态; D3+ 可通过扩展 resp kind 加 ACK/NACK
        } else {
            resp.kind.write(bundles::PcieTlpBundle::CPLD);
        }
        resp_out[p].write(resp);
        req_in[p].consume();
    }

    // ── Tick (单 tick 一次; MultiPortStreamAdapter 内部遍历全端口) ──

    void PcieMemoryDevice::tick() {
        cycle_counter_++;
        for (unsigned p = 0; p < NUM_PORTS; ++p) {
            handle_slave_port(p);
        }
        // v1.3 B3: 单 tick 一次; MultiPortStreamAdapter 内部遍历 N 端口
        if (adapter_) adapter_->tick();
    }

}  // namespace tlm::gpu