// usrlxemu_ioctl_stub_mvp.cc
// UsrLinuxEmu 4-IOCTL stub implementation
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/usrlxemu_ioctl_stub_mvp.hh"

namespace tlm::gpu {

    UsrLinuxEmuIoctlStub::UsrLinuxEmuIoctlStub(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
    }

    UsrLinuxEmuIoctlStub::~UsrLinuxEmuIoctlStub() = default;

    void UsrLinuxEmuIoctlStub::attach_board(DGpuBoardTLM* board) {
        board_ = board;
    }

    void UsrLinuxEmuIoctlStub::init() {
        initialized_ = true;
    }

    void UsrLinuxEmuIoctlStub::tick() {
    }

    void UsrLinuxEmuIoctlStub::set_stream_adapter(cpptlm::StreamAdapterBase*) {
    }
    void UsrLinuxEmuIoctlStub::set_stream_adapter(cpptlm::StreamAdapterBase*[]) {
    }

    IoctlResponse UsrLinuxEmuIoctlStub::ioctl(uint32_t request, const IoctlRequest& input) {
        IoctlResponse response{};
        if (!initialized_ || board_ == nullptr) {
            response.status = -19;
            return response;
        }

        switch (static_cast<UsrLinuxEmuIoctl>(request)) {
        case UsrLinuxEmuIoctl::LOAD_KERNEL_MODULE: {
            const uint8_t* bytes = input.image_bytes.empty() ? nullptr : input.image_bytes.data();
            response.image_handle = board_->install_kernel_module(bytes, input.image_bytes.size());
            response.status = response.image_handle == 0 ? -22 : 0;
            break;
        }
        case UsrLinuxEmuIoctl::LAUNCH_KERNEL_MODULE:
            response.status = -38;
            break;
        case UsrLinuxEmuIoctl::UNLOAD_KERNEL_MODULE:
            response.status = board_->uninstall_kernel_module(input.image_handle);
            break;
        case UsrLinuxEmuIoctl::PUSHBUFFER_SUBMIT_BATCH:
            response.status = board_->submit_kernel(input.launch);
            if (response.status == 0) {
                board_->write_reg(0x1000 + (input.stream_id << 2), input.wdu_offset);
            }
            break;
        default:
            response.status = -25;
            break;
        }
        return response;
    }

} // namespace tlm::gpu
