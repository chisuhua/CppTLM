// ptx_emu_bridge_stub.cc — PTX-EMU 独立构建模式的符号 stub (S1 / T-s1-2)
//
// 背景: ptxsim 65 文件列表不编译 cudart/ 下两个定义点:
//   1. g_cpptlm_bridge          定义于 cudart/cpptlm_bridge/PtxEmuDriverShim.cpp:10
//      (被 ptxsim/instructions/memory.cpp 引用 5 次, GLOBAL LD/ST bridge 路径)
//   2. get_gpu_clock_from_context()  实现于 cudart/cudart_sim.cpp
//      (被 utils/logger.cpp:8 引用, 日志时间戳)
//
// nullptr / 0 语义 = PTX-EMU 原生独立模式 (字节级兼容原同步路径,
// per PTX-EMU ADR-0021)。facade (T-s1-3) 之后通过 cpptlm_attach_bridge
// 或 facade 内部赋值激活, 本 stub 仅提供链接期占位。
//
// 注意: 必须与 PTX-EMU 源同库 (ptxemu_core), 保证同 archive 内符号闭合。

#include "cudart/cpptlm_bridge.h"  // CppTLM vendored (HSK-6, VERSION=2)
#include <cstddef>

// 定义点唯一性已由 cmake/PTXEmuCore.cmake 保证 (cudart/cpptlm_bridge/ 不编译)
CppTLMBridge* g_cpptlm_bridge = nullptr;

extern "C" size_t get_gpu_clock_from_context() {
    // 独立模式下无 GPUContext 时钟源; 日志显示 CLK:0。
    // T-s1-3 facade 落地后如需真实时钟, 改为弱符号 + facade 强定义覆盖。
    return 0;
}