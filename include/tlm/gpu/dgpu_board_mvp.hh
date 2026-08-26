// dgpu_board_mvp.hh
// DGpuBoardTLM: 8-component ChStreamModuleBase wrapper (s2 W3-4)
// Author: CppTLM Team
// Date: 2026-08-26
//
// Per Phase F-H.2 + Oracle ses_fe0b6e44 s2-dep fix
// Outer PIMPL: sub-components in Impl struct defined in .cc
#ifndef CPPTLM_DGPU_BOARD_MVP_H
#define CPPTLM_DGPU_BOARD_MVP_H

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <memory>

namespace tlm::gpu {

    struct KernelLaunchRequest {
        uint32_t task_id = 0;
        uint64_t vram_image_addr = 0;
        uint32_t grid_x = 0;
        uint32_t grid_y = 0;
        uint32_t grid_z = 0;
        uint32_t block_x = 0;
        uint32_t block_y = 0;
        uint32_t block_z = 0;
        uint32_t shared_mem_bytes = 0;
        uint64_t args_vram_addr = 0;
    };

    class DGpuBoardTLM : public ChStreamModuleBase {
    public:
        DGpuBoardTLM(const std::string& n, EventQueue* eq);
        ~DGpuBoardTLM() override;

        DGpuBoardTLM(const DGpuBoardTLM&) = delete;
        DGpuBoardTLM& operator=(const DGpuBoardTLM&) = delete;

        uint64_t install_kernel_module(const uint8_t* image_bytes, size_t size);
        int32_t uninstall_kernel_module(uint64_t image_handle);
        int32_t submit_kernel(const KernelLaunchRequest& req);
        void write_reg(uint32_t offset, uint32_t value);

        void init() override;
        void tick() override;
        void shutdown();

        void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
        unsigned num_ports() const override { return 1; }

        bool has_s1_components() const;

        // PCIe driver visibility: BAR0 status registers that driver polls
        // to observe SOC internal state (no direct sub-component access)
        bool cp_is_idle() const;
        size_t sq_pending_count() const;
        size_t sq_active_count() const;
        size_t sq_inflight_count() const;
        uint64_t doorbell_sq_tail(uint32_t stream_id) const;

        // PCIe BAR1 VRAM access (DMA-style)
        // Returns BAR0/1 region sizes for driver enumeration
        size_t bar0_size() const;
        size_t bar1_size() const;
        // Read BAR1 (VRAM) at offset (driver reads DMA-completion buffer)
        int32_t read_vram(uint64_t offset, void* host_buf, size_t size);
        // Write BAR1 (VRAM) at offset (driver H2D image upload)
        int32_t write_vram(uint64_t offset, const void* host_buf, size_t size);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace tlm::gpu

#endif // CPPTLM_DGPU_BOARD_MVP_H
