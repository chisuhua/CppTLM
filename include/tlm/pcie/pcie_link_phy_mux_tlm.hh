// include/tlm/pcie/pcie_link_phy_mux_tlm.hh
// PcieLinkPhyMuxTLM: LL/PHY/Mux composite ChStream 子模块 (Phase 1)
// 功能描述（per openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/design.md §1）：
//   - 封装 3 个强耦合 PCIe 子模块: PcieLinkLayer + PciePhyDigitalCtrl + PcieBypassMux
//   - 17 端口 TLP (1 PF + 16 VF), ChStreamModuleBase 派生
//   - INV-1: 构造顺序 link→phy→mux + 立即 phy.link_layer(&link) + set_link_up(true)
//   - INV-4: 内部直接指针调用, 禁止 Bundle 化 (0-cycle 同步语义)
//   - Phase 1 休眠: tick() 完整实现但 EP::tick 不调用 (Phase 2 task 2.1 激活)
//   - 单一所有权: static unordered_map<name, unique_ptr>; EP 仅持 raw observer
// 作者 CppTLM Team / 日期 2026-09-15
// 参考: openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/{proposal,design,tasks}.md
#ifndef TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_HH
#define TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_HH

#include "bundles/pcie_bundles_tlm.hh"
#include "bundles/pcie_dllp_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/stream_adapter_base.hh"
#include "framework/stream_adapter.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"

#include <memory>
#include <string>
#include <unordered_map>

namespace tlm::pcie {

/**
 * @brief PcieLinkPhyMuxTLM：LL + PHY + Mux 强耦合 composite
 *
 * 封装 pcie_endpoint_ip.cc 原 attach_composition 内 4 子模块组合逻辑:
 *   - 成员声明序 = 构造序: link_ → phy_ → mux_
 *   - 构造后: phy_.link_layer(&link_) + phy_.set_link_up(true) + mux_.set_phy_initialized(true)
 *   - tick() 顺序契约 (Oracle R3): phy → link → 17 adapters
 *   - Phase 1 休眠: EP::tick 不经 composite (Oracle 决策 a); Phase 2 由 EP::tick 激活
 *
 * 所有权: 静态注册表持有 unique_ptr; EP 仅持 raw observer (防 double-delete)。
 */
class PcieLinkPhyMuxTLM : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_TLP_PORTS = 17; // 1 PF + 16 VF

    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> req_in[NUM_TLP_PORTS];
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_TLP_PORTS];

    PcieLinkPhyMuxTLM(const std::string& name, EventQueue* eq);
    ~PcieLinkPhyMuxTLM() override = default;

    PcieLinkPhyMuxTLM(const PcieLinkPhyMuxTLM&) = delete;
    PcieLinkPhyMuxTLM& operator=(const PcieLinkPhyMuxTLM&) = delete;
    PcieLinkPhyMuxTLM(PcieLinkPhyMuxTLM&&) = delete;
    PcieLinkPhyMuxTLM& operator=(PcieLinkPhyMuxTLM&&) = delete;

    std::string get_module_type() const override {
        return "PcieLinkPhyMuxTLM";
    }

    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    unsigned num_ports() const override {
        return NUM_TLP_PORTS;
    }

    void init() override;
    // Phase 1 无调用者——EP::tick 不经 composite; Phase 2 task 2.1 EP tick override 时激活
    void tick() override;
    void do_reset(const ResetConfig&) override;

    // 内部子模块访问器（composite → 子模块引用，供 EP / shim 转发）
    PcieLinkLayer& link() noexcept {
        return link_;
    }
    PciePhyDigitalCtrl& phy() noexcept {
        return phy_;
    }
    PcieBypassMux& mux() noexcept {
        return mux_;
    }
    const PcieLinkLayer& link() const noexcept {
        return link_;
    }
    const PciePhyDigitalCtrl& phy() const noexcept {
        return phy_;
    }
    const PcieBypassMux& mux() const noexcept {
        return mux_;
    }

    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const {
        return (idx < NUM_TLP_PORTS) ? adapters_[idx] : nullptr;
    }

    // ========== 静态注册表 (Phase 1 单一所有权; Phase 2 弃用) ==========
    // Phase 2: attach/detach [[deprecated]],for_endpoint 改走 EP::find_composite
    // (composite 迁 EP internal_factory; 本静态表仅 legacy attach 直调路径保留至 Phase 3 清理)
    [[deprecated("Phase 2 composite 迁 internal_factory; Phase 3 清理")]]
    static PcieLinkPhyMuxTLM* attach_to_endpoint(const std::string& ep_name, EventQueue* eq);
    static PcieLinkPhyMuxTLM* for_endpoint(const std::string& ep_name) noexcept;
    [[deprecated("Phase 2 composite 迁 internal_factory; Phase 3 清理")]]
    static void detach_from_endpoint(const std::string& ep_name) noexcept;
    static std::size_t endpoint_count() noexcept;

    // test-only: Phase 1 PHY/LL tick 顺序断言钩子 (Phase 2 释放或 #ifdef CPPTLM_TESTING)
    unsigned tick_counts(unsigned idx) const noexcept {
        return (idx < 3) ? tick_counts_[idx] : 0u;
    }

private:
    // 成员声明顺序 = 构造顺序 (INV-1: LL 先于 PHY/Mux)
    PcieLinkLayer link_;
    PciePhyDigitalCtrl phy_;
    PcieBypassMux mux_;

    cpptlm::StreamAdapterBase* adapters_[NUM_TLP_PORTS] = {nullptr};

    // test-only tick 计数器: [0]=phy [1]=link [2]=adapters (Phase 1 顺序断言)
    unsigned tick_counts_[3] = {0, 0, 0};

    // Phase 1 单一所有权注册表 (unique_ptr 归注册表, EP 持 raw observer)
    static std::unordered_map<std::string, std::unique_ptr<PcieLinkPhyMuxTLM>>& registry();
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_LINK_PHY_MUX_TLM_HH
