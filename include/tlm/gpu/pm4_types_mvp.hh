// pm4_types_mvp.hh
// PM4 (PRECISION Micro-programming Method 4) 数据类型
// Author: CppTLM Team
// Date: 2026-08-26 (s3 commit 2026-08-28 扩展 Pm4MethodType + type 字段)
//
// 依据 UsrLinuxEmu gpfifo_translator.h:60-73 unpackPm4Header 真相源
// s3 将填充 Pm4MethodDispatch 详细字段 (grid/block/shared_mem/args_vram_addr)
#ifndef CPPTLM_PM4_TYPES_MVP_H
#define CPPTLM_PM4_TYPES_MVP_H

#include <cstdint>

namespace tlm::gpu {

    // PM4 method packet 类型枚举(per design.md §2 + Oracle P1-1,2026-08-28)
    // 4 个 method_addr ranges 对应 4 种 method type
    // UNKNOWN 是 parse_method 错误通道(per Oracle P1-2)
    enum class Pm4MethodType {
        DISPATCH_DIRECT,   // 0x4000-0x40FF — CTA 启动
        EVENT_WRITE,       // 0x4200-0x42FF — 时间戳/事件
        RELEASE_MEM,       // 0x4400-0x44FF — 显存释放
        ACQUIRE_MEM,       // 0x4500-0x45FF — 显存获取
        UNKNOWN,           // 不在 4 ranges 内 → parse_method 错误响应(不抛异常)
    };

    // NVIDIA PM4 method packet header
    // 32-bit word decomposed per UsrLinuxEmu gpfifo_translator.h unpackPm4Header
    struct Pm4MethodHeader {
        uint32_t inc : 1;            // bit 0  — Increment register flag
        uint32_t method_addr : 15;   // bits 1-15 — 32K method addresses
        uint32_t subchannel : 4;     // bits 16-19 — NVIDIA 4-bit subchannel ID
        uint32_t data_count : 4;     // bits 20-23 — payload DWORD count
        uint32_t reserved : 8;       // bits 24-31
    };
    static_assert(sizeof(Pm4MethodHeader) == 4, "Pm4MethodHeader must be 32-bit (LSB packing)");

    // Decoded PM4 method dispatch
    // s3 commit T-s3-1 新增 type 字段 + 填充语义字段(per Oracle P1-1)
    struct Pm4MethodDispatch {
        Pm4MethodType type = Pm4MethodType::UNKNOWN;
        uint16_t method_addr = 0;
        uint8_t subchannel_id = 0;
        uint8_t data_count = 0;
        // s3 T-s3-2 填充:
        //   grid_x, grid_y, grid_z (per CTA grid dimensions)
        //   block_x, block_y, block_z (per CTA block dimensions)
        //   shared_mem_bytes
        //   args_vram_addr (kernel args VRAM pointer)
    };

} // namespace tlm::gpu

#endif // CPPTLM_PM4_TYPES_MVP_H
