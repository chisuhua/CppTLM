// include/tlm/gpu/pcie_display_device.hh
// PcieDisplayDevice: D1 显示 IO 设备 MVP
//
// 功能描述：4KB MMIO 寄存器（BAR 0）+ 32MB Framebuffer（BAR 1）
//           + VBLANK MSI-X 中断触发（每 1024 cycle 一次）
// 作者: CppTLM Team / 日期: 2026-09-20
// 参考: openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/design.md §2
//       spec.md §PcieDisplayDevice Class
#ifndef CPPTLM_PCIE_DISPLAY_DEVICE_H
#define CPPTLM_PCIE_DISPLAY_DEVICE_H

#include "tlm/gpu/msix_table_mvp.hh"

#include <array>
#include <cstdint>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief D1 显示 IO 设备（Display Engine MVP）
     *
     * 设计原则（per design.md §2）：
     *   - BAR 0: 4KB MMIO 寄存器空间（kRegSize=4096）
     *   - BAR 1: 32MB Framebuffer backing（kFbSize=32MB，lazy alloc）
     *   - VBLANK: 每 1024 cycle（kVblankInterval）触发一次 MSI-X vector 0
     *   - tick(MsiXTable&): 由 PcieEndpointIP::tick() 调用注入
     *   - 不集成 SDMA / SM / Power Management（D2/D3 范围）
     *
     * ABI 影响: 0 个新 ABI 函数；走 DGpuBoard 私有方法路径
     */
    class PcieDisplayDevice {
    public:
        // ── 尺寸常量 ──
        static constexpr size_t kRegSize = 4096;                  // BAR 0 寄存器空间
        static constexpr size_t kFbSize = 32 * 1024 * 1024;       // BAR 1 framebuffer
        static constexpr uint64_t kVblankInterval = 1024;         // VBLANK 周期 (cycle)

        // ── 寄存器偏移 ──
        static constexpr uint32_t kRegDeviceIdentity = 0x00;       // R: vendor_id+device_id
        static constexpr uint32_t kRegDisplayMode    = 0x10;       // RW: 0=off, 1=1080p, 2=1440p, 3=2160p
        static constexpr uint32_t kRegPixelFormat    = 0x14;       // RW: 0=RGB888, 1=RGBA8888, 2=NV12
        static constexpr uint32_t kRegFbBaseLo      = 0x20;       // RW: framebuffer 基地址低 32-bit
        static constexpr uint32_t kRegFbSize        = 0x28;       // RW: framebuffer 大小
        static constexpr uint32_t kRegFbPitch       = 0x2C;       // RW: bytes per scanline
        static constexpr uint32_t kRegStatus        = 0x30;       // R/W1C: bit 0 = VBLANK pending
        static constexpr uint32_t kRegStatusClear   = 0x34;       // W: W1C for VBLANK pending
        static constexpr uint32_t kRegIntMask       = 0x40;       // RW: bit 0 = VBLANK mask (0=masked, 1=enabled)
        static constexpr uint32_t kRegIntStatus     = 0x44;       // R: 镜像 STATUS bit 0
        static constexpr uint32_t kRegScratch       = 0xF0;       // RW: 16-byte scratch 寄存器

        // 设备身份常量
        static constexpr uint16_t kVendorId  = 0x1002;  // AMD/ATI
        static constexpr uint16_t kDeviceId  = 0x0001;  // Display device

        // 状态位
        static constexpr uint32_t kStatusVblankPending = 0x1u;
        static constexpr uint32_t kIntMaskVblankEnable = 0x1u;

        // ── 构造 / 析构 ──
        PcieDisplayDevice();
        ~PcieDisplayDevice() = default;

        PcieDisplayDevice(const PcieDisplayDevice&) = delete;
        PcieDisplayDevice& operator=(const PcieDisplayDevice&) = delete;

        // ── MMIO read/write (BAR 0 寄存器, byte-level, 支持 1/2/4 字节 + unaligned) ──
        int mmio_read(uint64_t offset, void* buf, size_t len);
        int mmio_write(uint64_t offset, const void* buf, size_t len);

        // ── Backdoor read/write (BAR 1 framebuffer, byte-level) ──
        int backdoor_read(uint64_t offset, void* buf, size_t len);
        int backdoor_write(uint64_t offset, const void* buf, size_t len);

        // ── Tick (由 PcieEndpointIP::tick() 调用) ──
        // 推进 VBLANK counter，每 kVblankInterval 触发一次 MSI-X vector 0
        void tick(MsiXTable& msix);

        // ── 状态查询 ──
        uint32_t vblank_count() const noexcept { return vblank_count_; }
        uint64_t cycle_counter() const noexcept { return cycle_counter_; }
        size_t framebuffer_size_bytes() const noexcept {
            return framebuffer_.size();
        }

        // ── 设备身份 (供 DGpuBoard::pcie_config_read 路径使用) ──
        uint16_t vendor_id() const noexcept { return kVendorId; }
        uint16_t device_id() const noexcept { return kDeviceId; }

    private:
        // BAR 0 寄存器空间
        std::array<uint8_t, kRegSize> registers_{};

        // BAR 1 framebuffer (lazy alloc - 首次 write/read 时分配)
        std::vector<uint8_t> framebuffer_;

        // VBLANK timer
        uint64_t cycle_counter_ = 0;
        uint32_t vblank_count_ = 0;

        // VBLANK 触发 helper (写 STATUS bit0 + 调 msix.update_pending(0))
        void trigger_vblank(MsiXTable& msix);

        // Framebuffer lazy alloc helper
        void ensure_framebuffer_allocated();
    };

}  // namespace tlm::gpu

#endif  // CPPTLM_PCIE_DISPLAY_DEVICE_H