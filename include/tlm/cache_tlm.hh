// include/tlm/cache_tlm.hh
// CacheTLM：基于 ch_stream 语义的 Cache 模块（v2.1 新式模型）
// 功能描述：使用 InputStreamAdapter/OutputStreamAdapter 进行模块内部通信，
//           框架层通过 StreamAdapter 自动暴露为 Port
// 作者 CppTLM Team
// 日期 2026-04-12
#ifndef TLM_CACHE_TLM_HH
#define TLM_CACHE_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include <map>
#include <cstdint>

/**
 * @brief Cache TLM 模块（新式 ch_stream 内部模型）
 * 
 * 继承关系：
 *   SimObject
 *   └── ChStreamModuleBase
 *       └── CacheTLM
 * 
 * 设计原则：
 * - 模块内部使用 cpptlm::InputStreamAdapter/OutputStreamAdapter（ch_stream 语义）
 * - 框架层通过 StreamAdapter 自动转换为 MasterPort/SlavePort
 * - 业务逻辑不感知外部 Port 的存在
 * 
 * JSON 注册名："CacheTLM"
 */
class CacheTLM : public ChStreamModuleBase {
private:
    // 输入/输出适配器（提供 ch_stream valid/ready 语义）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>  req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;

    // 业务状态
    std::map<uint64_t, uint64_t> cache_lines_;

public:
    CacheTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

    ~CacheTLM() override = default;

    std::string get_module_type() const override { return "CacheTLM"; }

    // ChStreamModuleBase 接口
    void set_stream_adapter(StreamAdapterBase* /*adapter*/) override {
        // StreamAdapter 引用，当前由框架管理生命周期
    }

    // 模块业务逻辑
    void tick() override {
        // ch_stream 握手：valid && ready 时处理
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();

            uint64_t addr = req.address.read();
            bool is_write = req.is_write.read();

            // 缓存查找
            bool hit = cache_lines_.count(addr) > 0;

            if (is_write) {
                cache_lines_[addr] = req.data.read();
            }

            // 构建响应
            bundles::CacheRespBundle resp;
            resp.transaction_id.write(req.transaction_id.read());
            resp.data.write(hit ? cache_lines_[addr] : 0);
            resp.is_hit.write(hit ? 1 : 0);
            resp.error_code.write(0);

            resp_out_.write(resp);
            req_in_.consume(); // 握手完成，清除 valid
        }
    }

    void do_reset(const ResetConfig& /*config*/) override {
        cache_lines_.clear();
        req_in_.reset();
        resp_out_.reset();
    }

    // 访问器（供 StreamAdapter 使用）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        return req_in_;
    }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        return resp_out_;
    }
};

#endif // TLM_CACHE_TLM_HH
