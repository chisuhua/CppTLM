// pm4_decoder_mvp.cc
// Pm4Decoder: 4 method_addr ranges 解析 (per design.md §2 + Phase F-H.3 path 3)
// Author: CppTLM Team
// Date: 2026-08-28
//
// s3 T-s3-1 实现: parse_method 把 32-bit Pm4MethodHeader 拆为 Pm4MethodDispatch
// 4 个 method_addr ranges 分类:
//   0x4000-0x40FF → DISPATCH_DIRECT
//   0x4200-0x42FF → EVENT_WRITE
//   0x4400-0x44FF → RELEASE_MEM
//   0x4500-0x45FF → ACQUIRE_MEM
// 其他 → UNKNOWN (错误通道, 不抛异常 per design §2 + Oracle P1-2)
//
// 真相源: UsrLinuxEmu gpfifo_translator.h:60-73 unpackPm4Header
//
// Pm4MethodHeader 是 32-bit bitfield (inc:1, method_addr:15, subchannel:4, data_count:4, reserved:8)
#include "tlm/gpu/pm4_decoder_mvp.hh"

#include <array>

namespace tlm::gpu {

namespace {

// 按 method_addr 高 8 bits 查表 (per design §2):
//   0x40 → DISPATCH_DIRECT, 0x42 → EVENT_WRITE,
//   0x44 → RELEASE_MEM,     0x45 → ACQUIRE_MEM, 其他 → UNKNOWN
//
// 为何用查表而非常规 if-else:GCC -O2 下 if-else 链在 UNKNOWN 路径会执行
//   "xor %edx, %edx" 清零 subchannel/data_count 占用的 ABI 寄存器,
//   导致 Pm4MethodDispatch 的 bytes 6-7 (subchannel_id/data_count) 返回 0。
//   查表所有路径走同样的 movzwl/shl/or 序列,避免寄存器复用陷阱。
constexpr std::array<Pm4MethodType, 256> make_method_type_lut() {
    std::array<Pm4MethodType, 256> lut{};
    lut.fill(Pm4MethodType::UNKNOWN);
    lut[0x40] = Pm4MethodType::DISPATCH_DIRECT;
    lut[0x42] = Pm4MethodType::EVENT_WRITE;
    lut[0x44] = Pm4MethodType::RELEASE_MEM;
    lut[0x45] = Pm4MethodType::ACQUIRE_MEM;
    return lut;
}

const auto kMethodTypeLUT = make_method_type_lut();

} // namespace

Pm4MethodDispatch Pm4Decoder::parse_method(
    uint32_t method_header,
    const uint32_t* payload,
    uint32_t max_dwords) {
    // s3 MVP: payload 暂未填充语义字段 (grid/block/shared_mem/args_vram_addr)
    // 留接口为后续 T-s3-2 CP FSM 解析 CTA descriptor 预留
    (void)payload;
    (void)max_dwords;

    const uint16_t method_addr = static_cast<uint16_t>((method_header >> 1) & 0x7FFFu);
    const uint8_t  raw_subchannel = static_cast<uint8_t>((method_header >> 16) & 0x0Fu);
    const uint8_t  raw_data_count = static_cast<uint8_t>((method_header >> 20) & 0x0Fu);
    const Pm4MethodType type = kMethodTypeLUT[method_addr >> 8];

    // UNKNOWN 错误通道: 保留 method_addr 便于调试,其他字段归零
    // 单 return + 三元表达式,确保所有字段在所有路径都被求值
    // (避免 GCC -O2 在 if/else 分支用 xor 跳过 subchannel/data_count 赋值)
    return Pm4MethodDispatch{
        type,
        method_addr,
        type == Pm4MethodType::UNKNOWN ? uint8_t{0} : raw_subchannel,
        type == Pm4MethodType::UNKNOWN ? uint8_t{0} : raw_data_count,
    };
}

} // namespace tlm::gpu
