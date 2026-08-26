// usrlxemu_ioctl_stub_mvp.hh
// UsrLinuxEmu 4-IOCTL stub for dGPU MVP
// Author: CppTLM Team
// Date: 2026-08-26
#ifndef CPPTLM_USRLXEMU_IOCTL_STUB_MVP_H
#define CPPTLM_USRLXEMU_IOCTL_STUB_MVP_H

#include "core/chstream_module.hh"
#include "tlm/gpu/dgpu_board_mvp.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <memory>
#include <vector>

namespace tlm::gpu {

    enum class UsrLinuxEmuIoctl : uint32_t {
        LOAD_KERNEL_MODULE = 0x27,
        LAUNCH_KERNEL_MODULE = 0x28,
        UNLOAD_KERNEL_MODULE = 0x29,
        PUSHBUFFER_SUBMIT_BATCH = 0x01
    };

    struct IoctlRequest {
        uint64_t image_handle = 0;
        std::vector<uint8_t> image_bytes;
        KernelLaunchRequest launch;
        uint32_t stream_id = 0;
        uint32_t wdu_offset = 0;
    };

    struct IoctlResponse {
        int32_t status = 0;
        uint64_t image_handle = 0;
    };

    class DGpuBoardTLM;

    class UsrLinuxEmuIoctlStub : public ChStreamModuleBase {
    public:
        UsrLinuxEmuIoctlStub(const std::string& name, EventQueue* eq);
        ~UsrLinuxEmuIoctlStub() override;

        UsrLinuxEmuIoctlStub(const UsrLinuxEmuIoctlStub&) = delete;
        UsrLinuxEmuIoctlStub& operator=(const UsrLinuxEmuIoctlStub&) = delete;

        void attach_board(DGpuBoardTLM* board);
        IoctlResponse ioctl(uint32_t request, const IoctlRequest& input);

        void init() override;
        void tick() override;
        void set_stream_adapter(cpptlm::StreamAdapterBase*) override;
        void set_stream_adapter(cpptlm::StreamAdapterBase*[]) override;
        unsigned num_ports() const override { return 1; }

    private:
        DGpuBoardTLM* board_ = nullptr;
        bool initialized_ = false;
    };

} // namespace tlm::gpu

#endif // CPPTLM_USRLXEMU_IOCTL_STUB_MVP_H
