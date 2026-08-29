// dgpu_board_mvp.cc
// DGpuBoardTLM: 8-component ChStreamModuleBase wrapper implementation
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/dgpu_board_mvp.hh"

#include "tlm/gpu/command_processor_mvp.hh"
#include "tlm/gpu/completion_ring_mvp.hh"
#include "tlm/gpu/dgpu_bar.hh"
#include "tlm/gpu/doorbell_mvp.hh"
#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/submit_queue_mvp.hh"
#include "tlm/gpu/tmu_dispatch_processor_mvp.hh"
#include "tlm/gpu/tmu_handler_mvp.hh"

#include <cstring>

#ifdef CPPTLM_WITH_PTX_EMU
#include "tlm/gpu/cuda_core_adapter_mvp.hh"
#include "tlm/gpu/ptx_emu_submodule_mvp.hh"
#endif

namespace tlm::gpu {

    // S3SubmitQueueHandler 已删除 — handler 注入模式由端口连接替代 (per design §3.5 陷阱 5)

    struct DGpuBoardTLM::Impl {
        DGpuBar bar;
        Doorbell doorbell;
        SubmitQueueTLM sq;
        CompletionRing cq;
        CommandProcessor cp;
        TmuDispatchProcessor tmu;
        uint64_t next_image_handle = 1;
        bool initialized = false;
#ifdef CPPTLM_WITH_PTX_EMU
        std::unique_ptr<CudaCoreAdapterMVP> cuda_core;
        std::unique_ptr<PtxEmuSubmoduleMVP> ptx_emu;
#endif

        Impl() : sq("sq", nullptr) {}
    };

    DGpuBoardTLM::DGpuBoardTLM(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq), impl_(std::make_unique<Impl>()) {
    }

    DGpuBoardTLM::~DGpuBoardTLM() = default;

    bool DGpuBoardTLM::has_s1_components() const {
#ifdef CPPTLM_WITH_PTX_EMU
        return impl_->cuda_core != nullptr;
#else
        return false;
#endif
    }

    void DGpuBoardTLM::init() {
        if (impl_->initialized)
            return;

        impl_->bar.init();
        impl_->cp.set_decoder(std::make_unique<Pm4Decoder>());
        impl_->cp.set_vram_reader(
            [this](uint64_t va, void* out, size_t sz) { return this->read_vram(va, out, sz); });
        impl_->cp.set_dispatch_target(
            [this](const TmuDispatchRecord& rec) { return impl_->tmu.submit(rec); });
        // S3SubmitQueueHandler 已删除 — handler 注入模式由端口连接替代 (per design §3.5 陷阱 5)
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
        if (!impl_->initialized)
            return;
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
        if (!impl_->initialized)
            return 0;
        if (image_bytes == nullptr || size == 0)
            return 0;
        uint64_t handle = impl_->next_image_handle++;
        return handle;
    }

    int32_t DGpuBoardTLM::uninstall_kernel_module(uint64_t image_handle) {
        if (!impl_->initialized)
            return -1;
        if (image_handle == 0)
            return -1;
        return 0;
    }

    int32_t DGpuBoardTLM::submit_kernel(const KernelLaunchRequest& req) {
        if (!impl_->initialized)
            return -1;

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

    bool DGpuBoardTLM::cp_is_idle() const {
        return impl_->cp.state() == CommandProcessor::State::IDLE;
    }

    size_t DGpuBoardTLM::sq_pending_count() const {
        return impl_->sq.pending_count();
    }

    size_t DGpuBoardTLM::sq_active_count() const {
        return impl_->sq.active_count();
    }

    size_t DGpuBoardTLM::sq_inflight_count() const {
        return impl_->sq.inflight_count();
    }

    uint64_t DGpuBoardTLM::doorbell_sq_tail(uint32_t stream_id) const {
        return impl_->doorbell.sq_tail(stream_id);
    }

    size_t DGpuBoardTLM::bar0_size() const {
        return 0x10000;
    }

    size_t DGpuBoardTLM::bar1_size() const {
        return 256ULL * 1024ULL * 1024ULL;
    }

    int32_t DGpuBoardTLM::read_vram(uint64_t offset, void* host_buf, size_t size) {
        if (!impl_->initialized)
            return -19;
        if (host_buf == nullptr || size == 0)
            return -22;
        void* vram = impl_->bar.vram_base();
        if (vram == nullptr)
            return -5;
        if (offset > 256ULL * 1024ULL * 1024ULL || size > 256ULL * 1024ULL * 1024ULL - offset)
            return -22;
        std::memcpy(host_buf, static_cast<uint8_t*>(vram) + offset, size);
        return 0;
    }

    int32_t DGpuBoardTLM::write_vram(uint64_t offset, const void* host_buf, size_t size) {
        if (!impl_->initialized)
            return -19;
        if (host_buf == nullptr || size == 0)
            return -22;
        void* vram = impl_->bar.vram_base();
        if (vram == nullptr)
            return -5;
        if (offset > 256ULL * 1024ULL * 1024ULL || size > 256ULL * 1024ULL * 1024ULL - offset)
            return -22;
        std::memcpy(static_cast<uint8_t*>(vram) + offset, host_buf, size);
        return 0;
    }

    void DGpuBoardTLM::write_reg(uint32_t offset, uint32_t value) {
        if (offset == 0x0014) {
            impl_->doorbell.ring(0, value);
            impl_->cp.wake();
        } else if (offset >= 0x1000 && offset < 0x2000) {
            uint32_t stream_id = (offset - 0x1000) >> 2;
            impl_->doorbell.ring(stream_id, value);
            impl_->cp.wake();
        }
    }

    void DGpuBoardTLM::tick() {
        if (!impl_->initialized)
            return;

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
