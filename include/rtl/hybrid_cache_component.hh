// include/rtl/hybrid_cache_component.hh
// HybridCacheComponent: 纯 CppHDL RTL Cache 组件声明（C++20）
// 功能描述：声明 RTL 混合 Cache 组件的端口接口，由 CppHDL device 实例化。
//           端口全部为标量 ch_in/ch_out（不使用 ch_stream<> 作为 IO 字段，
//           原因：CppHDL 当前 AST/调度对 ch_stream 作为 Component 端口的
//           边沿采样支持不完整；标量端口兼容 ch_process / ch_when 调度）。
//           桥接逻辑由 src/rtl/hybrid_cache_component.cc 的 describe() 实现。
// 作者 CppTLM Team
// 日期 2026-06-06
#ifndef RTL_HYBRID_CACHE_COMPONENT_HH
#define RTL_HYBRID_CACHE_COMPONENT_HH

#include "ch.hpp"  // CppHDL 头文件入口：带入 ch::Component、ch::core::ch_uint、ch::core::ch_bool
// 显式不引入任何 CppTLM 头文件：保持本组件为纯 CppHDL RTL 头，
// 由 HybridCacheWrapper::Impl 通过 PIMPL 在 .cc 中按需 include。

namespace cpptlm {
namespace rtl {

// 简化命名空间引用：ch_uint / ch_bool 来自 ch::core，trailing underscore
// 的端口字段名直接使用短类型名以提升可读性。
using namespace ch::core;

/**
 * @brief 混合 Cache RTL 组件（CppHDL Component，纯 C++20）
 *
 * 设计要点：
 * 1. 端口全部为标量 ch_in/ch_out<ch_uint<N>> 或 ch_bool，避免 ch_stream 作为
 *    Component 顶层 IO（CppHDL 当前 AST 对 ch_stream 端口的边沿采样不完整）。
 * 2. 端口命名风格：trailing underscore（req_addr_ / req_ready_），与 CppTLM
 *    tlm/ 目录的成员变量命名约定保持一致。
 * 3. 单拍 FSM stub 由 describe() 在 .cc 中实现，本头只声明接口。
 *
 * 继承关系：
 *   ch::Component
 *     └── HybridCacheComponent
 *
 * 桥接关系（位于 .cc 端）：
 *   HybridCacheWrapper（PIMPL，TLM ChStreamModuleBase）
 *     └── HybridCacheWrapperImpl
 *           └── HybridCacheComponent（本类）
 *                 ↕ 标量端口桥接到 CacheReqBundle / CacheRespBundle
 *
 * 端口清单（15 个）：
 *   请求通道（输入：TLM→RTL）：9 个
 *     req_addr_/req_tid_/req_data_/req_opcode_/req_valid_/
 *     req_first_/req_last_/req_fragment_id_/req_fragment_total_
 *   请求 ready（输出：RTL→TLM）：1 个 req_ready_
 *   响应通道（输出：RTL→TLM）：4 个 resp_tid_/resp_data_/resp_hit_/resp_valid_
 *   响应 ready（输入：TLM→RTL）：1 个 resp_ready_
 */
class HybridCacheComponent : public ch::Component {
public:
    // ==== 请求通道（输入：TLM→RTL）====
    ch_in<ch_uint<64>> req_addr_;            // 请求地址（64-bit AXI 风格）
    ch_in<ch_uint<32>> req_tid_;             // 事务 ID（来自 TransactionContextExt）
    ch_in<ch_uint<64>> req_data_;            // 写数据首 8 字节（读时忽略）
    ch_in<ch_uint<8>>  req_opcode_;          // 操作码：0=RD 1=WR
    ch_in<ch_bool>     req_valid_;           // 请求有效（握手触发）
    ch_in<ch_bool>     req_first_;           // 首拍（fragment_id==0）
    ch_in<ch_bool>     req_last_;            // 末拍（fragment_id==fragment_total-1）
    ch_in<ch_uint<8>>  req_fragment_id_;     // 当前拍序号
    ch_in<ch_uint<8>>  req_fragment_total_;  // 总拍数

    // ==== 请求 ready（输出：RTL→TLM）====
    ch_out<ch_bool>    req_ready_;           // RTL 已接收请求

    // ==== 响应通道（输出：RTL→TLM）====
    ch_out<ch_uint<32>> resp_tid_;           // 响应事务 ID（与请求 tid 对齐）
    ch_out<ch_uint<64>> resp_data_;          // 读数据（命中时有效）
    ch_out<ch_bool>    resp_hit_;            // 命中标志（1=hit, 0=miss）
    ch_out<ch_bool>    resp_valid_;          // 响应有效（握手触发）

    // ==== 响应 ready（输入：TLM→RTL）====
    ch_in<ch_bool>     resp_ready_;          // TLM 已接收响应

    /**
     * @brief 构造函数
     *
     * @param parent 父组件指针（顶层时传 nullptr）
     * @param name   组件实例名（用于层次化命名、节点 ID 与日志）
     */
    explicit HybridCacheComponent(ch::Component* parent = nullptr,
                                  const std::string& name = "hybrid_cache")
        : ch::Component(parent, name) {}

    /**
     * @brief 创建端口（无操作：端口为直接成员字段）
     *
     * 端口在构造函数初始化阶段完成构造（ch_device 持有活跃的 ch::core::context，
     * 成员的 ch_in/ch_out 默认构造函数会调用 ctx->create_input/create_output）。
     * 此方法保留 override 以满足 ch::Component 接口契约。
     */
    void create_ports() override {
        // 端口为直接成员字段，无需 placement-new 构造 io_type 存储区。
    }

    /**
     * @brief 描述 RTL 行为（单拍 FSM stub）
     *
     * 实现位于 src/rtl/hybrid_cache_component.cc：包含 idle/lookup/respond
     * 三态 FSM 与 hit/miss 决策，命中时回写 {tid, data, hit=1}。
     * 当前 stub 实现将所有请求视为 hit，回写 req_addr_ 作为 data。
     */
    void describe() override;
};

}  // namespace rtl
}  // namespace cpptlm

#endif  // RTL_HYBRID_CACHE_COMPONENT_HH
