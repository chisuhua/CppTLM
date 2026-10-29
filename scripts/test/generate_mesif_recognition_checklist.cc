// scripts/generate_mesif_recognition_checklist.cc
// MESIF 逐格追认清单的**表格生成器** —— 从规范性载体 `mesif_state_spec.hh` 直接产出
//
// 用途(per `scripts/generate_mesif_recognition_checklist.sh`):
//   把 `docs/implementation/2026-10-29-cep-mesif-state-spec-recognition-checklist.md`
//   的**三张表**(转换表 / 投影表 / 偏离清单)**按载体重新生成** ⇒ 清单与载体**零漂移**。
//   ⇒ 「由载体生成」是可复现的,不是一次性的人工产出。
//
// ⚠️ 输出**只含表格**(散文段落由人工维护)⇒ 生成器**不**覆盖人工内容。
//
// 作者: CppTLM Team / 日期: 2026-10-29

#include "tlm/cep/mesif_state_spec.hh"

#include <cstdio>
#include <string>

namespace {

    using Spec = tlm::cep::MesifStateSpec;

    const char* yesno(bool b) {
        return b ? "yes" : "no";
    }

    std::string need(Spec::BasisKind k) {
        switch (k) {
        case Spec::BasisKind::Primary:
            return "**无需追认**(一手原文)—— 仅需确认该原文未被推翻";
        case Spec::BasisKind::Structural:
            return "确认**裁剪**成立(MOESI 6×6 模板行 → MESIF 5 态 / Forward 语义)";
        case Spec::BasisKind::Derived:
            return "确认**推导**成立(MESIF 目录 / 本地-远端事件语义)";
        case Spec::BasisKind::Analogical:
            return "确认**类比**成立(ArchForge Owner 唯一性约束 + CHI 同型先例)";
        }
        return "?";
    }

    const char* perm_name(Spec::Perm p) {
        switch (p) {
        case Spec::Perm::NONE:
            return "NONE";
        case Spec::Perm::READ:
            return "READ";
        case Spec::Perm::UNIQUE:
            return "UNIQUE";
        case Spec::Perm::WRITEBACK:
            return "WRITEBACK";
        }
        return "?";
    }

} // namespace

int main() {
    // ── 转换表 ────────────────────────────────────────────────────────
    std::printf("===MESHIF:transition===\n");
    std::printf("| # | 态 | 事件 | 目标态 | kind | pending | ArchForge 需确认什么 |\n");
    std::printf("|---|---|---|---|---|---|---|\n");
    int n = 0;
    for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
        for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
            const auto& b = Spec::kTable.transition_basis[s][e];
            const std::uint8_t to = Spec::kTable.transition[s][e];
            std::printf("| %d | `%s` | `%s` | `%s` | `%s` | %s | %s |\n", ++n,
                        Spec::state_name(static_cast<Spec::State>(s)),
                        Spec::event_name(static_cast<Spec::Event>(e)),
                        Spec::state_name(static_cast<Spec::State>(to)),
                        Spec::basis_kind_name(b.kind), yesno(b.pending), need(b.kind).c_str());
        }
    }

    // ── 投影表 ────────────────────────────────────────────────────────
    std::printf("===MESHIF:projection===\n");
    std::printf("| # | 态 | perm | kind | pending | ArchForge 需确认什么 |\n");
    std::printf("|---|---|---|---|---|---|\n");
    for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
        const auto& b = Spec::kTable.projection_basis[s];
        std::printf("| %zu | `%s` | `%s` | `%s` | %s | %s |\n", s + 1,
                    Spec::state_name(static_cast<Spec::State>(s)),
                    perm_name(Spec::kTable.projection[s]), Spec::basis_kind_name(b.kind),
                    yesno(b.pending), need(b.kind).c_str());
    }

    // ── 偏离清单 ──────────────────────────────────────────────────────
    std::printf("===MESHIF:deviations===\n");
    std::printf("| # | 态 | 事件 | 模板取值 | 本表取值 | 偏离理由 |\n");
    std::printf("|---|---|---|---|---|---|\n");
    for (std::size_t i = 0; i < Spec::kDeviationCount; ++i) {
        const auto& d = Spec::kDeviations[i];
        const std::size_t si = static_cast<std::size_t>(d.state);
        const std::size_t ei = static_cast<std::size_t>(d.event);
        const char* actual =
            Spec::state_name(static_cast<Spec::State>(Spec::kTable.transition[si][ei]));
        std::printf("| %zu | `%s` | `%s` | `%s` | `%s` | %s |\n", i + 1, Spec::state_name(d.state),
                    Spec::event_name(d.event), d.template_target, actual, d.reason);
    }
    return 0;
}
