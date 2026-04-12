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

public:
    // 请求方向端口（public 以便 StreamAdapter 访问）
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out[NUM_PORTS];

    CrossbarTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

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
                
                bundles::CacheRespBundle resp;
                resp.transaction_id.write(req.transaction_id.read());
                resp.data.write(req.data.read());
                resp.is_hit.write(1);
                resp.error_code.write(0);
                resp_out[dst].write(resp);
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
    }

    unsigned route_address(uint64_t addr) const {
        return (addr >> PORT_SHIFT) & PORT_MASK;
    }

    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const { return adapter[idx]; }
    unsigned num_ports() const override { return NUM_PORTS; }
};

#endif // TLM_CROSSBAR_TLM_HH
