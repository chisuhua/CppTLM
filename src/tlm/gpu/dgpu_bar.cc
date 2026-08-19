#include "tlm/gpu/dgpu_bar.hh"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace tlm::gpu {

    void DGpuBar::init() {
        bar0_regs_[0x00 / 4] = VENDOR_ID;
        bar0_regs_[0x04 / 4] = DEVICE_ID;
        bar0_regs_[0x08 / 4] = REVISION;

        vram_size_ = VRAM_SIZE;
        vram_base_ = std::aligned_alloc(4096, vram_size_);
        if (vram_base_ == nullptr) {
            vram_size_ = 0;
            throw std::bad_alloc();
        }
        std::memset(vram_base_, 0, vram_size_);
    }

    void DGpuBar::shutdown() {
        std::free(vram_base_);
        vram_base_ = nullptr;
        vram_size_ = 0;
    }

    uint32_t DGpuBar::read_reg(uint32_t offset) const {
        if ((offset & 0x3) != 0) {
            throw std::out_of_range("unaligned read");
        }
        if (offset >= BAR0_SIZE) {
            throw std::out_of_range("BAR0 OOB");
        }
        return bar0_regs_[offset / 4];
    }

    void DGpuBar::write_reg(uint32_t offset, uint32_t value) {
        if ((offset & 0x3) != 0) {
            throw std::out_of_range("unaligned write");
        }
        if (offset >= BAR0_SIZE) {
            throw std::out_of_range("BAR0 OOB");
        }
        bar0_regs_[offset / 4] = value;
    }

    void* DGpuBar::vram_base() const {
        return vram_base_;
    }

    size_t DGpuBar::vram_size() const {
        return vram_size_;
    }

} // namespace tlm::gpu
