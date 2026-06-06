// src/rtl/hybrid_cache_component.cc
// HybridCacheComponent describe() FSM 实现（C++20）
// 功能描述：实现 RTL 混合 Cache 组件的硬件行为（单拍 FSM stub）。
//           2 状态机：IDLE → PROCESS → IDLE，所有请求视为 hit 并回写 data。
//           端口全部为标量 ch_in/ch_out，避免 ch_stream 作为 Component 顶层 IO。
// 作者 CppTLM Team
// 日期 2026-06-06

// =============================================================================
// 公共头文件：HybridCacheComponent 类声明
// 注：头文件已使用 using namespace ch::core; 简化 ch_uint/ch_bool 访问
// =============================================================================
#include "rtl/hybrid_cache_component.hh"

namespace cpptlm {
namespace rtl {

/**
 * @brief HybridCacheComponent describe() 实现
 *
 * 状态机：
 *   state=0 (IDLE)   : 等待 req_valid_，就绪时锁存请求字段并转 PROCESS
 *   state=1 (PROCESS): 输出 resp_valid_，等待 resp_ready_，完成后回 IDLE
 *
 * 端口协议（AXI4-like 简化）：
 *   请求握手：req_valid_ & req_ready_ → 锁存（单拍）
 *   响应握手：resp_valid_ & resp_ready_ → 完成
 *
 * Spike scope 行为：
 *   - 单拍处理（fragment_total=1，first=last=1）
 *   - 总是 hit（resp_hit_=1）
 *   - data 回写 = req_addr_（Spike：地址作为数据回显）
 *
 * 安全性：
 *   - 所有响应端口默认安全值（PROCESS 状态外为 0/false）
 *   - 非法状态默认回 IDLE（state->next = 0）
 */
void HybridCacheComponent::describe() {
    // === 2 状态机寄存器 ===
    // 编码：state=0 为 IDLE，state=1 为 PROCESS
    // 使用 ch_uint<2> 而非 ch_bool 是为未来扩展预留（2 bit 最多支持 4 态）
    ch_reg<ch_uint<2>> state(ch_uint<2>(0_d));

    // === 当前状态判定（用于组合逻辑分支）===
    auto is_idle    = (state == ch_uint<2>(0_d));
    auto is_process = (state == ch_uint<2>(1_d));

    // === 握手信号：req_ready_（输出）===
    // 组合逻辑驱动：仅 IDLE 状态 ready（不接受新请求时拉低）
    // 注意：必须用组合逻辑赋值（state 当前值），不能用 next 状态
    req_ready_ = is_idle;

    // === 状态转移逻辑 ===
    // 触发 1：accept（IDLE + req_valid_）→ 进入 PROCESS
    // 触发 2：done （PROCESS + resp_ready_）→ 回到 IDLE
    // 默认：保持当前状态
    auto accept = is_idle & req_valid_;
    auto done   = is_process & resp_ready_;

    // select 嵌套：accept 优先 → 1；否则若 done → 0；否则保持 state
    // 注意：state 通过 ch_reg<ch_uint<2>> 隐式转换为 lnode<ch_uint<2>>
    state->next = select(accept, ch_uint<2>(1_d),
                    select(done,   ch_uint<2>(0_d),
                                     ch_uint<2>(0_d)));

    // === 响应端口驱动（PROCESS 状态有效）===
    // 安全默认值：IDLE 状态下所有响应端口拉零/无效
    resp_valid_ = is_process;
    resp_tid_   = select(is_process, req_tid_,   ch_uint<32>(0_d));
    resp_data_  = select(is_process, req_addr_,  ch_uint<64>(0_d));  // Spike: 回显地址
    resp_hit_   = is_process;                                          // Spike: 总是 hit
}

}  // namespace rtl
}  // namespace cpptlm
