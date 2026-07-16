// include/tlm/crossbar_tlm.hh
// CrossbarTLM：4 端口 Crossbar 模块（v2.1 ChStream 模型）
// 功能描述：支持 4 请求端口 → 4 响应端口的路由矩阵，地址位提取路由
// 作者 CppTLM Team
// 日期 2026-04-13
#ifndef TLM_CROSSBAR_TLM_HH
#define TLM_CROSSBAR_TLM_HH

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/simple_port.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include <cstdint>
#include <memory>
#include <vector>

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
    cpptlm::StreamAdapterBase* multi_adapter_ = nullptr; // P0-5b fix: single MultiPortStreamAdapter
    bool port_busy_[NUM_PORTS] = {false};

    // P3: helper 创建的 PortPair 容器,管理 lazy-bind 的端口对生命周期
    std::vector<std::unique_ptr<PortPair>> helper_pairs_;

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
        : ChStreamModuleBase(name, eq), stats_("crossbar"),
          stats_flits_received_(
              stats_.addScalar("flits_received", "Total flits received", "flits")),
          stats_flits_sent_(stats_.addScalar("flits_sent", "Total flits sent", "flits")),
          stats_flit_latency_(
              stats_.addDistribution("flit_latency", "Flit traversal latency", "cycle")),
          stats_buffer_occupancy_(
              stats_.addAverage("buffer_occupancy", "Average buffer occupancy", "flits")) {
    }

    ~CrossbarTLM() override = default;

    std::string get_module_type() const override {
        return "CrossbarTLM";
    }

    // ChStreamModuleBase 接口
    // P0-5b fix: 工厂通过单指针 set_stream_adapter(StreamAdapterBase*) 注入
    // MultiPortStreamAdapter（内部已遍历 N 端口）。需存储为 multi_adapter_ 并
    // 在 tick() 中单次调用，避免 N 次重复 tick。
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override {
        multi_adapter_ = a;
        adapter[0] = a; // 兼容 get_adapter(0) 访问
    }
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            adapter[i] = adapters[i];
        }
        if (NUM_PORTS > 0)
            multi_adapter_ = adapters[0];
    }

    // ChStreamModuleBase 统计接口
    tlm_stats::StatGroup* get_stats_group() override {
        return &stats_;
    }
    std::string get_stats_path() const override {
        return "system.crossbar";
    }

    void tick() override {
        bool conflicted = false;
        unsigned conflict_dst = NUM_PORTS;

        for (unsigned i = 0; i < NUM_PORTS; i++) {
            port_busy_[i] = false;
        }

        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (req_in[i].valid() && req_in[i].ready()) {
                const bundles::CacheReqBundle& req = req_in[i].data();
                unsigned dst = route_address(req.address.read());

                // 检测总线冲突：同一周期多个请求路由到同一端口
                if (port_busy_[dst]) {
                    conflicted = true;
                    conflict_dst = dst;
                } else {
                    port_busy_[dst] = true;
                }

                bundles::CacheRespBundle resp;
                resp.transaction_id.write(req.transaction_id.read());
                resp.data.write(req.data.read());
                resp.is_hit.write(1);
                resp.error_code.write(0);
                // P0-5b: 响应必须回到请求的**源端口**(i),不是路由的**目的端口**(dst)
                // 连接 cpu0→xbar.0 后,响应路径是 xbar.resp_out[0]→cpu0.resp_in[0]
                resp_out[i].write(resp);

                // 统计：发送 flit + 延迟采样
                ++stats_flits_sent_;
                stats_flit_latency_.sample(3);

                req_in[i].consume();
            }
        }
        // P0-5b fix: MultiPortStreamAdapter 内部遍历 N 端口，只需调用一次
        if (multi_adapter_) {
            multi_adapter_->tick();
        } else {
            // 回退：每个端口独立 adapter（legacy 数组接口）
            for (unsigned i = 0; i < NUM_PORTS; i++) {
                if (adapter[i])
                    adapter[i]->tick();
            }
        }
    }

    void do_reset(const ResetConfig&) override {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            req_in[i].reset();
            resp_out[i].reset();
            port_busy_[i] = false;
        }
        stats_.reset();
    }

    unsigned route_address(uint64_t addr) const {
        return (addr >> PORT_SHIFT) & PORT_MASK;
    }

    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const {
        return adapter[idx];
    }
    unsigned num_ports() const override {
        return NUM_PORTS;
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

    // P0 F12b-LD: 查询 device_addr 的路由延迟（3 周期固定占位）
    // Phase 9+ 改为真实路由表查表
    uint64_t query_latency(uint64_t device_addr) const { return 3; }

    // P3: helper 方法 - 借鉴 gem5 caches.py::L2Cache.connectCPUSideBus / connectMemSideBus
    // P3 partial: 不依赖 D.1, lazy 注册 cpu_side/mem_side 端口 + bind 总线
    // 参数类型为 ChStreamModuleBase* (非 SimModule*),
    //   因为 CacheTLM/CrossbarTLM/MemoryTLM 均派生自 ChStreamModuleBase 而非 SimModule
    void connectCPUSideBus(ChStreamModuleBase* bus);
    void connectMemSideBus(ChStreamModuleBase* bus);
};

#endif // TLM_CROSSBAR_TLM_HH
