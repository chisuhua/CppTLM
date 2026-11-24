// include/tlm/cep/memory_model_trace.hh
// 操作级轨迹模型 —— 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的**输入面**
//
// 依据:
//   · `ADR-CEP-22` 决策 1（域身份 = 迹侧域标识）· 决策 2（模型来源 = 实例的
//     `consistency_labels`）· 决策 3（`ppo_i` = 读侧/写侧授权表的并集）
//   · `ADR-CEP-13` 决策 1（一致性域 = 一个 Ordering Point 生效 + 一套自己的 `ppo` 生效）
//   · `ADR-CEP-12` 决策 3（`consistency_labels` 的**用途** = `ADR-CEP-13` 的跨域组合判据）
//   · `ADR-CEP-12` 决策 4（请求级标签）—— 本模型按**读写两级**消费（⛔ 请求级映射由协议侧解释）
//
// ⛔ **范围纪律（须逐字随引用传播）**:
//   · 本模型**只**承载「操作 / 域绑定 / 读自映射 / 同地址写序候选」—— ⛔ **不含** `fr`/`ws`
//     （它们是**派生边**，per `ADR-CEP-22` 决策 4 ⇒ 语料直接给出 ⇒ 拒收 `derived-edge-as-input`）。
//   · 本模型**不判定**任何公理 ⛔ 判定在 `tlm/cep/compound_graph_falsifier.hh`。
//   · 本模型**不承诺**域内一致性（值面 / SWMR）—— per `ADR-CEP-22` §4.2 缺口①：
//     域内一致性是**各域自身的验证义务**，不由复合公理检查。
//   · 本模型**不承诺**「迹中的域标识忠实代表 Ordering Point 作用域」—— per `ADR-CEP-22`
//     §4.2 缺口②：该假设由**迹作者**承担（域不进 schema ⇒ 配置面无机器判据）。
//
// 作者: CppTLM Team / 日期: 2026-11-24
#ifndef CPPTLM_CEP_MEMORY_MODEL_TRACE_HH
#define CPPTLM_CEP_MEMORY_MODEL_TRACE_HH

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace tlm::cep::mm {

// ─────────────────────────────────────────────────────────────────────────────
// 操作
// ─────────────────────────────────────────────────────────────────────────────

/// 操作类别（读 / 写）—— 决定程序序边的类别（RR / RW / WR / WW）。
enum class OpKind { Read, Write };

/// 一个内存操作（**顶点** = 操作；⛔ 不是 `(instance, network, vc)` —— 那是通道依赖图）。
struct Op {
    std::string id;      ///< 操作标识（**唯一**；重复 ⇒ `duplicate-op-id`）
    OpKind kind = OpKind::Read;
    std::string thread;  ///< 所属线程（线程内 op 序列的先后 ⇒ 程序序）
    std::string domain;  ///< 域标识（**必填**；缺 ⇒ `unbound-domain`）
    std::uint64_t addr = 0;
    std::uint64_t value = 0;  ///< 写值（读忽略）
};

/// 域声明（**迹侧**）—— 域标识 → 绑定的协议实例 `id`。
/// ⚠️ 域标识**可以**多于实例（同一实例可承载多个域，per `ADR-CEP-22` 决策 1）。
struct DomainDecl {
    std::string id;       ///< 域标识
    std::string instance; ///< 绑定的协议实例 `id`（⛔ 必须在声明面可解析）
};

/// **同地址**写序候选（一个地址上的写的全序）。
struct AddrCoCandidate {
    std::string id;                  ///< 候选标识（如 `co1`）
    std::vector<std::string> writes; ///< 该地址上的写的全序（按 id）
};

/// 一个地址上的候选集合（⛔ 空 = 该地址**无写序约束** ⇒ 唯一「空」候选）。
struct AddrCoChoices {
    std::uint64_t addr = 0;
    std::vector<AddrCoCandidate> candidates;
};

/// 操作级轨迹（**输入面**）。
struct Trace {
    std::string id;
    std::string family;        ///< 迹族（⛔ 非空）
    std::string source_litmus;  ///< 具名 litmus 来源（⛔ 非空）
    std::string anchor_id;      ///< 锚点 id（⛔ 非空且 ∈ `anchor_allowlist()`）
    std::vector<DomainDecl> domains;
    std::vector<Op> ops;
    std::vector<std::pair<std::string, std::string>> rf;  ///< (读 id, 写 id) —— 读自映射
    std::vector<AddrCoChoices> co_choices;                  ///< 逐地址写序候选
};

// ─────────────────────────────────────────────────────────────────────────────
// 一致性标签（`ADR-CEP-12` 决策 3 的词表）
// ─────────────────────────────────────────────────────────────────────────────

/// 一致性标签。⛔ `TSO` 与 `Unknown` 在 v1 **未登记**（per `ADR-CEP-22` 决策 3）
/// ⇒ 解析到它们的域 ⇒ 判定 `undecided` + `model-semantics-unavailable`
/// （⛔ **不得**静默当 SC 处理 ⇒ 假绿）。
enum class Label { SC, TSO, RcAcquire, RcRelease, RcData, Plo, Unknown };

/// 标签文本 → 枚举（⛔ 未在词表内 ⇒ `Unknown`）。
inline Label label_from_string(const std::string& s) {
    if (s == "SC") return Label::SC;
    if (s == "TSO") return Label::TSO;
    if (s == "RC-acquire") return Label::RcAcquire;
    if (s == "RC-release") return Label::RcRelease;
    if (s == "RC-data") return Label::RcData;
    if (s == "PLO") return Label::Plo;
    return Label::Unknown;
}

/// 枚举 → 标签文本（判据面用；⛔ `Unknown` ⇒ `"<unknown>"`）。
inline const char* label_name(Label l) {
    switch (l) {
        case Label::SC: return "SC";
        case Label::TSO: return "TSO";
        case Label::RcAcquire: return "RC-acquire";
        case Label::RcRelease: return "RC-release";
        case Label::RcData: return "RC-data";
        case Label::Plo: return "PLO";
        default: return "<unknown>";
    }
}

/// 实例声明面的一格（= `protocol_instances.json` 的某实例的 `consistency_labels`）。
struct InstanceLabels {
    Label read = Label::Unknown;
    Label write = Label::Unknown;
};

/// 声明面（实例 `id` → 标签二元组）。
using InstanceLabelMap = std::map<std::string, InstanceLabels>;

/// 从 `protocol_instances.json` 的**内容**抽取声明面。
///
/// ⛔ **只读** `instances[]`（⛔ 不校验 schema · 不校验 `status`）⇒ 纯查表。
/// ⚠️ 实例**不存在**于声明面 ⇒ 调用方以 `no-domain-instance` 拒收（per `ADR-CEP-22` 决策 1）。
inline InstanceLabelMap load_instance_labels(const nlohmann::json& instances_json) {
    InstanceLabelMap out;
    if (!instances_json.is_object()) return out;
    const auto it = instances_json.find("instances");
    if (it == instances_json.end() || !it->is_array()) return out;
    for (const auto& inst : *it) {
        if (!inst.is_object()) continue;
        const auto id_it = inst.find("id");
        const auto cl_it = inst.find("consistency_labels");
        if (id_it == inst.end() || !id_it->is_string()) continue;
        if (cl_it == inst.end() || !cl_it->is_object()) continue;
        InstanceLabels lbl;
        lbl.read = label_from_string(cl_it->value("read", std::string()));
        lbl.write = label_from_string(cl_it->value("write", std::string()));
        out[std::string(id_it->get<std::string>())] = lbl;
    }
    return out;
}

/// 锚点 allowlist（AC-5 的机检面）—— ⛔ **空锚点**或**不在表内** ⇒ 拒收。
inline const std::set<std::string>& anchor_allowlist() {
    static const std::set<std::string> kAllowlist = {
        "ADR-CEP-13-decision-2",  // 复合一致性公理（公理本体）
        "ADR-CEP-13-decision-1",  // 一致性域定义（域边界）
        "ADR-CEP-22-decision-3",  // `ppo_i` 授权表（域模型来源）
        "ADR-CEP-22-decision-4",  // 域间边构造规则
        "hetergen-vb-acyclic",    // 复合一致性公理的一手锚点（转引，仓内不可复核）
    };
    return kAllowlist;
}

// ─────────────────────────────────────────────────────────────────────────────
// 程序序边的授权（`ADR-CEP-22` 决策 3 的机器面）
// ─────────────────────────────────────────────────────────────────────────────

/// 程序序边的类别（源 → 目标 的读写组合）。
enum class PoEdgeKind { RR, RW, WR, WW };

inline PoEdgeKind po_edge_kind(OpKind src, OpKind dst) {
    if (src == OpKind::Read && dst == OpKind::Read) return PoEdgeKind::RR;
    if (src == OpKind::Read && dst == OpKind::Write) return PoEdgeKind::RW;
    if (src == OpKind::Write && dst == OpKind::Read) return PoEdgeKind::WR;
    return PoEdgeKind::WW;
}

/// **读侧**标签授权（决策 3 表的读侧列）。
///
/// · `SC` ⇒ **全部**程序序边（定义级事实 ⇒ 仓内可复核）
/// · `RC-acquire` ⇒ {源是读} = RR ∪ RW（acquire 之后的操作被其定序）
/// · `RC-data` ⇒ **∅**（relaxed 访问不保留任何程序序）
/// · `PLO` ⇒ **∅**（PLO 不强制 R→R）
/// · `TSO` / `Unknown` ⇒ **false**（v1 未登记 ⇒ 整域 `undecided`，⛔ 不得当 SC）
inline bool allow_read(Label l, PoEdgeKind k) {
    switch (l) {
        case Label::SC: return true;
        case Label::RcAcquire: return k == PoEdgeKind::RR || k == PoEdgeKind::RW;
        case Label::RcData: return false;
        case Label::Plo: return false;
        default: return false;
    }
}

/// **写侧**标签授权（决策 3 表的写侧列）。
///
/// · `SC` ⇒ **全部**
/// · `RC-release` ⇒ {目标是写} = RW ∪ WW（release 之前的操作被其定序）
/// · `PLO` ⇒ {目标是写} = RW ∪ WW（**只强制 W→W 与 R→W 序**，per `protocol-engine.md:104`）
/// · `RC-data` ⇒ **∅**
/// · `TSO` / `Unknown` ⇒ **false**
inline bool allow_write(Label l, PoEdgeKind k) {
    switch (l) {
        case Label::SC: return true;
        case Label::RcRelease: return k == PoEdgeKind::RW || k == PoEdgeKind::WW;
        case Label::Plo: return k == PoEdgeKind::RW || k == PoEdgeKind::WW;
        case Label::RcData: return false;
        default: return false;
    }
}

/// 域的 `ppo_i` 保留一条程序序边？= **读侧授权 ∪ 写侧授权**（决策 3 的组合规则：并集）。
inline bool domain_preserves_po(const InstanceLabels& lbl, PoEdgeKind k) {
    return allow_read(lbl.read, k) || allow_write(lbl.write, k);
}

// ─────────────────────────────────────────────────────────────────────────────
// 解析
// ─────────────────────────────────────────────────────────────────────────────

/// 解析失败（原因码取自 `compound_graph_falsifier.hh` 的闭集）。
struct ParseError {
    std::string code;    ///< 原因码文本
    std::string detail;  ///< 人读说明
};

/// 解析一份操作迹（JSON → `Trace`）。
///
/// ⛔ **拒收**（`ParseError`）的形态（per `ADR-CEP-22` 决策 4/5 + AC-3①/AC-10）：
/// · `derived-edge-as-input` —— 迹里出现**显式** `fr` / `ws` 字段（⛔ 派生边不得作输入）
/// · `channel-edge-not-op-edge` —— 迹里出现**通道**边字段（⛔ 通道图 ≠ 操作图 ⇒ 混图 = 假绿）
/// · `malformed-trace` —— 解析失败 / 缺 `family` / 缺 `source_litmus` / 缺 `anchor_id`
/// · `duplicate-op-id` —— 操作 id 重复
/// · `rf-target-missing` —— `rf` 指向不存在的操作 / 读写类别不符
/// · `rf-self-loop` —— `rf` 的读与写是同一操作
/// · `self-ppo-edge` —— 同一线程的 op 序列里同一 op 出现两次（⇒ 自反程序序边）
/// · `unbound-domain` —— 线程未声明域 / 域标识不在域声明表
/// · `no-domain-instance` —— 域声明的实例 id 不在声明面
inline bool parse_trace(const nlohmann::json& j, Trace& out, ParseError& err) {
    err = ParseError{};
    if (!j.is_object()) {
        err = {"malformed-trace", "迹不是 JSON 对象"};
        return false;
    }
    out.id = j.value("id", std::string());
    out.family = j.value("family", std::string());
    out.source_litmus = j.value("source_litmus", std::string());
    out.anchor_id = j.value("anchor_id", std::string());

    // ⓪ 派生边 / 通道边作为输入 ⇒ 拒收（⛔ 必须在建图之前）
    for (const char* forbidden : {"fr", "ws", "fr_edges", "ws_edges", "derived_edges"}) {
        if (j.contains(forbidden)) {
            err = {"derived-edge-as-input",
                   std::string("迹含显式派生边字段 `") + forbidden + "`（`fr`/`ws` 是派生边 ⇒ 不得作输入）"};
            return false;
        }
    }
    for (const char* forbidden : {"compound_edges", "from_net", "to_net", "from_vc", "to_vc"}) {
        if (j.contains(forbidden)) {
            err = {"channel-edge-not-op-edge",
                   std::string("迹含通道边字段 `") + forbidden + "`（通道依赖图 ≠ 操作图 ⇒ 混图 = 假绿）"};
            return false;
        }
    }

    if (out.family.empty() || out.source_litmus.empty() || out.anchor_id.empty()) {
        err = {"malformed-trace", "`family` / `source_litmus` / `anchor_id` 必须非空"};
        return false;
    }
    if (!anchor_allowlist().count(out.anchor_id)) {
        err = {"malformed-trace", "`anchor_id` 不在 allowlist 内: " + out.anchor_id};
        return false;
    }

    // ① 域声明
    const auto dom_it = j.find("domains");
    if (dom_it == j.end() || !dom_it->is_array()) {
        err = {"malformed-trace", "缺 `domains` 数组"};
        return false;
    }
    for (const auto& d : *dom_it) {
        if (!d.is_object()) {
            err = {"malformed-trace", "`domains` 元素不是对象"};
            return false;
        }
        DomainDecl dd;
        dd.id = d.value("id", std::string());
        dd.instance = d.value("instance", std::string());
        if (dd.id.empty() || dd.instance.empty()) {
            err = {"malformed-trace", "域声明缺 `id` 或 `instance`"};
            return false;
        }
        out.domains.push_back(std::move(dd));
    }

    // ② 线程（程序序 + 域绑定）
    const auto th_it = j.find("threads");
    if (th_it == j.end() || !th_it->is_array()) {
        err = {"malformed-trace", "缺 `threads` 数组"};
        return false;
    }
    struct ThreadSpec {
        std::string id;
        std::string domain;
        std::vector<std::string> ops;
    };
    std::vector<ThreadSpec> threads;
    for (const auto& t : *th_it) {
        if (!t.is_object()) {
            err = {"malformed-trace", "`threads` 元素不是对象"};
            return false;
        }
        ThreadSpec ts;
        ts.id = t.value("id", std::string());
        ts.domain = t.value("domain", std::string());
        if (ts.domain.empty()) {
            err = {"unbound-domain", "线程 `" + ts.id + "` 未声明域"};
            return false;
        }
        const auto ops_it = t.find("ops");
        if (ops_it == t.end() || !ops_it->is_array()) {
            err = {"malformed-trace", "线程 `" + ts.id + "` 缺 `ops` 数组"};
            return false;
        }
        for (const auto& o : *ops_it) {
            if (!o.is_string()) {
                err = {"malformed-trace", "线程 `" + ts.id + "` 的 `ops` 元素不是字符串"};
                return false;
            }
            ts.ops.push_back(o.get<std::string>());
        }
        threads.push_back(std::move(ts));
    }

    // ③ 操作
    const auto ops_it = j.find("ops");
    if (ops_it == j.end() || !ops_it->is_array()) {
        err = {"malformed-trace", "缺 `ops` 数组"};
        return false;
    }
    for (const auto& o : *ops_it) {
        if (!o.is_object()) {
            err = {"malformed-trace", "`ops` 元素不是对象"};
            return false;
        }
        Op op;
        op.id = o.value("id", std::string());
        const std::string kind = o.value("kind", std::string());
        if (op.id.empty() || (kind != "read" && kind != "write")) {
            err = {"malformed-trace", "操作缺 `id` 或 `kind` 非法"};
            return false;
        }
        op.kind = (kind == "read") ? OpKind::Read : OpKind::Write;
        op.addr = o.value("addr", std::uint64_t{0});
        op.value = o.value("value", std::uint64_t{0});
        out.ops.push_back(std::move(op));
    }

    // ④ 操作 id 唯一
    {
        std::set<std::string> ids;
        for (const auto& op : out.ops) {
            if (!ids.insert(op.id).second) {
                err = {"duplicate-op-id", "操作 id 重复: " + op.id};
                return false;
            }
        }
    }

    // ⑤ 读自映射
    const auto rf_it = j.find("rf");
    if (rf_it != j.end()) {
        if (!rf_it->is_array()) {
            err = {"malformed-trace", "`rf` 不是数组"};
            return false;
        }
        for (const auto& e : *rf_it) {
            if (!e.is_object()) {
                err = {"malformed-trace", "`rf` 元素不是对象"};
                return false;
            }
            const std::string r = e.value("read", std::string());
            const std::string w = e.value("write", std::string());
            if (r.empty() || w.empty()) {
                err = {"malformed-trace", "`rf` 元素缺 `read` / `write`"};
                return false;
            }
            if (r == w) {
                err = {"rf-self-loop", "`rf` 的读与写是同一操作: " + r};
                return false;
            }
            out.rf.emplace_back(r, w);
        }
    }

    // ⑥ 逐地址写序候选
    const auto co_it = j.find("co_candidates");
    if (co_it != j.end()) {
        if (!co_it->is_object()) {
            err = {"malformed-trace", "`co_candidates` 不是对象（键 = 地址）"};
            return false;
        }
        for (const auto& [addr_s, cands] : co_it->items()) {
            AddrCoChoices ch;
            try {
                ch.addr = std::stoull(addr_s);
            } catch (...) {
                err = {"malformed-trace", "`co_candidates` 的键不是地址: " + addr_s};
                return false;
            }
            if (!cands.is_array()) {
                err = {"malformed-trace", "`co_candidates` 的值不是数组"};
                return false;
            }
            for (const auto& c : cands) {
                if (!c.is_object()) {
                    err = {"malformed-trace", "`co_candidates` 候选不是对象"};
                    return false;
                }
                AddrCoCandidate ac;
                ac.id = c.value("id", std::string());
                const auto order_it = c.find("order");
                if (ac.id.empty() || order_it == c.end() || !order_it->is_array()) {
                    err = {"malformed-trace", "写序候选缺 `id` / `order`"};
                    return false;
                }
                for (const auto& w : *order_it) {
                    if (!w.is_string()) {
                        err = {"malformed-trace", "写序候选的 `order` 元素不是字符串"};
                        return false;
                    }
                    ac.writes.push_back(w.get<std::string>());
                }
                ch.candidates.push_back(std::move(ac));
            }
            out.co_choices.push_back(std::move(ch));
        }
    }

    // ⑦ 线程 → 操作：绑定域 + 程序序（⛔ 自反程序序边 ⇒ 拒收）
    {
        std::map<std::string, std::size_t> op_index;
        for (std::size_t i = 0; i < out.ops.size(); ++i) op_index[out.ops[i].id] = i;
        for (const auto& ts : threads) {
            bool domain_declared = false;
            for (const auto& d : out.domains)
                if (d.id == ts.domain) { domain_declared = true; break; }
            if (!domain_declared) {
                err = {"unbound-domain", "线程 `" + ts.id + "` 的域 `" + ts.domain + "` 不在域声明表内"};
                return false;
            }
            std::set<std::string> seen_in_thread;
            for (const auto& op_id : ts.ops) {
                const auto it = op_index.find(op_id);
                if (it == op_index.end()) {
                    err = {"malformed-trace", "线程 `" + ts.id + "` 引用不存在的操作: " + op_id};
                    return false;
                }
                if (!seen_in_thread.insert(op_id).second) {
                    err = {"self-ppo-edge", "线程 `" + ts.id + "` 的操作序列里 `" + op_id + "` 出现两次"};
                    return false;
                }
                out.ops[it->second].thread = ts.id;
                out.ops[it->second].domain = ts.domain;
            }
        }
    }

    // ⑧ 读自映射的端点存在且类别相符
    for (const auto& [r, w] : out.rf) {
        const auto rit = std::find_if(out.ops.begin(), out.ops.end(),
                                      [&](const Op& o) { return o.id == r; });
        const auto wit = std::find_if(out.ops.begin(), out.ops.end(),
                                      [&](const Op& o) { return o.id == w; });
        if (rit == out.ops.end() || wit == out.ops.end()) {
            err = {"rf-target-missing", "`rf` 指向不存在的操作: " + r + " ← " + w};
            return false;
        }
        if (rit->kind != OpKind::Read || wit->kind != OpKind::Write) {
            err = {"rf-target-missing", "`rf` 的端点类别不符（读端须为 read · 写端须为 write）"};
            return false;
        }
    }

    return true;
}

}  // namespace tlm::cep::mm

#endif  // CPPTLM_CEP_MEMORY_MODEL_TRACE_HH
