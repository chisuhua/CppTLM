// =============================================================================
// scripts/test/cep_ualink_inc_state_spec_probe.cc
//   —— 队列 #14 规约层 change `cpptlm-cep-ualink-inc-state-spec` 的**运行期探针**
//
// 作者: CppTLM Team / 日期: 2026-11-04
// 配套: include/tlm/cep/ualink_inc_state_spec.hh(被检载体 · 影子副本优先)
//       scripts/test/cep_ualink_inc_state_spec_reverse_verify.sh(驱动脚本)
//       openspec/changes/cpptlm-cep-ualink-inc-state-spec/(4 件套)
//       docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md(basis 唯一人工审计面)
//       台账 obj-cep-platform.md Entry #196 ~ #200
//
// ── 职责(per design §2.4 的跨载体元测试 + §4 的驱动脚本双分支)────────────────
//   本探针承担 **跨载体** 判据(载体**自足** ⇒ 自身**不可**引用平台头 ⇒ 跨载体比对只能在探针侧):
//   P-1  载体自足性(禁项扫描 · 去注释后零命中)          ⇒ 归因 `UIS-10`
//   P-2  归因 id 覆盖(载体的断言文案含 `UIS-01`~`UIS-11`) ⇒ 归因 `UIS-10`
//   P-3  **X-1**  原语覆盖 ⊆ `CollectiveOp`               ⇒ 归因 `UIS-01`
//   P-4  **X-1b** 原语覆盖 ⊊ `CollectiveOp`(严格子集 ⇒ 非副本) ⇒ 归因 `UIS-01`
//   P-5  **X-2**  pending 集合 == 锚点摘要 §3 的 `N-n`(逐项相等) ⇒ 归因 `UIS-10`
//   P-6  **X-3**  basis 完整 + `SelfDerived` 不引 `A-n` + 审计面 id 命名一致 ⇒ 归因 `UIS-10`
//
// ── 输出契约 ───────────────────────────────────────────────────────────────
//   每项检查输出一行:`PROBE:OK <check>` 或 `PROBE:FAIL <归因 id> <check>`
//   ⇒ 驱动脚本按 `PROBE:FAIL <id> ` **精确前缀**匹配(⛔ 不用裸子串)
//   ⚠️ 退出码:0 = 全绿;1 = 有失败项
//
// ── 用法 ───────────────────────────────────────────────────────────────────
//   cep_ualink_inc_state_spec_probe <载体路径> <仓库根>
//   (载体路径可指向**影子副本** ⇒ 探针吃到的就是被毒化的那一份)
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "tlm/cep/collective_plane.hh" // 平台原语族的**唯一真源**(X-1 / X-1b)
#include "tlm/cep/ualink_inc_state_spec.hh"

namespace {

    using namespace tlm::cep::ualink_inc;

    int g_fail = 0;

    void ok(std::string_view check) {
        std::cout << "PROBE:OK " << check << '\n';
    }

    void bad(std::string_view id, std::string_view check) {
        std::cout << "PROBE:FAIL " << id << ' ' << check << '\n';
        ++g_fail;
    }

    std::string read_file(const std::string& path) {
        std::ifstream in(path);
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

    /// 禁项清单(per 本 change 的 `design §2.1`)
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> kTokens{
            "CollectiveOp",      "ReduceOp",           "CollectiveEvt",
            "Status::",          "LookupResult",       "TrackerImpl",
            "CollectivePlaneIf", "LocalCollectivePlane", "kCollectivePlaneRegistry",
            "tlm::cep::",        "collective_region_table",
            "transition_",       "is_silent_upgrade"};
        return kTokens;
    }

    /// 从审计面文本里抽出形如 `N-3` / `A-1` 的条目 id(去重 · 排序)
    std::vector<std::string> extract_item_ids(const std::string& text, char prefix) {
        std::vector<std::string> ids;
        const std::string key = std::string("**") + prefix + '-';
        std::size_t pos = 0;
        while (true) {
            const std::size_t at = text.find(key, pos);
            if (at == std::string::npos)
                break;
            std::size_t j = at + key.size();
            std::string num;
            while (j < text.size() && text[j] >= '0' && text[j] <= '9')
                num.push_back(text[j++]);
            if (!num.empty()) {
                const std::string id = std::string(1, prefix) + '-' + num;
                if (std::find(ids.begin(), ids.end(), id) == ids.end())
                    ids.push_back(id);
            }
            pos = j;
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    std::string join(const std::vector<std::string>& v) {
        std::string out;
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i)
                out += ' ';
            out += v[i];
        }
        return out;
    }

    /// 平台的 `CollectiveOp` 名集合(**唯一真源** ⇒ 不在本探针里复制词表)
    std::vector<std::string> platform_primitive_names() {
        std::vector<std::string> names;
        for (std::size_t i = 0; i < tlm::cep::kCollectiveOpCount; ++i) {
            const char* n = tlm::cep::collective_op_name(static_cast<tlm::cep::CollectiveOp>(i));
            if (n && *n)
                names.emplace_back(n);
        }
        return names;
    }

    /// 载体用到的原语名集合
    std::vector<std::string> carrier_primitive_names() {
        std::vector<std::string> names;
        for (const auto& n : kUsedPrimitiveNames)
            names.emplace_back(n);
        std::sort(names.begin(), names.end());
        return names;
    }

    /// 载体的 11 个义务 id(`UIS-01` ~ `UIS-11`)
    std::vector<std::string> carrier_obligation_ids() {
        std::vector<std::string> ids;
        for (std::size_t i = 0; i < kObligationCount; ++i) {
            const int n = static_cast<int>(i) + 1;
            ids.push_back("UIS-" + (n < 10 ? std::string("0") + std::to_string(n) : std::to_string(n)));
        }
        return ids;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <载体路径> <仓库根>\n";
        return 2;
    }
    const std::string carrier_path = argv[1];
    const std::string repo_root = argv[2];

    const std::string raw = read_file(carrier_path);
    if (raw.empty()) {
        std::cout << "PROBE:FAIL UIS-10 carrier-unreadable\n";
        return 1;
    }
    const std::string stripped = strip_cpp_comments(raw);

    // ── P-1 载体自足性(禁项扫描)─────────────────────────────────────────────
    {
        std::vector<std::string> hits;
        for (const auto& t : forbidden_tokens()) {
            if (stripped.find(t) != std::string::npos)
                hits.push_back(t);
        }
        if (hits.empty()) {
            ok("P-1-self-contained");
        } else {
            std::cout << "        hits: " << join(hits) << '\n';
            bad("UIS-10", "P-1-self-contained");
        }
    }

    // ── P-1b 只 include **标准库**(per design §2.1 的「项目内头」禁令)──────
    //    ⚠️ 禁项清单里的 `tlm::cep::` 是**双冒号** ⇒ 匹配不到 `tlm/cep/collective_plane.hh`
    //    ⇒ 「项目内头」这条禁令须**单独**判(引号形式的 `#include` ⇒ 红)
    {
        bool only_angle = true;
        std::size_t pos = 0;
        std::vector<std::string> quoted;
        while (true) {
            const std::size_t at = stripped.find("#include", pos);
            if (at == std::string::npos)
                break;
            std::size_t i = at + 8;
            while (i < stripped.size() && (stripped[i] == ' ' || stripped[i] == '\t'))
                ++i;
            if (i < stripped.size() && stripped[i] == '"') {
                only_angle = false;
                const std::size_t end = stripped.find('"', i + 1);
                quoted.push_back(stripped.substr(i, (end == std::string::npos ? i + 40 : end) - i + 1));
            }
            pos = at + 8;
        }
        if (only_angle) {
            ok("P-1b-standard-library-only");
        } else {
            std::cout << "        quoted includes: " << join(quoted) << '\n';
            bad("UIS-10", "P-1b-standard-library-only");
        }
    }

    // ── P-2 归因 id 覆盖(断言文案含 11 个 id)────────────────────────────────
    {
        bool all = true;
        std::vector<std::string> missing;
        for (const auto& id : carrier_obligation_ids()) {
            if (stripped.find("\"" + id + ": ") == std::string::npos) {
                all = false;
                missing.push_back(id);
            }
        }
        if (all) {
            ok("P-2-attribution-id-coverage");
        } else {
            std::cout << "        missing: " << join(missing) << '\n';
            bad("UIS-10", "P-2-attribution-id-coverage");
        }
    }

    // ── P-3 / P-4 X-1 与 X-1b(跨载体:原语覆盖)─────────────────────────────
    {
        const auto platform = platform_primitive_names();
        const auto carrier = carrier_primitive_names();
        bool subset = true;
        for (const auto& n : carrier) {
            if (std::find(platform.begin(), platform.end(), n) == platform.end())
                subset = false;
        }
        if (subset) {
            ok("P-3-x1-primitive-subset");
        } else {
            bad("UIS-01", "P-3-x1-primitive-subset");
        }
        // 严格子集 ⇒ **不是**平台词表的副本(⚠️ 载体的 `size()==3` 断言**不蕴含**本条)
        if (subset && carrier.size() < platform.size()) {
            ok("P-4-x1b-primitive-strict-subset");
        } else {
            bad("UIS-01", "P-4-x1b-primitive-strict-subset");
        }
    }

    // ── P-5 X-2(跨载体:pending 集合 == 审计面 §3 的 `N-n`)──────────────────
    {
        const std::string anchor = read_file(repo_root + "/docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md");
        if (anchor.empty()) {
            std::cout << "        审计面读不到 ⇒ 归因摘要路径不可达\n";
            bad("UIS-10", "P-5-x2-pending-exact");
        } else {
            const auto anchor_ids = extract_item_ids(anchor, 'N');
            std::vector<std::string> carrier_ids;
            for (const auto& it : kPendingSemanticItems)
                carrier_ids.emplace_back(it.ref);
            std::sort(carrier_ids.begin(), carrier_ids.end());
            if (anchor_ids == carrier_ids) {
                ok("P-5-x2-pending-exact");
            } else {
                std::cout << "        anchor: " << join(anchor_ids) << '\n';
                std::cout << "        carrier: " << join(carrier_ids) << '\n';
                bad("UIS-10", "P-5-x2-pending-exact");
            }
        }
    }

    // ── P-6 X-3(跨载体:basis 完整 + `SelfDerived` 不引 `A-n` + 命名一致)──────
    {
        const std::string anchor = read_file(repo_root + "/docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md");
        bool all_ref = true;
        bool no_public = true;
        for (const auto& c : kBasisObligations) {
            if (!basis_cell_has_ref(c.ref))
                all_ref = false;
            if (c.kind == BasisKind::SelfDerived && !self_derived_avoids_public_source(c.ref))
                no_public = false;
        }
        for (const auto& c : kBasisIncOps) {
            if (!basis_cell_has_ref(c.ref))
                all_ref = false;
            if (c.kind == BasisKind::SelfDerived && !self_derived_avoids_public_source(c.ref))
                no_public = false;
        }
        if (all_ref && no_public) {
            ok("P-6-x3-basis-complete");
        } else {
            bad("UIS-10", "P-6-x3-basis-complete");
        }
        // 跨载体:审计面的 `A-n` id 必被载体的分类器认作 `PublicSource`
        bool naming = true;
        if (!anchor.empty()) {
            for (const auto& id : extract_item_ids(anchor, 'A')) {
                if (classify_ref_token(id) != RefClass::PublicSource)
                    naming = false;
            }
        } else {
            naming = false;
        }
        if (naming) {
            ok("P-6-x3-audit-naming-aligned");
        } else {
            bad("UIS-10", "P-6-x3-audit-naming-aligned");
        }
    }

    return (g_fail == 0) ? 0 : 1;
}
