// =============================================================================
// test/cep_anchor_ids.hh
//   —— 锚点摘要**条目号**收集器(test 内部工具头;生成器与 ctest 判据**同源**)
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: include/tlm/cep/tilelink_state_spec.hh
//       openspec/changes/cpptlm-cep-tilelink-state-spec/(delta spec TSS-07 子条件 ④ / ⑥)
//
// ── 为什么需要这个头 ─────────────────────────────────────────────────────────
// ⚠️ **不得**照抄 `scripts/test/generate_cxl_anchor_checklist.cc` 的 ID 文法:
//    其 `is_wellformed_id` 只认「`prefix` + **全数字**」,而 CXL 锚点用 `| D1 |`(无粗体)。
//    TileLink 锚点用的是 `| **D1** |`(**Markdown 粗体**)与 `| **R-T1** |`(**含字母后缀**)
//    ⇒ 照抄 ⇒ 合法 ID 集合为**空** ⇒ 逐格校验**恒真空转**(AC-6 失效)。
//
// ⇒ 本头实现 **3 件事**(per delta spec `TSS-07` 子条件 ⑥):
//   ① ID 文法**容忍 Markdown 粗体**(`| **TLC-01** |` 与 `| TLC-01 |` **等价**)
//   ② **接受** `R-T<n>` 这类**含字母**的前缀(`prefix` + 全数字后缀)
//   ③ **逐格**白名单比对 —— 载体 refs 与锚点 ids 必须**一一对应**(不是「只查首部 source id」)
//
// ── 与 CP-02 解析器的**区别**(勿混淆)─────────────────────────────────────
// `collective_plane_contract.hh` 的 `parse_canonical_flow_names` 扫 `ADR-CEP-02` 的
// 「首格**全数字**」表格行 ⇒ **本头的 `collect_ids` 刻意不同**:它按**前缀**匹配、
// 且容忍粗体 ⇒ 两者**互不干扰**。
// ⚠️ 但由本头的「容忍粗体」可知:往 `ADR-CEP-02` 加 SU 时**仍须**避开数字首格
//    (该陷阱已登记于台账 Entry #142 的 ④)。
// =============================================================================

#ifndef CEP_ANCHOR_IDS_HH
#define CEP_ANCHOR_IDS_HH

#include <cctype>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cpptlm {
    namespace cep {

        /// 去掉 Markdown 粗体标记(`**`)与首尾空白。
        inline std::string strip_bold_and_trim(std::string s) {
            const auto trim = [](std::string& v) {
                while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) {
                    v.erase(v.begin());
                }
                while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r')) {
                    v.pop_back();
                }
            };
            trim(s);
            // 反复去首尾 `**`(容忍 `**x**` 与 `****x****`)
            while (s.size() >= 4 && s.compare(0, 2, "**") == 0 &&
                   s.compare(s.size() - 2, 2, "**") == 0) {
                s = s.substr(2, s.size() - 4);
                trim(s);
            }
            return s;
        }

        /// 形如 `TLC-01` / `R-T1` / `D1` 的**条目号** = `prefix` + **全数字**后缀。
        /// ⚠️ 后缀**必须全为数字** —— 否则 `R-T1` 之类会被误当合法 id。
        inline bool is_wellformed_id(const std::string& id, const std::string& prefix) {
            if (prefix.empty() || id.size() <= prefix.size()) {
                return false;
            }
            if (id.compare(0, prefix.size(), prefix) != 0) {
                return false;
            }
            for (std::size_t i = prefix.size(); i < id.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(id[i]))) {
                    return false;
                }
            }
            return true;
        }

        /// 收集文档**表格单元格**里形如 `<prefix><数字>` 的条目号。
        ///
        /// ⚠️ **容忍 Markdown 粗体** —— `| **TLC-01** |` 与 `| TLC-01 |` 等价。
        /// ⚠️ **只认「整个单元格」** —— `| x TLC-01 |` 这类**不会**被误收。
        inline std::set<std::string> collect_ids(const std::string& doc,
                                                 const std::string& prefix) {
            std::set<std::string> ids;
            std::istringstream in(doc);
            std::string line;
            while (std::getline(in, line)) {
                std::size_t start = 0;
                while (true) {
                    const std::size_t bar = line.find('|', start);
                    if (bar == std::string::npos) {
                        break;
                    }
                    const std::size_t next = line.find('|', bar + 1);
                    const std::size_t end = (next == std::string::npos) ? line.size() : next;
                    const std::string cell =
                        strip_bold_and_trim(line.substr(bar + 1, end - bar - 1));
                    if (is_wellformed_id(cell, prefix)) {
                        ids.insert(cell);
                    }
                    if (next == std::string::npos) {
                        break;
                    }
                    start = next;
                }
            }
            return ids;
        }

        /// 收集**标题**里的条目号(`### A-1 **…**` / `#### C-3 …`)。
        ///
        /// ⚠️ **为什么必须有这个函数**:`collect_ids` **只扫表格单元格**,而锚点摘要的
        /// `A-n` 条目是 `###` **标题** ⇒ 只用 `collect_ids` 会得到**空集** ⇒
        /// 逐格核对**恒真空转**(= 判据不生效)。本函数与 `collect_ids` **配套**使用。
        ///
        /// 支持 `##` ~ `######`;容忍标题里的 Markdown 粗体。
        inline std::set<std::string> collect_heading_ids(const std::string& doc,
                                                         const std::string& prefix) {
            std::set<std::string> ids;
            std::istringstream in(doc);
            std::string line;
            while (std::getline(in, line)) {
                std::size_t h = 0;
                while (h < line.size() && line[h] == '#')
                    ++h;
                if (h < 2 || h > 6)
                    continue; // 不是 `##` ~ `######`
                if (h >= line.size() || line[h] != ' ')
                    continue;
                std::size_t b = h;
                while (b < line.size() && line[b] == ' ')
                    ++b;
                // 取标题的第一个 token
                std::size_t e = b;
                while (e < line.size() && line[e] != ' ' && line[e] != '\r')
                    ++e;
                const std::string head = strip_bold_and_trim(line.substr(b, e - b));
                if (is_wellformed_id(head, prefix))
                    ids.insert(head);
            }
            return ids;
        }

        /// **逐格**白名单比对:载体的 `refs` 与锚点的 `ids` 必须**一一对应**
        /// (集合相等 **且** 载体侧无重复)⇒ 任一侧被改 ⇒ **必红**。
        inline bool refs_resolve_one_to_one(const std::vector<std::string>& refs,
                                            const std::set<std::string>& ids) {
            if (refs.size() != ids.size()) {
                return false;
            }
            std::set<std::string> seen;
            for (const auto& r : refs) {
                if (ids.find(r) == ids.end()) {
                    return false; // 载体侧有锚点里**不存在**的条目号
                }
                if (!seen.insert(r).second) {
                    return false; // 载体侧重复引用同一条目号
                }
            }
            return seen.size() == ids.size();
        }

    } // namespace cep
} // namespace cpptlm

#endif // CEP_ANCHOR_IDS_HH
