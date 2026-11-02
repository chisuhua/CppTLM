// =============================================================================
// test/test_cep_cxl_dcoh_adapter.cc
//   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric` 的 **adapter 判据**
//     (CDX-01 零自有表副本 · CDX-02 委托规约载体 · CDX-03 无 transition API)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/cxl_dcoh_adapter.hh · include/tlm/cep/cxl_dcoh_state_spec.hh
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric/(delta spec CDX-01 ~ CDX-03)
//       docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md
//
// ── 判据口径 ──────────────────────────────────────────────────────────────
// · **零副本扫描器**须**自带鉴别力**:同一个扫描器 SHALll 对**确实存在**的成员字面量/表名
//   **报出**(正控)⇒ 否则「扫描器恒绿」= 判据没生效。
// · ⚠️ **成员字面量 ≠ 类型别名** —— `using DevCacheState = cxl_dcoh::DevCacheState;`
//   是**类型**别名(允许),`DevCacheState::M` 才是**成员**访问(禁止)。
// =============================================================================

#include "catch_amalgamated.hpp"

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/cxl_dcoh_adapter.hh"

namespace Spec = tlm::cep::cxl_dcoh;

namespace {

    namespace ad = tlm::cep;

    /// 去注释(行 + 块)
    std::string strip_comments(const std::string& code) {
        std::string out;
        std::size_t i = 0;
        while (i < code.size()) {
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
                while (i < code.size() && code[i] != '\n')
                    ++i;
                continue;
            }
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
                i += 2;
                while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/'))
                    ++i;
                i = (i + 1 < code.size()) ? i + 2 : code.size();
                out.push_back(' ');
                continue;
            }
            out.push_back(code[i]);
            ++i;
        }
        return out;
    }

    /// **成员字面量**禁词(7 个)—— 类型别名**不在**此列
    const std::vector<std::string>& forbidden_member_literals() {
        static const std::vector<std::string> v = {
            "DevCacheState::",   "DcohViewState::",    "HomeDirState::", "CxlMemChannel::",
            "CxlCacheChannel::", "ChannelDirection::", "Perm::"};
        return v;
    }

    /// **表名**禁词(11 个)—— 本 adapter **不暴露任何表**
    const std::vector<std::string>& forbidden_table_names() {
        static const std::vector<std::string> v = {
            "kPermProjection",          "kDeviceInitiatedSnoopSet", "kCxlMemChannelDirection",
            "kBasisDevCacheState",      "kBasisDcohViewState",      "kBasisHomeDirState",
            "kBasisCxlMemChannel",      "kBasisCxlCacheChannel",    "kBasisPermProjection",
            "kBasisSymmetricPredicate", "kPendingSemanticItems"};
        return v;
    }

    /// 扫描器:返回命中的禁词(空 = 合规)
    std::vector<std::string> scan_zero_copy_violations(const std::string& code) {
        const std::string src = strip_comments(code);
        std::vector<std::string> hits;
        for (const auto& w : forbidden_member_literals()) {
            if (src.find(w) != std::string::npos)
                hits.push_back(w);
        }
        for (const auto& w : forbidden_table_names()) {
            if (src.find(w) != std::string::npos)
                hits.push_back(w);
        }
        return hits;
    }

    std::string read_adapter_source() {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/cxl_dcoh_adapter.hh");
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// CDX-01 —— 零自有表副本
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-01:CxlDcohAdapter 零自有表副本(零成员字面量 + 零表名)", "[cep][cxl-dcoh][adapter]") {
    const std::string src = read_adapter_source();
    REQUIRE_FALSE(src.empty());

    // ① ② 真实载体:零违禁
    const auto hits = scan_zero_copy_violations(src);
    INFO("违禁命中: " << (hits.empty() ? "(无)" : hits.front()));
    CHECK(hits.empty());

    // ③ **类型别名允许**(类型名 ≠ 成员访问)
    const std::string stripped = strip_comments(src);
    CHECK(stripped.find("using DevCacheState = cxl_dcoh::DevCacheState;") != std::string::npos);
    CHECK(stripped.find("using Perm = cxl_dcoh::Perm;") != std::string::npos);

    // ④ ⑤ **正控 + 反向 fixture**:同一扫描器对**确实存在**的违禁 SHALL 报出
    //    ⇒ 证扫描器**有鉴别力**(否则「扫描器恒绿」= 判据没生效)
    {
        const std::string synth = "void f() { auto x = DevCacheState::M; }";
        const auto h = scan_zero_copy_violations(synth);
        CHECK(h.size() == 1u);
        CHECK(h.front() == "DevCacheState::");
    }
    {
        const std::string synth = "static constexpr auto kT = kPermProjection;";
        const auto h = scan_zero_copy_violations(synth);
        CHECK(h.size() == 1u);
        CHECK(h.front() == "kPermProjection");
    }
    {
        // ⚠️ **类型别名** SHALL NOT 被误报(否则扫描器过严 ⇒ 合法代码被判红)
        const std::string synth = "using Perm = cxl_dcoh::Perm;";
        CHECK(scan_zero_copy_violations(synth).empty());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-02 —— 委托规约载体
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-02:CxlDcohAdapter 逐项委托规约载体", "[cep][cxl-dcoh][adapter]") {
    // ① 基数常量逐项相等(委托,非副本)
    CHECK(ad::CxlDcohAdapter::kDevCacheStateCount == Spec::kDevCacheStateCount);
    CHECK(ad::CxlDcohAdapter::kDcohViewStateCount == Spec::kDcohViewStateCount);
    CHECK(ad::CxlDcohAdapter::kHomeDirStateCount == Spec::kHomeDirStateCount);
    CHECK(ad::CxlDcohAdapter::kCxlMemChannelCount == Spec::kCxlMemChannelCount);
    CHECK(ad::CxlDcohAdapter::kCxlCacheChannelCount == Spec::kCxlCacheChannelCount);

    // ② `perm` 投影逐项相等(4→4,仅缓存代理域)
    CHECK(ad::CxlDcohAdapter::perm_of(Spec::DevCacheState::M) == Spec::Perm::WRITEBACK);
    CHECK(ad::CxlDcohAdapter::perm_of(Spec::DevCacheState::E) == Spec::Perm::UNIQUE);
    CHECK(ad::CxlDcohAdapter::perm_of(Spec::DevCacheState::S) == Spec::Perm::READ);
    CHECK(ad::CxlDcohAdapter::perm_of(Spec::DevCacheState::I) == Spec::Perm::NONE);

    // ③ 窄义「对称」判别式逐项委托
    Spec::SymmetricCapability all;
    all.device_originated_bi_snoop = true;
    all.device_side_sf_or_directory = true;
    all.spans_multiple_independent_hosts = true;
    all.p2p_without_host_cpu = true;
    all.snooped_agent_is_caching_host = true;
    CHECK(ad::CxlDcohAdapter::is_cxl3_symmetric(all));
    CHECK_FALSE(ad::CxlDcohAdapter::is_cxl3_symmetric(Spec::SymmetricCapability{}));
    {
        auto c = all;
        c.p2p_without_host_cpu = false;
        CHECK_FALSE(ad::CxlDcohAdapter::is_cxl3_symmetric(c));
    }

    // ④ 名字常量
    CHECK(std::string(ad::CxlDcohAdapter::kAdapterName) == "CxlDcohAdapter");
    CHECK(std::string(ad::CxlDcohAdapter::kInstanceId) == "cxl_3_0_symmetric");
}

// ═══════════════════════════════════════════════════════════════════════════
// CDX-03 —— 不暴露 transition API
// ═══════════════════════════════════════════════════════════════════════════
TEST_CASE("CDX-03:CxlDcohAdapter 不暴露 transition API", "[cep][cxl-dcoh][adapter]") {
    const std::string src = read_adapter_source();
    REQUIRE_FALSE(src.empty());
    const std::string stripped = strip_comments(src);
    // ⚠️ 规约载体**无转移表** ⇒ adapter 提供 `transition_*` 即等于**自造转移表** ⇒ 禁止
    CHECK(stripped.find("transition_") == std::string::npos);
    // ⇒ 载体侧也确认无转移表(双重保险)
    CHECK(stripped.find("transition") == std::string::npos);
}
