// =============================================================================
// scripts/test/cep_nvlink_sharp_state_spec_probe.cc
//   —— 队列 #13 `cpptlm-cep-nvlink-sharp-state-spec` 的**反向验证探针**
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: scripts/test/cep_nvlink_sharp_state_spec_reverse_verify.sh(驱动本探针)
//       include/tlm/cep/nvlink_sharp_state_spec.hh(被毒化的对象)
//       openspec/changes/cpptlm-cep-nvlink-sharp-state-spec/(delta spec NSS-01 ~ NSS-09)
//
// ── 用途 ──────────────────────────────────────────────────────────────────
// 本探针在**影子目录**(被毒化的载体副本)下编译并运行 ⇒ 验证判据的**鉴别力**:
//   · **干净载体 ⇒ 必须通过**(基线正控)
//   · **毒化载体 ⇒ 必须失败**,且失败原因 SHALL 命中**期望的判据 id**
//
// ⚠️ **两类失败形态都要支持**(本 change 的判据是**双形态**):
//   ① **编译期红** ⇒ 载体的 `static_assert` 触发 ⇒ 归因 = 编译日志里的 `<判据 id>:`
//   ② **运行期红** ⇒ 探针的 `PROBE:FAIL <id>` 输出
//
// ── 为什么需要「源扫描」类判据(运行期兜底)─────────────────────────────────
// 铁律③:编译期锁**不得只以「编译红」充当 ctest 判据** ⇒ 每条 `static_assert` 都要有运行期判据。
// 而 `NSS-01`(载体自足 / 不暴露区域表)与 `NSS-07` / `NSS-08`(「不可表达」型)无法用谓词表达
// ⇒ 本探针**读载体源文件**做静态扫描 + 用 SFINAE 见证「类型上不可表达」。
//
// ⚠️ **扫描的是 `argv[1]` 指定的文件**(由驱动脚本传入影子副本)⇒ 毒化副本被扫到,
//    而仓内文件**零改动**。
//
// 用法:probe <carrier_path>
//   退出码 0 = 全部谓词成立;1 = 有谓词不成立
// =============================================================================

#include "tlm/cep/nvlink_sharp_state_spec.hh"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

namespace {

    namespace nvs = tlm::cep::nvlink_sharp;

    int g_fail = 0;

    void expect(bool cond, const char* id, const char* what) {
        if (!cond) {
            std::printf("PROBE:FAIL %s —— %s\n", id, what);
            ++g_fail;
        }
    }

    // ── 源扫描工具(去注释 / 去字面量)────────────────────────────────────
    std::string read_file(const char* path) {
        std::ifstream in(path);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去注释(行 + 块)
    std::string strip_comments(const std::string& src) {
        std::string out;
        bool line = false;
        bool block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (!line && !block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                line = true;
                ++i;
                continue;
            }
            if (!line && !block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                block = true;
                ++i;
                continue;
            }
            if (line && src[i] == '\n') {
                line = false;
                out.push_back('\n');
                continue;
            }
            if (block && src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                block = false;
                ++i;
                continue;
            }
            if (!line && !block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// 去注释 + 去字符串 / 字符字面量(「某标识符不得出现」类判据只看**代码标识符**)
    std::string strip_comments_and_strings(const std::string& src) {
        const std::string nc = strip_comments(src);
        std::string out;
        bool in_str = false;
        bool in_chr = false;
        for (std::size_t i = 0; i < nc.size(); ++i) {
            const char c = nc[i];
            if (!in_str && !in_chr && (c == '"' || c == '\'')) {
                in_str = (c == '"');
                in_chr = (c == '\'');
                continue;
            }
            if (in_str || in_chr) {
                if (c == '\\' && i + 1 < nc.size()) {
                    ++i;
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

    /// 取出所有 `#include` 行,返回「是否出现**引号形式**(项目内头)」。
    /// ⚠️ **唯一白名单**(2026-11-07,per 台账 Entry #266):`tlm/cep/source_registry.hh` ——
    ///    纯 `constexpr`、只依赖标准库的**来源类型注册表**(RefKind 标签)⇒ 允许引用;
    ///    ⛔ 它**不**引入平台标识 / 区域表 / 转移判定 ⇒ 不破坏「自足」的实质。
    ///    (先例:`ualink_inc` 的 UIS-10 同款白名单,per Entry #261)
    bool has_quoted_include(const std::string& src) {
        std::istringstream in(strip_comments(src));
        std::string line;
        while (std::getline(in, line)) {
            const auto pos = line.find("#include");
            if (pos == std::string::npos)
                continue;
            const auto q = line.find('"', pos);
            const auto a = line.find('<', pos);
            if (q != std::string::npos && (a == std::string::npos || q < a)) {
                // 白名单判定:该 include 行是否**恰好**指向注册表头
                const auto end = line.find('"', q + 1);
                const std::string path =
                    line.substr(q, (end == std::string::npos ? q + 40 : end) - q + 1);
                if (path == "\"tlm/cep/source_registry.hh\"") {
                    continue;  // 唯一白名单
                }
                return true;
            }
        }
        return false;
    }

    // ── SFINAE:检测类型是否**没有**某个成员(⇒ 表达「不可表达」)──────────────
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

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("PROBE:FAIL NSS-01 —— 用法: probe <carrier_path>\n");
        return 1;
    }
    const std::string raw = read_file(argv[1]);
    if (raw.empty()) {
        std::printf("PROBE:FAIL NSS-01 —— 载体源读取失败:%s\n", argv[1]);
        return 1;
    }
    const std::string idents = strip_comments_and_strings(raw);

    // ══════════════════════════════════════════════════════════════════════
    // NSS-01 / CP-05 —— 载体**自足**:只 include 标准库,不暴露区域表类型
    // ══════════════════════════════════════════════════════════════════════
    expect(!has_quoted_include(raw), "CP-05",
           "载体 SHALL 只 include 标准库(⛔ 不得以引号形式 include 项目内头)");
    expect(idents.find("collective_region_table") == std::string::npos, "CP-05",
           "载体 SHALL NOT 定义 / 暴露 collective_region_table 类型");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-01 / CP-OPCODE-CLASSIFIER —— 分类点唯一 + 源端不可影响分类
    // ══════════════════════════════════════════════════════════════════════
    expect(nvs::kIngressDecisionCount == 2, "CP-OPCODE-CLASSIFIER",
           "分类结果 SHALL 恰 2 个(DiverT / Pass)");
    // 运行期见证:两次调用只差「源端不可见」的事实 ⇒ 未命中时恒 Pass(与 opcode 无关)
    expect(nvs::classify_at_fabric_ingress(false, false) == nvs::IngressDecision::Pass,
           "CP-OPCODE-CLASSIFIER", "未命中 ⇒ 必为 Pass(与源端 opcode 无关)");
    expect(nvs::classify_at_fabric_ingress(false, true) == nvs::IngressDecision::Pass,
           "CP-OPCODE-CLASSIFIER", "未命中(epoch 已 commit)⇒ 仍 Pass");
    expect(nvs::classify_at_fabric_ingress(true, true) == nvs::IngressDecision::DiverT,
           "CP-OPCODE-CLASSIFIER", "命中 + 已 commit ⇒ DiverT");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-02 / CP-EPOCH-SEMANTICS —— R4 **双向**(未 commit ⇒ 放行)
    // ══════════════════════════════════════════════════════════════════════
    expect(!nvs::epoch_routes_to_collective(nvs::EpochState::Uncommitted), "CP-EPOCH-SEMANTICS",
           "R4:epoch 未 commit ⇒ SHALL 放行(⛔ 不得拒绝)");
    expect(nvs::epoch_routes_to_collective(nvs::EpochState::Committed), "CP-EPOCH-SEMANTICS",
           "R4:epoch 已 commit ⇒ SHALL divert");
    expect(nvs::classify_at_fabric_ingress(true, false) == nvs::IngressDecision::Pass,
           "CP-EPOCH-SEMANTICS", "命中但未 commit ⇒ 放行(R4 的合成见证)");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-03 / CP-DRAIN-FAIL —— 注册三步原子
    // ══════════════════════════════════════════════════════════════════════
    expect(nvs::register_region(true) == nvs::RegisterResult::Ok, "CP-DRAIN-FAIL",
           "R3:drain 成功 ⇒ Ok");
    expect(nvs::register_region(false) == nvs::RegisterResult::DrainFailed, "CP-DRAIN-FAIL",
           "R3:drain 失败 ⇒ DrainFailed(且不建 epoch)");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-04 / NSS-04-DESTROY-LEAK —— 注销回归 + **泄露可观测**(非恒真)
    // ══════════════════════════════════════════════════════════════════════
    expect(nvs::classify_after_destroy(false) == nvs::IngressDecision::Pass, "NSS-04-DESTROY-LEAK",
           "注销成功 ⇒ 回归 coherence 正常服务(Pass)");
    expect(nvs::classify_after_destroy(true) == nvs::IngressDecision::DiverT, "NSS-04-DESTROY-LEAK",
           "注销失败(仍留表内)⇒ 仍 divert ⇒ 泄露可被观测");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-05 / CP-DIVERT-STOP —— 计数器形状 + fail-stop
    // ══════════════════════════════════════════════════════════════════════
    {
        nvs::DivertCounter c;
        expect(c.total == 0u, "CP-DIVERT-STOP", "计数器形状:total 默认 0");
        expect(c.key.group == 0u, "CP-DIVERT-STOP", "计数器形状:可寻址键 group 存在");
        expect(c.key.region.valid(), "CP-DIVERT-STOP", "计数器形状:可寻址键 region 存在");
    }
    expect(nvs::is_fail_stop(true, true, true), "CP-DIVERT-STOP",
           "命中 + epoch 已 commit + coherence atomic ⇒ SHALL fail-stop");
    expect(!nvs::is_fail_stop(false, true, true), "CP-DIVERT-STOP", "未命中 ⇒ 非 fail-stop");
    expect(!nvs::is_fail_stop(true, false, true), "CP-DIVERT-STOP",
           "epoch 未 commit ⇒ 非 fail-stop");
    expect(!nvs::is_fail_stop(true, true, false), "CP-DIVERT-STOP",
           "无 coherence atomic ⇒ 非 fail-stop");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-06 / NSS-06-ORDERING-MIS —— ordering A/B/C/D
    // ══════════════════════════════════════════════════════════════════════
    expect(nvs::kOrderingRuleCount == 4, "NSS-06-ORDERING-MIS", "ordering SHALL 恰 4 类");
    expect(nvs::ordering_a_allows_reduce(true, true), "NSS-06-ORDERING-MIS",
           "A:atomic 已 completion ⇒ 允许 reduce");
    expect(!nvs::ordering_a_allows_reduce(false, true), "NSS-06-ORDERING-MIS",
           "A:atomic 未 completion ⇒ 禁止 reduce");
    expect(nvs::ordering_b_allows_atomic(true, true), "NSS-06-ORDERING-MIS",
           "B:reduce 已 commit ⇒ 允许 atomic");
    expect(!nvs::ordering_b_allows_atomic(false, true), "NSS-06-ORDERING-MIS",
           "B:reduce 未 commit ⇒ 禁止 atomic");
    expect(nvs::ordering_c_orders_reduce(1, 2), "NSS-06-ORDERING-MIS", "C:code 小者先");
    expect(!nvs::ordering_c_orders_reduce(2, 1), "NSS-06-ORDERING-MIS", "C:code 大者后");
    expect(nvs::kReduceCodeTieUnspecified, "NSS-06-ORDERING-MIS",
           "C:code 相等 ⇒ 顺序未定义 SHALL 显式登记");
    expect(!nvs::ordering_c_orders_reduce(7, 7), "NSS-06-ORDERING-MIS",
           "C:code 相等 ⇒ SHALL NOT 声称任何序");
    expect(nvs::ordering_d_allows_reduce(true, true), "NSS-06-ORDERING-MIS",
           "D:barrier 已完成 ⇒ 允许 reduce");
    expect(!nvs::ordering_d_allows_reduce(false, true), "NSS-06-ORDERING-MIS",
           "D:barrier 未完成 ⇒ 禁止 reduce");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-07 / CP-03 —— VC 预留**单一状态** + 四字段**不可表达**
    // ══════════════════════════════════════════════════════════════════════
    expect(nvs::kVcReservationStateCount == 1, "CP-03", "VC 预留 SHALL 只有单一取值");
    expect(static_cast<std::uint8_t>(nvs::VcReservationState::Unavailable) == 0u, "CP-03",
           "VC 预留的唯一取值为 Unavailable(枚举值 0)");
    // **不可表达**的运行期兜底(铁律③):类型上无 `vc_id` / 四字段
    expect(!HasVcId<nvs::VcReservationState>::value, "CP-03",
           "VcReservationState SHALL NOT 有 vc_id 成员");
    expect(!HasReason<nvs::VcReservationState>::value, "CP-03",
           "VcReservationState SHALL NOT 有 reason 成员");
    expect(!HasOwner<nvs::VcReservationState>::value, "CP-03",
           "VcReservationState SHALL NOT 有 owner 成员");
    expect(!HasRevisitTrigger<nvs::VcReservationState>::value, "CP-03",
           "VcReservationState SHALL NOT 有 revisit_trigger 成员");
    expect(idents.find("vc_id") == std::string::npos, "CP-03",
           "载体代码标识符 SHALL NOT 出现 vc_id");
    expect(idents.find("revisit_trigger") == std::string::npos, "CP-03",
           "载体代码标识符 SHALL NOT 出现 revisit_trigger");

    // ══════════════════════════════════════════════════════════════════════
    // NSS-08 / NSS-08-SCOPE-OVERREACH —— 能力域 = 节点内
    // ══════════════════════════════════════════════════════════════════════
    expect(nvs::kCollectiveScopeCount == 1, "NSS-08-SCOPE-OVERREACH",
           "能力域 SHALL 只有 IntraNode");
    expect(static_cast<std::uint8_t>(nvs::CollectiveScope::IntraNode) == 0u,
           "NSS-08-SCOPE-OVERREACH", "能力域的唯一取值为 IntraNode");
    for (const char* tok : {"CrossFabric", "InterNode", "GlobalScope"}) {
        expect(idents.find(tok) == std::string::npos, "NSS-08-SCOPE-OVERREACH",
               "载体代码标识符 SHALL NOT 出现跨 fabric 取值");
    }

    // ══════════════════════════════════════════════════════════════════════
    // NSS-09 / NSS-09-BASIS-MISSING —— 12 格 basis + 9 项 pending + 下限
    // ══════════════════════════════════════════════════════════════════════
    {
        std::size_t total = 0;
        std::size_t non_pending = 0;
        const auto check = [&](const nvs::Basis& b) {
            ++total;
            expect(b.ref != nullptr && b.ref[0] != '\0', "NSS-09-BASIS-MISSING",
                   "每格的 basis.ref SHALL 非空");
            expect(nvs::basis_is_valid(b), "NSS-09-BASIS-MISSING",
                   "每格的 basis SHALL 有效(ref 非空 + pending 与许可自洽)");
            if (!b.pending) {
                ++non_pending;
                expect(nvs::kSourceLicenseConfirmed[static_cast<std::size_t>(b.source)],
                       "NSS-09-BASIS-MISSING", "pending == false ⇒ 该来源的许可 SHALL 已人工确认");
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
        expect(total == 12u, "NSS-09-BASIS-MISSING", "逐格 basis SHALL 恰 12 格");
        expect(non_pending == nvs::kNonPendingCellCount, "NSS-09-BASIS-MISSING",
               "非 pending 格数 SHALL 与 kNonPendingCellCount 一致");
        expect(nvs::kNonPendingCellCount >= nvs::kNonPendingFloor, "NSS-09-BASIS-MISSING",
               "非 pending 格数 SHALL >= 下限");
    }
    {
        std::size_t n = 0;
        for (const auto& it : nvs::kPendingSemanticItems) {
            ++n;
            expect(it.id != nullptr && it.id[0] == 'N' && it.id[1] == '-', "NSS-09-BASIS-MISSING",
                   "pending 项 id SHALL 形如 N-<数字>");
            expect(it.what != nullptr && it.what[0] != '\0', "NSS-09-BASIS-MISSING",
                   "pending 项 what SHALL 非空");
            expect(it.ref != nullptr && it.ref[0] != '\0', "NSS-09-BASIS-MISSING",
                   "pending 项 ref SHALL 非空");
        }
        expect(n == 9u, "NSS-09-BASIS-MISSING", "9 项 pending SHALL 恰 9 条");
        expect(nvs::kPendingSemanticItemCount == 9u, "NSS-09-BASIS-MISSING",
               "kPendingSemanticItemCount SHALL == 9");
    }

    if (g_fail != 0) {
        std::printf("PROBE:SUMMARY %d 条谓词不成立\n", g_fail);
        return 1;
    }
    std::printf("PROBE:OK\n");
    return 0;
}
