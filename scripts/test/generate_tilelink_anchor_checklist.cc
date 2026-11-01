// =============================================================================
// scripts/test/generate_tilelink_anchor_checklist.cc
//   —— 由**规范性载体**生成 TileLink-C 规约的 12 格逐格核对表 + 统计块,
//      并对**锚点摘要**(basis 唯一人工审计面)做**逐格交叉核对**
//
// 用途(per delta spec `TSS-07` 子条件 ④ / ⑥ + tasks 3.3 / 3.4):
//   ① 生成核对表的 12 格(载体 → 清单);
//   ② 生成统计块(载体 → 锚点摘要 §2.3bis 的机器可读区);
//   ③ **逐格**交叉核对:每格的 `ref` SHALL 解析到锚点摘要 §2.3bis 的**真实条目号**;
//      每行的**目标掩码**与**分桶** SHALL 与载体实算一致
//      ⇒ 手改任一侧(载体或摘要)⇒ **必须因正确原因红**。
//
// ⚠️ **ID 文法不得照抄 CXL 版**(per 台账 Entry #141 / 本 change 的二审 finding B-7):
//    `generate_cxl_anchor_checklist.cc` 的 `is_wellformed_id` 只认「`prefix` + **全数字**」,
//    而 TileLink 锚点用 `| **TLC-01** |`(**Markdown 粗体**)与 `| **R-T1** |`(**含字母**)
//    ⇒ 照抄 ⇒ 合法 ID 集合为**空** ⇒ 逐格校验**恒真空转**。
//    ⇒ 本文件复用 `test/cep_anchor_ids.hh`(容忍粗体 + 接受含字母前缀),与 ctest 判据**同源**。
//
// ⚠️ 本探针**只读载体**(`tilelink_state_spec.hh`)与**锚点摘要**;判据逻辑不在此复写
//    ⇒ 生成器与 ctest 判据**同源**(同 `Spec::kTransitions`)。
//
// ⛔ 合规:本文件**不**含任何 TileLink 规范正文;`ref` 只指向**条款级定位**。
//
// 作者: CppTLM Team / 日期: 2026-11-02

#include "tlm/cep/tilelink_state_spec.hh"

#include "cep_anchor_ids.hh"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Spec = tlm::cep::TileLinkStateSpec;
namespace AnchorIds = cpptlm::cep;  // ⚠️ 命名空间别名(不能用 `using`)

std::string read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// 去掉反引号 + 首尾空白。
std::string unquote(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
    if (s.size() >= 2 && s.front() == '`' && s.back() == '`') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

/// 掩码字面量 `0b111` → 数值;失败返 -1。
int parse_mask(const std::string& s) {
    std::string t = unquote(s);
    if (t.size() < 3 || t.compare(0, 2, "0b") != 0) {
        return -1;
    }
    int v = 0;
    for (std::size_t i = 2; i < t.size(); ++i) {
        if (t[i] != '0' && t[i] != '1') {
            return -1;
        }
        v = (v << 1) | (t[i] == '1' ? 1 : 0);
    }
    return v;
}

/// 分桶名。
std::string bucket_of(const Spec::Transition& c) {
    if (c.kind == Spec::CellKind::Unknown) return "unknown";
    if (c.target_mask == 0) return "empty";
    if (Spec::popcount(c.target_mask) == 1) return "single";
    return "multi";
}

/// 锚点 §2.3bis 的一行(解析结果)。
struct AnchorRow {
    std::string id;
    std::string event;
    std::string state;
    int mask = -1;
    std::string bucket;
    bool found = false;
};

/// 解析锚点 §2.3bis 的表格行 ⇒ `id → AnchorRow`。
std::map<std::string, AnchorRow> parse_anchor_rows(const std::string& doc) {
    std::map<std::string, AnchorRow> out;
    std::istringstream in(doc);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("| **TLC-", 0) != 0) {
            continue;
        }
        std::vector<std::string> cells;
        std::string cur;
        for (std::size_t i = 1; i < line.size(); ++i) {
            if (line[i] == '|') {
                cells.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(line[i]);
            }
        }
        if (cells.size() < 8) {
            continue;
        }
        AnchorRow r;
        r.id = AnchorIds::strip_bold_and_trim(cells[0]);
        r.event = unquote(cells[1]);
        r.state = unquote(cells[2]);
        r.mask = parse_mask(cells[4]);
        r.bucket = AnchorIds::strip_bold_and_trim(cells[6]);
        r.found = true;
        out[r.id] = r;
    }
    return out;
}

/// 掩码 → 字面量。
std::string mask_literal(int m) {
    std::string s = "0b";
    for (int b = 2; b >= 0; --b) {
        s.push_back(((m >> b) & 1) ? '1' : '0');
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "用法:%s <锚点摘要路径>\n", argv[0]);
        return 2;
    }
    const std::string anchor_path = argv[1];
    const std::string anchor = read_all(anchor_path);
    if (anchor.empty()) {
        std::fprintf(stderr, "GEN:FAIL 锚点摘要不可读:%s\n", anchor_path.c_str());
        return 1;
    }

    int failures = 0;
    const auto fail = [&failures](const std::string& msg) {
        std::printf("CHECK:FAIL %s\n", msg.c_str());
        ++failures;
    };

    // ── 载体侧:12 格的 (ref, 事件, 源态, 掩码, 分桶) ──────────────────────
    std::vector<std::string> refs;
    std::map<std::string, std::string> carrier_meta;  // ref → "event|state|mask|bucket"
    for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
        for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
            const auto& c = Spec::kTransitions[e][s];
            refs.emplace_back(c.ref);
            carrier_meta[c.ref] = std::string(Spec::event_name(static_cast<Spec::Event>(e))) + "|" +
                                  Spec::state_name(static_cast<Spec::State>(s)) + "|" +
                                  mask_literal(c.target_mask) + "|" + bucket_of(c);
            // C4:逐格 basis 口径
            if (c.basis != Spec::BasisKind::Direct) {
                fail(std::string("载体格 ") + c.ref + " 的 basis.kind SHALL 为 direct");
            }
            if (c.pending) {
                fail(std::string("载体格 ") + c.ref + " 的 pending SHALL 为 false");
            }
            // C5:条款级定位
            const std::string note(c.note ? c.note : "");
            if (note.find("D1 §8.1.4") == std::string::npos) {
                fail(std::string("载体格 ") + c.ref + " 的 note 缺条款级定位(D1 §8.1.4)");
            }
        }
    }

    // ── C1:载体 refs ↔ 锚点条目号 **一一对应** ─────────────────────────
    const auto anchor_ids = AnchorIds::collect_ids(anchor, "TLC-");
    if (!AnchorIds::refs_resolve_one_to_one(refs, anchor_ids)) {
        fail("载体 12 个 ref 与锚点 §2.3bis 的条目号**未一一对应**(逐格白名单比对失败)");
    }

    // ── C2 / C3:锚点每行的事件/源态/掩码/分桶与载体一致 ─────────────────
    const auto rows = parse_anchor_rows(anchor);
    for (const auto& [id, meta] : carrier_meta) {
        const auto it = rows.find(id);
        if (it == rows.end()) {
            fail(std::string("锚点 §2.3bis 缺条目号 ") + id);
            continue;
        }
        const auto& r = it->second;
        std::string ev, st, mk, bk;
        {
            std::stringstream ss(meta);
            std::getline(ss, ev, '|');
            std::getline(ss, st, '|');
            std::getline(ss, mk, '|');
            std::getline(ss, bk, '|');
        }
        if (r.event != ev) {
            fail(id + ":事件不一致(锚点 " + r.event + " vs 载体 " + ev + ")");
        }
        if (r.state != st) {
            fail(id + ":源态不一致(锚点 " + r.state + " vs 载体 " + st + ")");
        }
        if (r.mask < 0 || mask_literal(r.mask) != mk) {
            fail(id + ":目标掩码不一致(锚点 " + (r.mask < 0 ? std::string("?") : mask_literal(r.mask)) +
                 " vs 载体 " + mk + ")");
        }
        if (r.bucket != bk) {
            fail(id + ":分桶不一致(锚点 " + r.bucket + " vs 载体 " + bk + ")");
        }
    }

    if (failures > 0) {
        std::printf("GEN:FAIL 交叉核对失败(%d 项)\n", failures);
        return 1;
    }

    // ── 输出块 ─────────────────────────────────────────────────────────
    // MATRIX:12 格核对表(载体 → 清单)
    std::printf("===TL:MATRIX===\n");
    std::printf("| 条目号 | 事件 | 源态 | 目标掩码 | 分桶 | 载体 `ref` |\n");
    std::printf("|---|---|---|---|---|---|\n");
    for (std::size_t e = 0; e < Spec::kEventCount; ++e) {
        for (std::size_t s = 0; s < Spec::kStateCount; ++s) {
            const auto& c = Spec::kTransitions[e][s];
            const std::string mk = mask_literal(c.target_mask);
            std::printf("| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` |\n", c.ref,
                        Spec::event_name(static_cast<Spec::Event>(e)),
                        Spec::state_name(static_cast<Spec::State>(s)), mk.c_str(),
                        bucket_of(c).c_str(), c.ref);
        }
    }

    // STATS:分桶统计 + 条目号清单
    std::printf("===TL:STATS===\n");
    std::printf("| 指标 | 值 |\n|---|---|\n");
    std::printf("| 稳定态数 | %zu |\n", Spec::kStateCount);
    std::printf("| 事件数 | %zu |\n", Spec::kEventCount);
    std::printf("| 转移格数 | %zu |\n", Spec::kTransitionCellCount);
    std::printf("| `single` | %zu |\n", Spec::kSingleCellCount);
    std::printf("| `multi` | %zu |\n", Spec::kMultiCellCount);
    std::printf("| `empty` | %zu |\n", Spec::kEmptyCellCount);
    std::printf("| `unknown` | %zu |\n", Spec::kUnknownCellCount);
    std::printf("| 逐格条目号 | %s |\n", [&] {
        std::string s;
        for (std::size_t i = 0; i < refs.size(); ++i) {
            if (i != 0) s += ", ";
            s += "`" + refs[i] + "`";
        }
        return s;
    }().c_str());

    std::printf("GEN:OK 载体 ↔ 锚点逐格交叉核对通过(%zu 格)\n", Spec::kTransitionCellCount);
    return 0;
}
