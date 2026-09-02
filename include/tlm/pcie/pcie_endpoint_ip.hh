// include/tlm/pcie/pcie_endpoint_ip.hh
// PcieEndpointIP: SR-IOV PCIe Endpoint IP 模型 (17 端口 = 1 PF + 16 VF)
// 功能描述：Phase 4 新类，独立于 PcieEndpointTLM (4 端口冻结布局)。
//           - 17 端口: port[0]=PF, port[1..16]=VF0..VF15
//           - 内置 PcieSriovVfPool (per-VF Config Space / MSI-X / FC / seq#)
//           - 内置 CompletionTracker (NP↔CplD trans_id 关联, per Q12)
//           - 通过静态注册表挂接 PcieLinkLayer / PciePhyDigitalCtrl / PcieBypassMux
//           - FLR: flr_pf() 全复位 / flr_vf(vfx) 仅对应 VF
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-7
//       design.md §8 (SR-IOV VF Pool) + §9.0 (23 ABI 边界)
// ⚠️ 不修改 include/abi/cpptlm_emulator.h（23 ABI 冻结边界，AD-088）
// ⚠️ 不修改 include/tlm/gpu/pcie_endpoint_tlm.h（PcieEndpointTLM 4 端口冻结）
#ifndef CPPTLM_PCIE_ENDPOINT_ABI_VERSION
#define CPPTLM_PCIE_ENDPOINT_ABI_VERSION 2  // v2 = 链路层 + PHY 数字 + SR-IOV
#endif

#ifndef TLM_PCIE_PCIE_ENDPOINT_IP_HH
#define TLM_PCIE_PCIE_ENDPOINT_IP_HH

#include "core/chstream_module.hh"
#include "core/sim_object.hh"
#include "framework/stream_adapter.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "tlm/pcie/pcie_completion_tracker_tlm.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace tlm::pcie {

/**
 * @brief PcieEndpointIP：SR-IOV PCIe Endpoint IP（17 端口）
 *
 * 与 PcieEndpointTLM 的差异：
 *   - 端口数 17（0=PF, 1..16=VF0..VF15）vs 4（冻结）
 *   - per-VF Config Space / MSI-X / FC / seq# 独立（PcieSriovVfPool）
 *   - Completion tracking（trans_id 关联, per Q12）
 *   - FLR: flr_pf() 全复位 / flr_vf() 仅对应 VF
 *
 * ChStreamModuleBase 派生：set_stream_adapter(adapters[17]) 多端口注入。
 */
class PcieEndpointIP : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_PORTS = 17;  // 0=PF, 1..16=VF0..VF15

    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_PORTS];

    PcieEndpointIP(const std::string& name, EventQueue* eq);
    ~PcieEndpointIP() override = default;

    PcieEndpointIP(const PcieEndpointIP&) = delete;
    PcieEndpointIP& operator=(const PcieEndpointIP&) = delete;
    PcieEndpointIP(PcieEndpointIP&&) = delete;
    PcieEndpointIP& operator=(PcieEndpointIP&&) = delete;

    std::string get_module_type() const override {
        return "PcieEndpointIP";
    }

    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    unsigned num_ports() const override {
        return NUM_PORTS;
    }

    void init() override;
    void tick() override;
    void do_reset(const ResetConfig&) override;
    void on_config_loaded() override;

    PcieSriovVfPool& vf_pool() noexcept { return pool_; }
    const PcieSriovVfPool& vf_pool() const noexcept { return pool_; }
    // completion 状态单一真源在 PcieSriovVfPool（生产路径 dispatch_tlp 登记到
    // pool_.completions()）；此处委托而非另持副本，避免双份 outstanding 失配。
    CompletionTracker& completions() noexcept { return pool_.completions(); }
    const CompletionTracker& completions() const noexcept { return pool_.completions(); }

    tlm::gpu::PcieConfigSpace& config_of(uint16_t stream_id) {
        return pool_.config_of(stream_id);
    }
    tlm::gpu::MsiXTable& msix_of(uint16_t stream_id) {
        return pool_.msix_of(stream_id);
    }

    void flr_pf() noexcept;
    void flr_vf(uint16_t vf_id) noexcept;

    PcieLinkLayer* link_layer() const noexcept;
    PciePhyDigitalCtrl* phy() const noexcept;
    PcieBypassMux* bypass_mux() const noexcept;

    bool all_ports_have_adapter() const;

    // 单 adapter 访问（测试断言）
    cpptlm::StreamAdapterBase* get_adapter(unsigned idx) const {
        return (idx < NUM_PORTS) ? adapters_[idx] : nullptr;
    }

private:
    PcieSriovVfPool pool_;
    cpptlm::StreamAdapterBase* adapters_[NUM_PORTS] = {nullptr};
    // BAR 空间 backing store（Phase 8 M1: AXI slave 写经地址路由落写/读回真实值）
    std::unordered_map<uint64_t, uint64_t> bar_store_;
    void attach_composition(const nlohmann::json& params);
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_ENDPOINT_IP_HH
