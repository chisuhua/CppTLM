// src/tlm/gpu/ptx_emu_submodule_mvp.cc
// PtxEmuSubmoduleMVP: PTX functional facade implementation (HSK-8 Phase 2)
//
// 编译防火墙: 此 .cc + adapter.cc 是 CppTLM 端**唯二**允许 include
// PTX-EMU 公共头 (`ptxemu/device_api.h` + `ptxemu/ir/statement.h`)
// 的 .cc 文件。PTX-EMU 内部头 (`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/
// `register/*.h`/`cudart/*.h` 等) 全部封装在 PTX-EMU 端的 .a 静态库内
// (HSK-8 spec §3 "cpp 不暴露")。
//
// 实现覆盖 (HSK-8 Phase 2 收窄, 与 PTX-EMU fcdad151 IPtxEmuDevice 实际签名 1:1 映射):
//   - Lifecycle (2): init / shutdown
//   - Execution (3): exe_once / sm_exe_once / warp_exe_once
//   - State Write (3): set_scoreboard / set_active_mask / set_next_pc
//   - State Query (3): get_thread_state / get_warp_status / is_finished
//   - HSK-4 Injection (1): attach_timing (3→1 聚合)
//   - WarpStatus 派生 helpers (3): get_active_mask_from_warp_status /
//                                 get_blocked_cycles_from_warp_status /
//                                 is_warp_finished_from_warp_status
//   - Stubs (8): read_register_* / write_register_* / read_global_* /
//                write_global_* / decode_ptxir
//     (PTX-EMU fcdad151 未暴露, 等待 HSK-9 扩展后实现)
//
// 作者 CppTLM Team / 2026-08-24 (HSK-8 Phase 2, Sisyphus)
// 参考:
//   - openspec/changes/cpptlm-ptxemu-public-device-api/{proposal,design}.md
//   - HSK-8 spec: docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md
//   - PTX-EMU device API: include/ptxemu/device_api.h (fcdad151)

#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

// HSK-8 Phase 2: 仅 include PTX-EMU 公共头 (PTXEMU_API_VERSION=1, IPtxEmuDevice)
#include "ptxemu/device_api.h"

namespace tlm {

// =============================================================================
// Impl — 不透明实现, 持有 IPtxEmuDevice unique_ptr
// =============================================================================
struct PtxEmuSubmoduleMVP::Impl {
    std::unique_ptr<ptxemu::IPtxEmuDevice> device;

    Impl() = default;
    ~Impl() {
        if (device) {
            ptxemu::destroy_device(device.release());
        }
    }
};

// =============================================================================
// RAII — ctor / dtor
// =============================================================================
PtxEmuSubmoduleMVP::PtxEmuSubmoduleMVP()
    : impl_(std::make_unique<Impl>()) {}

PtxEmuSubmoduleMVP::~PtxEmuSubmoduleMVP() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================
bool PtxEmuSubmoduleMVP::init(const std::string& ptx_emu_root, const DeviceConfig& config) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    ptx_emu_root_ = ptx_emu_root;
    config_ = config;
    impl_->device = ptxemu::create_device();
    if (!impl_->device) {
        return false;
    }
    bool ok = impl_->device->initialize(config);
    initialized_ = ok;
    return ok;
}

void PtxEmuSubmoduleMVP::shutdown() {
    if (impl_ && impl_->device) {
        impl_->device->shutdown();
    }
    initialized_ = false;
}

// =============================================================================
// Execution (IPtxEmuDevice 1:1 转发)
// =============================================================================
int PtxEmuSubmoduleMVP::exe_once() {
    if (!impl_ || !impl_->device) return -1;
    return impl_->device->exe_once();
}

int PtxEmuSubmoduleMVP::sm_exe_once(uint32_t sm_id) {
    if (!impl_ || !impl_->device) return -1;
    return impl_->device->sm_exe_once(sm_id);
}

int PtxEmuSubmoduleMVP::warp_exe_once(uint32_t sm_id, uint32_t warp_id) {
    if (!impl_ || !impl_->device) return -1;
    return impl_->device->warp_exe_once(sm_id, warp_id);
}

// =============================================================================
// State Write (IPtxEmuDevice 1:1 转发)
// =============================================================================
bool PtxEmuSubmoduleMVP::set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask) {
    if (!impl_ || !impl_->device) return false;
    return impl_->device->set_scoreboard(sm_id, warp_id, mask);
}

bool PtxEmuSubmoduleMVP::set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) {
    if (!impl_ || !impl_->device) return false;
    return impl_->device->set_active_mask(sm_id, warp_id, mask);
}

bool PtxEmuSubmoduleMVP::set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) {
    if (!impl_ || !impl_->device) return false;
    return impl_->device->set_next_pc(sm_id, warp_id, lane_id, pc);
}

// =============================================================================
// State Query (IPtxEmuDevice 1:1 转发)
// =============================================================================
ThreadState PtxEmuSubmoduleMVP::get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) {
    if (!impl_ || !impl_->device) return ThreadState::kIdle;
    return impl_->device->get_thread_state(sm_id, warp_id, lane_id);
}

WarpStatus PtxEmuSubmoduleMVP::get_warp_status(uint32_t sm_id, uint32_t warp_id) {
    if (!impl_ || !impl_->device) {
        return WarpStatus{};  // 默认构造 (warp_id=0, lanes 空)
    }
    return impl_->device->get_warp_status(sm_id, warp_id);
}

bool PtxEmuSubmoduleMVP::is_finished() {
    if (!impl_ || !impl_->device) return false;
    return impl_->device->is_finished();
}

// =============================================================================
// HSK-4 注入 (3→1 聚合)
// 注: HSK-4 跨仓 ABI 锁定 (pt:pc-3), CppTLM vendored 接口与 PTX-EMU 端
// forward decl 兼容; 编译防火墙仅 adapter.cc 持有具体类型, facade 用 void*
// 转发避免 vendored 头依赖外泄.
// =============================================================================
void PtxEmuSubmoduleMVP::attach_timing(void* scoreboard, void* pipeline, void* tensor_core) {
    if (!impl_ || !impl_->device) return;
    impl_->device->attach_timing(
        reinterpret_cast<ptxemu::IScoreboard*>(scoreboard),
        reinterpret_cast<ptxemu::IPipelineLatencyProvider*>(pipeline),
        reinterpret_cast<ptxemu::ITensorCoreTiming*>(tensor_core));
}

// =============================================================================
// WarpStatus 派生 helpers (Phase 1 S1 方法的语义等价, IPtxEmuDevice::get_warp_status 解析)
// =============================================================================
uint32_t PtxEmuSubmoduleMVP::get_active_mask_from_warp_status(uint32_t sm_id, uint32_t warp_id) {
    WarpStatus ws = get_warp_status(sm_id, warp_id);
    uint32_t mask = 0;
    for (uint32_t i = 0; i < ws.lanes.size() && i < kWarpSize; ++i) {
        if (ws.lanes[i].state != ThreadState::kIdle) {
            mask |= (1u << i);
        }
    }
    return mask;
}

int32_t PtxEmuSubmoduleMVP::get_blocked_cycles_from_warp_status(
    uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) {
    WarpStatus ws = get_warp_status(sm_id, warp_id);
    if (lane_id < ws.lanes.size()) {
        // PTX-EMU WarpStatus DTO 未直接暴露 blocked_cycles,
        // 从 LaneStatus.state 推断: BAR_SYNC = blocked on barrier = 1 cycle
        if (ws.lanes[lane_id].state == ThreadState::kBarSync) return 1;
    }
    return 0;
}

bool PtxEmuSubmoduleMVP::is_warp_finished_from_warp_status(uint32_t sm_id, uint32_t warp_id) {
    WarpStatus ws = get_warp_status(sm_id, warp_id);
    if (ws.lanes.empty()) return false;
    for (const auto& lane : ws.lanes) {
        if (lane.state != ThreadState::kExit) return false;
    }
    return true;
}

// =============================================================================
// Stubs: PTX-EMU 端 fcdad151 未暴露的 S1 facade 方法
// (待 HSK-9 扩展 PTX-EMU device API 后实现)
// =============================================================================
bool PtxEmuSubmoduleMVP::read_register_u32(uint32_t, uint32_t, uint32_t,
                                          const char*, uint32_t& out_value) {
    out_value = 0;
    return false;  // PTX-EMU 端未暴露 register 读写
}
bool PtxEmuSubmoduleMVP::read_register_u64(uint32_t, uint32_t, uint32_t,
                                          const char*, uint64_t& out_value) {
    out_value = 0;
    return false;
}
bool PtxEmuSubmoduleMVP::write_register_u32(uint32_t, uint32_t, uint32_t,
                                           const char*, uint32_t) {
    return false;
}
bool PtxEmuSubmoduleMVP::write_register_u64(uint32_t, uint32_t, uint32_t,
                                           const char*, uint64_t) {
    return false;
}
bool PtxEmuSubmoduleMVP::read_global_u32(uint64_t, uint32_t& out_value) {
    out_value = 0;
    return false;  // PTX-EMU 端未暴露 global memory 读写
}
bool PtxEmuSubmoduleMVP::read_global_u64(uint64_t, uint64_t& out_value) {
    out_value = 0;
    return false;
}
bool PtxEmuSubmoduleMVP::write_global_u32(uint64_t, uint32_t) {
    return false;
}
bool PtxEmuSubmoduleMVP::write_global_u64(uint64_t, uint64_t) {
    return false;
}
std::vector<StatementContext> PtxEmuSubmoduleMVP::decode_ptxir(
    const std::vector<uint8_t>& /*ptxir_bytes*/) {
    // PTX-EMU fcdad151 device_api.h 未暴露 decode_ptxir 自由函数
    // (impl 内部使用, 公共 API 暂无). CppTLM 端 facade 返回空 vector 占位.
    return {};
}

}  // namespace tlm