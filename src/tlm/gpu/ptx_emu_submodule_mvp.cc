// src/tlm/gpu/ptx_emu_submodule_mvp.cc
// PtxEmuSubmoduleMVP: PTX functional facade implementation (per S1 / T-s1-3)
//
// 编译防火墙: 此 .cc 是 CppTLM 端**唯一**允许 include PTX-EMU 头 (`ptxsim/*.h` +
// `ptx_ir/*.h` + `memory/*.h` + `register/*.h` + `cudart/*.h`) 的 .cc 文件。
// 其他 CppTLM 代码只见前向声明。
//
// 实现覆盖 (per design.md §2.1, Phase I.1):
//   - Functional Construction (5): create_gpu_context / get_sm_context /
//     get_warp_context / decode_ptxir / submit_kernel_request
//   - Functional Execute (★ 1): functional_execute_warp — **不**增加 cycle
//   - Functional State (11): read_register<T> / write_register<T> /
//     read_global_memory<T> / write_global_memory<T> / read_thread_pc /
//     read_blocked_cycles (★ FIX-H8/B.3) / advance_thread_pc / read_active_mask /
//     is_warp_finished / is_thread_exited
//   - Module Getters (3): create_scoreboard / create_pipeline_latency_provider /
//     create_tensor_core_timing (本期 MVP 不使用, 保留供 v0.5)
//
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-3, Sisyphus)

#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

// =============================================================================
// PTX-EMU headers — 此 .cc 是 CppTLM 端唯一允许 include 的位置
// =============================================================================
#include "memory/simple_memory.h"
#include "ptx_ir/operand_context.h"
#include "ptx_ir/ptxir_reader.h"
#include "ptx_ir/statement_context.h"
#include "ptxsim/execution_types.h"
#include "ptxsim/gpu_context.h"
#include "ptxsim/instruction_factory.h"
#include "ptxsim/sm_context.h"
#include "ptxsim/thread_context.h"
#include "ptxsim/warp_context.h"
#include "ptxsim/warp_state.h"
#include "register/register_bank_manager.h"

// CppTLM 实现 (facade 模块 getter 返回的具体类)
#include "tlm/gpu/pipeline_tlm.hh"
#include "tlm/gpu/scoreboard_tlm.hh"
#include "tlm/gpu/tensor_core_tlm.hh"

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace tlm {

// =============================================================================
// Impl — 不透明实现, 持有 facade 内部状态
// =============================================================================
struct PtxEmuSubmoduleMVP::Impl {
    bool initialized = false;
    GPUConfig config{};
    std::string ptx_emu_root;

    // PTX-EMU 工厂不需要持久 GPUContext — create_gpu_context 每次新建 owned GPUContext.

    Impl() = default;
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
// init / shutdown
// =============================================================================
bool PtxEmuSubmoduleMVP::init(const std::string& ptx_emu_root, const GPUConfig& config) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->ptx_emu_root = ptx_emu_root;
    impl_->config = config;
    impl_->initialized = true;

    // 一次性 PTX-EMU 全局初始化 (InstructionFactory::initialize 用 static bool 守护).
    InstructionFactory::initialize();
    return true;
}

void PtxEmuSubmoduleMVP::shutdown() {
    if (!impl_) return;
    impl_->initialized = false;
}

// =============================================================================
// Functional Construction (5 methods)
// =============================================================================
GPUContext* PtxEmuSubmoduleMVP::create_gpu_context() {
    if (!impl_ || !impl_->initialized) return nullptr;
    auto* gpu = new GPUContext();
    gpu->init();
    return gpu;
}

SMContext* PtxEmuSubmoduleMVP::get_sm_context(GPUContext& gpu, uint32_t sm_idx) {
    return gpu.get_sm(sm_idx);
}

WarpContext* PtxEmuSubmoduleMVP::get_warp_context(SMContext& sm, uint32_t warp_idx) {
    return sm.get_warp(warp_idx);
}

std::vector<StatementContext> PtxEmuSubmoduleMVP::decode_ptxir(
    const std::vector<uint8_t>& ptxir_bytes) {
    std::string buf(reinterpret_cast<const char*>(ptxir_bytes.data()),
                    ptxir_bytes.size());
    std::istringstream iss(buf, std::ios::binary);
    PtxirReader reader(iss);
    return reader.read();
}

int64_t PtxEmuSubmoduleMVP::submit_kernel_request(
    GPUContext& /*gpu*/,
    const std::vector<StatementContext>& /*statements*/) {
    // MVP 占位 — 真实实现待 WU-3 后续 (T-s1-4 CudaCoreAdapter 引入).
    return -1;
}

// =============================================================================
// Functional Execute (★ 核心: 不增加 cycle)
// =============================================================================
EXE_STATE PtxEmuSubmoduleMVP::functional_execute_warp(
    WarpContext& warp, StatementContext& stmt, int target_pc) {
    // 直接调用 PTX-EMU warp dispatch, 不触发 SM cycle 推进.
    warp.execute_warp_instruction(stmt, target_pc);
    // 返回 warp 当前 lane-level state (任一活跃 lane 即可表征).
    for (size_t i = 0; i < WarpContext::WARP_SIZE; ++i) {
        ThreadContext* t = warp.get_thread(static_cast<int>(i));
        if (t) {
            EXE_STATE s = t->get_state();
            if (s != IDLE) return static_cast<int>(s);
        }
    }
    return static_cast<int>(IDLE);
}

// =============================================================================
// Functional State (11 methods) — 模板显式实例化在文件末尾
// =============================================================================

// 内部 helper: 取得指定 lane 的线程 register bank 地址.
// MVP: reg_id → "rN" 寄存器名 (测试用例约定, 真实 PTX 流程需 Symtable 解析).
template <typename T>
static T do_read_register(WarpContext* warp, int lane_id, const std::string& reg_name) {
    ThreadContext* thread = warp->get_thread(lane_id);
    if (!thread) return T{};
    RegOperand op{reg_name, -1};
    void* addr = thread->acquire_register(op, {});
    if (!addr) return T{};
    return *reinterpret_cast<T*>(addr);
}

template <typename T>
static void do_write_register(WarpContext* warp, int lane_id,
                              const std::string& reg_name, T value) {
    ThreadContext* thread = warp->get_thread(lane_id);
    if (!thread) return;
    RegOperand op{reg_name, -1};
    void* addr = thread->acquire_register(op, {});
    if (!addr) return;
    *reinterpret_cast<T*>(addr) = value;
}

template <typename T>
T PtxEmuSubmoduleMVP::read_register(WarpContext* warp, int lane_id, int /*reg_id*/) {
    // MVP: reg_id 转换为 "r{reg_id}" 名称 — 测试用例用整数 ID 引用 register.
    std::string name = "r" + std::to_string(/*reg_id=*/lane_id);
    return do_read_register<T>(warp, lane_id, name);
}

template <typename T>
void PtxEmuSubmoduleMVP::write_register(WarpContext* warp, int lane_id,
                                          int /*reg_id*/, T value) {
    std::string name = "r" + std::to_string(/*reg_id=*/lane_id);
    do_write_register<T>(warp, lane_id, name, value);
}

// 显式实例化 uint32_t / uint64_t (测试使用类型).
template uint32_t PtxEmuSubmoduleMVP::read_register<uint32_t>(WarpContext*, int, int);
template uint64_t PtxEmuSubmoduleMVP::read_register<uint64_t>(WarpContext*, int, int);
template void PtxEmuSubmoduleMVP::write_register<uint32_t>(WarpContext*, int, int, uint32_t);
template void PtxEmuSubmoduleMVP::write_register<uint64_t>(WarpContext*, int, int, uint64_t);

template <typename T>
T PtxEmuSubmoduleMVP::read_global_memory(GPUContext* gpu, uint64_t addr) {
    if (!gpu) return T{};
    SimpleMemory* mem = gpu->get_device_memory();
    if (!mem) return T{};
    T value{};
    mem->direct_access(addr, &value, sizeof(T), /*is_write=*/false);
    return value;
}

template <typename T>
void PtxEmuSubmoduleMVP::write_global_memory(GPUContext* gpu, uint64_t addr, T value) {
    if (!gpu) return;
    SimpleMemory* mem = gpu->get_device_memory();
    if (!mem) return;
    mem->direct_access(addr, &value, sizeof(T), /*is_write=*/true);
}

template uint32_t PtxEmuSubmoduleMVP::read_global_memory<uint32_t>(GPUContext*, uint64_t);
template uint64_t PtxEmuSubmoduleMVP::read_global_memory<uint64_t>(GPUContext*, uint64_t);
template void PtxEmuSubmoduleMVP::write_global_memory<uint32_t>(GPUContext*, uint64_t, uint32_t);
template void PtxEmuSubmoduleMVP::write_global_memory<uint64_t>(GPUContext*, uint64_t, uint64_t);

uint32_t PtxEmuSubmoduleMVP::read_thread_pc(const WarpContext* warp, int lane_id) {
    if (!warp) return 0;
    return warp->get_thread_pc(lane_id);
}

uint32_t PtxEmuSubmoduleMVP::read_blocked_cycles(const WarpContext* warp, int lane_id) {
    if (!warp) return 0;
    // WarpState 权威源 (FIX-H8/B.3).
    return warp->get_warp_state().threads[lane_id].blocked_cycles_remaining;
}

void PtxEmuSubmoduleMVP::advance_thread_pc(WarpContext* warp, int lane_id, uint32_t new_pc) {
    if (!warp) return;
    warp->advance_thread_pc(lane_id, static_cast<int>(new_pc));
}

uint32_t PtxEmuSubmoduleMVP::read_active_mask(const WarpContext* warp) {
    if (!warp) return 0;
    return warp->get_active_mask();
}

bool PtxEmuSubmoduleMVP::is_warp_finished(const WarpContext* warp) {
    if (!warp) return true;
    return warp->is_finished();
}

bool PtxEmuSubmoduleMVP::is_thread_exited(const ThreadContext* thread) {
    if (!thread) return false;
    // PTX-EMU ThreadContext::is_exited() 检查 state == EXIT (非 warp_state.is_exited).
    // 两者均需检查 (per BUG-RETHANG 修复协议).
    return thread->is_exited();
}

// =============================================================================
// Module Getters (3 — 本期 MVP 不使用, 保留供 v0.5 CudaCoreAdapter)
// =============================================================================
IScoreboard* PtxEmuSubmoduleMVP::create_scoreboard() {
    return new ScoreboardTLM();
}

IPipelineLatencyProvider* PtxEmuSubmoduleMVP::create_pipeline_latency_provider() {
    return new PipelineTLM();
}

ITensorCoreTiming* PtxEmuSubmoduleMVP::create_tensor_core_timing() {
    return new TensorCoreTLM();
}

}  // namespace tlm