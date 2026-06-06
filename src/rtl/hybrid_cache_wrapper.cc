// src/rtl/hybrid_cache_wrapper.cc
// HybridCacheWrapper PIMPL 实现（C++20）
// 功能描述：将 CppHDL HybridCacheComponent 包装为 CppTLM TLM 模块。
//           内部通过 PIMPL(unique_ptr<Impl>) 隔离 CppHDL/AST 依赖，
//           公共头文件（include/rtl/hybrid_cache_wrapper.hh）保持纯 C++17。
//           业务侧使用 cpptlm::InputStreamAdapter/OutputStreamAdapter
//           与 CppTLM 框架通信，内部通过 FragmentMapper 与 RTL 端口桥接。
// 作者 CppTLM Team
// 日期 2026-06-06

// =============================================================================
// 公共头文件（C++17 兼容，保持轻量）
// =============================================================================
#include "rtl/hybrid_cache_wrapper.hh"
#include "rtl/hybrid_cache_component.hh"
#include "rtl/fragment_mapper.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "core/packet_pool.hh"
#include "core/packet.hh"
#include "ext/transaction_context_ext.hh"

// =============================================================================
// PIMPL 内部使用的 CppHDL 依赖（仅在 .cc 可见）
// =============================================================================
#include "ch.hpp"
#include "simulator.h"

#include <cstring>
#include <cstdint>
#include <memory>
#include <string>

// =============================================================================
// PIMPL 实现类：完整定义隐藏在 .cc 中
// 设计动机：include/rtl/hybrid_cache_wrapper.hh 不应暴露 CppHDL 头，
//          否则所有引用该头文件的 TU（含 C++17 模块）都被迫引入 C++20/AST。
// =============================================================================
class HybridCacheWrapperImpl {
public:
    // CppHDL 设备 + 仿真器：HybridCacheComponent 在 cpptlm::rtl 命名空间
    ch::ch_device<cpptlm::rtl::HybridCacheComponent> device_;
    ch::Simulator                                   simulator_;

    // TLM 适配器指针（由 HybridCacheWrapper::tick() 每周期更新）
    // 不持有所有权，wrapper 是真正的所有者
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>*   req_in_  = nullptr;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>* resp_out_ = nullptr;

    // Spike scope 状态：单拍请求跟踪
    // 多拍时由 FragmentMapper 维护 fragment_id/total（暂不实现）
    bool     has_pending_       = false;
    uint64_t pending_tid_        = 0;
    uint64_t pending_addr_       = 0;
    uint64_t pending_data_       = 0;
    bool     pending_is_write_   = false;

    // 构造：创建设备 + 仿真器
    HybridCacheWrapperImpl()
        : device_(),
          simulator_(device_.context(), false /*trace_on*/) {
        // Spike scope：单拍、无 trace 收集
    }

    // 析构：PIMPL 默认即可（device_/simulator_ 通过 unique_ptr 链式析构）
    ~HybridCacheWrapperImpl() = default;

    // 禁用拷贝/移动（持有 CppHDL 上下文，地址敏感）
    HybridCacheWrapperImpl(const HybridCacheWrapperImpl&) = delete;
    HybridCacheWrapperImpl& operator=(const HybridCacheWrapperImpl&) = delete;
    HybridCacheWrapperImpl(HybridCacheWrapperImpl&&) = delete;
    HybridCacheWrapperImpl& operator=(HybridCacheWrapperImpl&&) = delete;

    // 重置设备 + 仿真器
    void reset() {
        // 仿真器 reset 重新初始化所有 reg 与 port 默认值
        simulator_.reset();
        has_pending_ = false;
    }

    // 主推进：TLM → RTL → TLM 三阶段（Spike scope 单拍）
    void tick() {
        if (!req_in_ || !resp_out_) return;

        auto& comp = device_.instance();

        // ========== 阶段 1：读取 TLM 请求 ==========
        // 仅在没有待处理请求且上游有 valid 时启动一次桥接
        if (!has_pending_ && req_in_->valid() && req_in_->ready()) {
            const auto& req = req_in_->data();

            // 从 CacheReqBundle 提取字段（ch_uint<W> 需调用 .read()）
            pending_tid_      = req.transaction_id.read();
            pending_addr_     = req.address.read();
            pending_data_     = req.data.read();
            pending_is_write_ = req.is_write.read();
            has_pending_      = true;

            // 驱动 RTL 请求端口
            // 注：使用 simulator_.set_value() 写入输入端口
            //     ch_reg/ch_in 的 operator= 在编译期 static_assert 拒绝写入
            simulator_.set_value(comp.req_addr_,           pending_addr_);
            simulator_.set_value(comp.req_tid_,            static_cast<uint32_t>(pending_tid_ & 0xFFFFFFFFu));
            simulator_.set_value(comp.req_data_,           pending_data_);
            simulator_.set_value(comp.req_opcode_,         pending_is_write_ ? 1u : 0u);
            simulator_.set_value(comp.req_valid_,          1u);
            simulator_.set_value(comp.req_first_,          1u);
            simulator_.set_value(comp.req_last_,           1u);
            simulator_.set_value(comp.req_fragment_id_,    0u);
            simulator_.set_value(comp.req_fragment_total_, 1u);

            // ========== 阶段 2：推进 RTL 仿真（IDLE → PROCESS） ==========
            // Cycle 1：IDLE 状态下检测到 valid&ready，锁存字段，state 切到 1
            simulator_.tick();

            // 关闭 req_valid（FSM 进入 PROCESS 后不再关心）
            simulator_.set_value(comp.req_valid_, 0u);
            // 拉高 resp_ready 让 FSM 在下一拍回到 IDLE
            simulator_.set_value(comp.resp_ready_, 1u);

            // Cycle 2：PROCESS 状态下输出响应 valid，若 resp_ready 则 state 回到 0
            simulator_.tick();

            // ========== 阶段 3：读取 RTL 响应 ==========
            // sdata_type 通过 operator uint64_t() 转值
            const uint64_t resp_tid_v   = static_cast<uint64_t>(simulator_.get_value(comp.resp_tid_));
            const uint64_t resp_data_v  = static_cast<uint64_t>(simulator_.get_value(comp.resp_data_));
            const uint64_t resp_hit_v   = static_cast<uint64_t>(simulator_.get_value(comp.resp_hit_));
            const uint64_t resp_valid_v = static_cast<uint64_t>(simulator_.get_value(comp.resp_valid_));

            // 释放 resp_ready（FSM 已回 IDLE）
            simulator_.set_value(comp.resp_ready_, 0u);

            // 仅当 RTL 端 valid 时才产生 TLM 响应（避免脏数据）
            if (resp_valid_v != 0u) {
                // 构造响应 Bundle（ch_uint/ch_bool 通过 .write() 写入）
                bundles::CacheRespBundle resp;
                resp.transaction_id.write(resp_tid_v);
                resp.data.write(resp_data_v);
                resp.is_hit.write(resp_hit_v);
                resp.error_code.write(0);

                // 输出到 TLM 下游（设置 OutputStreamAdapter valid）
                resp_out_->write(resp);
            }

            // 消费 TLM 输入（清除 InputStreamAdapter valid）
            req_in_->consume();
            has_pending_ = false;
        }
    }
};

// =============================================================================
// HybridCacheWrapper 非内联方法定义
// 头文件（hybrid_cache_wrapper.hh）声明为 C++17 接口，
// 实现位于 .cc 并启用 C++20 特性。
// =============================================================================

// 构造：初始化 ChStreamModuleBase + 创建 PIMPL 实例
HybridCacheWrapper::HybridCacheWrapper(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq),
      impl_(std::make_unique<HybridCacheWrapperImpl>()) {
    // PIMPL 内部已完成 device_/simulator_ 的构建（ch_device 构造时调用 describe()）
}

// 析构：PIMPL 自动通过 unique_ptr 释放
// 必须在 .cc 中定义（编译器需要 HybridCacheWrapperImpl 完整类型以析构 unique_ptr）
HybridCacheWrapper::~HybridCacheWrapper() = default;

// 保存 StreamAdapter 注入句柄（ModuleFactory Step 7 调用）
void HybridCacheWrapper::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    adapter_ = adapter;
}

// 主推进：每周期同步适配器引用并委托给 PIMPL
void HybridCacheWrapper::tick() {
    if (!impl_) return;
    // 同步适配器引用（指针赋值，开销可忽略）
    impl_->req_in_  = &req_in_;
    impl_->resp_out_ = &resp_out_;
    // PIMPL 推进
    impl_->tick();
    // 委托适配器 tick（输出方向数据搬运）
    if (adapter_) adapter_->tick();
}

// 重置：委托给 PIMPL + 重置适配器 + 调用基类
void HybridCacheWrapper::do_reset(const ResetConfig& cfg) {
    if (impl_) impl_->reset();
    req_in_.reset();
    resp_out_.reset();
    // 调用基类重置（SimObject::do_reset 处理端口/状态机重置）
    SimObject::do_reset(cfg);
}
