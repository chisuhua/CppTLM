// include/tlm/memory_tlm.hh
// MemoryTLM：基于 ch_stream 语义的简化 Memory 模块（v2.1 新式模型）
// 功能描述：作为 CacheTLM 下游模块，接收请求后返回模拟响应
//           用于验证 StreamAdapter 完整数据通路
//           v2.2: 新增 backing-store 注入 + 零时 memcpy 路径
//           (per openspec/changes/cpptlm-minimal-dgpu-soc-v1 A1)
// 作者 CppTLM Team
// 日期 2026-04-12 (v2.2: 2027-02-09)
#ifndef TLM_MEMORY_TLM_HH
#define TLM_MEMORY_TLM_HH

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>
#include <cstring>

class MemoryTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // backing-store 注入 (v2.2, per spec/memory-tlm-backing-store)
    // nullptr = legacy 路径 (0xDEADBEEF + latency 采样); 非空 = 零时 memcpy 路径
    uint8_t* backing_ptr_ = nullptr;
    uint64_t backing_size_ = 0;
    uint64_t size_cap_ = 0;  // set_size_bytes 上限 (0 = 用 backing_size_)

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
        : ChStreamModuleBase(name, eq), stats_("memory"),
          stats_requests_read_(stats_.addScalar("requests_read", "Memory read requests", "count")),
          stats_requests_write_(
              stats_.addScalar("requests_write", "Memory write requests", "count")),
          stats_row_hits_(stats_.addScalar("row_hits", "Row buffer hits", "count")),
          stats_row_misses_(stats_.addScalar("row_misses", "Row buffer misses", "count")),
          stats_latency_read_(
              stats_.addDistribution("latency_read", "Memory read latency", "cycle")),
          stats_latency_write_(
              stats_.addDistribution("latency_write", "Memory write latency", "cycle")),
          stats_row_buffer_hit_rate_(
              stats_.addFormula("row_buffer_hit_rate", "Row buffer hit rate", "ratio", [this]() {
                  auto hits = stats_row_hits_.value();
                  auto misses = stats_row_misses_.value();
                  return (hits + misses) > 0 ? static_cast<double>(hits) / (hits + misses) : 0.0;
              })) {
    }

    ~MemoryTLM() override = default;

    std::string get_module_type() const override {
        return "MemoryTLM";
    }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // ChStreamModuleBase 统计接口
    tlm_stats::StatGroup* get_stats_group() override {
        return &stats_;
    }
    std::string get_stats_path() const override {
        return "system.memory";
    }

    // v2.2: backing-store API (per spec/memory-tlm-backing-store)
    // backing 已注入: tick() 走零时 memcpy 路径
    // backing == nullptr: 保留 v2.1 legacy 行为 (0xDEADBEEF + latency 采样)
    void set_backing_store(uint8_t* ptr, uint64_t size_bytes) noexcept {
        backing_ptr_ = ptr;
        backing_size_ = size_bytes;
        if (size_cap_ == 0) size_cap_ = size_bytes;
    }
    void set_size_bytes(uint64_t sz) noexcept { size_cap_ = sz; }
    uint8_t* host_addr(uint64_t offset) const noexcept {
        return backing_ptr_ ? (backing_ptr_ + offset) : nullptr;
    }
    uint64_t backing_size() const noexcept { return backing_size_; }
    bool has_backing() const noexcept { return backing_ptr_ != nullptr; }

    // on_config_loaded: 从 cfg.params 读 capacity_gb 换算 → size_cap_
    void on_config_loaded() override {
        // capacity_gb 是 memory.params 的可选字段; 缺失时保持 size_cap_ (派生自 backing)
        // 此接口留作 v1.1 扩展 (JSON params 显式约束)
        // 当前实现: 不动 size_cap_, 由调用方在 set_backing_store 之前/之后调
    }

    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            const auto& req = req_in_.data();
            bool is_write = req.is_write.read();
            uint64_t addr = req.address.read();
            uint64_t tid = req.transaction_id.read();

            bundles::CacheRespBundle resp;
            resp.transaction_id.write(tid);

            if (backing_ptr_ != nullptr) {
                // ── v2.2 backing 零时路径 (per spec/memory-tlm-backing-store) ──
                const uint64_t sz = req.size.read();
                const uint64_t cap = size_cap_ ? size_cap_ : backing_size_;
                if (addr + sz > cap) {
                    resp.error_code.write(1);  // OUT_OF_RANGE
                    resp.is_hit.write(0);
                    resp.data.write(0);
                } else if (is_write) {
                    // 写: req.data 是 ch_uint<64> = 8 字节, sz > 8 截断
                    uint64_t val = req.data.read();
                    std::memcpy(backing_ptr_ + addr, &val, std::min<size_t>(sz, 8));
                    resp.error_code.write(0);
                    resp.is_hit.write(1);
                    resp.data.write(0);
                    ++stats_requests_write_;
                } else {
                    // 读: 直读 backing
                    uint64_t val = 0;
                    std::memcpy(&val, backing_ptr_ + addr, std::min<size_t>(sz, 8));
                    resp.error_code.write(0);
                    resp.is_hit.write(1);
                    resp.data.write(val);
                    ++stats_requests_read_;
                }
                resp_out_.write(resp);
                req_in_.consume();
            } else {
                // ── v2.1 legacy 路径 (per D3 防回归硬约束, 逐字节保留) ──
                if (is_write) {
                    ++stats_requests_write_;
                    stats_latency_write_.sample(120);
                } else {
                    ++stats_requests_read_;
                    stats_latency_read_.sample(100);
                }

                bool row_hit = (addr & 0xF000) == 0;
                if (row_hit) ++stats_row_hits_;
                else ++stats_row_misses_;

                resp.data.write(0xDEADBEEF);
                resp.is_hit.write(row_hit ? 1 : 0);
                resp.error_code.write(0);
                resp_out_.write(resp);
                req_in_.consume();
            }
        }
        if (adapter_)
            adapter_->tick();
    }

    void do_reset(const ResetConfig& /*config*/) override {
        req_in_.reset();
        resp_out_.reset();
        stats_.reset();
    }

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() {
        return req_in_;
    }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() {
        return resp_out_;
    }
    // P0-5b: 被动响应模块,无 req 输出/resp 输入。返回静态 dummy 供 StreamAdapter 统一调用。
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out() {
        static cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> dummy;
        return dummy;
    }
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in() {
        static cpptlm::InputStreamAdapter<bundles::CacheRespBundle> dummy;
        return dummy;
    }
    cpptlm::StreamAdapterBase* get_adapter() const {
        return adapter_;
    }

    // 统计访问器
    tlm_stats::StatGroup& stats() {
        return stats_;
    }
    const tlm_stats::StatGroup& stats() const {
        return stats_;
    }

    void dumpStats(std::ostream& os) const {
        stats_.dump(os);
    }
};

#endif // TLM_MEMORY_TLM_HH
