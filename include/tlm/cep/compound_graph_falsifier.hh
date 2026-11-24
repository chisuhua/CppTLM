// include/tlm/cep/compound_graph_falsifier.hh
// 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的**证伪工装**（操作级）
//
// 依据:
//   · `ADR-CEP-13` 决策 2 —— 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`,
//     其中 `ppo_com(t) ≡ ppo_i`(t 属于域 i)
//   · `ADR-CEP-13` 决策 5 ② —— 「正确性由**随机 / litmus 实验在声明的 `verification_scope` 内
//     尝试证伪**;**平台不提供无违例保证**」⇒ 本工装**只尝试证伪**,⛔ **不得**输出「已证明」
//   · `ADR-CEP-22` 决策 4（域间边构造规则）· 决策 5（候选与 ∃/∀ 聚合）· 决策 6（双图折衷）
//     · 决策 7（判定面四态 + 原因码闭集）
//   · `ADR-CEP-12` 决策 3 —— `consistency_labels` 的**用途** = `ADR-CEP-13` 的跨域组合判据
//
// ⚠️ **图的两型（`ADR-CEP-22` 决策 6 · 双图折衷）**:
//   · **公理面**（证伪图的输入）= `ppo_com` ∪ `rfe` ∪ `fr` ∪ `ws`
//     —— ⛔ **不含**域内 `rf`（`rf` 不在公理公式里 ⇒ 域内读由**域自身模型**约束）
//   · **观测面**（判定产物报告）= 公理面 4 类 **+** 域内 `rf`
//   ⇒ 两面的边类集合**不得混用**：证伪函数只接受公理面图。
//
// ⚠️ **诚实边界（本工装的第一结论，不是缺陷）**:
//   ① 本工装**只尝试证伪**:无环 ⇒ 「在本次语料范围内**未证伪**」,⛔ 不是「已证明」。
//   ② 本工装**不检查域内一致性**（值面 / SWMR / reads-see-latest）—— per `ADR-CEP-22`
//      §4.2 缺口①：域内一致性是**各域自身的验证义务**。
//   ③ 本工装**不验证**「迹中的域标识忠实代表 Ordering Point 作用域」—— per `ADR-CEP-22`
//      §4.2 缺口②：该假设由**迹作者**承担（域不进 schema ⇒ 配置面无机器判据）。
//   ④ `fr` 的**约定形态**（读→写 vs 写→写）须由实施侧以一手锚点复核并**显式登记**
//      —— per `ADR-CEP-22` 决策 4 的「约定依赖」段（⚠️ 论文原文不在本仓 ⇒ 仓内不可复核）。
//
// 作者: CppTLM Team / 日期: 2026-11-21（消歧声明 2026-11-23 追加 · 2026-11-24 操作级改型）
#ifndef CPPTLM_CEP_COMPOUND_GRAPH_FALSIFIER_HH
#define CPPTLM_CEP_COMPOUND_GRAPH_FALSIFIER_HH

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/memory_model_trace.hh"

namespace tlm::cep {

// ── 消歧常量（2026-11-23 · 「litmus」命名与能力错配的消歧 · 阶段 1）────────────
//
// ⚠️ **本工装不是内存模型 litmus**（`herd7` memory-model litmus）—— 两者**同名不同物**：
//   · **本工装**：检验**操作级复合图**上的**无环性**（`ppo_com ∪ rfe ∪ fr ∪ ws`）。
//     顶点 = **操作**（读 / 写 + 地址 + 值）；域 = Ordering Point 作用域。
//   · **论文侧不是本工装**：Oswald 博士论文 §8.7.2 / HeteroGen §VII 用 `herd7` 生成 111 个异构 litmus，
//     那**不是**本工装 —— 它检验**执行迹的允许结果**（运行期语义的可满足性）。
//   ⇒ **范畴不同**：一个是**操作级复合图**的**结构**判据，一个是运行期**内存模型公理**的语义判据。
//   ⛔ 因此**不得**据本工装的存在声称「CEP 已能证伪复合一致性公理」
//     （per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」）。
//
// ⚠️ **不要**由**旧文件名** `litmus_falsifier` 推断本工装具备内存模型 litmus 能力 ——
//   该能力**不存在**；该旧名已于 2026-11-24 退役为 `compound_graph_falsifier`（改名 · 判定语义零变更）。
// ==============================================================================

/// **消歧常量（2026-11-23）** —— 本工装的判据作用域（= **操作级复合图**上的**无环性**）。
///
/// ⛔ **不是** `herd7` memory-model litmus（后者检验执行迹的允许结果，见文件头「消歧声明」段）。
/// 下游可 `static_assert(!kFalsifierIsMemoryModelLitmus)` 把「本工装不是内存模型 litmus」
/// 变成**编译期**事实，而非散文。
inline constexpr const char* kFalsifierScope = "operation-level-composite-graph";

/// **消歧常量（2026-11-23）** —— 本工装**不是**内存模型 litmus（herd7 memory-model litmus）。
///
/// ⛔ **恒为 `false`**；改 `true` ⇒ 违反 `test/test_cep_compound_graph_falsification.cc` 的
/// `static_assert` ⇒ 编译失败（见 `scripts/test/cep_litmus_naming_check.sh` 的 probe 反 fixture）。
inline constexpr bool kFalsifierIsMemoryModelLitmus = false;

// ─────────────────────────────────────────────────────────────────────────────
// 判定面（`ADR-CEP-22` 决策 7 · 四态 + 原因码闭集）
// ─────────────────────────────────────────────────────────────────────────────

/// 判定四态。⛔ **无「已证明」一档**（per `ADR-CEP-13` 决策 5 ②）。
enum class FalsifyVerdict {
    Falsified,     ///< **已证伪**：全部候选成环
    NotFalsified,  ///< **未证伪**：存在可行候选（⛔ ≠「无违例」≠「已证明」）
    Undecided,     ///< **不可判**：截断 / 域模型未登记
    Invalid,       ///< **输入非法（拒收）**：⛔ **不是**判定态，与「不可判」必须区分
};

/// 原因码**闭集**（`ADR-CEP-22` 决策 7 · 每码一条反 fixture）。
inline const std::vector<std::string>& falsify_reason_codes() {
    static const std::vector<std::string> kCodes = {
        "derived-edge-as-input",   // 语料直接给出 `fr` / `ws`（派生边 ⇒ 不得作输入）
        "channel-edge-not-op-edge",// 通道边混入操作图（通道图 ≠ 操作图 ⇒ 混图 = 假绿）
        "unbound-domain",          // 线程未声明域 / 域标识不在域声明表
        "no-domain-instance",       // 域声明的实例 id 不在声明面
        "model-semantics-unavailable", // 域标签在 v1 规则表外（TSO / 未知）
        "search-truncated",         // 候选枚举触上限（⛔ 不得判「未证伪」）
        "malformed-trace",          // 解析失败 / 缺必填字段 / 锚点不在 allowlist
        "rf-target-missing",        // `rf` 指向不存在的操作 / 端点类别不符
        "duplicate-op-id",          // 操作 id 重复
        "self-ppo-edge",            // 线程的 op 序列里同一 op 出现两次（自反程序序边）
        "rf-self-loop",             // `rf` 的读与写是同一操作
    };
    return kCodes;
}

/// 一次复合图证伪尝试的判定。
struct CompoundGraphVerdict {
    FalsifyVerdict verdict = FalsifyVerdict::Invalid;
    std::string reason;                    ///< 原因码（⛔ 恒在 `falsify_reason_codes()` 内或空）
    std::string detail;                    ///< 人读说明
    bool search_truncated = false;         ///< 候选枚举是否触上限
    std::size_t candidates_examined = 0;   ///< 已考察的候选数
    std::size_t candidates_total = 0;      ///< 候选总数（笛卡尔积规模）
    std::vector<std::string> edge_classes_observed;    ///< 观测面：全部出现的边类
    std::vector<std::string> cross_domain_edge_classes;/// 观测面：跨域关系上的边类（⊆ schema 枚举）
    std::vector<std::string> rf_edge_classes;          ///< 观测面：读自族的边类（`rf` / `rfe`）
    std::vector<std::string> cycle;        ///< 见证：环上的操作 id 序列（成环时）
    std::string co_order;                  ///< 成环候选的标识（⛔ 逐地址候选 id 的连接）
    /// **逐候选**判定（AC-7 的「逐候选判定」面）—— ⛔ 聚合判定见 `verdict`。
    struct CandidateOutcome {
        std::string id;        ///< 候选标识（逐地址候选 id 的连接）
        bool cyclic = false;   ///< 该候选下公理面图是否成环
    };
    std::vector<CandidateOutcome> candidate_outcomes;
    /// **公理面**的边（`"from→to:cls"` 形态）—— 逐条可核对（AC-3② 的「派生边集合逐条等于期望集」）。
    /// ⚠️ 取**第一个**被考察候选的边集（⛔ 不是并集 ⇒ 跨候选会变）。
    std::vector<std::string> axiom_edges;
    std::string coverage_note;             ///< ⚠️ 覆盖说明（必须随判定一起读）—— 恒不为空
};

namespace detail {

/// 公理面图的边（顶点 = 操作下标）。
struct AxEdge {
    std::size_t from = 0;
    std::size_t to = 0;
    const char* cls = "";  ///< `ppo_com` / `rfe` / `fr` / `ws`
};

/// DFS 三色环检测（返回找到的第一个环；空 = 无环）。
inline std::vector<std::size_t> find_cycle(const std::vector<AxEdge>& edges, std::size_t n) {
    std::vector<std::vector<std::size_t>> adj(n);
    for (const auto& e : edges) adj[e.from].push_back(e.to);
    std::vector<int> color(n, 0);  // 0 白 · 1 灰 · 2 黑
    std::vector<std::size_t> stack;
    std::vector<std::size_t> cycle;

    for (std::size_t start = 0; start < n && cycle.empty(); ++start) {
        if (color[start] != 0) continue;
        // 迭代式 DFS（避免深递归）
        std::vector<std::pair<std::size_t, std::size_t>> frames;  // (顶点, 邻接游标)
        frames.emplace_back(start, 0);
        color[start] = 1;
        stack.push_back(start);
        while (!frames.empty() && cycle.empty()) {
            auto& [v, idx] = frames.back();
            if (idx < adj[v].size()) {
                const std::size_t next = adj[v][idx++];
                if (color[next] == 1) {
                    // 回边 ⇒ 截取环片段
                    const auto it = std::find(stack.begin(), stack.end(), next);
                    cycle.assign(it, stack.end());
                    cycle.push_back(next);
                } else if (color[next] == 0) {
                    color[next] = 1;
                    stack.push_back(next);
                    frames.emplace_back(next, 0);
                }
            } else {
                color[v] = 2;
                stack.pop_back();
                frames.pop_back();
            }
        }
    }
    return cycle;
}

}  // namespace detail

/// 复合图证伪尝试（**操作级**）。
///
/// **输入**：一份操作迹（`mm::Trace`）+ 声明面（实例 `id` → `consistency_labels`）+ 候选上限。
/// **输出**：四态判定 + 观测面边类 + 见证。
///
/// **判定语义**（`ADR-CEP-22` 决策 5 ③）：**∃ 可行候选 ⇒ `not_falsified`**（短路）；
/// **∀ 候选成环 ⇒ `falsified`**（需穷举）；**触上限且未找到可行候选 ⇒ `undecided`**。
/// ⛔ **不得**把「某候选成环」直接当「证伪」（那是假阳性）。
inline CompoundGraphVerdict evaluate_compound_graph_falsification(
    const mm::Trace& trace, const mm::InstanceLabelMap& instances,
    std::size_t co_enumeration_cap = 4096) {
    CompoundGraphVerdict v;
    v.coverage_note =
        "复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的判定面 = **操作级**复合图"
        "(顶点 = 操作);⛔ **只尝试证伪**、⛔ **不检查域内一致性**(各域自身义务,per `ADR-CEP-22`"
        " §4.2 缺口①)、⛔ **不验证**域标识对 Ordering Point 作用域的忠实性(缺口②);"
        "⛔ 无环**不等于**公理成立(per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」)";

    // ── ① 域模型解析（决策 2：来源 = 域绑定实例的 `consistency_labels`）────────
    std::map<std::string, mm::InstanceLabels> domain_labels;
    for (const auto& d : trace.domains) {
        const auto it = instances.find(d.instance);
        if (it == instances.end()) {
            v.verdict = FalsifyVerdict::Invalid;
            v.reason = "no-domain-instance";
            v.detail = "域 `" + d.id + "` 绑定的实例 `" + d.instance + "` 不在声明面内";
            return v;
        }
        domain_labels[d.id] = it->second;
        // ⛔ v1 未登记的标签（TSO / 未知）⇒ 整域不可判（⛔ 不得静默当 SC）
        if (it->second.read == mm::Label::TSO || it->second.write == mm::Label::TSO ||
            it->second.read == mm::Label::Unknown || it->second.write == mm::Label::Unknown) {
            v.verdict = FalsifyVerdict::Undecided;
            v.reason = "model-semantics-unavailable";
            v.detail = "域 `" + d.id + "`（实例 `" + d.instance + "`）的标签 = " +
                       std::string(mm::label_name(it->second.read)) + "/" +
                       mm::label_name(it->second.write) +
                       " ⇒ v1 规则表未登记 ⇒ **不可判**（⛔ 不得静默当 SC）";
            return v;
        }
    }

    // ── ② 操作索引 ──────────────────────────────────────────────────────────
    std::map<std::string, std::size_t> op_index;
    for (std::size_t i = 0; i < trace.ops.size(); ++i) op_index[trace.ops[i].id] = i;
    const std::size_t n = trace.ops.size();

    // ── ③ 线程内程序序（`ppo_com` 的源；授权 per 决策 3）────────────────────
    std::vector<std::vector<std::size_t>> thread_ops;
    {
        std::map<std::string, std::vector<std::size_t>> by_thread;
        for (std::size_t i = 0; i < n; ++i) by_thread[trace.ops[i].thread].push_back(i);
        for (auto& [tid, ops] : by_thread) {
            (void)tid;
            thread_ops.push_back(std::move(ops));
        }
    }

    // ── ④ 读自映射 → 域内 `rf` / 跨域 `rfe`（决策 4）─────────────────────────
    struct RfEdge {
        std::size_t w = 0;
        std::size_t r = 0;
        bool cross_domain = false;
    };
    std::vector<RfEdge> rf_edges;
    for (const auto& [rid, wid] : trace.rf) {
        const auto rit = op_index.find(rid);
        const auto wit = op_index.find(wid);
        if (rit == op_index.end() || wit == op_index.end()) {
            v.verdict = FalsifyVerdict::Invalid;
            v.reason = "rf-target-missing";
            v.detail = "`rf` 指向不存在的操作";
            return v;
        }
        RfEdge e;
        e.w = wit->second;
        e.r = rit->second;
        e.cross_domain = (trace.ops[e.w].domain != trace.ops[e.r].domain);
        rf_edges.push_back(e);
    }

    // ── ⑤ 候选枚举空间（逐地址写序候选的**笛卡尔积**，per 决策 5 ①）──────────
    std::vector<mm::AddrCoChoices> choices = trace.co_choices;
    std::sort(choices.begin(), choices.end(),
              [](const mm::AddrCoChoices& a, const mm::AddrCoChoices& b) { return a.addr < b.addr; });
    std::size_t total = 1;
    for (const auto& ch : choices) total *= std::max<std::size_t>(1, ch.candidates.size());
    v.candidates_total = total;

    // 观测面边类（⛔ 与公理面**分开**报告，per 决策 6）
    std::set<std::string> observed;
    std::set<std::string> cross_classes;
    std::set<std::string> rf_classes;
    for (const auto& e : rf_edges) {
        if (e.cross_domain) {
            observed.insert("rfe");
            cross_classes.insert("rfe");
            rf_classes.insert("rfe");
        } else {
            observed.insert("rf");  // 域内 `rf`：**只进观测面**（⛔ 不进公理面）
            rf_classes.insert("rf");
        }
    }

    // ── ⑥ 逐候选：构造**静态图**并判环 ────────────────────────────────────
    std::vector<std::size_t> picked(choices.size(), 0);
    bool found_acyclic = false;
    bool truncated = false;
    std::vector<std::string> cycle_ids;
    std::string co_order;
    std::string current_co_id;

    do {
        // 当前候选的逐地址写序（每个地址取 picked[i] 号候选；⛔ 无候选 ⇒ 无写序约束）
        std::map<std::size_t, std::vector<std::size_t>> ws_order;  // 写 id 下标 → 同址写的序列
        std::vector<std::string> co_ids;
        for (std::size_t i = 0; i < choices.size(); ++i) {
            const auto& ch = choices[i];
            if (ch.candidates.empty()) continue;
            const auto& cand = ch.candidates[picked[i] % ch.candidates.size()];
            co_ids.push_back(cand.id);
            std::vector<std::size_t> seq;
            for (const auto& wid : cand.writes) {
                const auto it = op_index.find(wid);
                if (it != op_index.end()) seq.push_back(it->second);
            }
            ws_order[ch.addr] = std::move(seq);
        }

        // 公理面图
        std::vector<detail::AxEdge> edges;
        // ⑥.1 `ppo_com`（域内程序序，按决策 3 的授权表过滤）
        for (const auto& ops : thread_ops) {
            for (std::size_t a = 0; a < ops.size(); ++a) {
                for (std::size_t b = a + 1; b < ops.size(); ++b) {
                    const auto& src = trace.ops[ops[a]];
                    const auto& dst = trace.ops[ops[b]];
                    const auto lbl_it = domain_labels.find(src.domain);
                    if (lbl_it == domain_labels.end()) continue;
                    const auto kind = mm::po_edge_kind(src.kind, dst.kind);
                    if (!mm::domain_preserves_po(lbl_it->second, kind)) continue;
                    edges.push_back({ops[a], ops[b], "ppo_com"});
                    observed.insert("ppo_com");
                }
            }
        }
        // ⑥.2 `rfe`（跨域读自）
        for (const auto& e : rf_edges) {
            if (!e.cross_domain) continue;
            edges.push_back({e.w, e.r, "rfe"});
        }
        // ⑥.3 `ws`（**同地址**写序，per 决策 4）
        for (const auto& [addr, seq] : ws_order) {
            (void)addr;
            for (std::size_t i = 0; i < seq.size(); ++i) {
                for (std::size_t j = i + 1; j < seq.size(); ++j) {
                    edges.push_back({seq[i], seq[j], "ws"});
                    observed.insert("ws");
                    if (trace.ops[seq[i]].domain != trace.ops[seq[j]].domain)
                        cross_classes.insert("ws");
                }
            }
        }
        // ⑥.4 `fr(r, w2) ⟺ ∃w1: rf(w1,r) ∧ ws(w1,w2)`（决策 4 · 静态读→写形态）
        for (const auto& e : rf_edges) {
            for (const auto& [addr, seq] : ws_order) {
                (void)addr;
                for (std::size_t i = 0; i < seq.size(); ++i) {
                    if (seq[i] != e.w) continue;  // ws(w1 → w2) 的 w1
                    for (std::size_t j = i + 1; j < seq.size(); ++j) {
                        edges.push_back({e.r, seq[j], "fr"});
                        observed.insert("fr");
                        if (trace.ops[e.r].domain != trace.ops[seq[j]].domain)
                            cross_classes.insert("fr");
                    }
                }
            }
        }

        ++v.candidates_examined;
        {
            current_co_id.clear();
            for (const auto& cid : co_ids) {
                if (!current_co_id.empty()) current_co_id += "+";
                current_co_id += cid;
            }
        }
        const auto cyc = detail::find_cycle(edges, n);
        v.candidate_outcomes.push_back({current_co_id, !cyc.empty()});
        if (v.axiom_edges.empty()) {
            for (const auto& e : edges) {
                v.axiom_edges.push_back(trace.ops[e.from].id + "→" + trace.ops[e.to].id + ":" + e.cls);
            }
        }
        if (cyc.empty()) {
            found_acyclic = true;  // ∃ 可行候选 ⇒ 未证伪（短路）
            break;
        }
        if (cycle_ids.empty()) {  // 记录**第一个**成环候选作见证
            for (const auto idx : cyc) cycle_ids.push_back(trace.ops[idx].id);
            co_order = current_co_id;
        }

        // 下一个候选（笛卡尔积的 odometer：进位时**复位**低位）
        std::size_t dim = 0;
        bool done = true;
        while (dim < choices.size()) {
            if (choices[dim].candidates.empty()) { ++dim; continue; }
            ++picked[dim];
            if (picked[dim] < choices[dim].candidates.size()) { done = false; break; }
            picked[dim] = 0;  // 进位
            ++dim;
        }
        if (done) break;
        if (v.candidates_examined >= co_enumeration_cap) {
            truncated = true;
            break;
        }
    } while (true);

    v.edge_classes_observed.assign(observed.begin(), observed.end());
    v.cross_domain_edge_classes.assign(cross_classes.begin(), cross_classes.end());
    v.rf_edge_classes.assign(rf_classes.begin(), rf_classes.end());
    v.search_truncated = truncated;

    if (found_acyclic) {
        v.verdict = FalsifyVerdict::NotFalsified;
        v.detail = "存在可行候选 ⇒ 公理**未被反驳**（本次语料范围内**未证伪**）";
        return v;
    }
    if (truncated) {
        v.verdict = FalsifyVerdict::Undecided;
        v.reason = "search-truncated";
        v.detail = "候选枚举触上限（已考察 " + std::to_string(v.candidates_examined) + " / 共 " +
                   std::to_string(v.candidates_total) + "）且尚未找到可行候选 ⇒ **不可判**"
                   "（⛔ 不得判「未证伪」——尚有未查候选；⛔ 不得判「已证伪」——未穷举）";
        return v;
    }
    // ∀ 候选成环 ⇒ 已证伪
    v.verdict = FalsifyVerdict::Falsified;
    v.cycle = cycle_ids;
    v.co_order = co_order;
    v.detail = "全部候选均成环 ⇒ 复合公理在该迹下被**推翻**（见证 = " +
               (co_order.empty() ? std::string("<单一候选>") : co_order) + "）";
    return v;
}

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_COMPOUND_GRAPH_FALSIFIER_HH
