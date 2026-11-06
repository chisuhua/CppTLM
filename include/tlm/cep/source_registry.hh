// include/tlm/cep/source_registry.hh
// 来源类型注册表 —— 把 `07 号` SU 2026-11-06 · 四 D-1 的「效力不可机器判」补成可机器判。
//
// 功能:
//   1. `RefKind` 三档唯一真源(SpecClause / PublicNonnormative / RepoDerived);
//   2. `kSources[]` 编译期查找表(与 `sources.json` 逐条一致,由
//      `test/test_cep_source_registry.cc` 比对);
//   3. `resolve()` 把 source id 解析为 {kind, registered}。
//
// 纪律:
//   · ⛔ 本头**不**定义任何载体的 basis 枚举 —— 4 载体语义不同,映射由各载体
//     `basis_ref_kind_consistent()` 按**载体**规则断言(见 change design D-2);
//   · ⛔ 未登记 id 一律返回 `registered=false` + `RepoDerived`(fail-closed,由测试判红)。
//
// 关联: `07 号` SU 2026-11-06 · 四(台账 Entry #249)· `ADR-CEP-12` SU 2026-11-06 ·
//       `ADR-CEP-11`(版本锁定)· change `cpptlm-cep-basis-source-kind-and-cell-free`
#ifndef CPPT_LM_CEP_SOURCE_REGISTRY_HH
#define CPPT_LM_CEP_SOURCE_REGISTRY_HH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tlm::cep {

/// 证据来源类型(三档,per `07 号` SU · 四 D-1)。
enum class RefKind : std::uint8_t {
    SpecClause = 0,         ///< 规范正文 §/Table/Figure 级 ⇒ 可计 `Direct`
    PublicNonnormative = 1, ///< 公开非规范来源(论文/内核文档/厂商补丁/webinar)⇒ ⛔ 不计入 B3b
    RepoDerived = 2         ///< 本仓/跨仓已签锚点或本仓推导
};

/// 单条来源登记。`id` 同时是载体 `ref` 的来源前缀。
struct SourceEntry {
    std::string_view id;
    RefKind kind;
    std::string_view title;
    std::string_view locator;
    std::string_view sha256;   ///< 完整值或已取字节的截断摘要;取不到时为 "n/a"
    std::string_view retrieved; ///< 抓取 / 签批日期(ISO 8601)
    std::string_view license;
};

/// 注册表(与 `sources.json` 逐条一致 —— 顺序、id、kind、title 均须对齐)。
inline constexpr std::array<SourceEntry, 8> kSources{{
    {"tilelink-c", RefKind::SpecClause,
     "SiFive TileLink Specification (free and open standard)",
     "https://static.dev.sifive.com/docs/tilelink/tilelink-spec-1.7.1.pdf",
     "sha256:30c09b5f…a2dd", "2026-11-02", "© SiFive（只记条款定位，不复制原文）"},
    {"tilelink-t3-anchor", RefKind::SpecClause,
     "TileLink-C T3 映射锚点摘要（规范条款结论的仓内转述，不复制原文）",
     "docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md §2/§4",
     "n/a（仓内摘要，条款定位见文档）", "2026-11-06", "仓内（转述 SiFive 条款定位）"},
    {"cxl-d1", RefKind::PublicNonnormative,
     "Tan / Donaldson / Wickerson, Formalising CXL Cache Coherence (ASPLOS'25)",
     "arXiv:2410.15908v2", "sha256:n/a（pin=strong，2026-11-01 已取字节）",
     "2026-11-01", "CC-BY-4.0"},
    {"cxl-d2", RefKind::PublicNonnormative,
     "Sharma / Blankenship / Berger, An Introduction to the CXL Interconnect (ACM CSUR 2024)",
     "arXiv:2306.11227v3", "sha256:n/a（pin=strong，许可 absent）",
     "2026-11-01", "许可 absent（元数据/页脚均无声明）"},
    {"archforge", RefKind::RepoDerived,
     "ArchForge 真源文档（32 号 / 42 号）",
     "ArchForge:docs/soc_arch/…（跨仓已签锚点）", "n/a（仓内文档）",
     "2026-10-25", "仓内"},
    {"ualink-c10", RefKind::RepoDerived,
     "ADR-CEP-04 SU 2026-11-06 的 ualink_4 锚点 C-10（非规范公开来源支撑的建模声明）",
     "ADR-CEP-04 §Status Update 2026-11-06", "n/a（仓内已签锚点）",
     "2026-11-06", "仓内"},
    {"ualink-pending", RefKind::PublicNonnormative,
     "UALink INC 待核白名单 N-1 ~ N-9（Evaluation Copy 约束，未读正文）",
     "ADR-CEP-04 / ualink_inc_state_spec pending 白名单", "n/a（禁读正文）",
     "2026-11-04", "受限（Evaluation Copy）"},
    {"repo", RefKind::RepoDerived,
     "本仓推导（无外部锚点 / 缺失项编号 R-n）",
     "n/a", "n/a", "2026-11-06", "仓内"},
}};

/// 按 id 查登记条目;未登记返回 nullptr。
constexpr const SourceEntry* find_source(std::string_view id) noexcept {
    for (const auto& s : kSources) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

/// 解析结果(未登记 ⇒ `registered=false`,`kind=RepoDerived` fail-closed)。
struct SourceResolution {
    std::string_view id;
    RefKind kind;
    bool registered;
};

constexpr SourceResolution resolve(std::string_view id) noexcept {
    const SourceEntry* e = find_source(id);
    return SourceResolution{id, e != nullptr ? e->kind : RefKind::RepoDerived, e != nullptr};
}

constexpr bool starts_with(std::string_view s, std::string_view prefix) noexcept {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

constexpr bool contains(std::string_view s, std::string_view needle) noexcept {
    return s.find(needle) != std::string_view::npos;
}

}  // namespace tlm::cep

#endif  // CPPT_LM_CEP_SOURCE_REGISTRY_HH
