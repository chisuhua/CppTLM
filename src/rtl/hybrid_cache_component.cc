// src/rtl/hybrid_cache_component.cc
// HybridCacheComponent::describe() 实现（C++20）
// 功能描述：单拍 Cache FSM (IDLE → PROCESS → IDLE)。
//           describe() 直接访问 io().req_in.payload.* 字段。
//           端口为 ch_stream<CacheReqBundleRTL/RespBundleRTL>。
// 作者 CppTLM Team / 日期 2026-06-07
#include "rtl/hybrid_cache_component.hh"

namespace cpptlm {
namespace rtl {

using namespace ch::core;
using namespace ch::core::literals;

void HybridCacheComponent::describe() {
    // === FSM 状态寄存器 ===
    ch_reg<ch_uint<2>> state(0_d);  // 0=IDLE, 1=PROCESS

    // === 跨拍锁存变量（首拍 → 响应使用）===
    ch_reg<ch_uint<64>> saved_tid(0_d);
    ch_reg<ch_uint<64>> saved_parent(0_d);
    ch_reg<ch_uint<8>>  saved_frag_id(0_d);
    ch_reg<ch_uint<8>>  saved_frag_total(0_d);
    ch_reg<ch_uint<64>> saved_addr(0_d);
    ch_reg<ch_bool>     saved_is_write(ch_bool(false));

    // === 组合逻辑: ready 信号（仅 IDLE 接受新请求）===
    io().req_in.ready = (state == 0_d);

    // === 主 FSM ===
    switch (static_cast<uint64_t>(state)) {
        case 0:  // IDLE: 等待 req
            if (io().req_in.valid && io().req_in.ready) {
                // 直接 Bundle 访问 - 无字段解包
                saved_tid        = io().req_in.payload.transaction_id;
                saved_parent     = io().req_in.payload.parent_id;
                saved_frag_id    = io().req_in.payload.fragment_id;
                saved_frag_total = io().req_in.payload.fragment_total;
                saved_addr       = io().req_in.payload.address;
                saved_is_write   = io().req_in.payload.is_write;
                state = 1_d;
            }
            break;

        case 1:  // PROCESS: 模拟 Cache 查找（单周期完成）
            // 生成响应
            io().resp_out.payload.transaction_id = saved_tid;
            io().resp_out.payload.parent_id      = saved_parent;
            io().resp_out.payload.fragment_id    = saved_frag_id;
            io().resp_out.payload.fragment_total = saved_frag_total;
            io().resp_out.payload.data           = saved_addr;  // Spike: echo addr 作为 data
            io().resp_out.payload.is_hit         = ch_bool(true);
            io().resp_out.payload.error_code     = 0_d;
            io().resp_out.payload.first          = ch_bool(true);  // 单拍
            io().resp_out.payload.last           = ch_bool(true);
            io().resp_out.valid = ch_bool(true);

            // 等待下游 ready，然后回到 IDLE
            if (io().resp_out.ready) {
                io().resp_out.valid = ch_bool(false);
                state = 0_d;
            }
            break;

        default:
            // 非法状态：复位到 IDLE
            state = 0_d;
            break;
    }
}

} // namespace rtl
} // namespace cpptlm
