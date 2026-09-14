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
#include <unordered_set>

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

    // Stage 1.4-followups §2: ACS 策略表 (INV-F: 显式 grant, 默认 strict)
    class AcsPolicy {
    public:
        bool allow(uint32_t src_bdf, uint32_t dst_bdf) const noexcept {
            return grants_.count((static_cast<uint64_t>(src_bdf) << 32) | dst_bdf) != 0;
        }

        void grant(uint32_t src_bdf, uint32_t dst_bdf) noexcept {
            grants_.insert((static_cast<uint64_t>(src_bdf) << 32) | dst_bdf);
        }

    private:
        std::unordered_set<uint64_t> grants_;  // 默认空 → strict (BLOCKED)
    };

    // P2P 路由: src → dst, addr/len 为 DMA 范围
    // INV-D: 任何失败必须显式返回 (never silent)
    // Stage 1.4-followups §2: 可选 AcsPolicy — nullptr → strict (BLOCKED,
    // 向后兼容既有调用方); SUCCESS 仅当显式 policy 放行 (勘误: null 逻辑反转)
    inline P2PResult p2p_dma_route(uint32_t src_bdf, uint32_t dst_bdf,
                                    uint64_t addr, std::size_t len,
                                    const AcsPolicy* policy = nullptr) noexcept {
        if (src_bdf == 0 || dst_bdf == 0) {
            return P2PResult(P2PResult::Code::NO_ROUTE);
        }
        if (src_bdf == dst_bdf) {
            return P2PResult(P2PResult::Code::NO_ROUTE);  // 同 BDF 自路由无意义
        }
        if (!policy || !policy->allow(src_bdf, dst_bdf)) {
            return P2PResult(P2PResult::Code::BLOCKED_BY_ACS);
        }
        return P2PResult(P2PResult::Code::SUCCESS);
    }

} // namespace tlm::pcie

#endif // CPPTLM_PCIE_P2P_HH
