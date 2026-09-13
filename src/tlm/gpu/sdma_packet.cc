// src/tlm/gpu/sdma_packet.cc
// SdmaPacket 实现 (Stage 1.3a, SD-RB-2)
// 作者 CppTLM Team / 日期 2026-09-13
//
// 全部逻辑在 header (inline), 此 .cc 仅为 CMake 显式源文件列表完整性。

#include "tlm/gpu/sdma_packet.hh"

namespace tlm::gpu {
    // 无需独立实现; 类方法均在 header inline 定义.
    // 此文件保留是为未来扩展 (例如 SgDescriptor wire-format 序列化 helper).
}  // namespace tlm::gpu
