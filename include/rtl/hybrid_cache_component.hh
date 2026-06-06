// include/rtl/hybrid_cache_component.hh
// HybridCacheComponent: CppHDL RTL 组件（C++20）
// 功能描述：混合缓存 RTL 组件，使用 ch_stream<BundleT> 接口（canonical pattern）。
//           遵循 v2.1 §4.1 + example_rtl_modules.md:20-66。
//           单拍 FSM stub（IDLE → PROCESS → RESPOND）。
// 作者 CppTLM Team / 日期 2026-06-07
#ifndef RTL_HYBRID_CACHE_COMPONENT_HH
#define RTL_HYBRID_CACHE_COMPONENT_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "bundles/cache_bundles_rtl.hh"

namespace cpptlm {
namespace rtl {

using namespace ch::core;  // ch_uint/ch_bool/ch_reg/ch_stream
using namespace ch::core::literals;  // UDL _d, _b

/**
 * @brief 混合缓存 RTL 组件（ch_stream<BundleT> 接口）
 *
 * 设计原则（v2.1 §4.1 + example_rtl_modules.md canonical pattern）：
 *   - 2 个 ch_stream<Bundle> 端口（不是 15 个标量端口）
 *   - Bundle 自带 fragment 元数据（parent_id/fragment_id/fragment_total）
 *   - Bundle 自带 first/last 标志（响应侧）
 *   - 跨拍状态用 ch_reg<> 锁存
 *
 * 状态机（2 态）：
 *   IDLE    : 等待 req_in.valid && req_in.ready，锁存首拍数据
 *   PROCESS : 输出 resp_out.valid，等待 resp_out.ready
 *
 * Spike 范围：单拍、总是 hit、echo addr 作为 data。
 * Day 2+ 扩展：多拍 fragment、错误处理、JIT cycle 对齐。
 */
class HybridCacheComponent : public ch::Component {
public:
    __io(
        ch_stream<bundles::CacheReqBundleRTL>  req_in;
        ch_stream<bundles::CacheRespBundleRTL> resp_out;
    );

    HybridCacheComponent(ch::Component* parent = nullptr,
                       const std::string& name = "hybrid_cache")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (this->io_storage_) io_type;
    }

    void describe() override;
};

} // namespace rtl
} // namespace cpptlm

#endif // RTL_HYBRID_CACHE_COMPONENT_HH
