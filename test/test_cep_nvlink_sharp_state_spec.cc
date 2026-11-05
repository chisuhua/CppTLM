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
// ⚠️ 本文件 = **Phase 1 的最小覆盖 + Phase 2 的运行期见证**(12 格 basis / 9 项 pending / 下限);
//    **只提供正控**;
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

#include "cep_anchor_ids.hh"
#include "tlm/cep/nvlink_sharp_state_spec.hh"

namespace {

    namespace nvs = tlm::cep::nvlink_sharp;

    constexpr const char* kCarrierRelPath = "include/tlm/cep/nvlink_sharp_state_spec.hh";
    constexpr const char* kAnchorRelPath =
        "docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md";

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

    /// **去注释 + 去字符串 / 字符字面量** —— 用于「某标识符**不得出现**」类判据。
    /// ⚠️ 为什么必须去字面量:断言文案**有义务点名**被禁的东西(如
    /// `"…不得表达 vc_id…"`)⇒ 若把字面量算进去,判据会**因正确的文案而红**(假阳性)。
    /// 判据本意是「载体不得**声明 / 使用**该标识符」⇒ 只看代码标识符。
    std::string strip_cpp_comments_and_strings(const std::string& src) {
        const std::string no_comments = strip_cpp_comments(src);
        std::string out;
        bool in_str = false;
        bool in_chr = false;
        for (std::size_t i = 0; i < no_comments.size(); ++i) {
            const char c = no_comments[i];
            if (!in_str && !in_chr && (c == '"' || c == '\'')) {
                in_str = (c == '"');
                in_chr = (c == '\'');
                continue;
            }
            if (in_str || in_chr) {
                if (c == '\\' && i + 1 < no_comments.size()) {
                    ++i; // 跳过转义的下一个字符
                    continue;
                }
                if ((in_str && c == '"') || (in_chr && c == '\'')) {
                    in_str = false;
                    in_chr = false;
                }
                continue;
            }
            out.push_back(c);
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
TEST_CASE("NSS-04:注销后 ⇒ 正常服务;注销**失败** ⇒ 仍 divert(泄露可观测)",
          "[cep][nvlink-sharp][state-spec]") {
    // 注销**成功**(区域已退出表)⇒ 回归 coherence 正常服务
    CHECK(nvs::classify_after_destroy(false) == nvs::IngressDecision::Pass);
    // ⚠️ 铁律① 正控:该判据**不是恒真** —— 注销**失败**(区域仍留表内)⇒ 仍 divert
    //    ⇒ 泄露可被观测(⛔ 不得静默 Pass)
    CHECK(nvs::classify_after_destroy(true) == nvs::IngressDecision::DiverT);
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
    // ⚠️ 2026-11-05(Entry #221,per architect 裁决 oracle 复核):`is_fail_stop` 有 **3 个 bool 输入
    //   = 8 种组合**;此前只断言 4 种 ⇒ 登记文档若写「穷尽证明」即**超出证据**。
    //   ⇒ 补齐**剩余 4 种组合**(每种至少一个输入为 false ⇒ 期望 false)。
    CHECK(nvs::is_fail_stop(false, false, true) == false);  // 未命中 + 未 commit
    CHECK(nvs::is_fail_stop(false, true, false) == false);  // 未命中 + 无 coherence atomic
    CHECK(nvs::is_fail_stop(true, false, false) == false);  // 未 commit + 无 coherence atomic
    CHECK(nvs::is_fail_stop(false, false, false) == false); // 全不满足

    // 计数器**形状**(载体不持有状态):恰 2 字段 —— 可寻址键(group + 区域)+ 计数
    nvs::DivertCounter c;
    CHECK(c.total == 0u);
    CHECK(c.key.group == 0u);
    CHECK(c.key.region.valid());
    // 「按 group / 区域可寻址」由**键类型**保证(delta spec NSS-05)
    CHECK(sizeof(nvs::DivertCounterKey) >= sizeof(nvs::GroupId));
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

    // 载体**代码标识符**里**不得**出现 `vc_id` / 四字段(断言文案点名它们是**允许**的)
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE(!raw.empty());
    const std::string idents = strip_cpp_comments_and_strings(raw);
    CHECK(idents.find("vc_id") == std::string::npos);
    CHECK(idents.find("revisit_trigger") == std::string::npos);
    // ⚠️ `reason` / `owner` 是常见词 ⇒ 只查**枚举值 / 成员**形态(不含散文)
    CHECK(idents.find("VcReservationReason") == std::string::npos);
    CHECK(idents.find("VcReservationOwner") == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-09 —— 逐格 `basis`(12 格)两轴 + 9 项 pending 登记 + **非 pending 下限**
//   (Phase 2;`design §3` 的编译期锁在本文件**重复见证**为运行期判据 —— 铁律③)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-09:12 格 basis 三字段齐全 + 9 项 pending 良构 + 非 pending 下限",
          "[cep][nvlink-sharp][state-spec]") {
    // ① 12 格:`ref` 非空 + `basis_is_valid` + `pending == false ⇒ 来源许可已确认`
    std::size_t total = 0;
    std::size_t not_pending = 0;
    const auto check = [&](const nvs::Basis& b) {
        ++total;
        INFO("ref=" << (b.ref != nullptr ? b.ref : "(null)"));
        REQUIRE(b.ref != nullptr);
        CHECK(b.ref[0] != '\0');
        CHECK(nvs::basis_is_valid(b));
        if (!b.pending) {
            ++not_pending;
            CHECK(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(b.source)]);
        }
    };
    for (const auto& b : nvs::kBasisIngressDecision)
        check(b);
    for (const auto& b : nvs::kBasisRegisterResult)
        check(b);
    for (const auto& b : nvs::kBasisEpochState)
        check(b);
    for (const auto& b : nvs::kBasisOrderingRule)
        check(b);
    for (const auto& b : nvs::kBasisVcReservationState)
        check(b);
    for (const auto& b : nvs::kBasisCollectiveScope)
        check(b);

    // ② 精确计数:12 格;其中 `pending == false` 恰 **12**(全部出自已签发 ADR ⇒ D-2)
    CHECK(total == 12u);
    CHECK(not_pending == 12u);
    CHECK(nvs::kNonPendingCellCount == 12u);
    CHECK(nvs::kNonPendingCellCount >= nvs::kNonPendingFloor);

    // ③ 分组:每组 basis.size() == 该组格数(逐格无空洞)
    CHECK(nvs::kBasisIngressDecision.size() == nvs::kIngressDecisionCount);
    CHECK(nvs::kBasisRegisterResult.size() == nvs::kRegisterResultCount);
    CHECK(nvs::kBasisEpochState.size() == nvs::kEpochStateCount);
    CHECK(nvs::kBasisOrderingRule.size() == nvs::kOrderingRuleCount);
    CHECK(nvs::kBasisVcReservationState.size() == nvs::kVcReservationStateCount);
    CHECK(nvs::kBasisCollectiveScope.size() == nvs::kCollectiveScopeCount);

    // ④ 证据强度:本 change 的 12 格**全部** `self-derived`(ADR 契约,非公开来源转述)
    for (const auto& b : nvs::kBasisIngressDecision)
        CHECK(b.kind == nvs::EvidenceKind::SelfDerived);
    for (const auto& b : nvs::kBasisCollectiveScope)
        CHECK(b.kind == nvs::EvidenceKind::SelfDerived);

    // ⑤ 来源许可表:**保守**(S1/S2/S3/Derived = false;AdrContract = true)
    CHECK_FALSE(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(nvs::SourceId::S1)]);
    CHECK_FALSE(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(nvs::SourceId::S2)]);
    CHECK_FALSE(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(nvs::SourceId::S3)]);
    CHECK(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(nvs::SourceId::AdrContract)]);
    CHECK_FALSE(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(nvs::SourceId::Derived)]);
}

TEST_CASE("NSS-09:9 项 pending 逐条可寻址 + 未建模项零出现", "[cep][nvlink-sharp][state-spec]") {
    STATIC_REQUIRE(nvs::kPendingSemanticItemCount == 9u);
    CHECK(nvs::kPendingSemanticItems.size() == 9u);

    std::set<std::string> ids;
    for (const auto& it : nvs::kPendingSemanticItems) {
        INFO("id=" << (it.id != nullptr ? it.id : "(null)"));
        REQUIRE(it.id != nullptr);
        REQUIRE(it.what != nullptr);
        REQUIRE(it.ref != nullptr);
        CHECK(std::string(it.id).rfind("N-", 0) == 0);
        CHECK(std::string(it.ref).rfind("\u00a7", 0) == 0); // 以 § 开头(锚点摘要条目)
        CHECK(ids.insert(it.id).second);                    // id 唯一
    }
    // 恰好覆盖 N-1 ~ N-9
    for (int i = 1; i <= 9; ++i)
        CHECK(ids.count("N-" + std::to_string(i)) == 1u);

    // ⚠️ 「不得出现具体取值」:载体**代码标识符**里零处出现 reduce 算子名 / 报文格式常量
    const std::string raw = read_repo_file(kCarrierRelPath);
    REQUIRE(!raw.empty());
    const std::string idents = strip_cpp_comments_and_strings(raw);
    for (const char* tok :
         {"BitAnd", "BitOr", "BitXor", "ReduceScatter", "AllGather", "opcode_layout"})
        CHECK(idents.find(tok) == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// NSS-09(3.1 逐格核对表)—— 12 格 `ref` ↔ 锚点摘要条目**一一可解析**
//   ⚠️ **防空转**:若收集器退化为空集,「一一对应」会**恒真**(队列 #11 Entry #141 B-7 同型陷阱)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NSS-09:逐格 ref 可解析 —— 12 格 ↔ 锚点摘要条目(含防空转)",
          "[cep][nvlink-sharp][state-spec]") {
    const std::string doc = read_repo_file(kAnchorRelPath);
    REQUIRE(!doc.empty());

    // ① 锚点摘要有**两种**条目形态:`A-n` 在 `###` **标题**;`N-n` / `C-n` 在**表格单元格**
    const auto a_ids = cpptlm::cep::collect_heading_ids(doc, "A-");
    const auto n_ids = cpptlm::cep::collect_ids(doc, "N-");
    const auto c_ids = cpptlm::cep::collect_ids(doc, "C-");

    // ② **防空转**:三者都**非空**且条数精确 ⇒ 下面的「一一对应」才有意义
    CHECK(a_ids.size() == 5u); // A-1 ~ A-5
    CHECK(n_ids.size() == 9u); // N-1 ~ N-9
    CHECK(c_ids.size() == 9u); // C-1 ~ C-9

    // ③ 载体侧:12 格的 `ref` token
    std::set<std::string> used_a;
    std::set<std::string> used_c;
    std::size_t cells = 0;
    std::size_t bad_tokens = 0;
    const auto collect = [&](const nvs::Basis& b) {
        ++cells;
        std::istringstream iss(b.ref != nullptr ? b.ref : "");
        std::string tok;
        while (iss >> tok) {
            if (tok.rfind("A-", 0) == 0) {
                used_a.insert(tok);
            } else if (tok.rfind("C-", 0) == 0) {
                used_c.insert(tok);
            } else {
                ++bad_tokens;
                INFO("未预期的 ref token: " << tok);
            }
        }
    };
    for (const auto& b : nvs::kBasisIngressDecision)
        collect(b);
    for (const auto& b : nvs::kBasisRegisterResult)
        collect(b);
    for (const auto& b : nvs::kBasisEpochState)
        collect(b);
    for (const auto& b : nvs::kBasisOrderingRule)
        collect(b);
    for (const auto& b : nvs::kBasisVcReservationState)
        collect(b);
    for (const auto& b : nvs::kBasisCollectiveScope)
        collect(b);

    CHECK(cells == 12u);
    CHECK(bad_tokens == 0u);
    // ④ 每个 token 都**真实存在于**锚点摘要(逐格可解析)
    for (const auto& t : used_c)
        CHECK(c_ids.count(t) == 1u);
    for (const auto& t : used_a)
        CHECK(a_ids.count(t) == 1u);
    // ⑤ `C-1` ~ `C-9` **全部**被引用(无空洞 —— 「非 pending 下限」的另一面)
    CHECK(used_c == c_ids);
    // ⑥ 佐证指针:`A-1` / `A-3` / `A-5` 被引用;`A-2` / `A-4` 已登记为「无载体格」
    //    (锚点摘要 §2bis.2 口径 3 + `tasks 2.8c`)
    CHECK(used_a.count("A-1") == 1u);
    CHECK(used_a.count("A-3") == 1u);
    CHECK(used_a.count("A-5") == 1u);
    CHECK(used_a.count("A-2") == 0u);
    CHECK(used_a.count("A-4") == 0u);

    // ⑦ 9 项 pending 的 `ref` 覆盖 `N-1` ~ `N-9`(锚点摘要 §3 的表格条目)
    std::set<std::string> used_n;
    for (const auto& it : nvs::kPendingSemanticItems) {
        std::istringstream iss(it.ref != nullptr ? it.ref : "");
        std::string tok;
        while (iss >> tok) {
            if (tok.rfind("N-", 0) == 0)
                used_n.insert(tok);
        }
    }
    CHECK(used_n == n_ids);
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
    const std::string idents = strip_cpp_comments_and_strings(raw);
    CHECK(idents.find("CrossFabric") == std::string::npos);
    CHECK(idents.find("InterNode") == std::string::npos);
    CHECK(idents.find("GlobalScope") == std::string::npos);
}
