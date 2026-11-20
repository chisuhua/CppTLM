// include/tlm/cep/litmus_falsifier.hh
// 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的**证伪工装**
//
// 依据（**全部是既有已签批内容**，本文件不新增任何语义）:
//   · `ADR-CEP-13` 决策 5 ② —— 「正确性由**随机 / litmus 实验在声明的 `verification_scope` 内
//     尝试证伪**;**平台不提供无违例保证**」⇒ 本工装**只尝试证伪**,⛔ **不得**输出「已证明」。
//   · `ADR-CEP-13` 决策 2 —— 复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`,
//     其中 `ppo_com(t) ≡ ppo_i`(t 属于域 i)。
//   · `openspec/specs/cep-core-contracts/spec.md` 的「复合一致性判据(IC1)」Scenario ——
//     域的定义 = **一个 Ordering Point 生效 + 一套自己的 `ppo` 生效**。
//
// ⚠️ **诚实边界（本工装的第一结论,不是缺陷）**:
//   `rfe` / `fr` / `ws` 三类边在 **CppTLM 与 ArchForge 两仓均无定义**
//   （实测:两仓 grep `rfe`/`fr`/`ws` 仅命中公理公式本身）⇒ 复合公理所需边类
//   **当前没有声明面** ⇒ ⇒ **复合公理当前「不可证伪」**。
//   本工装据此给出 `reachable == false` 的**真结果**,⛔ **不得**被表述为「公理已成立」。
//   待结构化声明项落地（本仓 I-6 的 schema 13→14）⇒ 语料非空 ⇒ 判定翻转 ⇒ 工装「有牙」。
//
// 作者: CppTLM Team / 日期: 2026-11-21
#ifndef CPPTLM_CEP_LITMUS_FALSIFIER_HH
#define CPPTLM_CEP_LITMUS_FALSIFIER_HH

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/channel_dependency_graph.hh"

namespace tlm::cep {

/// 一次 litmus 证伪尝试的判定（⛔ **无「已证明」一档** —— 见文件头）
struct LitmusVerdict {
    /// 复合公理**当前是否可达**（= 是否有跨域边声明面）
    bool reachable = false;
    /// 不可达时的**具体原因**（可达时为空）
    std::string unreachable_reason;
    /// 参与尝试的语料边数（= 跨域边条数）
    std::size_t compound_edges = 0;
    /// 语料里出现过的边类（⛔ 仅**记录**,⛔ 不解释语义 —— 类定义当前不存在）
    std::vector<std::string> classes_seen;
    /// 找到的环（每个元素 = 一条环的顶点序列文本）
    std::vector<std::string> cycles;
    /// 是否**证伪成功**（= 找到环 ⇒ 复合公理在该语料下被推翻）
    bool falsified = false;
    /// ⚠️ 覆盖说明（必须随判定一起读）—— 恒不为空
    std::string coverage_note;
};

/// 复合图构造 + 证伪尝试。
///
/// **图** = 既有逐网络通道依赖边（`decl.per_network_edges`）+ 语料里的**跨域边**（`corpus`）。
/// **顶点** = (instance, network, vc) —— 与既有装配期闸 `channel_dependency_graph.hh` 同形。
///
/// ⚠️ 本函数**只尝试证伪**:无环 ⇒ 「在本次语料范围内**未证伪**」,⛔ 不是「已证明」。
inline LitmusVerdict evaluate_litmus_falsification(const ChannelDeclaration& decl,
                                                     const nlohmann::json& corpus) {
    LitmusVerdict v;
    v.coverage_note =
        "复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的**边类**(`rfe`/`fr`/`ws`)在两仓"
        "**均无定义** ⇒ 本工装只对**已声明的跨域边**尝试证伪;⛔ 无环**不等于**公理成立"
        "(per `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」)";

    // ① 语料里的跨域边（⛔ `class` 必须非空 —— 空类 = 未声明语义 ⇒ 拒收）
    struct Extra {
        std::string instance;
        std::string from_net;
        unsigned from_vc = 0;
        std::string to_net;
        unsigned to_vc = 0;
    };
    std::vector<Extra> extras;
    std::set<std::string> classes;
    const nlohmann::json* edges = nullptr;
    if (corpus.is_object()) {
        const auto it = corpus.find("compound_edges");
        if (it != corpus.end() && it->is_array())
            edges = &(*it);
    }
    if (edges != nullptr) {
        for (const auto& e : *edges) {
            const std::string cls = e.value("class", std::string());
            if (cls.empty())
                continue; // ⛔ 空类 = 未声明语义 ⇒ 跳过（并由 reachable 反映语料不足）
            classes.insert(cls);
            Extra x;
            x.instance = e.value("instance", std::string());
            x.from_net = e.value("from_net", std::string());
            x.to_net = e.value("to_net", std::string());
            x.from_vc = e.value("from_vc", 0u);
            x.to_vc = e.value("to_vc", 0u);
            extras.push_back(std::move(x));
        }
    }
    v.compound_edges = extras.size();
    v.classes_seen.assign(classes.begin(), classes.end());

    if (extras.empty()) {
        v.reachable = false;
        v.unreachable_reason =
            "语料 `compound_edges` 为空 ⇒ 复合公理所需的跨域边**无声明面** ⇒ 复合公理"
            "**当前不可证伪**(⛔ 不得表述为「已成立」)";
        return v;
    }
    v.reachable = true;

    // ② 建图:顶点(逐实例 × 逐网络 × 已绑定 vc)+ 既有逐网络边 + 语料跨域边
    ChannelDependencyGraph g;
    for (const auto& inst : decl.instances) {
        for (const auto& [net, classes_map] : inst.vc_binding) {
            std::set<unsigned> vcs;
            for (const auto& [cls, vc] : classes_map) {
                if (vc >= 0 && static_cast<unsigned>(vc) <= kMaxVcId)
                    vcs.insert(static_cast<unsigned>(vc));
            }
            for (const unsigned vc : vcs)
                g.add_vertex(Vertex{inst.id, net, vc});
        }
    }
    for (const auto& inst : decl.instances) {
        for (const auto& [net, net_edges] : decl.per_network_edges) {
            if (inst.vc_binding.count(net) == 0)
                continue;
            for (const auto& [from, to] : net_edges) {
                const Vertex a{inst.id, net, from};
                const Vertex b{inst.id, net, to};
                if (g.has_vertex(a) && g.has_vertex(b))
                    g.add_edge(a, b);
            }
        }
    }
    for (const auto& x : extras) {
        const Vertex a{x.instance, x.from_net, x.from_vc};
        const Vertex b{x.instance, x.to_net, x.to_vc};
        if (!g.has_vertex(a) || !g.has_vertex(b))
            continue; // 端点未绑定 ⇒ 语料与声明不自洽 ⇒ 该边不参与（由语料校验闸兜底）
        g.add_edge(a, b);
    }

    // ③ 证伪尝试:找环
    const auto cycle = g.find_cycle();
    if (!cycle.empty()) {
        std::string text;
        for (std::size_t i = 0; i < cycle.size(); ++i) {
            if (i != 0)
                text += " -> ";
            text += vertex_text(cycle[i]);
        }
        v.cycles.push_back(std::move(text));
        v.falsified = true;
    }
    return v;
}

/// 读语料文件（⛔ 缺文件 = **不可达**,⛔ 不是「通过」）
inline nlohmann::json load_litmus_corpus(const std::string& path, bool* readable) {
    if (readable != nullptr)
        *readable = false;
    std::ifstream in(path);
    if (!in.good())
        return nlohmann::json::object();
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded())
        return nlohmann::json::object();
    if (readable != nullptr)
        *readable = true;
    return j;
}

} // namespace tlm::cep

#endif // CPPTLM_CEP_LITMUS_FALSIFIER_HH
