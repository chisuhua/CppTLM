// include/tlm/gpu/pcie_memory_device.hh
// PcieMemoryDevice: D2 Memory 设备 MVP — Phase 3 (driver-visible-minimal-soc v1.8)
//
// 功能描述：4KB MMIO 寄存器（BAR 0）+ 8GB Memory Backing（BAR 2）
//           无 MSI-X 中断（区别于 D1 PcieDisplayDevice）
//
// v1.8 H1 修订 (Phase 3): 从独立类 → ChStreamModuleBase 派生
//   - 2 SlavePorts (req_in[0]=SDMA, req_in[1]=GMMU) + 2 MasterPorts (resp_out[0/1])
//   - 单 MultiPortStreamAdapter* adapter_ (v1.3 B3 撤销 N4 双 adapter)
//   - wire-format: PcieTlpBundle 统一 (v1.8 H1)
//   - backing 注入: set_backing_view (Phase 2 ADR-DGPU-10 已实现), bound=backing_view_size_
//   - handle_slave_port: bound 检查 + SLVERR 传播 (v1.4 B9)
//   - kRegMemSizeLo/Hi 读 injected backing_view_size_ (v1.4 B12/B27)
//
// 作者: CppTLM Team / 日期: 2026-09-26 (Phase 3 2027-02-09)
// 参考: openspec/changes/cpptlm-driver-visible-minimal-soc/design.md §4
#ifndef CPPTLM_PCIE_MEMORY_DEVICE_H
#define CPPTLM_PCIE_MEMORY_DEVICE_H

#include "bundles/pcie_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/sim_object.hh"
#include "framework/stream_adapter.hh"

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief D2 Memory 设备（Memory Device MVP）— ChStreamModuleBase 化
     *
     * 设计原则（per design.md §4, v1.8 H1）：
     *   - BAR 0: 4KB MMIO 寄存器空间（kRegSize=4096）
     *   - BAR 2: 8GB Memory backing（注入式 backing_view_, 非 lazy alloc）
     *   - 2 SlavePorts: req_in[0]=SDMA mem_out, req_in[1]=GMMU req_out
     *   - 2 MasterPorts: resp_out[0/1]
     *   - 单 adapter_ (v1.3 B3): MultiPortStreamAdapter 内部遍历全端口
     *   - wire-format: PcieTlpBundle (v1.8 H1 统一)
     *   - tick()：推进 cycle_counter_ + handle_slave_port(p) + adapter_->tick()
     *   - device_id = 0x0002（区别 D1 的 0x0001）
     *
     * ABI 影响: 0 个新 ABI 函数；走 DGpuBoard 私有方法路径
     */
    class PcieMemoryDevice : public ChStreamModuleBase {
    public:
        static constexpr unsigned NUM_PORTS = 2;
        // 端口索引（per design.md §4 Port index ordering lock）
        static constexpr unsigned PORT_SDMA = 0;  // sdma.mem_out 接入
        static constexpr unsigned PORT_GMMU = 1;  // gmmu.req_out 接入

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
        explicit PcieMemoryDevice(const std::string& name, EventQueue* eq)
            : ChStreamModuleBase(name, eq) {
            init_identity_regs();
            // v1.4 B7: 不再预分配 8GB; backing 由 DGpuBoard 注入 (vram_storage_)
            // v1.4 B10: 保留 mutex (保护 sim_thread + host mmio 并发访问)
        }
        ~PcieMemoryDevice() override = default;

        PcieMemoryDevice(const PcieMemoryDevice&) = delete;
        PcieMemoryDevice& operator=(const PcieMemoryDevice&) = delete;
        PcieMemoryDevice(PcieMemoryDevice&&) = delete;
        PcieMemoryDevice& operator=(PcieMemoryDevice&&) = delete;

        std::string get_module_type() const override { return "PcieMemoryDevice"; }
        unsigned num_ports() const override { return NUM_PORTS; }


        // ── ChStream SlavePort/MasterPort（多端口适配器要求 public）──
        cpptlm::InputStreamAdapter<bundles::PcieTlpBundle>  req_in[NUM_PORTS];
        cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_PORTS];

        // v1.3 B3 撤销 N4: 单 MultiPortStreamAdapter* adapter_ (非 adapters_[2])
        void set_stream_adapter(cpptlm::StreamAdapterBase* a) override {
            adapter_ = a;  // 单 adapter 存储
        }

        // ── MMIO read/write (BAR 0 寄存器, byte-level, 支持 1/2/4 字节 + unaligned) ──
        int mmio_read(uint64_t offset, void* buf, size_t len);
        int mmio_write(uint64_t offset, const void* buf, size_t len);

        // ── Memory backing read/write (BAR 2, byte-level)
        // v1.4 B7 (ADR-DGPU-05): memory_backing_ 删除, 改用 backing_view_ 注入
        // v1.4 B8: bound = backing_view_size_ (非 kDefaultMemSize)
        // v1.4 B12: backing_view_==nullptr 返 -ENODEV
        // v1.4 B10: mutex 保护 host/sim 并发访问
        int memory_read(uint64_t offset, void* buf, size_t len);
        int memory_write(uint64_t offset, const void* buf, size_t len);

        // ── backing view 注入 (per ADR-DGPU-10, 替代旧 memory_backing_ lazy alloc)
        void set_backing_view(uint8_t* ptr, uint64_t size) noexcept {
            std::lock_guard<std::mutex> lock(backing_mutex_);
            backing_view_ = ptr;
            backing_view_size_ = size;
        }
        [[nodiscard]] bool has_backing_view() const noexcept {
            std::lock_guard<std::mutex> lock(backing_mutex_);
            return backing_view_ != nullptr;
        }
        uint64_t backing_size() const noexcept {
            std::lock_guard<std::mutex> lock(backing_mutex_);
            return backing_view_size_;
        }

        // ── Tick (单 tick 一次; MultiPortStreamAdapter 内部遍历全端口) ──
        void tick() override;

        // ── 状态查询 ──
        uint64_t cycle_counter() const noexcept { return cycle_counter_; }

        // ── 设备身份 (供 DGpuBoard::pcie_config_read 路径使用) ──
        uint16_t vendor_id() const noexcept { return kVendorId; }
        uint16_t device_id() const noexcept { return kDeviceId; }

    private:
        void init_identity_regs();
        // v1.4 B9: 检查 memory_read/write 返回值; 失败置 SLVERR (resp.kind=CPLD + data=0xDEAD)
        void handle_slave_port(unsigned p);

        cpptlm::StreamAdapterBase* adapter_ = nullptr;  // 单 adapter (非数组, v1.3 B3)

        // BAR 0 寄存器空间
        std::array<uint8_t, kRegSize> registers_{};

        // BAR 2 memory backing (injected by DGpuBoard, per ADR-DGPU-05 v1.4 B7)
        uint8_t* backing_view_ = nullptr;
        uint64_t backing_view_size_ = 0;
        // v1.4 B10: 保留 mutex (保护 host mmio + sim_thread 并发访问)
        mutable std::mutex backing_mutex_;

        // Cycle counter
        uint64_t cycle_counter_ = 0;
    };

}  // namespace tlm::gpu

#endif  // CPPTLM_PCIE_MEMORY_DEVICE_H
