// src/tlm/pcie/pcie_endpoint_ip.cc
// PcieEndpointIP 实现 (T-P4-7)
// 作者 CppTLM Team / 日期 2026-10-13
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"
#include <nlohmann/json.hpp>

namespace tlm::pcie {

PcieEndpointIP::PcieEndpointIP(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {
    pool_.init_all();
    completions_.init();
}

void PcieEndpointIP::init() {
    ChStreamModuleBase::init();
    pool_.init_all();
    completions_.init();
}

void PcieEndpointIP::do_reset(const ResetConfig&) {
    pool_.init_all();
    completions_.init();
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
    completions_.flr_pf();
}

void PcieEndpointIP::flr_vf(uint16_t vf_id) noexcept {
    pool_.flr_vf(vf_id);
    completions_.flr_vf(vf_id);
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