// scripts/test/generate_cxl_anchor_checklist.cc
//   —— 由**规范性载体**生成 CXL.cache 规约的 68 格逐格核对表 + 统计块,
//      并对**锚点摘要**(basis 唯一人工审计面)做**交叉核对**
//
// 用途(per delta spec `CSS-06` / 提案 `AC-3`):
//   ① 生成核对表的 68 格(载体 → 清单);
//   ② 生成统计块(载体 → 摘要 §8.4 的机器可读区);
//   ③ **交叉核对**:每格的 `ref` SHALL 解析到锚点摘要 §8 的**真实条目号**;
//      每格 `note` 里的 `R-n` SHALL 解析到摘要 §6 的**真实残余风险编号**
//      ⇒ 手改任一侧(载体或摘要)⇒ **必须因正确原因红**。
//
// ⚠️ 本探针**只读载体**(`cxl_state_spec.hh`)与**锚点摘要**;判据逻辑不在此复写
//    ⇒ 生成器与 ctest 判据**同源**(同 `Spec::kTransitions`)。
//
// ⛔ 合规:本文件**不**含任何 CXL 规范正文;`ref` 只指向**公开非规范来源**的条目号。
//
// 作者: CppTLM Team / 日期: 2026-11-01

#include "tlm/cep/cxl_state_spec.hh"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Spec = tlm::cep::CxlStateSpec;

std::string read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// 形如 `D12` / `R-3` 的**合法条目号**(纯数字后缀)—— 排除 `D2H Req` 这类通道名。
bool is_wellformed_id(const std::string& id, const char* prefix) {
    const std::size_t plen = std::strlen(prefix);
    if (id.size() <= plen || id.compare(0, plen, prefix) != 0) {
        return false;
    }
    for (std::size_t i = plen; i < id.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(id[i]))) {
            return false;
        }
    }
    return true;
}

/// 从摘要里抽出形如 `| D12 |` / `| R-3 |` 的**条目号**集合。
std::set<std::string> collect_ids(const std::string& doc, const char* prefix) {
    std::set<std::string> ids;
    const std::string key = std::string("| ") + prefix;
    std::size_t pos = 0;
    while ((pos = doc.find(key, pos)) != std::string::npos) {
        const std::size_t s = pos + 2;  // 跳过 "| "
        std::size_t e = doc.find('|', s);
        if (e == std::string::npos) {
            break;
        }
        std::string id = doc.substr(s, e - s);
        while (!id.empty() && (id.back() == ' ' || id.back() == '\t')) {
            id.pop_back();
        }
        if (!id.empty() && is_wellformed_id(id, prefix)) {
            ids.insert(id);
        }
        pos = e;
    }
    return ids;
}

/// 取 `ref` 里的**首部条目号**(如 `D1 p.5 Fig.4` → `D1`);无则空串。
std::string leading_id(const std::string& ref) {
    std::size_t i = 0;
    while (i < ref.size() && (ref[i] == ' ' || ref[i] == '\t')) {
        ++i;
    }
    std::size_t j = i;
    while (j < ref.size() && (std::isalnum(static_cast<unsigned char>(ref[j])) || ref[j] == '-')) {
        ++j;
    }
    return ref.substr(i, j - i);
}

/// 抽出 `note` 里出现的全部 `R-n` 编号。
std::vector<std::string> collect_r_ids(const std::string& note) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while ((pos = note.find("R-", pos)) != std::string::npos) {
        std::size_t j = pos + 2;
        while (j < note.size() && std::isdigit(static_cast<unsigned char>(note[j]))) {
            ++j;
        }
        if (j > pos + 2) {
            out.push_back(note.substr(pos, j - pos));
        }
        pos = j;
    }
    return out;
}

std::string cell_kind_name(Spec::CellKind k) {
    switch (k) {
        case Spec::CellKind::Known: return "Known";
        case Spec::CellKind::Illegal: return "Illegal";
        case Spec::CellKind::Unknown: return "Unknown";
    }
    return "?";
}

std::string sanitize(std::string s) {
    for (auto& c : s) {
        if (c == '|') {
            c = '/';
        }
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string summary_path =
        argc > 1 ? argv[1] : "docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md";
    const std::string summary = read_all(summary_path);

    int fail = 0;
    if (summary.empty()) {
        std::printf("CHECK:summary_readable:FAIL(%s)\n", summary_path.c_str());
        ++fail;
    }

    const std::set<std::string> d_ids = collect_ids(summary, "D");
    const std::set<std::string> r_ids = collect_ids(summary, "R-");

    std::printf("CHECK:summary_d_ids:%zu\n", d_ids.size());
    std::printf("CHECK:summary_r_ids:%zu\n", r_ids.size());
    if (d_ids.empty()) {
        std::printf("CHECK:d_ids_present:FAIL\n");
        ++fail;
    }
    if (r_ids.empty()) {
        std::printf("CHECK:r_ids_present:FAIL\n");
        ++fail;
    }

    // ── 逐格交叉核对 ────────────────────────────────────────────────────────
    std::size_t n_known = 0, n_illegal = 0, n_unknown = 0, n_pending_false = 0;
    for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
        for (std::size_t st = 0; st < Spec::kStateCount; ++st) {
            const auto& cell = Spec::kTransitions[e][st];
            switch (cell.kind) {
                case Spec::CellKind::Known: ++n_known; break;
                case Spec::CellKind::Illegal: ++n_illegal; break;
                case Spec::CellKind::Unknown: ++n_unknown; break;
            }
            if (!cell.pending && cell.kind == Spec::CellKind::Known) {
                ++n_pending_false;
            }
            // ① `ref` SHALL 解析到摘要 §8 的真实条目号
            if (cell.ref[0] != '\0') {
                const std::string id = leading_id(cell.ref);
                if (id.empty() || d_ids.count(id) == 0) {
                    std::printf("CHECK:ref_resolves:%s/%s:FAIL(%s)\n",
                                Spec::event_name(static_cast<Spec::Event>(e)),
                                Spec::state_name(static_cast<Spec::State>(st)), cell.ref);
                    ++fail;
                }
            }
            // ② `note` 里的 `R-n` SHALL 解析到摘要 §6 的真实编号
            for (const auto& rid : collect_r_ids(cell.note)) {
                if (r_ids.count(rid) == 0) {
                    std::printf("CHECK:r_id_resolves:%s/%s:FAIL(%s)\n",
                                Spec::event_name(static_cast<Spec::Event>(e)),
                                Spec::state_name(static_cast<Spec::State>(st)), rid.c_str());
                    ++fail;
                }
            }
            // ③ `Unknown` 格的 `note` SHALL 带 `R-` 编号(判据 CSS-03 ⑨)
            if (cell.kind == Spec::CellKind::Unknown && collect_r_ids(cell.note).empty()) {
                std::printf("CHECK:unknown_has_r:%s/%s:FAIL\n",
                            Spec::event_name(static_cast<Spec::Event>(e)),
                            Spec::state_name(static_cast<Spec::State>(st)));
                ++fail;
            }
        }
    }

    // ── 输出:68 格核对表 ───────────────────────────────────────────────────
    std::printf("===CXL:MATRIX===\n");
    std::printf("| # | 事件 | 态 | 格语义 | 目标态 | kind | pending | ref | note |\n");
    std::printf("|---|---|---|---|---|---|---|---|---|\n");
    std::size_t idx = 0;
    for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
        for (std::size_t st = 0; st < Spec::kStateCount; ++st) {
            const auto& cell = Spec::kTransitions[e][st];
            const std::string target_txt =
                (cell.kind == Spec::CellKind::Known) ? Spec::state_name(cell.target) : "-";
            std::printf("| %zu | %s | %s | %s | %s | %s | %s | %s | %s |\n", ++idx,
                        Spec::event_name(static_cast<Spec::Event>(e)),
                        Spec::state_name(static_cast<Spec::State>(st)),
                        cell_kind_name(cell.kind).c_str(), target_txt.c_str(),
                        Spec::basis_kind_name(cell.basis), cell.pending ? "true" : "false",
                        sanitize(cell.ref).c_str(), sanitize(cell.note).c_str());
        }
    }

    // ── 输出:统计块(写入摘要的机器可读区)──────────────────────────────────
    std::printf("===CXL:STATS===\n");
    std::printf("cells=%zu\n", Spec::kTransitionCellCount);
    std::printf("known=%zu\n", n_known);
    std::printf("illegal=%zu\n", n_illegal);
    std::printf("unknown=%zu\n", n_unknown);
    std::printf("pending_false=%zu\n", n_pending_false);
    std::printf("coverage=%zu/%zu\n", Spec::kLegalTransitionCoverageNum,
                Spec::kLegalTransitionCoverageDen);
    std::printf("coverage_floor=%zu\n", Spec::kCoverageFloor);
    std::printf("pending_false_floor=%zu\n", Spec::kPendingFalseFloor);

    if (fail != 0) {
        std::printf("GEN:FAIL(%d)\n", fail);
        return 1;
    }
    std::printf("GEN:PASS\n");
    return 0;
}
