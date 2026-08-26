// pm4_decoder_mvp.hh
// PM4 Decoder 纯接口头 - s3 填充具体 Pm4Decoder 实现
// Author: CppTLM Team
// Date: 2026-08-26
//
// 依据 Phase F-H.3 路径 3: GPFIFO 外壳 + PM4 嵌入式 (NVIDIA method packet)
// s3 实现 4 个 method_addr ranges:
//   - 0x4000-0x40FF: DISPATCH_DIRECT
//   - 0x4200-0x42FF: EVENT_WRITE
//   - 0x4400-0x44FF: RELEASE_MEM
//   - 0x4500-0x45FF: ACQUIRE_MEM
#ifndef CPPTLM_PM4_DECODER_MVP_H
#define CPPTLM_PM4_DECODER_MVP_H

#include "tlm/gpu/pm4_types_mvp.hh"
#include <cstdint>
#include <memory>

namespace tlm::gpu {

    // 纯虚接口 — s3 实现具体 Pm4Decoder
    class Pm4DecoderInterface {
    public:
        virtual ~Pm4DecoderInterface() = default;

        // 解析单个 PM4 method packet
        // @param method_header 32-bit header word (per Pm4MethodHeader bit layout)
        // @param payload pointer to additional DWORD data
        // @param max_dwords upper bound on payload size (for safety)
        // @return parsed dispatch info (Pm4MethodDispatch)
        virtual Pm4MethodDispatch parse_method(
            uint32_t method_header,
            const uint32_t* payload,
            uint32_t max_dwords) = 0;
    };

    // s3 可选添加具体类:
    // class Pm4Decoder : public Pm4DecoderInterface {
    //     Pm4MethodDispatch parse_method(uint32_t, const uint32_t*, uint32_t) override;
    // };

} // namespace tlm::gpu

#endif // CPPTLM_PM4_DECODER_MVP_H
