// scripts/test/cep_source_effect_check_probe.cc
// `cep_source_effect_check.sh` 的编译探针 —— 验证来源判据**有鉴别力**。
//
// 三类判据(各有独立变异态):
//   ① basis ↔ ref_kind 映射(tilelink 主矩阵 + T3)
//   ② CellKind::Free 与消费端 `transition_is_free` 的一致性(cxl)
//   ③ (注册表一致性由脚本的 JSON↔头文件比对承担,不需编译)
//
// 期望签名:
//   clean      ⇒ "SRK clean" 且 rc=0
//   downgraded ⇒ "SRK-02 inconsistency: <row|state/event>" 且 rc=1
//   misclassified ⇒ "FREE-KIND mismatch s=<n> e=<n>" 且 rc=1
#include <cstdio>

#include "tlm/cep/cxl_cache_adapter.hh"
#include "tlm/cep/cxl_state_spec.hh"
#include "tlm/cep/tilelink_state_spec.hh"

using namespace tlm::cep;

int main() {
    for (const auto& cell : TileLinkStateSpec::kT3MappingCells) {
        if (!TileLinkStateSpec::t3_basis_ref_kind_consistent(cell)) {
            std::printf("SRK-02 inconsistency: %s\n", cell.row_ref ? cell.row_ref : "<null>");
            return 1;
        }
    }
    for (std::uint8_t s = 0; s < TileLinkStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < TileLinkStateSpec::kEventCount; ++e) {
            const auto t = TileLinkStateSpec::transition(
                static_cast<TileLinkStateSpec::State>(s),
                static_cast<TileLinkStateSpec::Event>(e));
            if (!TileLinkStateSpec::basis_ref_kind_consistent(t)) {
                std::printf("SRK-02 inconsistency: state=%u event=%u\n", unsigned(s), unsigned(e));
                return 1;
            }
        }
    }
    // ② `Free` ⇔ `transition_is_free` 必须逐格一致(否则「规范明确实现自由」会被
    //    `Unknown` 吞掉 ⇒ 消费端降级语义漂移)。
    for (std::uint8_t s = 0; s < CxlStateSpec::kStateCount; ++s) {
        for (std::uint8_t e = 0; e < CxlStateSpec::kEventCount; ++e) {
            const auto st = static_cast<CxlStateSpec::State>(s);
            const auto ev = static_cast<CxlStateSpec::Event>(e);
            const bool kind_free =
                CxlStateSpec::transition(st, ev).kind == CxlStateSpec::CellKind::Free;
            if (CxlCacheAdapter::transition_is_free(st, ev) != kind_free) {
                std::printf("FREE-KIND mismatch s=%u e=%u\n", unsigned(s), unsigned(e));
                return 1;
            }
        }
    }
    std::printf("SRK clean\n");
    return 0;
}
