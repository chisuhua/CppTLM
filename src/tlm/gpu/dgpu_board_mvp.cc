// dgpu_board_mvp.cc
// DGpuBoardTLM: 8-component ChStreamModuleBase wrapper implementation
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/dgpu_board_mvp.hh"

#include "tlm/gpu/dgpu_bar.hh"
#include "tlm/gpu/doorbell_mvp.hh"
#include "tlm/gpu/completion_ring_mvp.hh"
#include "tlm/gpu/submit_queue_mvp.hh"
#include "tlm/gpu/command_processor_mvp.hh"
#include "tlm/gpu/tmu_dispatch_processor_mvp.hh"

#ifdef CPPTLM_WITH_PTX_EMU
#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"
#endif

namespace tlm::gpu {

#ifdef CPPTLM_WITH_PTX_EMU
    struct DGpuBoardTLM::Impl {
        DGpuBar bar;
        Doorbell doorbell;
        SubmitQueue sq;
        CompletionRing cq;
        CommandProcessor cp;
        TmuDispatchProcessor tmu;
        std::unique_ptr<CudaCoreAdapterMVP> cuda_core;
        std::unique_ptr<PtxEmuSubmoduleMVP> ptx_emu;
        uint64_t next_image_handle = 1;
        bool initialized = false;
    };
#else
    struct DGpuBoardTLM::Impl {
        DGpuBar bar;
        Doorbell doorbell;
        SubmitQueue sq;
        CompletionRing cq;
        CommandProcessor cp;
        TmuDispatchProcessor tmu;
        uint64_t next_image_handle = 1;
        bool initialized = false;
    };
#endif

    DGpuBoardTLM::DGpuBoardTLM(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq), impl_(std::make_unique<Impl>()) {}

    DGpuBoardTLM::~DGpuBoardTLM() = default;

    bool DGpuBoardTLM::has_s1_components() const {
#ifdef CPPTLM_WITH_PTX_EMU
        return impl_->cuda_core != nullptr;
#else
        return false;
#endif
    }

    void DGpuBoardTLM::init() {
        if (impl_->initialized) return;

        impl_->bar.init();
#ifdef CPPTLM_WITH_PTX_EMU
        if (!impl_->cuda_core) {
            impl_->cuda_core = std::make_unique<CudaCoreAdapterMVP>();
        }
        if (!impl_->ptx_emu) {
            impl_->ptx_emu = std::make_unique<PtxEmuSubmoduleMVP>();
        }
#endif
        impl_->initialized = true;
    }

    void DGpuBoardTLM::shutdown() {
        if (!impl_->initialized) return;
#ifdef CPPTLM_WITH_PTX_EMU
        if (impl_->ptx_emu) {
            impl_->ptx_emu->shutdown();
        }
#endif
        impl_->bar.shutdown();
        impl_->initialized = false;
    }

    void DGpuBoardTLM::set_stream_adapter(cpptlm::StreamAdapterBase*) {
        // No-op for s2 MVP (single-port module, no stream adapter needed)
    }

    void DGpuBoardTLM::set_stream_adapter(cpptlm::StreamAdapterBase*[]) {
        // No-op for s2 MVP
    }

    uint64_t DGpuBoardTLM::install_kernel_module(const uint8_t* image_bytes, size_t size) {
        if (!impl_->initialized) return 0;
        if (image_bytes == nullptr || size == 0) return 0;
        uint64_t handle = impl_->next_image_handle++;
        return handle;
    }

    int32_t DGpuBoardTLM::uninstall_kernel_module(uint64_t image_handle) {
        if (!impl_->initialized) return -1;
        if (image_handle == 0) return -1;
        return 0;
    }

    int32_t DGpuBoardTLM::submit_kernel(const KernelLaunchRequest& req) {
        if (!impl_->initialized) return -1;

        CtaDescriptor cta{};
        cta.task_id = req.task_id;
        cta.vram_image_addr = req.vram_image_addr;
        cta.grid_x = req.grid_x;
        cta.grid_y = req.grid_y;
        cta.grid_z = req.grid_z;
        cta.block_x = req.block_x;
        cta.block_y = req.block_y;
        cta.block_z = req.block_z;
        cta.shared_mem_bytes = req.shared_mem_bytes;
        cta.args_vram_addr = req.args_vram_addr;

        return impl_->sq.enqueue(cta) ? 0 : -1;
    }

    void DGpuBoardTLM::write_reg(uint32_t offset, uint32_t value) {
        if (offset >= 0x1000 && offset < 0x2000) {
            uint32_t stream_id = (offset - 0x1000) >> 2;
            impl_->doorbell.ring(stream_id, value);
            impl_->cp.wake();
        }
    }

    void DGpuBoardTLM::tick() {
        if (!impl_->initialized) return;

        impl_->cp.tick();
        impl_->sq.tick();
        impl_->doorbell.tick();
        impl_->cq.tick();
#ifdef CPPTLM_WITH_PTX_EMU
        if (impl_->cuda_core) {
            impl_->cuda_core->tick();
        }
#endif
    }

} // namespace tlm::gpu
