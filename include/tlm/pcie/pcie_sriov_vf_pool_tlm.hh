// include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh
// PcieSriovVfPool: SR-IOV VF Pool 路由表（17 端口 stream_id 分发）
// 功能描述：将 17 端口 stream_id (0..16) 路由到对应 PF/VF 内部状态：
//           stream_id=0  → PF0
//           stream_id=1..16 → VF0..VF15
//           错误 stream_id 拒绝（dispatch 返 false）。
//           持有 PcieConfigSpacePerVf + PcieMsixTablePerVf + ARI router 三个池。
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-4
//       design.md §8 (VF Pool 共享 StreamAdapter + 内部 stream_id 路由)
#ifndef TLM_PCIE_PCIE_SRIOV_VF_POOL_TLM_HH
#define TLM_PCIE_PCIE_SRIOV_VF_POOL_TLM_HH

#include "bundles/pcie_bundles_tlm.hh"
#include "tlm/pcie/pcie_ari_router_tlm.hh"
#include "tlm/pcie/pcie_config_space_per_vf_tlm.hh"
#include "tlm/pcie/pcie_flow_control_token_bucket.hh"
#include "tlm/pcie/pcie_msix_per_vf_tlm.hh"

#include <array>
#include <cstdint>
#include <memory>

namespace tlm::pcie {

/**
 * @brief SR-IOV VF Pool 路由表（17 端口 stream_id）
 *
 * 共享 MultiPortStreamAdapter，每个 port 对应一个 PF/VF：
 *   - port[0]  = PF0
 *   - port[1]  = VF0
 *   - ...
 *   - port[16] = VF15
 *
 * 内部通过 stream_id 区分请求/响应，分发到 per-VF Config Space / MSI-X。
 * ARI router 决定 routing-id → slot 映射方式。
 */
class PcieSriovVfPool {
public:
    static constexpr uint16_t PF_SLOT = 0;
    static constexpr uint16_t NUM_PORTS = 17;  // 1 PF + 16 VF

    PcieSriovVfPool() {
        init_all();
    }

    // 初始化所有 per-VF 状态
    void init_all() {
        config_pool_.init_all();
        msix_pool_.init_all();
    }

    // ========== 路由验证 ==========
    [[nodiscard]] uint16_t num_ports() const noexcept { return NUM_PORTS; }
    [[nodiscard]] bool is_valid_stream_id(uint16_t stream_id) const noexcept {
        return stream_id < NUM_PORTS;
    }

    // ========== per-VF 状态访问（直接暴露给 PcieEndpointIP）==========
    PcieConfigSpacePerVf& config_pool() noexcept { return config_pool_; }
    PcieMsixTablePerVf& msix_pool() noexcept { return msix_pool_; }
    AriRouter& ari_router() noexcept { return ari_router_; }

    // 便捷：按 slot 访问 Config Space
    tlm::gpu::PcieConfigSpace& config_of(uint16_t stream_id) {
        return config_pool_.config_of(stream_id);
    }
    const tlm::gpu::PcieConfigSpace& config_of(uint16_t stream_id) const {
        return config_pool_.config_of(stream_id);
    }

    // 便捷：按 slot 访问 MSI-X table
    tlm::gpu::MsiXTable& msix_of(uint16_t stream_id) {
        return msix_pool_.table_of(stream_id);
    }
    const tlm::gpu::MsiXTable& msix_of(uint16_t stream_id) const {
        return msix_pool_.table_of(stream_id);
    }

    bool msix_pending(uint16_t stream_id, uint16_t vector) const {
        return msix_pool_.pending(stream_id, vector);
    }

    // ========== per-VF FC Token Bucket（FcEngine 池, per Q11 单 VC0）==========
    FcEngine& fc_engine() noexcept { return fc_engine_; }
    FcTokenBucket& bucket_of(uint16_t stream_id) {
        return fc_engine_.bucket(stream_id);
    }

    // ========== per-VF TLP seq 计数（12-bit, FLR 复位归 0）==========
    // 返回当前 seq 并递增（wrap @4095→0, per PCIe 5.0 §3.6）
    uint16_t next_seq(uint16_t stream_id) noexcept;
    // 当前 seq 值（不发号, 诊断/测试）
    uint16_t seq_of(uint16_t stream_id) const noexcept;

    // ========== FLR（per Q10 简化）==========
    // flr_pf(): 全状态复位 (PF + 16 VF)
    // flr_vf(vfx): 仅复位对应 VF (PF + 其他 VF 不变)
    void flr_pf() noexcept;
    void flr_vf(uint16_t vf_id) noexcept;

    // ========== TLP 分发（按 stream_id 路由到对应 VF Config Space）==========
    // 处理 CFG_READ/CFG_WRITE；其他 kind 暂不处理（per proposal.md P4-G1 范围）
    bool dispatch_tlp(uint16_t stream_id, const bundles::PcieTlpBundle& tlp);

    // ========== MSI-X pending 分发（按 stream_id 路由到对应 VF MSI-X）==========
    bool dispatch_msix(uint16_t stream_id, uint16_t vector);

private:
    PcieConfigSpacePerVf config_pool_;
    PcieMsixTablePerVf msix_pool_;
    AriRouter ari_router_;
    FcEngine fc_engine_;
    std::array<uint16_t, NUM_PORTS> tlp_seq_{};
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_SRIOV_VF_POOL_TLM_HH