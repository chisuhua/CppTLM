// src/tlm/gpu/ptx_emu_driver_shim.cc
// cpptlm_set_driver ABI 入口 + 全局 g_ptx_emu_driver 管理 — P1 Phase 4 Wave 0 跟进
// 功能: PTX-EMU 端 initialize_environment() 调用 cpptlm_set_driver(shim, api)
//       传递 PtxEmuDriverShim opaque pointer 和 C-compatible vtable,
//       DriverWrapper 将其适配为 IPtxEmuDriver 供 KernelLaunchTLM::tick() 使用
// 作者 CppTLM Team / 日期 2026-07-18
// 参考:
//   - include/tlm/gpu/ptx_emu_driver.hh (PtxEmuDriverApi + DriverWrapper + IPtxEmuDriver)
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §4.0.4
//   - openspec/changes/cpptlm-p1-ptxemu-shim (PTX-EMU 姊妹 change, PtxEmuDriverShim)
//
// ABI 方向: PTX-EMU → CppTLM (cpptlm_attach_bridge 的反向)
//   PTX-EMU: initialize_environment() 创建 PtxEmuDriverShim → cpptlm_set_driver(shim, api)
//   CppTLM: 存储 → DriverWrapper 包装 → 设置 g_ptx_emu_driver 全局指针
//
// cpptlm_set_driver 是 extern "C" 符号, 在 libcpptlm_cudart.so 中可见。
// 调用时机: PTX-EMU initialize_environment() (在 g_gpu_context 创建后立即调用)
#include "tlm/gpu/ptx_emu_driver.hh"

#include <memory>
#include <cstdio>

namespace tlm {
namespace {

/// DriverWrapper 实例生命周期 (由 cpptlm_set_driver 管理)
/// ModuleFactory 和 main.cpp 中的模块通过 g_ptx_emu_driver 全局指针访问
std::unique_ptr<DriverWrapper> g_driver_wrapper;

}  // anonymous namespace

// 定义全局 g_ptx_emu_driver (声明在 ptx_emu_driver.hh)
// 初始值 nullptr = PTX-EMU 未连接, 零退化
IPtxEmuDriver* g_ptx_emu_driver = nullptr;

}  // namespace tlm

/// cpptlm_set_driver — PTX-EMU → CppTLM 驱动注册 ABI 入口
///
/// PTX-EMU 端 initialize_environment() 在创建 PtxEmuDriverShim 后调用此函数,
/// 传递 opaque shim 指针和 C-compatible vtable, CppTLM 将其包装为 IPtxEmuDriver。
///
/// @param shim_ctx  PtxEmuDriverShim 不透明指针 (non-owning)
/// @param api       C-compatible vtable (方法表)
///
/// @note 可重复调用 — 后调用替换前调用 (最后一次调用的 driver 生效)
/// @note nullptr shim_ctx 或 api 为空指针表时重置为 nullptr (安全退场)
extern "C" void cpptlm_set_driver(void* shim_ctx, tlm::PtxEmuDriverApi api) {
    if (!shim_ctx) {
        // nullptr shim → 重置驱动 (安全退场, 如 PTX-EMU 卸载)
        tlm::g_driver_wrapper.reset();
        tlm::g_ptx_emu_driver = nullptr;
        std::fprintf(stdout, "[INFO] cpptlm_set_driver: driver reset (nullptr)\n");
        return;
    }

    // 创建 DriverWrapper 包装 shim
    try {
        tlm::g_driver_wrapper = std::make_unique<tlm::DriverWrapper>(shim_ctx, api);
        tlm::g_ptx_emu_driver = tlm::g_driver_wrapper.get();
        std::fprintf(stdout,
                     "[INFO] cpptlm_set_driver: wrapped PtxEmuDriverShim @ %p into "
                     "DriverWrapper -> g_ptx_emu_driver = %p\n",
                     static_cast<void*>(shim_ctx),
                     static_cast<void*>(tlm::g_ptx_emu_driver));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[ERROR] cpptlm_set_driver: failed to create DriverWrapper: %s\n",
                     e.what());
        tlm::g_driver_wrapper.reset();
        tlm::g_ptx_emu_driver = nullptr;
    }
}