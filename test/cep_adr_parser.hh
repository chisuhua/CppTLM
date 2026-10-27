// test/cep_adr_parser.hh
// CEP Stage 0 — ADR 不变量解析器(test 内部工具头,per ADR-CEP-14 决策 5)
// 作者: CppTLM Team / 日期: 2026-10-21
//
// 设计要点(per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/design.md D2):
//   1. **不复制** invariant 文本 —— 一律解析 owner ADR 文档
//   2. **区分「提及」与「定义」** —— 跳空 ID(IC4)被多处提及但从未定义;
//      若按"提及"计数会多算 1 条(per 2026-10-21 实测教训)
//   3. 判定采用**集合语义**(set),不是计数语义 —— 幂等,重复提及不重复计数
#ifndef CEP_ADR_PARSER_HH
#define CEP_ADR_PARSER_HH

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpptlm {
    namespace cep {

        // ── invariant id 形态 ─────────────────────────────────────────────
        // CEN-Ib<N>                    Coherence Interface(ADR-CEP-02)
        // INV-SC<N>                    Stub Cache(ADR-CEP-05)
        // INV-CEP-<NN|07A|NUM>-IC<N>   其余分组
        inline bool is_invariant_id(const std::string& id) {
            static const std::regex re(
                R"(^(?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+)$)");
            return std::regex_match(id, re);
        }

        // ── 读取文件 ──────────────────────────────────────────────────────
        inline std::string read_file(const std::string& path) {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        // ── 定义位判定 ────────────────────────────────────────────────────
        // 仅在 **owner ADR** 内调用;跨 ADR 引用因此天然被排除。
        // 五种定义形态(与 22 份 ADR 的实际写法对齐):
        //   P1  **ID**                      加粗定义
        //   P2  - ID:  /  - **ID**:         列表项定义
        //   P3  **标签**(ID):               行内定义(如 "**不变量**(INV-CEP-04-IC1):")
        //   P4  **标签 ID** / **标签 `ID`**: 加粗 span 内含 ID(如 ADR-CEP-07:61 /
        //                                     ADR-CEP-08:76 / ADR-CEP-13:82)
        //   P5  ### 决策 N:`ID` —— …        决策标题形态(如 ADR-CEP-09:108)
        //
        // ⚠️ 2026-10-21 教训:此前只有 P1~P3,66/66 的覆盖是**偶然**撑起来的 ——
        //    12 份 owner ADR 在「§3 不变量」节**镜像**了一份 `- **ID**:` 列表,
        //    掩盖了各自**首定义位**用 P4/P5 写法的事实。实测注入
        //    `**不变量 \`INV-CEP-04-IC8\`**: …` 到 ADR-CEP-04 → 7 case 全绿,
        //    即**新增 invariant 可完全绕过契约层**。补 P4/P5 后该注入必红。
        inline std::set<std::string> defined_invariant_ids(const std::string& doc_text) {
            std::set<std::string> out;
            if (doc_text.empty())
                return out;

            // P1:加粗 ID —— 逐个候选 token 检查其前后是否为 **
            static const std::regex cand(
                R"((?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+))");
            std::sregex_iterator it(doc_text.begin(), doc_text.end(), cand);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                const std::string id = it->str();
                const std::size_t b = static_cast<std::size_t>(it->position());
                const std::size_t e = b + id.size();
                const bool bold_left = (b >= 2 && doc_text.compare(b - 2, 2, "**") == 0);
                const bool bold_right =
                    (e + 2 <= doc_text.size() && doc_text.compare(e, 2, "**") == 0);
                if (bold_left && bold_right)
                    out.insert(id);
            }

            // P2:列表项起始 —— "- ID:" / "- **ID**"
            static const std::regex bullet(
                R"(^[\t ]*-\s+\*{0,2}((?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+))\*{0,2}\s*[:：])",
                std::regex::multiline);
            for (std::sregex_iterator i2(doc_text.begin(), doc_text.end(), bullet), e2; i2 != e2;
                 ++i2) {
                out.insert(i2->str(1));
            }

            // P3:行内定义 —— "**标签**(ID):"
            static const std::regex inline_def(
                R"(\*\*[^*\n]{1,40}\*\*\(((?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+))\)\s*[:：])");
            for (std::sregex_iterator i3(doc_text.begin(), doc_text.end(), inline_def), e3;
                 i3 != e3; ++i3) {
                out.insert(i3->str(1));
            }

            // P4:加粗 span 内含 ID + 紧跟冒号 —— "**不变量 `ID`**:" / "**不变量 ID**:"
            //   span 内容不得含 '*'(避免跨 ** 匹配);ID 须为**完整 token**(词边界)。
            static const std::regex bold_span(
                R"(\*\*[^*\n]{0,60}?\b((?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+))\b[^*\n]{0,20}?\*\*\s*[:：])");
            for (std::sregex_iterator i4(doc_text.begin(), doc_text.end(), bold_span), e4; i4 != e4;
                 ++i4) {
                out.insert(i4->str(1));
            }

            // P5:决策标题形态 —— "### 决策 N:`ID` —— …"
            static const std::regex decision_head(
                R"(^#{2,6}\s*决策\s*\d+\s*[:：]\s*`?\b((?:CEN-Ib\d+|INV-SC\d+|INV-CEP-(?:\d+[A-Z]?|07A|NUM)-IC\d+))\b)",
                std::regex::multiline);
            for (std::sregex_iterator i5(doc_text.begin(), doc_text.end(), decision_head), e5;
                 i5 != e5; ++i5) {
                out.insert(i5->str(1));
            }

            return out;
        }

        // ── owner ADR → invariant id 集合 ─────────────────────────────────
        // owner 映射是**显式**的:每个 ID 前缀只属于一个 ADR。
        // 这保证"跨 ADR 引用"不会被误判为"定义"。
        struct OwnerSpec {
            std::string adr_file;                 // docs/adr/ 下的文件名
            std::vector<std::string> id_prefixes; // 该 ADR 拥有的 id 前缀
        };

        inline const std::vector<OwnerSpec>& owner_specs() {
            static const std::vector<OwnerSpec> specs = {
                {"ADR-CEP-02-coherence-barrier-semantics-clarification.md", {"CEN-Ib"}},
                {"ADR-CEP-05-stub-cache-core-interface.md", {"INV-SC", "INV-CEP-05-IC"}},
                {"ADR-CEP-04-sharer-tracker-abstraction-redesign.md", {"INV-CEP-04-IC"}},
                {"ADR-CEP-06-experiment-protocol.md", {"INV-CEP-06-IC"}},
                {"ADR-CEP-07-simulation-and-integration-semantics.md", {"INV-CEP-07-IC"}},
                {"ADR-CEP-NUMBER-MAP.md", {"INV-CEP-NUM-IC"}},
                {"ADR-CEP-08-ordering-point-portability.md", {"INV-CEP-08-IC"}},
                {"ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md", {"INV-CEP-09-IC"}},
                {"ADR-CEP-11-protocol-version-locking.md", {"INV-CEP-11-IC"}},
                {"ADR-CEP-12-protocol-engine-capability-map.md", {"INV-CEP-12-IC"}},
                {"ADR-CEP-13-coherence-domain-ownership.md", {"INV-CEP-13-IC"}},
                {"ADR-CEP-14-stub-contract-tests.md", {"INV-CEP-14-IC"}},
                {"ADR-CEP-15-cachetlm-alignment.md", {"INV-CEP-15-IC"}},
                {"ADR-CEP-16-06-evolution-route-mapping.md", {"INV-CEP-16-IC"}},
                {"ADR-CEP-07A-06bis-artifact-disposition.md", {"INV-CEP-07A-IC"}},
            };
            return specs;
        }

        // 仓库根(CPPTLM_SOURCE_DIR 由 test/CMakeLists.txt 注入)
        inline std::filesystem::path repo_root() {
#ifdef CPPTLM_SOURCE_DIR
            return std::filesystem::path(CPPTLM_SOURCE_DIR);
#else
            return std::filesystem::current_path();
#endif
        }

        // 本就不定义 invariant 的 ADR(治理 / 真源映射 / 旁路内部 / 协议族 / 生成范围)。
        // 这些 ADR **一旦开始定义 invariant 即为契约违反** → 由 no_inv_adr_files() 反向守门。
        inline const std::vector<std::string>& no_inv_adr_files() {
            static const std::vector<std::string> v = {
                "ADR-CEP-00-platform-charter-and-governance.md",
                "ADR-CEP-00A-real-source-anchor-map.md",
                "ADR-CEP-01-fabric-collective-plane.md",
                "ADR-CEP-03-fabric-collective-plane-internals.md",
                "ADR-CEP-17-ruby-slicc-protocol-family.md",
                "ADR-CEP-18-protocol-generation-scope.md",
                // 2026-10-27(Entry #74):ADR-CEP-19 = CohIf 载体形态(纯交付机制;
                // **新增 invariant 数 = 0**,per 其决策 2 / 8)⇒ 与 scripts/test/cep_docs_sync_check.sh
                // 的 NO_INV 白名单**同步登记**(两处规则重复,修一处必须记得改另一处)。
                "ADR-CEP-19-cohif-carrier-and-protocol-bridge.md",
            };
            return v;
        }

        // owner 前缀 → owner ADR 文件名(由 owner_specs() 派生)
        // 用途:**定义语义**判定 —— 一个 ID 只有在**其 owner ADR 内**才算「被定义」;
        //      在其它 ADR 内出现的是**跨 ADR 引用**(如 ADR-CEP-17 的引用表里
        //      `**INV-CEP-04-IC7**` 是引用 ADR-CEP-04 的规则,不是定义)。
        inline std::map<std::string, std::string> owner_prefix_map() {
            std::map<std::string, std::string> m;
            for (const auto& s : owner_specs()) {
                for (const auto& p : s.id_prefixes)
                    m[p] = s.adr_file;
            }
            return m;
        }

        // 某 id 的 owner ADR 文件名;无 owner → 空串
        inline std::string owner_of(const std::string& id) {
            static const std::map<std::string, std::string> m = owner_prefix_map();
            for (const auto& kv : m) {
                if (id.rfind(kv.first, 0) == 0)
                    return kv.second;
            }
            return {};
        }

        // 仓库内**全部** ADR-CEP-*.md(用于「全仓」完备性守门)
        inline std::vector<std::string> all_cep_adr_files() {
            std::vector<std::string> out;
            const std::filesystem::path dir = repo_root() / "docs" / "adr";
            if (!std::filesystem::exists(dir))
                return out;
            for (const auto& e : std::filesystem::directory_iterator(dir)) {
                const std::string name = e.path().filename().string();
                if (name.rfind("ADR-CEP-", 0) == 0 && e.path().extension() == ".md") {
                    out.push_back(name);
                }
            }
            std::sort(out.begin(), out.end());
            return out;
        }


        // 全仓"已定义"invariant id 集合(owner 之和)
        inline std::set<std::string> all_defined_invariant_ids() {
            std::set<std::string> all;
            for (const auto& s : owner_specs()) {
                const std::string text =
                    read_file((repo_root() / "docs" / "adr" / s.adr_file).string());
                const auto ids = defined_invariant_ids(text);
                all.insert(ids.begin(), ids.end());
            }
            return all;
        }

        // 某个 id 是否在**其 owner ADR** 内被定义
        inline bool is_defined(const std::string& id) {
            for (const auto& s : owner_specs()) {
                bool mine = false;
                for (const auto& p : s.id_prefixes) {
                    if (id.rfind(p, 0) == 0) {
                        mine = true;
                        break;
                    }
                }
                if (!mine)
                    continue;
                const std::string text =
                    read_file((repo_root() / "docs" / "adr" / s.adr_file).string());
                return defined_invariant_ids(text).count(id) > 0;
            }
            return false;
        }

        // 抽取指定标题的 Markdown 节(到下一个同级或更高级标题为止)
        inline std::string read_section(const std::string& doc_text,
                                        const std::string& heading_pattern) {
            if (doc_text.empty())
                return {};
            static const std::regex head(heading_pattern, std::regex::multiline);
            std::smatch m;
            if (!std::regex_search(doc_text, m, head))
                return {};
            const std::size_t body_start = static_cast<std::size_t>(m.position(0) + m.length(0));
            const std::string rest = doc_text.substr(body_start);
            static const std::regex next_head(R"(^##\s)", std::regex::multiline);
            std::smatch m2;
            if (std::regex_search(rest, m2, next_head))
                return rest.substr(0, static_cast<std::size_t>(m2.position(0)));
            return rest;
        }

        // 展开 07 号 §4 总表**第 3 列**的 ID 区间表达 → ID 集合。
        //
        // 支持的真源实际写法(per 2026-10-21 扫描 07 号 §4):
        //   `CEN-Ib1` ~ `CEN-Ib11`                    全名区间
        //   `INV-SC1` ~ `INV-SC6` + `INV-CEP-05-IC1` 区间 + 独立项
        //   `INV-CEP-04-IC1/2/3/5/6`(IC4 跳空)+ `INV-CEP-04-IC7`  斜杠紧凑式 + 独立项
        //   `INV-CEP-08-IC1` ~ `IC3`                  右端裸后缀
        //
        // ⚠️ 此前只比「条数」,**区间写错 / 组间互换不可见** —— 这是编号漂移的变体。
        inline std::set<std::string> expand_id_cell(const std::string& cell) {
            std::set<std::string> out;
            std::string t;
            for (char c : cell) {
                if (c != '`')
                    t += c;
            }
            if (t.empty())
                return out;

            static const std::string FULL = "(?:CEN-Ib|INV-SC|INV-CEP-(?:\\d+[A-Z]?|07A|NUM)-IC)";

            // 安全整数解析:越界/非数字 → 抛带上下文的异常(便于定位)
            auto to_int = [](const std::string& v, const char* what) {
                try {
                    return std::stoi(v);
                } catch (const std::exception& ex) {
                    throw std::runtime_error(std::string("expand_id_cell: ") + what +
                                             " 解析失败: '" + v + "' (" + ex.what() + ")");
                }
            };

            // A) 区间 "X ~ Y"
            //    拆成**两个**正则而非一个带 alternation 的 —— 后者的 group 索引极易错位
            //    (实测 `(?:IC)` 是非捕获组 → alt2 的数字落在 group 5 而非 6)。
            //    A1 两端全名:`INV-CEP-08-IC1 ~ INV-CEP-08-IC3` → g1=leftFULL g2=leftNum
            //    g3=rightFULL g4=rightNum A2 右端裸后缀:`INV-CEP-08-IC1 ~ IC3`           →
            //    g1=leftFULL g2=leftNum g3=bareNum
            std::regex range_full("(" + FULL + ")?(\\d+)\\s*~\\s*(" + FULL + ")?(\\d+)");
            std::regex range_bare("(" + FULL + ")?(\\d+)\\s*~\\s*(?:IC)(\\d+)");
            std::string rest;
            std::size_t cursor = 0;
            for (std::sregex_iterator it(t.begin(), t.end(), range_full), e; it != e; ++it) {
                const std::size_t b = static_cast<std::size_t>(it->position());
                const std::size_t len = static_cast<std::size_t>(it->length());
                rest += t.substr(cursor, b - cursor);
                cursor = b + len;
                const std::string lp = it->str(1);
                const int ln = to_int(it->str(2), "区间左端数字");
                std::string rp = it->str(3);
                const int rn = to_int(it->str(4), "区间右端数字");
                if (rp.empty())
                    rp = lp;
                if (!rp.empty() && ln >= 1 && rn >= ln && rn - ln < 1000) {
                    for (int k = ln; k <= rn; ++k)
                        out.insert(rp + std::to_string(k));
                }
            }
            rest += t.substr(cursor);

            { // A2:右端裸后缀
                std::string rest_b;
                cursor = 0;
                for (std::sregex_iterator it(rest.begin(), rest.end(), range_bare), e; it != e;
                     ++it) {
                    const std::size_t b = static_cast<std::size_t>(it->position());
                    const std::size_t len = static_cast<std::size_t>(it->length());
                    rest_b += rest.substr(cursor, b - cursor);
                    cursor = b + len;
                    const std::string lp = it->str(1);
                    const int ln = to_int(it->str(2), "区间左端数字");
                    const int rn = to_int(it->str(3), "区间右端裸后缀");
                    if (!lp.empty() && ln >= 1 && rn >= ln && rn - ln < 1000) {
                        for (int k = ln; k <= rn; ++k)
                            out.insert(lp + std::to_string(k));
                    }
                }
                rest_b += rest.substr(cursor);
                rest = rest_b;
            }

            // B) 斜杠紧凑式 "PREFIX-n/m/k"
            std::regex slash("(" + FULL + ")(\\d+)((?:/\\d+)+)");
            std::string rest2;
            cursor = 0;
            for (std::sregex_iterator it(rest.begin(), rest.end(), slash), e; it != e; ++it) {
                const std::size_t b = static_cast<std::size_t>(it->position());
                const std::size_t len = static_cast<std::size_t>(it->length());
                rest2 += rest.substr(cursor, b - cursor);
                cursor = b + len;
                const std::string prefix = it->str(1);
                out.insert(prefix + it->str(2));
                const std::string tails = it->str(3);
                std::size_t p = 0;
                while (p < tails.size()) {
                    if (tails[p] == '/') {
                        std::size_t q = p + 1;
                        while (q < tails.size() &&
                               std::isdigit(static_cast<unsigned char>(tails[q])))
                            ++q;
                        out.insert(prefix + tails.substr(p + 1, q - p - 1));
                        p = q;
                    } else {
                        ++p;
                    }
                }
            }
            rest2 += rest.substr(cursor);

            // C) 剩余的独立全名 ID
            std::regex single("(" + FULL + ")(\\d+)");
            for (std::sregex_iterator it(rest2.begin(), rest2.end(), single), e; it != e; ++it) {
                out.insert(it->str(1) + it->str(2));
            }
            return out;
        }

        // 读 07 号真源 **§4** 的总表行 —— 形如
        //   | **Coherence Interface** | 11 | `CEN-Ib1` ~ `CEN-Ib11` | ... |
        // 注意:必须限定在 §4 节内 —— 07 号全文还有其他 `| **X** | N |` 形态的表。
        // 返回 (组名, 声明条数, 第 3 列展开出的 ID 集合)。
        struct TruthGroup {
            std::string name;
            int declared_count = 0;
            std::set<std::string> ids;
        };

        inline std::vector<TruthGroup> read_truth_source_groups() {
            std::vector<TruthGroup> out;
            const std::string text = read_file(
                (repo_root() / "docs" / "architecture" / "07-coherence-exploration-platform.md")
                    .string());
            const std::string sec = read_section(text, R"(^##\s*4\.\s*67\s*编号位)");
            if (sec.empty())
                return out;
            static const std::regex row(
                R"(^\|\s*\*\*([^*]+?)\*\*\s*\|\s*(\d+)\s*\|\s*([^|]*?)\s*\|)",
                std::regex::multiline);
            std::sregex_iterator it(sec.begin(), sec.end(), row), end;
            for (; it != end; ++it) {
                TruthGroup g;
                g.name = it->str(1);
                g.declared_count = std::stoi(it->str(2));
                g.ids = expand_id_cell(it->str(3));
                out.push_back(std::move(g));
            }
            return out;
        }

        // ── 源码静态扫描工具(协议标识符负向断言用)──────────────────────
        // per ADR-CEP-14 §2 决策 3 C9(「Stub 源码 grep 协议标识符」)
        //     + ADR-CEP-07A §3 INV-CEP-07A-IC2(协议泄漏通用禁令)
        //
        // ⚠️ **必须先去注释再扫** —— 否则两头都错:
        //   · 头文件里解释「折叠表 / 禁止暴露」的中文散文会把被禁流名喂进来(假阳性);
        //   · 把禁用词写进注释里又能绕过(假阴性)。
        //   (per `test_cep_stub_cache_core.cc` Scenario 1/3 的既有口径)

        /// 去掉 `//` 行注释与 `/* */` 块注释
        inline std::string strip_comments(const std::string& src) {
            std::string out;
            out.reserve(src.size());
            bool in_line = false, in_block = false;
            for (size_t i = 0; i < src.size(); ++i) {
                const char c = src[i];
                const char n = (i + 1 < src.size()) ? src[i + 1] : '\0';
                if (in_line) {
                    if (c == '\n') {
                        in_line = false;
                        out.push_back(c);
                    }
                    continue;
                }
                if (in_block) {
                    if (c == '*' && n == '/') {
                        in_block = false;
                        ++i;
                    }
                    continue;
                }
                if (c == '/' && n == '/') {
                    in_line = true;
                    ++i;
                    continue;
                }
                if (c == '/' && n == '*') {
                    in_block = true;
                    ++i;
                    continue;
                }
                out.push_back(c);
            }
            return out;
        }

        /// 抽取源码中的**标识符词集**(大写归一)
        ///
        /// 切分规则(**双重边界**):先按 `_` 切,再按 CamelCase 边界切 ——
        ///   `chi_rnf`      → {CHI, RNF}
        ///   `ChiRnfAdapter`→ {CHI, RNF, ADAPTER}
        ///   `chip_id`      → {CHIP, ID}   ← 「chip」(芯片)不误判为「CHI」(协议)
        ///   `hierarchical` → {HIERARCHICAL}
        /// ⇒ 协议名可被**完整词**精确匹配,而非子串匹配(子串会把 `ARCHITECT`
        ///   误判为含 `CHI`)。
        inline std::set<std::string> identifier_words(const std::string& src) {
            std::set<std::string> out;
            // 标识符 = 字母/下划线开头,后接字母/数字/下划线
            static const std::regex tok_re(R"([A-Za-z_][A-Za-z0-9_]*)");
            for (auto it = std::sregex_iterator(src.begin(), src.end(), tok_re);
                 it != std::sregex_iterator(); ++it) {
                std::string tok = it->str();
                // 按 `_` 再切
                std::string part;
                auto flush = [&]() {
                    if (part.empty())
                        return;
                    // 按 CamelCase 边界切:连续大写 / 大写+小写串 / 小写数字串
                    static const std::regex cw_re(R"([A-Z]+(?![a-z])|[A-Z][a-z0-9]*|[a-z0-9]+)");
                    for (auto w = std::sregex_iterator(part.begin(), part.end(), cw_re);
                         w != std::sregex_iterator(); ++w) {
                        std::string s = w->str();
                        for (auto& c : s)
                            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                        out.insert(std::move(s));
                    }
                    part.clear();
                };
                for (char c : tok) {
                    if (c == '_') {
                        flush();
                    } else {
                        part.push_back(c);
                    }
                }
                flush();
            }
            return out;
        }

        /// 抽取源码中的**原始标识符**(不拆词、不改大小写)
        ///
        /// 与 `identifier_words()` 的区别:后者按 `_` + CamelCase **拆词**并大写归一
        /// (适合「协议名」这类**单词级**判定);本函数保留**原样拼写** ——
        /// 用于**复合标识符**的整体判定(如 `snoop_completion_ack` 拆词后会变成
        /// {SNOOP, COMPLETION, ACK} 三个独立单词 ⇒ 复合身份丢失)。
        /// 归一化比较请配 `normalize_identifier()`(小写 + 去 `_`)。
        inline std::set<std::string> identifier_tokens(const std::string& src) {
            std::set<std::string> out;
            static const std::regex tok_re(R"([A-Za-z_][A-Za-z0-9_]*)");
            for (auto it = std::sregex_iterator(src.begin(), src.end(), tok_re);
                 it != std::sregex_iterator(); ++it) {
                out.insert(it->str());
            }
            return out;
        }

        /// 协议标识符命中判定 —— 命中即返回命中的协议名
        ///
        /// **关键词集合**与 `test_cep_schema.cc` 的 C9 负向断言对齐
        /// ({MESI, MOESI, CHI, CXL, TILELINK, NVLINK, UALINK} + MESIF),
        /// 但**不含 TOKEN** —— token 是 MOESI_CMP 的**协议私有状态**概念
        /// (per ADR-CEP-17 §2 决策 3),不是协议名;源码扫描里 `token` 常见于
        /// 词法解析的局部变量名 ⇒ 纳入即假阳性。
        inline std::set<std::string> protocol_identifier_hits(
            const std::string& src,
            const std::set<std::string>& keywords = {"MESI", "MOESI", "CHI", "CXL", "TILELINK",
                                                     "NVLINK", "UALINK", "MESIF"}) {
            std::set<std::string> out;
            for (const auto& w : identifier_words(src)) {
                if (keywords.count(w))
                    out.insert(w);
            }
            return out;
        }

        /// 标识符**归一化**比较:小写 + 去 `_`
        ///   ⇒ `vc_binding` / `vcBinding` / `VC_Binding` 归一后同形
        ///
        /// ⚠️ **不可**用 `identifier_words()` 做这件事 —— 它会大写并按 `_` 拆词,
        ///    返回的是 `{VC, BINDING}` 而非 `{VC_BINDING}` ⇒ 与小写原名比对
        ///    **恒不命中** = 负向断言恒真空(2026-10-27 变异测试实测捕获,
        ///    per 判据纪律 ③「纯负向断言必须有正控」)。
        inline std::string normalize_identifier(const std::string& raw) {
            std::string out;
            for (char c : raw) {
                if (c == '_')
                    continue;
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

        /// 源码中是否出现指定标识符(归一化后比较,见 `normalize_identifier`)
        inline bool contains_identifier(const std::string& src, const std::string& ident) {
            const std::string want = normalize_identifier(ident);
            if (want.empty())
                return false;
            static const std::regex tok_re(R"([A-Za-z_][A-Za-z0-9_]*)");
            for (auto it = std::sregex_iterator(src.begin(), src.end(), tok_re);
                 it != std::sregex_iterator(); ++it) {
                if (normalize_identifier(it->str()) == want)
                    return true;
            }
            return false;
        }

    } // namespace cep
} // namespace cpptlm

#endif // CEP_ADR_PARSER_HH
