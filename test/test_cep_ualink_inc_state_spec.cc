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

    // ⑥ pending 白名单:恰 9 项 · id 与 ref 一一对应
    REQUIRE(kPendingCount == 9u);
    REQUIRE(kPendingSemanticItems.size() == 9u);
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

    // ⑧ 5 格 pending + 5 格 self-derived(与编译期断言同形)
    REQUIRE(obligation_is_pending(Obligation::Uis04));
    REQUIRE(obligation_is_pending(Obligation::Uis05));
    REQUIRE(obligation_is_pending(Obligation::Uis06));
    REQUIRE(obligation_is_pending(Obligation::Uis08));
    REQUIRE(obligation_is_pending(Obligation::Uis09));
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
    //    ③ `Pending` 却引仓内锚点(拿已定稿冒充待定)
    REQUIRE_FALSE(basis_kind_matches_ref_class(BasisKind::Pending, "C-9"));
    //    正控:合规组合必须判真
    REQUIRE(basis_kind_matches_ref_class(BasisKind::SelfDerived, "C-1"));
    REQUIRE(basis_kind_matches_ref_class(BasisKind::Pending, "N-3"));
    REQUIRE(basis_kind_matches_ref_class(BasisKind::Pending, "C-6 N-1")); // INC 混合格

    // ⑤ 逐格:5 格 pending / 6 格 self-derived(含 UIS-11)
    REQUIRE(obligation_is_pending(Obligation::Uis04));
    REQUIRE(obligation_is_pending(Obligation::Uis05));
    REQUIRE(obligation_is_pending(Obligation::Uis06));
    REQUIRE(obligation_is_pending(Obligation::Uis08));
    REQUIRE(obligation_is_pending(Obligation::Uis09));
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
    // ② id 唯一且落在 N1 ~ N9 内(**不**要求 id == 数组下标)
    REQUIRE(pending_ids_unique_and_in_range(kPendingSemanticItems));
    // ③ id ↔ ref 一一对应
    REQUIRE(pending_refs_match_ids(kPendingSemanticItems));
    // ④ 正控 + **判否形态**(负向样本 ⇒ 判定函数有牙;照队列 #13 的
    //    `all_pending_items_wellformed(items)` **带参**形态)
    const std::array<PendingItem, 1> bad_ref{{PendingItem{PendingId::N1, "N-2", "x"}}};
    const std::array<PendingItem, 2> dup_id{
        {PendingItem{PendingId::N1, "N-1", "x"}, PendingItem{PendingId::N1, "N-1", "y"}}};
    const std::array<PendingItem, 1> ok_id{{PendingItem{PendingId::N9, "N-9", "x"}}};
    REQUIRE_FALSE(pending_refs_match_ids(bad_ref));
    REQUIRE_FALSE(pending_ids_unique_and_in_range(dup_id));
    REQUIRE(pending_ids_unique_and_in_range(ok_id)); // 正控:越界样本本身 id 合法
}

// ══════════════════════════════════════════════════════════════════════════
// 自足性 —— **禁项清单**的文本扫描(编译期锁的运行期 counterpart)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("UIS-10: 载体自足 —— 只 include 标准库 + 零平台标识 + 零区域表 + 零转移判定",
          "[cep][ualink-inc][self-contained]") {
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE_FALSE(raw.empty());

    // ① 只 include **标准库**(尖括号形式)⇒ 出现引号形式的项目内头即红
    const auto includes = extract_include_lines(raw);
    REQUIRE_FALSE(includes.empty());
    for (const auto& inc : includes) {
        INFO("include: " << inc);
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
