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

        bool has_s1_components() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace tlm::gpu

#endif // CPPTLM_DGPU_BOARD_MVP_H
