// =============================================================================
// test/test_cep_ualink_inc_state_spec.cc
//   —— 队列 #14 **规约层** change `cpptlm-cep-ualink-inc-state-spec` 的**运行期兜底判据**
//
// 作者: CppTLM Team / 日期: 2026-11-04
// 配套: include/tlm/cep/ualink_inc_state_spec.hh(被检载体)
//       openspec/changes/cpptlm-cep-ualink-inc-state-spec/(4 件套)
//       docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md(basis 唯一人工审计面)
//       台账 obj-cep-platform.md Entry #194 / #195
//
// ── 判据纪律(队列 #12 沉淀的 4 铁律)──────────────────────────────────────
//   ① 纯负向断言须配正控 ② 每条判据须有可执行反向 fixture 且**因正确原因红**
//   ③ **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 本文件是编译期锁的**运行期见证**
//   ④ fixture 状态变迁后须重新基线化
//
// ── ⚠️ 本文件只覆盖 Phase 1(载体形态定稿)—— 判据面 / 反向 fixture 属 Phase 2 ──
// =============================================================================

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/ualink_inc_state_spec.hh"

namespace {

    using namespace tlm::cep::ualink_inc;

    constexpr const char* kCarrierRelPath = "include/tlm/cep/ualink_inc_state_spec.hh";

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// **去注释**(行 + 块)—— 否则注释里提到被禁标识符会假阳性
    std::string strip_cpp_comments(const std::string& src) {
        std::string out;
        bool in_line = false;
        bool in_block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (!in_line && !in_block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                in_line = true;
                ++i;
                continue;
            }
            if (!in_line && !in_block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                in_block = true;
                ++i;
                continue;
            }
            if (in_line && src[i] == '\n') {
                in_line = false;
                out.push_back('\n');
                continue;
            }
            if (in_block && src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                in_block = false;
                ++i;
                continue;
            }
            if (!in_line && !in_block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// 提取 `#include` 行(只取引号形式的内容)
    std::vector<std::string> extract_include_lines(const std::string& src) {
        std::vector<std::string> out;
        std::size_t pos = 0;
        while (true) {
            const std::size_t at = src.find("#include", pos);
            if (at == std::string::npos)
                break;
            std::size_t i = at + 8;
            while (i < src.size() && (src[i] == ' ' || src[i] == '\t'))
                ++i;
            if (i < src.size() && (src[i] == '<' || src[i] == '"')) {
                const char close = (src[i] == '<') ? '>' : '"';
                const std::size_t end = src.find(close, i + 1);
                if (end != std::string::npos) {
                    out.push_back(src.substr(i, end - i + 1));
                    pos = end;
                    continue;
                }
            }
            pos = at + 8;
        }
        return out;
    }

    /// 禁项清单(per 本 change 的 `design §2.1`)
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> kTokens{"CollectiveOp",
                                                      "ReduceOp",
                                                      "CollectiveEvt",
                                                      "Status::",
                                                      "LookupResult",
                                                      "TrackerImpl",
                                                      "CollectivePlaneIf",
                                                      "LocalCollectivePlane",
                                                      "kCollectivePlaneRegistry",
                                                      "tlm::cep::",
                                                      "collective_region_table",
                                                      "transition_",
                                                      "is_silent_upgrade"};
        return kTokens;
    }

    /// 返回**命中**的禁项(空 = 未命中)—— 供正控与毒化自测**共用**同一实现
    std::vector<std::string> scan_forbidden(const std::string& stripped_src) {
        std::vector<std::string> hits;
        for (const auto& t : forbidden_tokens()) {
            if (stripped_src.find(t) != std::string::npos)
                hits.push_back(t);
        }
        return hits;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// 载体形态(Phase 1)—— 编译期锁的**运行期见证**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIS-01/UIS-02/UIS-03/UIS-07/UIS-10: 载体的编译期锁在运行期仍成立", "[cep][ualink-inc]") {
    // ① INC 原语恰 4
    REQUIRE(kIncOpCount == 4u);
    REQUIRE(kIncOps.size() == 4u);

    // ② AllReduce = REDUCE + BROADCAST 的复合(⛔ 不是第 5 原语)
    REQUIRE(inc_op_primitive_count(IncOp::AllReduce) == 2u);
    REQUIRE(inc_op_primitive_name(IncOp::AllReduce, 0u) == "REDUCE");
    REQUIRE(inc_op_primitive_name(IncOp::AllReduce, 1u) == "BROADCAST");
    // 其余 3 个各 1 个
    REQUIRE(inc_op_primitive_count(IncOp::ReadReduce) == 1u);
    REQUIRE(inc_op_primitive_count(IncOp::WriteMulticast) == 1u);
    REQUIRE(inc_op_primitive_count(IncOp::Broadcast) == 1u);
    // 越界 ⇒ 空串
    REQUIRE(inc_op_primitive_name(IncOp::ReadReduce, 1u).empty());

    // ③ 映射不越出载体用到的原语名(3 个)
    //    ⚠️ 本断言只陈述「本载体用到的名字恰 3 个」这一**事实**;
    //    「不是平台词表的副本」需「载体集 ⊊ 平台集」⇒ 载体**不可自证**
    //    ⇒ 由**跨载体**的 X-1 判(per design §2.4)。
    REQUIRE(kUsedPrimitiveNames.size() == 3u);
    REQUIRE(inc_op_mapping_within_used_names(IncOp::ReadReduce));
    REQUIRE(inc_op_mapping_within_used_names(IncOp::WriteMulticast));
    REQUIRE(inc_op_mapping_within_used_names(IncOp::Broadcast));
    REQUIRE(inc_op_mapping_within_used_names(IncOp::AllReduce));

    // ④ 声明锁
    REQUIRE(kOperatorFamilyDefinedInCarrier == false);
    REQUIRE(kDagRerunIsCertificate == false);
    REQUIRE(kRegionTableExposed == false);

    // ⑤ 形状型谓词的极性
    REQUIRE(partial_failure_must_be_explicit(false));
    REQUIRE_FALSE(partial_failure_must_be_explicit(true));
    REQUIRE(vc_inheritance_is_well_defined(true));
    REQUIRE_FALSE(vc_inheritance_is_well_defined(false));

    // ⑥ pending 白名单:恰 **7** 项(2026-11-06:`N-8` / `N-9` 转正 ⇒ 9 → 7)· id 与 ref 一一对应
    REQUIRE(kPendingCount == 7u);
    REQUIRE(kPendingSemanticItems.size() == 7u);
    REQUIRE(pending_ids_unique_and_in_range(kPendingSemanticItems));
    REQUIRE(pending_refs_match_ids(kPendingSemanticItems));
    for (const auto& it : kPendingSemanticItems) {
        INFO("pending ref: " << std::string(it.ref));
        REQUIRE_FALSE(it.statement.empty());
    }

    // ⑦ basis 完整性:每格有 ref · self-derived 不指向公开来源
    REQUIRE(all_basis_cells_have_ref());
    REQUIRE(all_self_derived_avoid_public_source());
    REQUIRE(kBasisObligations.size() == kObligationCount);
    REQUIRE(kBasisIncOps.size() == kIncOpBasisCount);

    // ⑧ **3** 格 pending + **7** 格 self-derived(与编译期断言同形)
    //    ⚠️ 2026-11-06(per `ADR-CEP-04` SU 2026-11-06):`Uis08` / `Uis09` 由 pending 转 self-derived
    //    (basis = `C-10` = 「非规范公开来源支撑的建模声明」,⛔ 非「有规范依据」)
    REQUIRE(obligation_is_pending(Obligation::Uis04));
    REQUIRE(obligation_is_pending(Obligation::Uis05));
    REQUIRE(obligation_is_pending(Obligation::Uis06));
    REQUIRE_FALSE(obligation_is_pending(Obligation::Uis08));
    REQUIRE_FALSE(obligation_is_pending(Obligation::Uis09));
    REQUIRE(obligation_is_self_derived(Obligation::Uis08));
    REQUIRE(obligation_is_self_derived(Obligation::Uis09));
    REQUIRE(obligation_is_self_derived(Obligation::Uis01));
    REQUIRE(obligation_is_self_derived(Obligation::Uis02));
    REQUIRE(obligation_is_self_derived(Obligation::Uis03));
    REQUIRE(obligation_is_self_derived(Obligation::Uis07));
    REQUIRE(obligation_is_self_derived(Obligation::Uis10));
    REQUIRE(obligation_is_self_derived(Obligation::Uis11));
}

// ══════════════════════════════════════════════════════════════════════════
// UIS-04/05/06/08/09 + UIS-10 —— 逐格 `basis` 的 `kind` ↔ `ref` **类别自洽**
//   (判据**有牙**:三种冒充必须被判定函数判否 ⇒ 否则「遍历表断言」无鉴别力)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIS-10: 逐格 basis 的 kind 与 ref 类别自洽 —— 三种冒充必须被判否",
          "[cep][ualink-inc][basis]") {
    // ① **token 级类别分类**(⛔ 不用子串 —— 短 token 会误伤)
    REQUIRE(classify_ref_token("C-2") == RefClass::Contract);
    REQUIRE(classify_ref_token("N-3") == RefClass::PendingRef);
    REQUIRE(classify_ref_token("A-1") == RefClass::PublicSource);
    REQUIRE(classify_ref_token("X-1") == RefClass::Unknown);
    REQUIRE(classify_ref_token("C") == RefClass::Unknown);

    // ② `ref` 的多 token 归类
    REQUIRE(ref_contains_class("C-1 C-8 C-9", RefClass::Contract));
    REQUIRE_FALSE(ref_contains_class("C-1 C-8 C-9", RefClass::PendingRef));
    REQUIRE(ref_only_class("C-1 C-8 C-9", RefClass::Contract));
    REQUIRE_FALSE(ref_only_class("C-6 N-1", RefClass::Contract)); // 混合 ⇒ 非纯 Contract
    REQUIRE(ref_contains_class("C-6 N-1", RefClass::PendingRef)); // 混合 ⇒ 含 pending
    REQUIRE_FALSE(ref_contains_class("C-6 N-1", RefClass::PublicSource));

    // ③ **遍历真实表格**(⛔ 不是字面量)—— 两张表都自洽
    REQUIRE(all_obligations_kind_match_ref_class());
    REQUIRE(all_inc_op_basis_kind_match_ref_class());

    // ④ ⭐ **三种冒充必须被判否**(证明判定函数有牙 ⇒ 遍历表断言才有鉴别力)
    //    ① `SelfDerived` 却引 pending 锚点
    REQUIRE_FALSE(basis_kind_matches_ref_class(BasisKind::SelfDerived, "N-3"));
    //    ② `Pending` 却引公开来源(拿公开来源冒充 pending 的理由)
    REQUIRE_FALSE(basis_kind_matches_ref_class(BasisKind::Pending, "A-1"));
    //    ③ `Pending` 却**纯**引仓内锚点(无 `N-` ⇒ 拿已定稿冒充待定)
    REQUIRE_FALSE(basis_kind_matches_ref_class(BasisKind::Pending, "C-9"));
    //    ④ `Pending` 格含**未知类** token ⇒ 判否(收紧后的实现)
    REQUIRE_FALSE(basis_kind_matches_ref_class(BasisKind::Pending, "N-3 X-9"));
    //    ⑤ 正控:`Pending` 格**附加** `C-` token(名字来源)仍合法
    REQUIRE(basis_kind_matches_ref_class(BasisKind::Pending, "C-6 N-1"));
    //    ⑥ 无编号 token(`C-`)⇒ 未知类 ⇒ 判否(不误认作已签锚点)
    REQUIRE(classify_ref_token("C-") == RefClass::Unknown);
    REQUIRE(classify_ref_token("N-") == RefClass::Unknown);
    //    正控:合规组合必须判真
    REQUIRE(basis_kind_matches_ref_class(BasisKind::SelfDerived, "C-1"));
    REQUIRE(basis_kind_matches_ref_class(BasisKind::Pending, "N-3"));
    REQUIRE(basis_kind_matches_ref_class(BasisKind::Pending, "C-6 N-1")); // INC 混合格

    // ⑤ 逐格:**3** 格 pending / **8** 格 self-derived(含 UIS-11)
    //    ⚠️ 2026-11-06:`Uis08` / `Uis09` 转 self-derived(per `ADR-CEP-04` SU 2026-11-06)
    REQUIRE(obligation_is_pending(Obligation::Uis04));
    REQUIRE(obligation_is_pending(Obligation::Uis05));
    REQUIRE(obligation_is_pending(Obligation::Uis06));
    REQUIRE_FALSE(obligation_is_pending(Obligation::Uis08));
    REQUIRE_FALSE(obligation_is_pending(Obligation::Uis09));
    REQUIRE(obligation_is_self_derived(Obligation::Uis08));
    REQUIRE(obligation_is_self_derived(Obligation::Uis09));
    REQUIRE_FALSE(obligation_is_pending(Obligation::Uis11));
    REQUIRE(obligation_is_self_derived(Obligation::Uis11));

    // ⑥ UIS-11 的 basis 格有 ref
    REQUIRE(basis_cell_has_ref(kBasisObligations[static_cast<std::size_t>(Obligation::Uis11)].ref));
    REQUIRE(kBasisObligations.size() == kObligationCount);
}

TEST_CASE("UIS-10: pending 白名单的**可逆性** —— id 唯一且在范围内(不强制重编号)",
          "[cep][ualink-inc][pending]") {
    // ① 自洽型:表大小 == kPendingCount
    REQUIRE(kPendingSemanticItems.size() == kPendingCount);
    // ② id 唯一且落在 N1 ~ N7 内(**不**要求 id == 数组下标)—— 2026-11-06:`N-8`/`N-9` 转正 ⇒ 上界 `N7`
    REQUIRE(pending_ids_unique_and_in_range(kPendingSemanticItems));
    // ③ id ↔ ref 一一对应
    REQUIRE(pending_refs_match_ids(kPendingSemanticItems));
    // ④ 正控 + **判否形态**(负向样本 ⇒ 判定函数有牙;照队列 #13 的
    //    `all_pending_items_wellformed(items)` **带参**形态)
    const std::array<PendingItem, 1> bad_ref{{PendingItem{PendingId::N1, "N-2", "x"}}};
    const std::array<PendingItem, 2> dup_id{
        {PendingItem{PendingId::N1, "N-1", "x"}, PendingItem{PendingId::N1, "N-1", "y"}}};
    const std::array<PendingItem, 1> ok_id{{PendingItem{PendingId::N7, "N-7", "x"}}};
    REQUIRE_FALSE(pending_refs_match_ids(bad_ref));
    REQUIRE_FALSE(pending_ids_unique_and_in_range(dup_id));
    REQUIRE(pending_ids_unique_and_in_range(ok_id)); // 正控:越界样本本身 id 合法
}


// ══════════════════════════════════════════════════════════════════════════
// UIS-11 —— 证书规约面(四条证书的逐边依据 + 参照域 + `IC1` 边界 + U-1/U-2 登记)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIS-11: 证书规约面齐备,且 ⛔ 不得以「重跑 DAG」为判据",
          "[cep][ualink-inc][certificate]") {
    // ① 四条证书齐备 + 顺序稳定
    REQUIRE(kCertificateCount == 4u);
    REQUIRE(kCertificateSpecs.size() == 4u);
    REQUIRE(kCertificateSpecs[0].id == CertificateId::C1Prime);
    REQUIRE(kCertificateSpecs[1].id == CertificateId::C2);
    REQUIRE(kCertificateSpecs[2].id == CertificateId::C3);
    REQUIRE(kCertificateSpecs[3].id == CertificateId::C4);
    REQUIRE(all_certificate_specs_wellformed());
    // 每行的 detail 子标记与 ADR E.3.1 一致(⛔ 不新造)
    REQUIRE(kCertificateSpecs[0].detail_tag == "/C-1\xe2\x80\xb2");
    REQUIRE(kCertificateSpecs[1].detail_tag == "/C-2");
    REQUIRE(kCertificateSpecs[2].detail_tag == "/C-3");
    REQUIRE(kCertificateSpecs[3].detail_tag == "/C-4");

    // ② C-1′ 的**参照域 = 两域并集**(per E.3.3)
    REQUIRE(kCertificateSpecs[0].domains == (kDomainArchForgeDerived | kDomainRepoOwned));
    REQUIRE(kDomainArchForgeDerived != kDomainRepoOwned);

    // ③ **映射不一致 ⇒ 不得用 M-2**(双向 = 正控 + 负控)
    REQUIRE(m2_usable_on_network(true));
    REQUIRE_FALSE(m2_usable_on_network(false));

    // ④ ⛔ 「重跑 DAG」**不是**证书判据(实测恒真)
    REQUIRE(kDagRerunIsCertificate == false);

    // ⑤ `IC1` 的鉴别力 = **三项**;对 `vc_binding` 的**语义分配**零鉴别力
    REQUIRE(kIc1Discrimination.edge_endpoints_bound);
    REQUIRE(kIc1Discrimination.vc_id_in_range);
    REQUIRE(kIc1Discrimination.declared_edge_table_dag);
    REQUIRE_FALSE(kIc1Discrimination.vc_binding_semantics);

    // ⑥ U-1 / U-2 登记(⛔ 不得冒充成「已验证 / 已满足」)
    REQUIRE(kUntestedCount == 2u);
    REQUIRE(kUntestedItems.size() == 2u);
    REQUIRE(kUntestedItems[0].ref == "U-1");
    REQUIRE(kUntestedItems[1].ref == "U-2");
    REQUIRE(all_untested_not_satisfied());
    // **判否形态**(负向样本 ⇒ 判定函数有牙)
    REQUIRE_FALSE(untested_is_not_satisfied("该假设已验证"));
    REQUIRE_FALSE(untested_is_not_satisfied("本项已满足"));
    // 正控
    REQUIRE(untested_is_not_satisfied("永远无法被检验"));
}

// ══════════════════════════════════════════════════════════════════════════
// 自足性 —— **禁项清单**的文本扫描(编译期锁的运行期 counterpart)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIS-10: 载体自足 —— 只 include 标准库 + 零平台标识 + 零区域表 + 零转移判定",
          "[cep][ualink-inc][self-contained]") {
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE_FALSE(raw.empty());

    // ① 只 include **标准库**(尖括号形式)⇒ 出现引号形式的项目内头即红
    // ⚠️ 唯一白名单(2026-11-06,per 台账 Entry #252):`source_registry.hh` 是
    //    纯 constexpr、只依赖标准库的来源注册表(来源类型单一真源);载体引用它
    //    **不**引入平台标识 / 区域表 / 转移判定 ⇒ 不破坏「自足」的实质。
    const auto includes = extract_include_lines(raw);
    REQUIRE_FALSE(includes.empty());
    for (const auto& inc : includes) {
        INFO("include: " << inc);
        if (inc == "\"tlm/cep/source_registry.hh\"") {
            continue;
        }
        REQUIRE(inc.front() == '<'); // 项目内头是引号形式 ⇒ 出现即红
    }

    // ② 禁项清单(**去注释后**)—— 零命中
    const std::string stripped = strip_cpp_comments(raw);
    const auto hits = scan_forbidden(stripped);
    for (const auto& h : hits) {
        INFO("forbidden token: " << h);
    }
    REQUIRE(hits.empty());

    // ③ ⭐ **鉴别力自测**:同一扫描器对**毒化文本**必须命中
    //    (否则「零命中」可能只是**扫描器瞎了** ⇒ 该判据无鉴别力)
    const std::string poisoned = stripped + "\n// x\nCollectiveOp poisoned;\n";
    const auto poisoned_hits = scan_forbidden(poisoned);
    REQUIRE_FALSE(poisoned_hits.empty());
    REQUIRE(std::find(poisoned_hits.begin(), poisoned_hits.end(), "CollectiveOp") !=
            poisoned_hits.end());
}
