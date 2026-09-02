// src/tlm/pcie/pcie_endpoint_ip.cc
// PcieEndpointIP 实现 (T-P4-7)
// 作者 CppTLM Team / 日期 2026-10-13
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"
#include "tlm/pcie/pcie_axi_adapter_tlm.hh"
#include <nlohmann/json.hpp>

namespace tlm::pcie {

PcieEndpointIP::PcieEndpointIP(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {
    pool_.init_all();
}

void PcieEndpointIP::init() {
    ChStreamModuleBase::init();
    pool_.init_all();
}

void PcieEndpointIP::do_reset(const ResetConfig&) {
    pool_.init_all();
}

void PcieEndpointIP::set_stream_adapter(cpptlm::StreamAdapterBase* a) {
    if (a) {
        adapters_[0] = a;
    }
}

void PcieEndpointIP::set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
    if (!adapters) {
        return;
    }
    for (unsigned i = 0; i < NUM_PORTS; ++i) {
        adapters_[i] = adapters[i];
    }
}

bool PcieEndpointIP::all_ports_have_adapter() const {
    for (unsigned i = 0; i < NUM_PORTS; ++i) {
        if (!adapters_[i]) {
            return false;
        }
    }
    return true;
}

void PcieEndpointIP::on_config_loaded() {
    const auto& params = get_config();  // SimObject::get_config()
    attach_composition(params);
}

void PcieEndpointIP::attach_composition(const nlohmann::json& params) {
    // AXI Stream Adapter 独立挂接（per Phase 5 T-P5-6, 不依赖 link_layer 分支）
    if (params.contains("axi_adapter")) {
        auto* ax = PcieAxiAdapter::attach_to_endpoint(getName(), event_queue);
        if (ax) {
            ax->set_endpoint(this);
            // Phase 6 T-P6-4: JSON axi4_mapper_inject: true → 注入 AXI4Mapper（缺省 false 不注入）
            const auto& axi_json = params["axi_adapter"];
            const bool mapper_inject = axi_json.value("axi4_mapper_inject", false);
            ax->set_mapper_injected(mapper_inject);
        }
    } else {
        PcieAxiAdapter::detach_from_endpoint(getName());
    }
    if (!params.contains("link_layer")) {
        return;
    }
    const auto& ll_json = params["link_layer"];
    const bool enabled = ll_json.value("enabled", true);
    if (!enabled) {
        PcieLinkLayer::detach_from_endpoint(getName());
        PciePhyDigitalCtrl::detach_from_endpoint(getName());
        PcieBypassMux::detach_from_endpoint(getName());
        return;
    }
    tlm::pcie::PcieLinkLayerConfig ll_cfg;
    ll_cfg.enabled = enabled;
    ll_cfg.fc_capacity = ll_json.value("fc_token_bucket_capacity", 256u);
    ll_cfg.fc_init_p = ll_json.value("fc_initial_credit_p", 256u);
    ll_cfg.fc_init_np = ll_json.value("fc_initial_credit_np", 256u);
    ll_cfg.fc_init_cpl = ll_json.value("fc_initial_credit_cpl", 256u);
    ll_cfg.retry_buffer_size = ll_json.value("retry_buffer_size", 4096u);

    auto* ll = PcieLinkLayer::attach_to_endpoint(getName(), event_queue, ll_cfg);
    auto* phy = PciePhyDigitalCtrl::attach_to_endpoint(getName(), event_queue);
    if (phy) {
        phy->link_layer(ll);
        phy->set_link_up(true);
    }
    auto* mux = PcieBypassMux::attach_to_endpoint(getName(), ll);
    if (mux) {
        mux->set_phy_initialized(phy != nullptr);
        const std::string bypass_mode = ll_json.value("bypass_mode", std::string("Full"));
        if (bypass_mode == "Bypass") {
            mux->apply_mode(BypassMode::Bypass);
        } else if (bypass_mode == "Partial") {
            mux->apply_mode(BypassMode::Partial);
        }
    }
}

void PcieEndpointIP::tick() {
    // Phase 8 M1: 真实 AXI 数据路径接线 — PcieEndpointIP::tick() 驱动
    // PcieAxiAdapter 消费 slave_in 请求，EP 内部真实处理并产生真实响应。
    // HostBypass/RC (Host 侧 master) ↔ PcieAxiAdapter (EP 侧 slave) 双向闭环。
    if (auto* ax = PcieAxiAdapter::for_endpoint(getName())) {
        cpptlm::Axi4StreamAdapter& axi = ax->axi();

        if (axi.slave_req_valid()) {
            const bundles::Axi4Bundle& req = axi.slave_req_data();
            if (req.awid.read() != 0 || req.awaddr.read() != 0 || req.awlen.read() != 0) {
                // 写请求：配置空间偏移 (< config_size) vs BAR 空间
                const uint64_t addr = req.awaddr.read();
                const uint16_t bid = static_cast<uint16_t>(req.awid.read());

                if (addr < pool_.config_of(0).config_size()) {
                    pool_.config_of(0).write(static_cast<uint16_t>(addr),
                                             static_cast<uint32_t>(req.wdata.read()));
                } else {
                    // BAR 空间：地址低位路由到 backing store（8B 对齐字）
                    bar_store_[addr & ~0x7ULL] = req.wdata.read();
                }

                bundles::Axi4Bundle wresp;
                wresp.bid.write(bid);
                wresp.bresp.write(0);
                axi.slave_resp(wresp);
            } else {
                // 读请求：配置空间偏移 (< config_size) vs BAR 空间
                const uint64_t addr = req.araddr.read();
                const uint16_t rid = static_cast<uint16_t>(req.arid.read());
                uint64_t rdata = 0;

                if (addr < pool_.config_of(0).config_size()) {
                    rdata = pool_.config_of(0).read(static_cast<uint16_t>(addr));
                } else {
                    const auto it = bar_store_.find(addr & ~0x7ULL);
                    if (it != bar_store_.end()) {
                        rdata = it->second;
                    }
                }

                bundles::Axi4Bundle rresp;
                rresp.rid.write(rid);
                rresp.rdata.write(rdata);
                rresp.rresp.write(0);
                rresp.rlast.write(1);
                axi.slave_resp(rresp);
            }
            axi.slave_req_consume();
        }

        axi.tick();
    }

    for (unsigned i = 0; i < NUM_PORTS; ++i) {
        if (adapters_[i]) {
            adapters_[i]->tick();
        }
    }
    if (auto* ll = PcieLinkLayer::for_endpoint(getName())) {
        ll->tick();
    }
}

void PcieEndpointIP::flr_pf() noexcept {
    pool_.flr_pf();
}

void PcieEndpointIP::flr_vf(uint16_t vf_id) noexcept {
    pool_.flr_vf(vf_id);
}

PcieLinkLayer* PcieEndpointIP::link_layer() const noexcept {
    return PcieLinkLayer::for_endpoint(getName());
}

PciePhyDigitalCtrl* PcieEndpointIP::phy() const noexcept {
    return PciePhyDigitalCtrl::for_endpoint(getName());
}

PcieBypassMux* PcieEndpointIP::bypass_mux() const noexcept {
    return PcieBypassMux::for_endpoint(getName());
}

} // namespace tlm::pcie