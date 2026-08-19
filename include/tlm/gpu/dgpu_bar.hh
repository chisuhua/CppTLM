#ifndef CPPTLM_DGPU_BAR_H
#define CPPTLM_DGPU_BAR_H

#include <cstddef>
#include <cstdint>

namespace tlm::gpu {

    class DGpuBar {
    public:
        static constexpr uint16_t VENDOR_ID = 0x10DE;
        static constexpr uint16_t DEVICE_ID = 0x1234;
        static constexpr uint8_t REVISION = 0x01;

        DGpuBar();
        ~DGpuBar();

        DGpuBar(const DGpuBar&) = delete;
        DGpuBar& operator=(const DGpuBar&) = delete;
        DGpuBar(DGpuBar&&) = delete;
        DGpuBar& operator=(DGpuBar&&) = delete;

        void init();
        void shutdown();

        uint32_t read_reg(uint32_t offset) const;
        void write_reg(uint32_t offset, uint32_t value);

        void* vram_base() const;
        size_t vram_size() const;

    private:
        static constexpr size_t BAR0_SIZE = 0x10000;
        static constexpr size_t VRAM_SIZE = 256 * 1024 * 1024;

        uint32_t bar0_regs_[BAR0_SIZE / 4] = {};
        void* vram_base_ = nullptr;
        size_t vram_size_ = 0;
    };

} // namespace tlm::gpu

#endif // CPPTLM_DGPU_BAR_H
