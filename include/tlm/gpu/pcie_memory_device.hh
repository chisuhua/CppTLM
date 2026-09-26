// include/tlm/gpu/pcie_memory_device.hh
// PcieMemoryDevice: D2 Memory 设备 MVP
//
// 功能描述：4KB MMIO 寄存器（BAR 0）+ 8GB Memory Backing（BAR 2）
//           无 MSI-X 中断（区别于 D1 PcieDisplayDevice）
// 作者: CppTLM Team / 日期: 2026-09-26
// 参考: openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/design.md §2
//       openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/specs/memory-device-mvp/spec.md
#ifndef CPPTLM_PCIE_MEMORY_DEVICE_H
#define CPPTLM_PCIE_MEMORY_DEVICE_H

#include <array>
#include <cstdint>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief D2 Memory 设备（Memory Device MVP）
     *
     * 设计原则（per design.md §2）：
     *   - BAR 0: 4KB MMIO 寄存器空间（kRegSize=4096）
     *   - BAR 2: 8GB Memory backing（kDefaultMemSize=8GB，lazy alloc）
     *   - tick()：仅推进 cycle_counter_，无 MSI-X
     *   - device_id = 0x0002（区别 D1 的 0x0001）
     *
     * ABI 影响: 0 个新 ABI 函数；走 DGpuBoard 私有方法路径
     */
    class PcieMemoryDevice {
    public:
        // ── 尺寸常量 ──
        static constexpr size_t kRegSize = 4096;                              // BAR 0 寄存器空间
        static constexpr uint64_t kDefaultMemSize = 8ULL * 1024 * 1024 * 1024;  // 8GB

        // ── 寄存器偏移 ──
        static constexpr uint32_t kRegDeviceIdentity = 0x00;  // R: vendor_id+device_id
        static constexpr uint32_t kRegMemSizeLo       = 0x10;  // RW: 容量低 32-bit
        static constexpr uint32_t kRegMemSizeHi       = 0x14;  // RW: 容量高 32-bit
        static constexpr uint32_t kRegMemBaseLo       = 0x18;  // RW: 主机可见基地址低 32-bit
        static constexpr uint32_t kRegMemBaseHi       = 0x1C;  // RW: 主机可见基地址高 32-bit
        static constexpr uint32_t kRegStatus          = 0x20;  // RW: bit 0=ready, bit 1=error
        static constexpr uint32_t kRegScratch        = 0xF0;  // RW: 16-byte scratch 寄存器

        // 设备身份常量
        static constexpr uint16_t kVendorId  = 0x1002;  // AMD/ATI
        static constexpr uint16_t kDeviceId  = 0x0002;  // Memory device

        // 状态位
        static constexpr uint32_t kStatusReady = 0x1u;
        static constexpr uint32_t kStatusError = 0x2u;

        // ── 构造 / 析构 ──
        PcieMemoryDevice();
        ~PcieMemoryDevice() = default;

        PcieMemoryDevice(const PcieMemoryDevice&) = delete;
        PcieMemoryDevice& operator=(const PcieMemoryDevice&) = delete;

        // ── MMIO read/write (BAR 0 寄存器, byte-level, 支持 1/2/4 字节 + unaligned) ──
        int mmio_read(uint64_t offset, void* buf, size_t len);
        int mmio_write(uint64_t offset, const void* buf, size_t len);

        // ── Memory backing read/write (BAR 2, byte-level) ──
        int memory_read(uint64_t offset, void* buf, size_t len);
        int memory_write(uint64_t offset, const void* buf, size_t len);

        // ── Tick (由 PcieEndpointIP::tick() 调用) ──
        // 推进 cycle counter，无 MSI-X
        void tick();

        // ── 状态查询 ──
        [[nodiscard]] bool has_memory_backing() const noexcept { return !memory_backing_.empty(); }
        uint64_t cycle_counter() const noexcept { return cycle_counter_; }

        // ── 设备身份 (供 DGpuBoard::pcie_config_read 路径使用) ──
        uint16_t vendor_id() const noexcept { return kVendorId; }
        uint16_t device_id() const noexcept { return kDeviceId; }

    private:
        // BAR 0 寄存器空间
        std::array<uint8_t, kRegSize> registers_{};

        // BAR 2 memory backing (lazy alloc - 首次 memory_read/write 时分配)
        std::vector<uint8_t> memory_backing_;

        // Cycle counter
        uint64_t cycle_counter_ = 0;

        // Memory backing lazy alloc helper
        void ensure_memory_backing_allocated();
    };

}  // namespace tlm::gpu

#endif  // CPPTLM_PCIE_MEMORY_DEVICE_H