// include/tlm/pcie/pcie_ari_router_tlm.hh
// AriRouter: SR-IOV ARI (Alternative Routing-ID Interpretation) 路由解析
// 功能描述：将 16-bit routing-id 解析为 PF/VF slot ID（0=PF，1..16=VF0..VF15）。
//           ARI enabled: 低 8 位是 Function Number，0=PF，1..16=VF。
//           ARI disabled: 16-bit BDF（bus/dev/fn），仅 fn=0 命中 PF，其余路由到 PF。
// 作者 CppTLM Team / 日期 2026-10-13
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-3
//       design.md §8 (ARI 紧凑 routing-id 解析)
#ifndef TLM_PCIE_PCIE_ARI_ROUTER_TLM_HH
#define TLM_PCIE_PCIE_ARI_ROUTER_TLM_HH

#include <cstdint>

namespace tlm::pcie {

/**
 * @brief ARI (Alternative Routing-ID Interpretation) 路由解析器
 *
 * ARI enabled（per PCI-SIG ARI spec）：
 *   - routing-id[7:0] = Function Number
 *   - routing-id[15:8] = Device/Bus（reserved for future ARI hierarchy）
 *   - Function 0 = PF
 *   - Function 1..16 = VF0..VF15
 *
 * ARI disabled（legacy）：
 *   - routing-id[2:0] = Function Number（仅 fn=0 = PF）
 *   - routing-id[7:3] = Device Number
 *   - routing-id[15:8] = Bus Number
 *   - 单功能设备：仅 fn=0 有效 → 所有请求 → PF
 *
 * 切换机制：PCI Express Capability ARI Forwarding Enable bit (bit 0 of ARI Capability Control)
 */
class AriRouter {
public:
    static constexpr uint16_t PF_SLOT = 0;
    static constexpr uint16_t NUM_SLOTS = 17;  // 1 PF + 16 VF

    AriRouter() = default;

    // ARI Forwarding Enable（PCI Express Capability 偏移 0x14 bit 0）
    void set_ari_enabled(bool enabled) noexcept { ari_enabled_ = enabled; }
    [[nodiscard]] bool ari_enabled() const noexcept { return ari_enabled_; }

    // 路由解析：输入 routing-id（16-bit），返回 slot ID（0=PF, 1..16=VF0..VF15）
    // 越界 Function（>16）默认映射到 PF（保守 fallback）
    [[nodiscard]] uint16_t route_id_to_vf(uint16_t routing_id) const noexcept {
        if (ari_enabled_) {
            const uint16_t fn = routing_id & 0xFFu;  // 低 8 位 = Function
            if (fn == 0) {
                return PF_SLOT;
            }
            if (fn >= 1 && fn <= 16) {
                return fn;  // 1..16 直接 = slot ID
            }
            // 越界 fn（>16）：保守 fallback 到 PF
            return PF_SLOT;
        }
        // ARI disabled: 仅 fn=0 是 PF，其余 fn（1..7）非 ARI 多功能 → 路由到 PF
        return PF_SLOT;
    }

    // 检查 Function number 是否在合法范围（0..16）
    [[nodiscard]] bool is_valid_function(uint16_t fn) const noexcept {
        return fn <= 16u;
    }

private:
    bool ari_enabled_ = false;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_ARI_ROUTER_TLM_HH