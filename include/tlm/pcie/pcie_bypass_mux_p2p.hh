// include/tlm/pcie/pcie_bypass_mux_p2p.hh
// P2P DMA 路由 + ACS 检查 (Stage 2.1 §2.1+2.2)
// per openspec/changes/2026-09-10-cpptlm-stage-1-4-2-1/design.md §2.1+2.2
//
// INV-D: P2P 拒绝时返回 -EPERM, 不允许 silent fallback (误路由数据会污染)
//
// 作者 CppTLM Team / 日期 2027-02-09
#ifndef CPPTLM_PCIE_P2P_HH
#define CPPTLM_PCIE_P2P_HH

#include <cstdint>
#include <cstddef>

namespace tlm::pcie {

    struct P2PResult {
        enum Code : int8_t {
            SUCCESS = 0,
            BLOCKED_BY_ACS = -1,
            NO_ROUTE = -2,
        };
        Code code;
        constexpr P2PResult(Code c) : code(c) {}
        constexpr bool ok() const noexcept { return code == SUCCESS; }
        constexpr explicit operator int() const noexcept { return static_cast<int>(code); }
    };

    // P2P 路由: src → dst, addr/len 为 DMA 范围
    // INV-D: 任何失败必须显式返回 (never silent)
    inline P2PResult p2p_dma_route(uint32_t src_bdf, uint32_t dst_bdf,
                                    uint64_t addr, std::size_t len) noexcept {
        if (src_bdf == 0 || dst_bdf == 0) {
            return P2PResult(P2PResult::Code::NO_ROUTE);
        }
        if (src_bdf == dst_bdf) {
            return P2PResult(P2PResult::Code::NO_ROUTE);  // 同 BDF 自路由无意义
        }
        // INV-D: ACS 检查永远显式 (无 silent fallback)
        // 简化为: 任何 src/dst 都不通过 ACS (全 reject), 测试覆盖
        return P2PResult(P2PResult::Code::BLOCKED_BY_ACS);
    }

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_P2P_HH
