// include/tlm/memory_tlm.hh
// MemoryTLM：基于 ch_stream 语义的简化 Memory 模块（v2.1 新式模型）
// 功能描述：作为 CacheTLM 下游模块，接收请求后返回模拟响应
//           用于验证 StreamAdapter 完整数据通路
// 作者 CppTLM Team
// 日期 2026-04-12
#ifndef TLM_MEMORY_TLM_HH
#define TLM_MEMORY_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles.hh"
#include <cstdint>

/**
 * @brief Memory TLM 模块（简化下游模型）
 * 
 * 设计原则：
 * - 与 CacheTLM 使用相同的通信协议（CacheReqBundle/CacheRespBundle）
 * - 不实现真实延迟模型（Phase 4 完善）
 * - 固定返回模拟数据 0xDEADBEEF
 * 
 * JSON 注册名："MemoryTLM"
 */
class MemoryTLM : public ChStreamModuleBase {
private:
    bundles::InputStreamAdapter<bundles::CacheReqBundle>  req_in_;
    bundles::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;

public:
    MemoryTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

    ~MemoryTLM() override = default;

    std::string get_module_type() const override { return "MemoryTLM"; }

    void set_stream_adapter(StreamAdapterBase* /*adapter*/) override {}

    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();

            bundles::CacheRespBundle resp;
            resp.transaction_id.write(req.transaction_id.read());
            resp.data.write(0xDEADBEEF);  // 模拟内存数据
            resp.is_hit.write(0);          // Memory 总是 miss（无缓存）
            resp.error_code.write(0);

            resp_out_.write(resp);
            req_in_.consume();
        }
    }

    void do_reset(const ResetConfig& /*config*/) override {
        req_in_.reset();
        resp_out_.reset();
    }

    bundles::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        return req_in_;
    }
    bundles::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        return resp_out_;
    }
};

#endif // TLM_MEMORY_TLM_HH
