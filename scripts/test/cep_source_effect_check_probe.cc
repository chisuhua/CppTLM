// scripts/test/cep_source_effect_check_probe.cc
// `cep_source_effect_check.sh` 的编译探针 —— 验证 basis ↔ ref_kind 映射有鉴别力。
//
// clean   : 真源 ⇒ 打印 "SRK clean" 且 rc=0
// downgraded: 影子头把 `tilelink-c` 的 kind 改为 PublicNonnormative ⇒
//             T3 `Direct` 行不再一致 ⇒ 打印 "SRK-02 inconsistency: <row>" 且 rc=1
#include <cstdio>

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
    std::printf("SRK clean\n");
    return 0;
}
