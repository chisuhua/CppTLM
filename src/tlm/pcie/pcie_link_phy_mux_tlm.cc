// src/tlm/pcie/pcie_link_phy_mux_tlm.cc
// PcieLinkPhyMuxTLM 实现：LL + PHY + Mux composite (Phase 1 休眠 tick)
// 作者 CppTLM Team / 日期 2026-09-15
// 参考: openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/design.md §1.2-1.3
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"

namespace tlm::pcie {

    namespace {
        // Phase 1 单一所有权: 注册表持有 unique_ptr; EP 仅持 raw observer (防 double-delete)
        // Phase 2 迁移到 EP internal_factory (design §2.2); 本表届时弃用
        std::unordered_map<std::string, std::unique_ptr<PcieLinkPhyMuxTLM>>& lpm_registry() {
            static std::unordered_map<std::string, std::unique_ptr<PcieLinkPhyMuxTLM>> reg;
            return reg;
        }
    } // namespace

    PcieLinkPhyMuxTLM::PcieLinkPhyMuxTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq),
          link_(eq),
          phy_(eq),
          mux_(&link_) { // 构造即持 link_ 指针 (成员声明序 = 构造序)
        // INV-1: 封装 pcie_endpoint_ip.cc 原 attach 语义 (构造后立即绑定)
        phy_.link_layer(&link_);      // PHY 持 LL 指针 (rate_switch 0-cycle 同步调用)
        phy_.set_link_up(true);       // link_up 立即置位 (Detect + initialized)
        mux_.set_phy_initialized(true);
    }

    void PcieLinkPhyMuxTLM::init() {
        ChStreamModuleBase::init();
        link_.reset_fc_buckets();         // 触发 LL 默认 FC 配置
        mux_.apply_mode(BypassMode::Full); // 默认 Full mode
    }

    void PcieLinkPhyMuxTLM::do_reset(const ResetConfig& cfg) {
        ChStreamModuleBase::do_reset(cfg);
        link_.reset_fc_buckets();
        mux_.apply_mode(BypassMode::Full);
    }

    void PcieLinkPhyMuxTLM::tick() {
        // Phase 1 休眠 (Oracle 决策 a): 完整实现但 EP::tick 不调用;
        // Phase 2 task 2.1 EP tick override 时激活, 同时迁移外部 phy->tick() 驱动。
        //
        // INV-4: 内部保持直接指针, 0-cycle 同步。
        // 顺序契约 (Oracle R3): PHY 先于 LL — PHY 的 LTSSM/rate-switch 完成检测
        // 需在同拍驱动 LL 的 wire 状态; LL 消费错误注入 + wire busy 推进。
        phy_.tick();
        ++tick_counts_[0];
        link_.tick();
        ++tick_counts_[1];
        // mux_ 无显式 tick (状态机在 apply_mode 同步触发)

        // 17 TLP 端口 adapter 转发
        for (unsigned i = 0; i < NUM_TLP_PORTS; ++i) {
            if (adapters_[i]) {
                adapters_[i]->tick();
            }
        }
        ++tick_counts_[2];
    }

    void PcieLinkPhyMuxTLM::set_stream_adapter(cpptlm::StreamAdapterBase* a) {
        if (a) {
            adapters_[0] = a;
        }
    }

    void PcieLinkPhyMuxTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        if (!adapters) {
            return;
        }
        for (unsigned i = 0; i < NUM_TLP_PORTS; ++i) {
            adapters_[i] = adapters[i];
        }
    }

    // ========== 静态注册表 (Phase 1: unique_ptr 归注册表; EP 仅 raw observer) ==========

    std::unordered_map<std::string, std::unique_ptr<PcieLinkPhyMuxTLM>>&
    PcieLinkPhyMuxTLM::registry() {
        return lpm_registry();
    }

    PcieLinkPhyMuxTLM* PcieLinkPhyMuxTLM::attach_to_endpoint(const std::string& ep_name,
                                                           EventQueue* eq) {
        auto& reg = registry();
        auto it = reg.find(ep_name);
        if (it != reg.end()) {
            it->second = std::make_unique<PcieLinkPhyMuxTLM>(ep_name + "_lpm", eq);
            return it->second.get();
        }
        auto [new_it, _] =
            reg.emplace(ep_name, std::make_unique<PcieLinkPhyMuxTLM>(ep_name + "_lpm", eq));
        return new_it->second.get();
    }

    PcieLinkPhyMuxTLM* PcieLinkPhyMuxTLM::for_endpoint(const std::string& ep_name) noexcept {
        auto& reg = registry();
        auto it = reg.find(ep_name);
        return (it != reg.end()) ? it->second.get() : nullptr;
    }

    PcieLinkPhyMuxTLM* lpm_for_endpoint(const std::string& ep_name) noexcept {
        return PcieLinkPhyMuxTLM::for_endpoint(ep_name);
    }

    PciePhyDigitalCtrl* lpm_phy_for_endpoint(const std::string& ep_name) noexcept {
        if (auto* lpm = PcieLinkPhyMuxTLM::for_endpoint(ep_name)) {
            return &lpm->phy();
        }
        return nullptr;
    }

    PcieBypassMux* lpm_mux_for_endpoint(const std::string& ep_name) noexcept {
        if (auto* lpm = PcieLinkPhyMuxTLM::for_endpoint(ep_name)) {
            return &lpm->mux();
        }
        return nullptr;
    }

    PcieLinkLayer* lpm_ll_for_endpoint(const std::string& ep_name) noexcept {
        if (auto* lpm = PcieLinkPhyMuxTLM::for_endpoint(ep_name)) {
            return &lpm->link();
        }
        return nullptr;
    }

    void PcieLinkPhyMuxTLM::detach_from_endpoint(const std::string& ep_name) noexcept {
        registry().erase(ep_name);
    }

    std::size_t PcieLinkPhyMuxTLM::endpoint_count() noexcept {
        return registry().size();
    }

} // namespace tlm::pcie
