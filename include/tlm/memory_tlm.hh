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
#include "metrics/stats.hh"
#include <cstdint>

class MemoryTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>  req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // 性能统计
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_requests_read_;
    tlm_stats::Scalar& stats_requests_write_;
    tlm_stats::Scalar& stats_row_hits_;
    tlm_stats::Scalar& stats_row_misses_;
    tlm_stats::Distribution& stats_latency_read_;
    tlm_stats::Distribution& stats_latency_write_;
    tlm_stats::Formula& stats_row_buffer_hit_rate_;

public:
    MemoryTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          stats_("memory"),
          stats_requests_read_(stats_.addScalar("requests_read", "Memory read requests", "count")),
          stats_requests_write_(stats_.addScalar("requests_write", "Memory write requests", "count")),
          stats_row_hits_(stats_.addScalar("row_hits", "Row buffer hits", "count")),
          stats_row_misses_(stats_.addScalar("row_misses", "Row buffer misses", "count")),
          stats_latency_read_(stats_.addDistribution("latency_read", "Memory read latency", "cycle")),
          stats_latency_write_(stats_.addDistribution("latency_write", "Memory write latency", "cycle")),
          stats_row_buffer_hit_rate_(stats_.addFormula("row_buffer_hit_rate", "Row buffer hit rate", "ratio",
              [this]() {
                  auto hits = stats_row_hits_.value();
                  auto misses = stats_row_misses_.value();
                  return (hits + misses) > 0 ? static_cast<double>(hits) / (hits + misses) : 0.0;
              })) {}

    ~MemoryTLM() override = default;

    std::string get_module_type() const override { return "MemoryTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();
            bool is_write = req.is_write.read();
            
            // 统计：读/写请求
            if (is_write) {
                ++stats_requests_write_;
                stats_latency_write_.sample(120);  // 模拟写延迟
            } else {
                ++stats_requests_read_;
                stats_latency_read_.sample(100);   // 模拟读延迟
            }
            
            // 模拟行缓冲命中（基于地址位简单模拟）
            uint64_t addr = req.address.read();
            bool row_hit = (addr & 0xF000) == 0;  // 地址低 16KB 为行缓冲命中
            
            if (row_hit) {
                ++stats_row_hits_;
            } else {
                ++stats_row_misses_;
            }
            
            bundles::CacheRespBundle resp;
            resp.transaction_id.write(req.transaction_id.read());
            resp.data.write(0xDEADBEEF);
            resp.is_hit.write(row_hit ? 1 : 0);
            resp.error_code.write(0);
            resp_out_.write(resp);
            req_in_.consume();
        }
        if (adapter_) adapter_->tick();
    }

    void do_reset(const ResetConfig& /*config*/) override {
        req_in_.reset();
        resp_out_.reset();
        stats_.reset();
    }

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() { return req_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() { return resp_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

    // 统计访问器
    tlm_stats::StatGroup& stats() { return stats_; }
    const tlm_stats::StatGroup& stats() const { return stats_; }

    void dumpStats(std::ostream& os) const {
        stats_.dump(os);
    }
};

#endif // TLM_MEMORY_TLM_HH
