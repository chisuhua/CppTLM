// =============================================================================
// test/test_cep_nvlink_sharp_state_spec.cc
//   —— 队列 #13 `cpptlm-cep-nvlink-sharp-state-spec` 规约载体的**最小覆盖**
//     (Phase 1 编译期断言的**运行期见证** + 载体**红线**的可执行检查)
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: include/tlm/cep/nvlink_sharp_state_spec.hh
//       openspec/changes/cpptlm-cep-nvlink-sharp-state-spec/(change 4 件套)
//       台账 obj-cep-platform.md Entry #180(Phase 1)
//
// ── 范围声明(重要)─────────────────────────────────────────────────────────
// ⚠️ 本文件是 **Phase 1 的最小覆盖**,**只提供正控**;
//    ⛔ **不含反向 fixture**(毒化 ⇒ 必红)—— 那是 **Phase 3**(tasks 3.2)的义务。
// ⚠️ 本文件**不新建归因 id**(per tasks 3.6 ②:新增 id 单一登记在 `design §3` + `tasks 3.2`)
//    ⇒ 此处的断言归入**载体自带**的判据(NSS-01 ~ NSS-08 的**运行期等价物**)。
// ⚠️ 「编译期锁」不得只以「编译红」充当 ctest 判据(铁律 ③)
//    ⇒ 载体的 `static_assert` 在此**重复见证**为**运行期**断言。
// =============================================================================

#include "catch_amalgamated.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "tlm/cep/nvlink_sharp_state_spec.hh"

namespace {

    namespace nvs = tlm::cep::nvlink_sharp;

    constexpr const char* kCarrierRelPath = "include/tlm/cep/nvlink_sharp_state_spec.hh";

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// **去注释**(行注释 + 块注释)—— 否则注释里提到被禁标识符会假阳性
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

    /// 取出源文件里的 `#include` 行(去注释后)
    std::vector<std::string> extract_include_lines(const std::string& src) {
        std::vector<std::string> out;
        std::istringstream in(strip_cpp_comments(src));
        std::string line;
        while (std::getline(in, line)) {
            const auto pos = line.find("#include");
            if (pos == std::string::npos)
                continue;
            std::size_t b = line.find_first_of("<\"", pos);
            std::size_t e = line.find_last_of(">");
            if (b == std::string::npos || e == std::string::npos || e <= b)
                continue;
            out.push_back(line.substr(b, e - b + 1));
        }
        return out;
    }

    // ── SFINAE:检测类型是否**没有**某个成员(⇒ 表达「不可表达」)──────────
    template <typename T, typename = void> struct HasVcId : std::false_type {};
    template <typename T>
    struct HasVcId<T, std::void_t<decltype(std::declval<T>().vc_id)>> : std::true_type {};

    template <typename T, typename = void> struct HasReason : std::false_type {};
    template <typename T>
    struct HasReason<T, std::void_t<decltype(std::declval<T>().reason)>> : std::true_type {};

    template <typename T, typename = void> struct HasOwner : std::false_type {};
    template <typename T>
    struct HasOwner<T, std::void_t<decltype(std::declval<T>().owner)>> : std::true_type {};

    template <typename T, typename = void> struct HasRevisitTrigger : std::false_type {};
    template <typename T>
    struct HasRevisitTrigger<T, std::void_t<decltype(std::declval<T>().revisit_trigger)>>
        : std::true_type {};

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// NSS-01 —— 载体**自足**:只 include 标准库;不承载词表型语义;不暴露区域表类型
//   (判据本体在 `ADR-CEP-03 §9` V8 / `CP-05`;本文件是其**运行期等价物**)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-01:载体自足 —— 只 include 标准库,无词表型枚举,无区域表类型",
          "[cep][nvlink-sharp][state-spec]") {
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE(!raw.empty());
    const std::string src = strip_cpp_comments(raw);

    // ① 只 include **标准库**(尖括号形式)⇒ ⛔ 不 include 任何项目内头
    const auto includes = extract_include_lines(raw);
    REQUIRE(!includes.empty());
    for (const auto& inc : includes) {
        INFO("include: " << inc);
        CHECK(inc.front() == '<'); // 项目内头是引号形式 ⇒ 出现即红
    }

    // ② ⛔ 不承载**词表型**语义(唯一真源在 `tlm::cep::ReduceOp` / `CollectiveOp`)
    CHECK(src.find("enum class ReduceOp") == std::string::npos);
    CHECK(src.find("enum class CollectiveOp") == std::string::npos);
    CHECK(src.find("enum class AccessMode") == std::string::npos);
    // ⛔ 也不得以 `using` 别名把平台词表搬进来
    CHECK(src.find("using ReduceOp") == std::string::npos);
    CHECK(src.find("using CollectiveOp") == std::string::npos);

    // ③ ⛔ 不定义 / 暴露**区域表**类型(区域表的运行时状态归实现层)
    CHECK(src.find("struct collective_region_table") == std::string::npos);
    CHECK(src.find("class collective_region_table") == std::string::npos);

    // ④ 编译期锁的**运行期见证**
    static_assert(nvs::kIngressDecisionCount == 2, "");
    CHECK(nvs::kIngressDecisionCount == 2u);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-02 —— R1/R2 分类判据 + **R4 epoch 语义双向**(回归锁)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-02:分类判据 —— 命中且 commit ⇒ divert;其余 ⇒ 放行(R4 回归锁)",
          "[cep][nvlink-sharp][state-spec]") {
    // 分类:命中 + commit ⇒ divert
    CHECK(nvs::classify_at_fabric_ingress(true, true) == nvs::IngressDecision::DiverT);
    // 命中但**未 commit** ⇒ 放行(R4:未 commit 前**正常服务**)
    CHECK(nvs::classify_at_fabric_ingress(true, false) == nvs::IngressDecision::Pass);
    // 未命中 ⇒ 放行(即便 epoch 已 commit)
    CHECK(nvs::classify_at_fabric_ingress(false, true) == nvs::IngressDecision::Pass);
    // 全不命中 ⇒ 放行
    CHECK(nvs::classify_at_fabric_ingress(false, false) == nvs::IngressDecision::Pass);

    // ⭐ **R4 回归锁** —— 未 commit ⇒ **放行**(⛔ 不是「拒绝」;本仓已错过一次)
    CHECK(nvs::epoch_routes_to_collective(nvs::EpochState::Uncommitted) == false);
    CHECK(nvs::epoch_routes_to_collective(nvs::EpochState::Committed) == true);
    CHECK(nvs::kEpochStateCount == 2u);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-03 —— R3 注册三步原子(drain 失败 ⇒ 注册失败且**不建 epoch**)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-03:注册三步原子 —— drain 失败 ⇒ DrainFailed", "[cep][nvlink-sharp][state-spec]") {
    CHECK(nvs::register_region(true) == nvs::RegisterResult::Ok);
    CHECK(nvs::register_region(false) == nvs::RegisterResult::DrainFailed);
    CHECK(nvs::kRegisterResultCount == 2u);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-04 —— R4 注销 ⇒ 失效化并**回归 coherence 域**(正常服务)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-04:注销后 ⇒ 正常服务(pass)", "[cep][nvlink-sharp][state-spec]") {
    CHECK(nvs::decision_after_destroy() == nvs::IngressDecision::Pass);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-05 —— R5 可观测性:divert 计数器形状 + **fail-stop** 断言
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-05:fail-stop —— 命中 + epoch 已 commit + coherence atomic ⇒ error",
          "[cep][nvlink-sharp][state-spec]") {
    CHECK(nvs::is_fail_stop(true, true, true));
    CHECK(nvs::is_fail_stop(false, true, true) == false); // 未命中 ⇒ 非 fail-stop
    CHECK(nvs::is_fail_stop(true, false, true) == false); // epoch 未 commit ⇒ 非 fail-stop
    CHECK(nvs::is_fail_stop(true, true, false) == false); // 无 coherence atomic ⇒ 非 fail-stop

    // 计数器**形状**(载体不持有状态)
    nvs::DivertCounter c;
    CHECK(c.total == 0u);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-06 —— 跨平面 ordering A/B/C/D 的**可判定**谓词
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-06:ordering A/B/C/D —— 四类各一组正反例", "[cep][nvlink-sharp][state-spec]") {
    // A:同址 coherent atomic → reduce ⇒ atomic 必须先 completion
    CHECK(nvs::ordering_a_allows_reduce(true, true));
    // 无 reduce 请求 ⇒ **平凡允许**(不构成排序约束)⇒ 期望 true
    CHECK(nvs::ordering_a_allows_reduce(false, false));
    // 有 reduce 请求但 atomic 未 completion ⇒ **禁止**
    CHECK(nvs::ordering_a_allows_reduce(false, true) == false);
    // B:同址 reduce → atomic ⇒ reduce 必须先 commit
    CHECK(nvs::ordering_b_allows_atomic(true, true));
    CHECK(nvs::ordering_b_allows_atomic(false, true) == false);
    // C:同 group 内 reduce 由 code 排序 ⇒ code 小者先
    CHECK(nvs::ordering_c_orders_reduce(1, 2));
    CHECK(nvs::ordering_c_orders_reduce(2, 1) == false);
    // ⚠️ code 相等 ⇒ 顺序**未定义** ⇒ 显式登记(禁止实现层默认某一种序)
    CHECK(nvs::kReduceCodeTieUnspecified);
    CHECK(nvs::ordering_c_orders_reduce(7, 7) == false); // 相等 ⇒ 不声称任何序
    // D:barrier → reduce ⇒ rendezvous 必须先完成
    CHECK(nvs::ordering_d_allows_reduce(true, true));
    CHECK(nvs::ordering_d_allows_reduce(false, true) == false);

    CHECK(nvs::kOrderingRuleCount == 4u);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-07 —— 集合 VC 预留 = **单一状态**;四字段与 `vc_id` **不可表达**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-07:VC 预留 —— 单一取值 `Unavailable`;`vc_id` 与四字段不可表达",
          "[cep][nvlink-sharp][state-spec]") {
    CHECK(nvs::kVcReservationStateCount == 1u);
    CHECK(static_cast<std::uint8_t>(nvs::VcReservationState::Unavailable) == 0u);

    // 「不可表达」由**类型**保证 ⇒ SFINAE 见证
    static_assert(!HasVcId<nvs::VcReservationState>::value, "");
    static_assert(!HasReason<nvs::VcReservationState>::value, "");
    static_assert(!HasOwner<nvs::VcReservationState>::value, "");
    static_assert(!HasRevisitTrigger<nvs::VcReservationState>::value, "");
    CHECK(!HasVcId<nvs::VcReservationState>::value);
    CHECK(!HasReason<nvs::VcReservationState>::value);
    CHECK(!HasOwner<nvs::VcReservationState>::value);
    CHECK(!HasRevisitTrigger<nvs::VcReservationState>::value);

    // 载体命名空间内**不得**出现 `vc_id` / 四字段标识符
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE(!raw.empty());
    const std::string src = strip_cpp_comments(raw);
    CHECK(src.find("vc_id") == std::string::npos);
    CHECK(src.find("revisit_trigger") == std::string::npos);
    // ⚠️ `reason` / `owner` 是常见词 ⇒ 只查**枚举值 / 成员**形态(不含散文)
    CHECK(src.find("VcReservationReason") == std::string::npos);
    CHECK(src.find("VcReservationOwner") == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-08 —— 能力域 = **节点内**;跨 fabric 值**不可表达**
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-08:能力域 —— 唯一取值 `IntraNode`(跨 fabric 值不可表达)",
          "[cep][nvlink-sharp][state-spec]") {
    CHECK(nvs::kCollectiveScopeCount == 1u);
    CHECK(static_cast<std::uint8_t>(nvs::CollectiveScope::IntraNode) == 0u);

    // 载体命名空间内**不得**出现跨 fabric 的取值
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE(!raw.empty());
    const std::string src = strip_cpp_comments(raw);
    CHECK(src.find("CrossFabric") == std::string::npos);
    CHECK(src.find("InterNode") == std::string::npos);
    CHECK(src.find("GlobalScope") == std::string::npos);
}
