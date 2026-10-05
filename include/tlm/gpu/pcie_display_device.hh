// include/tlm/gpu/pcie_display_device.hh
// PcieDisplayDevice: D1 显示 IO 设备 MVP (D3a-2 注入式重构)
//
// 功能描述：4KB MMIO 寄存器（BAR 0）+ Framebuffer（BAR 1, non-owning injected view）
//           + VBLANK MSI-X 中断触发（每 1024 cycle 一次）
//
// D3a-2 (cpptlm-dgpu-d3a1-display-injection-refactor, per ADR-DGPU-10):
//   - 删除自有 framebuffer_ backing (32MB lazy alloc) + kFbSize 常量 + ensure_framebuffer_allocated()
//   - 改为 non-owning + set_backing_view(ptr, size) 注入视图 (与 PcieMemoryDevice 精确镜像)
//   - backing 唯一真源在 DGpuBoard::display_fb_storage_ (per ADR-DGPU-05)
//   - backdoor_read/write 改 view-only: 无注入时返 -ENODEV (无 silent lazy alloc)
//   - framebuffer_size_bytes() 保留 (返回 backing_view_size_, 兼容现有测试)
//
// 作者: CppTLM Team / 日期: 2026-09-20 (D3a-2: 2026-10-04)
// 参考: openspec/changes/2026-09-20-cpptlm-pcie-display-io-mvp/design.md §2
//       openspec/changes/cpptlm-dgpu-d3a1-display-injection-refactor/design.md §2.1
//       spec.md §PcieDisplayDevice Class
#ifndef CPPTLM_PCIE_DISPLAY_DEVICE_H
#define CPPTLM_PCIE_DISPLAY_DEVICE_H

#include "tlm/gpu/msix_table_mvp.hh"

#include <array>
#include <cstdint>

namespace tlm::gpu {

    /**
     * @brief D1 显示 IO 设备（Display Engine MVP）
     *
     * 设计原则（per design.md §2 + D3a-2 design.md §2.1）：
     *   - BAR 0: 4KB MMIO 寄存器空间（kRegSize=4096）
     *   - BAR 1: Framebuffer backing（non-owning injected view, per ADR-DGPU-10）
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
        // D3a-2: view-only —— 无注入 backing 时返 -ENODEV (无 silent lazy alloc)
        int backdoor_read(uint64_t offset, void* buf, size_t len);
        int backdoor_write(uint64_t offset, const void* buf, size_t len);

        // ── D3a-2 注入式 API (per ADR-DGPU-10, 与 PcieMemoryDevice 精确镜像) ──
        void set_backing_view(uint8_t* ptr, uint64_t size) noexcept {
            backing_view_ = ptr;
            backing_view_size_ = size;
        }
        [[nodiscard]] bool has_backing_view() const noexcept {
            return backing_view_ != nullptr;
        }
        // 镜像 PcieMemoryDevice::backing_size() (line 124 of pcie_memory_device.hh)
        [[nodiscard]] uint64_t backing_size() const noexcept {
            return backing_view_size_;
        }

        // ── Tick (由 PcieEndpointIP::tick() 调用) ──
        // 推进 VBLANK counter，每 kVblankInterval 触发一次 MSI-X vector 0
        void tick(MsiXTable& msix);

        // ── 状态查询 ──
        uint32_t vblank_count() const noexcept { return vblank_count_; }
        uint64_t cycle_counter() const noexcept { return cycle_counter_; }
        // D3a-2: 保留 (返回 backing_view_size_, 兼容现有测试; 语义与旧 framebuffer_.size() 等价)
        size_t framebuffer_size_bytes() const noexcept {
            return static_cast<size_t>(backing_view_size_);
        }

        // ── 设备身份 (供 DGpuBoard::pcie_config_read 路径使用) ──
        uint16_t vendor_id() const noexcept { return kVendorId; }
        uint16_t device_id() const noexcept { return kDeviceId; }

    private:
        // BAR 0 寄存器空间
        std::array<uint8_t, kRegSize> registers_{};

        // BAR 1 framebuffer (non-owning injected view, per ADR-DGPU-05/10)
        // 删除: std::vector<uint8_t> framebuffer_ ← D3a-2 关键删除
        uint8_t* backing_view_      = nullptr;  // injected from DGpuBoard
        uint64_t backing_view_size_ = 0;

        // VBLANK timer
        uint64_t cycle_counter_ = 0;
        uint32_t vblank_count_ = 0;

        // VBLANK 触发 helper (写 STATUS bit0 + 调 msix.update_pending(0))
        void trigger_vblank(MsiXTable& msix);

        // 删除: void ensure_framebuffer_allocated(); ← D3a-2 关键删除
    };

}  // namespace tlm::gpu

#endif  // CPPTLM_PCIE_DISPLAY_DEVICE_H