// include/tlm/crossbar_tlm.hh
// CrossbarTLM：4 端口 Crossbar 模块（v2.1 ChStream 模型）
// 功能描述：支持 4 请求端口 → 4 响应端口的路由矩阵，地址位提取路由
// 作者 CppTLM Team
// 日期 2026-04-13
#ifndef TLM_CROSSBAR_TLM_HH
#define TLM_CROSSBAR_TLM_HH

#include "core/chstream_module.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>

/**
 * @brief Crossbar TLM 模块（4 端口路由）
 * 
 * 端口拓扑：
 * - req_in[0-3]   — 请求输入（来自 CPU/上游）
 * - resp_out[0-3] — 响应输出（路由后到下游 Memory/Cache）
 * 
 * 路由策略：addr >> 12 & 0x3
 * - Port 0: 0x0000-0x0FFF
 * - Port 1: 0x1000-0x1FFF
 * - Port 2: 0x2000-0x2FFF
 * - Port 3: 0x3000-0x3FFF
 */
class CrossbarTLM : public ChStreamModuleBase {
private:
    static constexpr unsigned NUM_PORTS = 4;
    static constexpr unsigned PORT_SHIFT = 12;
    static constexpr unsigned PORT_MASK = 0x3;

    cpptlm::StreamAdapterBase* adapter[NUM_PORTS] = {nullptr};

    // 性能统计
    tlm_stats::StatGroup stats_;
    tlm_stats::Scalar& stats_flits_received_;
    tlm_stats::Scalar& stats_flits_sent_;
    tlm_stats::Distribution& stats_flit_latency_;
    tlm_stats::Average& stats_buffer_occupancy_;

public:
    // 请求方向端口（public 以便 StreamAdapter 访问）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out[NUM_PORTS];

    CrossbarTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          stats_("crossbar"),
          stats_flits_received_(stats_.addScalar("flits_received", "Total flits received", "flits")),
          stats_flits_sent_(stats_.addScalar("flits_sent", "Total flits sent", "flits")),
          stats_flit_latency_(stats_.addDistribution("flit_latency", "Flit traversal latency", "cycle")),
          stats_buffer_occupancy_(stats_.addAverage("buffer_occupancy", "Average buffer occupancy", "flits")) {}

    ~CrossbarTLM() override = default;

    std::string get_module_type() const override { return "CrossbarTLM"; }

    // ChStreamModuleBase 接口
    void set_stream_adapter(cpptlm::StreamAdapterBase*) override {}
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            adapter[i] = adapters[i];
        }
    }

    void tick() override {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (req_in[i].valid() && req_in[i].ready()) {
                const bundles::CacheReqBundle& req = req_in[i].data();
                unsigned dst = route_address(req.address.read());
                
                // 统计：接收 flit
                ++stats_flits_received_;
                
                bundles::CacheRespBundle resp;
                resp.transaction_id.write(req.transaction_id.read());
                resp.data.write(req.data.read());
                resp.is_hit.write(1);
                resp.error_code.write(0);
                resp_out[dst].write(resp);
                
                // 统计：发送 flit + 延迟采样
                ++stats_flits_sent_;
                stats_flit_latency_.sample(3);  // 模拟 3 cycle 穿越延迟
                
                req_in[i].consume();
            }
        }
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (adapter[i]) adapter[i]->tick();
        }
    }

    void do_reset(const ResetConfig&) override {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            req_in[i].reset();
            resp_out[i].reset();
        }
        stats_.reset();
    }

    unsigned route_address(uint64_t addr) const {
        return (addr >> PORT_SHIFT) & PORT_MASK;
    }

    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const { return adapter[idx]; }
    unsigned num_ports() const override { return NUM_PORTS; }

    // 统计访问器
    tlm_stats::StatGroup& stats() { return stats_; }
    const tlm_stats::StatGroup& stats() const { return stats_; }

    void dumpStats(std::ostream& os) const {
        stats_.dump(os);
    }
};

#endif // TLM_CROSSBAR_TLM_HH
