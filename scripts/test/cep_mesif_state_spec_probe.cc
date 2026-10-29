// scripts/test/cep_mesif_state_spec_probe.cc
// MESIF 规约判据的**探针** —— 反向验证脚本的载体
//
// 用途(per `scripts/test/cep_mesif_state_spec_reverse_verify.sh`):
//   把 `mesif_state_spec.hh` 的**毒化副本**放进 shadow 目录,用本探针编译并运行 ⇒
//   毒化必须让探针**失败**(编译期锁报红 或 运行期谓词为假)⇒ 证明判据**有牙**。
//
// ⚠️ 本探针**只调用载体里的谓词**(判据逻辑**不**在此复写)⇒ 探针与 ctest 判据同源。
//
// 作者: CppTLM Team / 日期: 2026-10-29

#include "tlm/cep/mesif_state_spec.hh"

#include <cstdio>

namespace {

    using Spec = tlm::cep::MesifStateSpec;

    int g_fail = 0;

    void check(const char* name, bool ok) {
        if (!ok) {
            std::printf("PROP:%s:FAIL\n", name);
            ++g_fail;
        }
    }

} // namespace

int main() {
    const Spec::Table& t = Spec::kTable;

    check("table_is_complete", Spec::table_is_complete(t));
    check("projection_is_complete", Spec::projection_is_complete(t));
    check("basis_is_consistent", Spec::basis_is_consistent(t));
    check("write_rule_holds", Spec::write_rule_holds(t));
    check("rank_rule_holds", Spec::rank_rule_holds(t));
    check("deviations_are_consistent", Spec::deviations_are_consistent(t));
    check(
        "transients_have_no_projection",
        Spec::transients_have_no_projection(t, Spec::kTransientStates, Spec::kTransientStateCount));

    check("pending_count", Spec::pending_count(t) == Spec::kExpectedPendingCount);
    check("primary_count", Spec::primary_count(t) == Spec::kExpectedPrimaryCount);
    check("structural_count",
          Spec::kind_count(t, Spec::BasisKind::Structural) == Spec::kExpectedStructuralCount);
    check("derived_count",
          Spec::kind_count(t, Spec::BasisKind::Derived) == Spec::kExpectedDerivedCount);
    check("analogical_count",
          Spec::kind_count(t, Spec::BasisKind::Analogical) == Spec::kExpectedAnalogicalCount);
    check("transition_primary_count",
          Spec::transition_primary_count(t) == Spec::kExpectedTransitionPrimary);
    check("projection_primary_count",
          Spec::projection_primary_count(t) == Spec::kExpectedProjectionPrimary);

    // 关键单点(语义「锚」)
    check("E_plus_Wr_to_M", Spec::transition(Spec::State::E, Spec::Event::Wr) ==
                                static_cast<std::uint8_t>(Spec::State::M));
    check("shared_write_unchanged", Spec::transition(Spec::State::S, Spec::Event::Wr) ==
                                        static_cast<std::uint8_t>(Spec::State::S));
    check("forward_write_unchanged", Spec::transition(Spec::State::F, Spec::Event::Wr) ==
                                         static_cast<std::uint8_t>(Spec::State::F));
    check("F_projection_is_writeback", Spec::kTable.projection[4] == Spec::Perm::WRITEBACK);
    check("F_projection_basis_is_primary",
          Spec::kTable.projection_basis[4].kind == Spec::BasisKind::Primary &&
              Spec::has_archforge_anchor(Spec::kTable.projection_basis[4].ref));

    if (g_fail != 0) {
        std::printf("PROBE:FAIL(%d)\n", g_fail);
        return 1;
    }
    std::printf("PROBE:PASS\n");
    return 0;
}
