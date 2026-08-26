// pm4_types_mvp.hh
// PM4 (PRECISION Micro-programming Method 4) 数据类型
// Author: CppTLM Team
// Date: 2026-08-26
//
// 依据 UsrLinuxEmu gpfifo_translator.h:60-73 unpackPm4Header 真相源
// s3 将填充 Pm4MethodDispatch 详细字段 (grid/block/shared_mem/args_vram_addr)
#ifndef CPPTLM_PM4_TYPES_MVP_H
#define CPPTLM_PM4_TYPES_MVP_H

#include <cstdint>

namespace tlm::gpu {

    // NVIDIA PM4 method packet header
    // 32-bit word decomposed per UsrLinuxEmu gpfifo_translator.h unpackPm4Header
    struct Pm4MethodHeader {
        uint32_t inc : 1;            // bit 0  — Increment register flag
        uint32_t method_addr : 15;   // bits 1-15 — 32K method addresses
        uint32_t subchannel : 4;     // bits 16-19 — NVIDIA 4-bit subchannel ID
        uint32_t data_count : 4;     // bits 20-23 — payload DWORD count
        uint32_t reserved : 8;       // bits 24-31
    };

    // Decoded PM4 method dispatch (s3 fills semantic fields)
    struct Pm4MethodDispatch {
        uint16_t method_addr = 0;
        uint8_t subchannel_id = 0;
        uint8_t data_count = 0;
        // s3 will add:
        //   grid_x, grid_y, grid_z (per CTA grid dimensions)
        //   block_x, block_y, block_z (per CTA block dimensions)
        //   shared_mem_bytes
        //   args_vram_addr (kernel args VRAM pointer)
    };

} // namespace tlm::gpu

#endif // CPPTLM_PM4_TYPES_MVP_H
