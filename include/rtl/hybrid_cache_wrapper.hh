// include/rtl/hybrid_cache_wrapper.hh
// HybridCacheWrapper：混合仿真 Cache 桥接模块（PIMPL 头文件）
// 功能描述：将外部 CppHDL 生成的 RTL Cache 实现包装为 CppTLM TLM 模块，
//           内部使用 PIMPL 模式隔离 C++20/CppHDL 依赖，公共头文件保持纯 C++17。
// 作者 CppTLM Team
// 日期 2026-06-06
#ifndef RTL_HYBRID_CACHE_WRAPPER_HH
#define RTL_HYBRID_CACHE_WRAPPER_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include <memory>
#include <string>

// 前向声明：实现类包含 CppHDL 依赖，完整定义位于 .cc 文件
class HybridCacheWrapperImpl;

/**
 * @brief 混合仿真 Cache 桥接模块（PIMPL 接口）
 *
 * 继承关系：
 *   SimObject
 *   └── ChStreamModuleBase
 *       └── HybridCacheWrapper
 *
 * 设计原则：
 * - 头文件保持纯 C++17，不暴露 CppHDL AST/运行时细节
 * - 通过 PIMPL(unique_ptr<Impl>) 将 C++20/CppHDL 依赖隔离在 .cc 中
 * - 业务侧使用 cpptlm::InputStreamAdapter/OutputStreamAdapter 与框架通信
 *
 * JSON 注册名："HybridCacheWrapper"
 */
class HybridCacheWrapper : public ChStreamModuleBase {
private:
    // 输入/输出适配器（ch_stream 语义，与 CacheTLM 保持一致）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>  req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;

    // StreamAdapter 注入句柄（ModuleFactory 在 Step 7 注入）
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // PIMPL：实现类包含 CppHDL/RTL 桥接逻辑，完整定义在 .cc 中
    std::unique_ptr<HybridCacheWrapperImpl> impl_;

public:
    explicit HybridCacheWrapper(const std::string& name, EventQueue* eq);

    // 析构函数：仅在头文件声明，由 .cc 提供定义以保证 unique_ptr<Impl> 完整类型可见
    ~HybridCacheWrapper() override;

    // 禁用拷贝与移动（PIMPL 拥有独占资源）
    HybridCacheWrapper(const HybridCacheWrapper&) = delete;
    HybridCacheWrapper& operator=(const HybridCacheWrapper&) = delete;
    HybridCacheWrapper(HybridCacheWrapper&&) = delete;
    HybridCacheWrapper& operator=(HybridCacheWrapper&&) = delete;

    std::string get_module_type() const override { return "HybridCacheWrapper"; }

    // ChStreamModuleBase 接口
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;

    // 模块业务逻辑
    void tick() override;
    void do_reset(const ResetConfig& cfg) override;

    // 访问器（供 StreamAdapter 与 PIMPL 实现使用）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() { return req_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() { return resp_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

    // PIMPL 实现访问器（仅 .cc 内部使用）
    HybridCacheWrapperImpl* impl() { return impl_.get(); }
    const HybridCacheWrapperImpl* impl() const { return impl_.get(); }
};

#endif // RTL_HYBRID_CACHE_WRAPPER_HH
