// include/tlm/memory_tlm.hh
// MemoryTLM：基于 ch_stream 语义的简化 Memory 模块（v2.1 新式模型）
// 功能描述：作为 CacheTLM 下游模块，接收请求后返回模拟响应
//           用于验证 StreamAdapter 完整数据通路
// 作者 CppTLM Team
// 日期 2026-04-12
#ifndef TLM_MEMORY_TLM_HH
#define TLM_MEMORY_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>

class MemoryTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>  req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;

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
            resp.data.write(0xDEADBEEF);
            resp.is_hit.write(0);
            resp.error_code.write(0);
            resp_out_.write(resp);
            req_in_.consume();
        }
    }

    void do_reset(const ResetConfig& /*config*/) override {
        req_in_.reset();
        resp_out_.reset();
    }

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() { return req_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() { return resp_out_; }
};

#endif // TLM_MEMORY_TLM_HH
