// include/tlm/cep/channel_dependency_graph.hh
// 装配期通道依赖图 + 无环判据(ADR-CEP-09 Decision A / INV-CEP-09-IC1)
//
// 功能:把「顶点 = (协议实例, network, vc_id) 三元组」的通道依赖图与无环判据落成可执行契约,
//       并输出**逐 (network, vc)** 的 verdict(不是一句全局结论)。
// 作者: CppTLM Team / 日期: 2026-10-25
//
// ── 顶点为什么升级到三元组(per `ADR-CEP-09 §8`)─────────────────────────
//   VC 分配是 **per-network** 的(`ADR-CEP-12` 决策 6):同一 vc_id 在两张网里承载不同
//   消息类。若顶点只到 `(实例, vc_id)`,两张网的边会被合并且**制造假环**
//   (S3 spike 的 reverse 段实测:降级后必报环)。
//
// ── 边从哪来(实施期校准,per 台账 Entry #62 + ArchForge 32 号 §6.0.3)────
//   边**不是**本文件硬编码的常量,也不是单靠 `vc_binding` 可推导的规则:
//   ArchForge 给出了**逐网络边表**(compute_noc 5 条 + ualink_fabric 3 条 = 8 条,
//   并附 VC-R1 / VC-R2 两条支撑规则)。该表作为**声明数据**随
//   `include/tlm/cep/protocol_instances.json` 的 `channel_dependency_edges` 落地,
//   本文件只做「读声明 → 建图 → 判环」。⇒ 声明改则统计改,无镜像可漂。
//
// ── 三态(与 PASS 可区分)────────────────────────────────────────────────
//   SKIP = 没有任何协议实例声明(本仓 53 个 configs 中绝大多数无 `cep` 段)
//   PASS = 有声明且无环且端点全部自洽
//   FAIL = 有环 / 边端点引用了未绑定的 vc / vc 越界
//   `SKIP` **不得**被当作 `PASS`(空声明不是「检查通过」,是「没有可检查的对象」)。
#ifndef CPPTLM_CEP_CHANNEL_DEPENDENCY_GRAPH_HH
#define CPPTLM_CEP_CHANNEL_DEPENDENCY_GRAPH_HH

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace tlm::cep {

// ─────────────────────────────────────────────────────────────────────
// 三态判定
// ─────────────────────────────────────────────────────────────────────
enum class GateVerdict : std::uint8_t { SKIP = 0, PASS = 1, FAIL = 2 };

inline const char* gate_verdict_name(GateVerdict v) {
    switch (v) {
        case GateVerdict::SKIP: return "SKIP";
        case GateVerdict::PASS: return "PASS";
        case GateVerdict::FAIL: return "FAIL";
    }
    return "UNKNOWN";
}

// ─────────────────────────────────────────────────────────────────────
// 顶点 = (协议实例, network, vc_id) 三元组
// ─────────────────────────────────────────────────────────────────────
inline constexpr unsigned kMaxVcId = 3;  // per fabric.schema.json vc_count.maximum

struct Vertex {
    std::string instance;
    std::string network;
    unsigned vc = 0;

    bool operator<(const Vertex& o) const {
        if (instance != o.instance)
            return instance < o.instance;
        if (network != o.network)
            return network < o.network;
        return vc < o.vc;
    }
};

inline std::string vertex_key(const Vertex& v) {
    std::ostringstream os;
    os << v.instance << '|' << v.network << '|' << v.vc;
    return os.str();
}

inline std::string vertex_text(const Vertex& v) {
    std::ostringstream os;
    os << '(' << v.instance << ", " << v.network << ", vc=" << v.vc << ')';
    return os.str();
}

struct GraphStats {
    std::size_t vertices = 0;
    std::size_t edges = 0;
    std::size_t cross_net_edges = 0;

    bool operator==(const GraphStats& o) const {
        return vertices == o.vertices && edges == o.edges && cross_net_edges == o.cross_net_edges;
    }
};

// ─────────────────────────────────────────────────────────────────────
// 图 + DFS 三色环检测(算法迁自 examples/cep_poc_spike_s3.cc,不复制常量)
// ─────────────────────────────────────────────────────────────────────
class ChannelDependencyGraph {
public:
    void add_vertex(const Vertex& v) {
        if (index_.count(vertex_key(v)) != 0)
            return;
        index_[vertex_key(v)] = vertices_.size();
        vertices_.push_back(v);
        adj_.push_back({});
    }

    bool has_vertex(const Vertex& v) const { return index_.count(vertex_key(v)) != 0; }

    /// 端点必须已在图中(调用方负责先判 has_vertex 并报错)
    void add_edge(const Vertex& from, const Vertex& to) {
        const auto a = index_.find(vertex_key(from));
        const auto b = index_.find(vertex_key(to));
        if (a == index_.end() || b == index_.end())
            return;
        adj_[a->second].push_back(b->second);
    }

    std::size_t vertex_count() const { return vertices_.size(); }

    std::size_t edge_count() const {
        std::size_t n = 0;
        for (const auto& outs : adj_)
            n += outs.size();
        return n;
    }

    std::size_t cross_network_edge_count() const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < adj_.size(); ++i) {
            for (const auto j : adj_[i]) {
                if (vertices_[i].network != vertices_[j].network)
                    ++n;
            }
        }
        return n;
    }

    GraphStats stats() const {
        GraphStats s;
        s.vertices = vertex_count();
        s.edges = edge_count();
        s.cross_net_edges = cross_network_edge_count();
        return s;
    }

    const std::vector<Vertex>& vertices() const { return vertices_; }

    std::vector<Vertex> successors(const Vertex& v) const {
        std::vector<Vertex> out;
        const auto it = index_.find(vertex_key(v));
        if (it == index_.end())
            return out;
        for (const auto j : adj_[it->second])
            out.push_back(vertices_[j]);
        return out;
    }

    /// 返回找到的**第一个**环的顶点序列(空 = 无环)
    std::vector<Vertex> find_cycle() const {
        const std::size_t n = vertices_.size();
        std::vector<int> color(n, 0);  // 0=white 1=gray 2=black
        std::vector<std::size_t> stack;
        std::vector<std::size_t> cycle;

        // 迭代式 DFS(避免深图递归爆栈;语义与 spike 的递归版一致)
        for (std::size_t start = 0; start < n; ++start) {
            if (color[start] != 0)
                continue;

            std::vector<std::pair<std::size_t, std::size_t>> frames;  // (vertex, next child idx)
            frames.emplace_back(start, 0);
            color[start] = 1;
            stack.push_back(start);

            while (!frames.empty()) {
                const std::size_t v = frames.back().first;
                std::size_t& next = frames.back().second;

                if (next >= adj_[v].size()) {
                    color[v] = 2;
                    frames.pop_back();
                    stack.pop_back();
                    continue;
                }

                const std::size_t w = adj_[v][next++];
                if (color[w] == 1) {
                    // 回边 → 从栈中截取环片段(栈上 w 之后的所有顶点 + w)
                    cycle.clear();
                    bool started = false;
                    for (const auto s : stack) {
                        if (s == w)
                            started = true;
                        if (started)
                            cycle.push_back(s);
                    }
                    cycle.push_back(w);

                    std::vector<Vertex> out;
                    out.reserve(cycle.size());
                    for (const auto idx : cycle)
                        out.push_back(vertices_[idx]);
                    return out;
                }
                if (color[w] == 0) {
                    color[w] = 1;
                    stack.push_back(w);
                    frames.emplace_back(w, 0);
                }
            }
        }
        return {};
    }

private:
    std::vector<Vertex> vertices_;
    std::map<std::string, std::size_t> index_;
    std::vector<std::vector<std::size_t>> adj_;
};

// ─────────────────────────────────────────────────────────────────────
// 声明模型(与冻结 schema 的字段对齐的**最小投影**)
// ─────────────────────────────────────────────────────────────────────
struct DeclaredInstance {
    std::string id;
    std::string status;        // planned / implemented / excluded
    std::string sharer_impl;   // FullMapDirectory / … / 不参与
    std::string completeness;  // EXACT / CONSERVATIVE(PARTIAL 仅合成声明可造)
    /// 决策 2 的广播兜底声明 —— **不是**冻结 schema 的字段(合法声明恒为 false);
    /// 仅供合成声明驱动 J2 的 PARTIAL 分支。
    bool broadcast_fallback = false;
    bool hierarchical = false;       // combination_capabilities.hierarchical
    bool exposes_unique_perm = false;  // perm 含 UNIQUE(⇒ 可静默升级到 M)
    std::map<std::string, std::map<std::string, int>> vc_binding;      // net → class → vc
    std::map<std::string, std::map<unsigned, std::string>> ordering;   // net → vc → assumption
};

struct ChannelDeclaration {
    std::string derived_from;
    std::string edges_derived_from;
    std::vector<DeclaredInstance> instances;
    std::map<std::string, std::vector<std::pair<unsigned, unsigned>>> per_network_edges;

    /// 声明源**配置错误** —— 与「无可检查对象」严格区分:
    ///   · `source_error == true` ⇒ 配置 fail(显式指定的源不可读 / JSON 畸形 /
    ///     类型非法 / 显式给了一份解析出 0 实例的声明)。这些是**作者写错了**,不是外部条件。
    ///   · `source_error == false && instances.empty()` ⇒ SKIP(默认源缺失/为空 —— 外部条件)。
    /// 二者混用同一 SKIP 出口,等于「一个会降级的闸门」,违反 pytest 侧
    /// `cep_decl_alignment_check.sh` 已写明的自家原则。
    bool source_error = false;
    std::string source_error_detail;

    /// SKIP 时的诊断提示(如「缺省源按 CWD 解析」)—— 只影响日志,不影响判据。
    std::string skip_hint;

    bool empty() const { return instances.empty(); }
};

namespace detail {

inline std::string json_string_or(const nlohmann::json& j, const char* key) {
    if (j.is_object() && j.contains(key) && j[key].is_string())
        return j[key].get<std::string>();
    return {};
}

}  // namespace detail

inline DeclaredInstance parse_declared_instance(const nlohmann::json& j) {
    DeclaredInstance d;
    if (!j.is_object())
        return d;
    d.id = detail::json_string_or(j, "id");
    d.status = detail::json_string_or(j, "status");
    d.broadcast_fallback = j.value("broadcast_fallback", false);

    if (j.contains("vc_binding") && j["vc_binding"].is_object()) {
        for (auto net = j["vc_binding"].begin(); net != j["vc_binding"].end(); ++net) {
            if (!net.value().is_object())
                continue;
            for (auto cls = net.value().begin(); cls != net.value().end(); ++cls) {
                if (cls.value().is_number_integer())
                    d.vc_binding[net.key()][cls.key()] = cls.value().get<int>();
            }
        }
    }

    if (j.contains("ordering_assumption") && j["ordering_assumption"].is_object()) {
        for (auto net = j["ordering_assumption"].begin(); net != j["ordering_assumption"].end();
             ++net) {
            if (!net.value().is_object())
                continue;
            for (auto vc = net.value().begin(); vc != net.value().end(); ++vc) {
                if (!vc.value().is_string())
                    continue;
                try {
                    const unsigned id = static_cast<unsigned>(std::stoul(vc.key()));
                    d.ordering[net.key()][id] = vc.value().get<std::string>();
                } catch (const std::exception&) {
                    // 非数字键:忽略(严格校验归 cep_decl_alignment_check.sh)
                }
            }
        }
    }

    if (j.contains("sharer_tracker") && j["sharer_tracker"].is_object()) {
        d.sharer_impl = detail::json_string_or(j["sharer_tracker"], "impl");
        if (j["sharer_tracker"].contains("caps") && j["sharer_tracker"]["caps"].is_object())
            d.completeness = detail::json_string_or(j["sharer_tracker"]["caps"], "completeness");
    }

    if (j.contains("combination_capabilities") && j["combination_capabilities"].is_object())
        d.hierarchical = j["combination_capabilities"].value("hierarchical", false);

    if (j.contains("perm") && j["perm"].is_array()) {
        for (const auto& p : j["perm"]) {
            if (p.is_string() && p.get<std::string>() == "UNIQUE")
                d.exposes_unique_perm = true;
        }
    }
    return d;
}

inline ChannelDeclaration parse_channel_declaration(const nlohmann::json& doc) {
    ChannelDeclaration d;
    if (!doc.is_object())
        return d;

    if (doc.contains("derived_from")) {
        if (doc["derived_from"].is_object())
            d.derived_from = detail::json_string_or(doc["derived_from"], "source_doc");
        else if (doc["derived_from"].is_string())
            d.derived_from = doc["derived_from"].get<std::string>();
    }

    if (doc.contains("instances") && doc["instances"].is_array()) {
        // 数组里混入非对象项 / 解析不出非空 `id` 的项 ⇒ **数据损坏**,不是「没有实例」。
        // 旧实现无条件 push_back(可能是一个空 DeclaredInstance)⇒ `instances.empty()` 为 false
        // ⇒ 既不走 SKIP 也不走 source_error ⇒ **双闸 PASS 但 0 顶点 0 边**(比 SKIP 更糟的绿灯)。
        for (std::size_t i = 0; i < doc["instances"].size(); ++i) {
            const auto& j = doc["instances"][i];
            if (!j.is_object()) {
                d.source_error = true;
                d.source_error_detail = "instances[" + std::to_string(i) + "] 不是对象(实际为 " +
                                        std::string(j.type_name()) + ')';
                continue;
            }
            DeclaredInstance inst = parse_declared_instance(j);
            if (inst.id.empty()) {
                d.source_error = true;
                d.source_error_detail = "instances[" + std::to_string(i) + "] 缺少非空的 `id`";
                continue;
            }
            d.instances.push_back(std::move(inst));
        }
    }

    if (doc.contains("channel_dependency_edges") && doc["channel_dependency_edges"].is_object()) {
        const auto& e = doc["channel_dependency_edges"];
        d.edges_derived_from = detail::json_string_or(e, "derived_from");
        if (e.contains("per_network") && e["per_network"].is_object()) {
            for (auto net = e["per_network"].begin(); net != e["per_network"].end(); ++net) {
                if (!net.value().is_array())
                    continue;
                for (const auto& pair : net.value()) {
                    if (!pair.is_array() || pair.size() != 2)
                        continue;
                    if (!pair[0].is_number_integer() || !pair[1].is_number_integer())
                        continue;
                    d.per_network_edges[net.key()].emplace_back(
                        static_cast<unsigned>(pair[0].get<int>()),
                        static_cast<unsigned>(pair[1].get<int>()));
                }
            }
        }
    }
    return d;
}

inline ChannelDeclaration load_channel_declaration(const std::string& path,
                                                  bool explicit_source = true) {
    ChannelDeclaration d;
    std::ifstream in(path);
    if (!in) {
        // 显式指定却读不到 ⇒ 作者给的路径错了(配置 fail);默认源读不到 ⇒ 外部条件(SKIP)
        if (explicit_source) {
            d.source_error = true;
            d.source_error_detail = "显式指定的声明源不可读: " + path;
        }
        return d;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    try {
        d = parse_channel_declaration(nlohmann::json::parse(ss.str()));
    } catch (const std::exception& exc) {
        ChannelDeclaration bad;
        if (explicit_source) {
            bad.source_error = true;
            bad.source_error_detail = std::string("声明源 JSON 畸形: ") + path + " (" + exc.what() +
                                      ')';
        }
        return bad;
    }
    if (explicit_source && d.instances.empty() && !d.source_error) {
        d.source_error = true;
        d.source_error_detail = "显式指定的声明源解析出 0 个协议实例: " + path;
    }
    return d;
}

/// 声明输入源的缺省路径(相对仓库根;与 CLI 共用同一常量,避免两处各写一份)
inline constexpr const char* kDefaultDeclarationPath = "include/tlm/cep/protocol_instances.json";

/// 从拓扑 config 取声明(`config["cep"]["protocol_instances"]`:字符串 = 路径 / 对象 = 内联 / 数组 = 实例表)
///
/// ⚠️ **有 `cep` 段但未指定声明源** ⇒ 落到本仓默认声明(`kDefaultDeclarationPath`),
/// 而不是静默 SKIP —— 否则「写了 cep 段却忘写声明源」会变成一条无牙的通过路径。
/// 默认源不可读时仍然 SKIP(跨工作目录是外部条件,不阻断装配)。
///
/// ⚠️ 但**显式指定**的源不可读 / JSON 畸形 / 类型非法 / 解析出 0 实例 ⇒ `source_error`
/// ⇒ 判据 FAIL。这四类都是作者写错了,不是外部条件 —— 若一律 SKIP,声明源写错一个字符
/// 就等于闸不存在(且日志只说「no declared protocol instance」,无法定位)。
inline ChannelDeclaration parse_channel_declaration_from_config(const nlohmann::json& config) {
    if (!config.is_object() || !config.contains("cep"))
        return {};
    const auto& cep = config["cep"];
    ChannelDeclaration d;
    if (!cep.is_object()) {
        d.source_error = true;
        d.source_error_detail = "config 的 `cep` 段必须是对象(实际为 " +
                                std::string(cep.type_name()) + ')';
        return d;
    }
    if (!cep.contains("protocol_instances")) {
        ChannelDeclaration d =
            load_channel_declaration(kDefaultDeclarationPath, /*explicit_source=*/false);
        if (d.instances.empty()) {
            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            d.skip_hint = std::string("缺省声明源按**当前工作目录**解析为 '") +
                          kDefaultDeclarationPath + "'(cwd=" +
                          (ec ? std::string("<unavailable>") : cwd.string()) +
                          ") —— 从仓库根以外运行时需改用显式路径(否则 SKIP,闸不生效)";
        }
        return d;
    }
    const auto& pi = cep["protocol_instances"];
    if (pi.is_string())
        return load_channel_declaration(pi.get<std::string>(), /*explicit_source=*/true);
    if (pi.is_object()) {
        d = parse_channel_declaration(pi);
        if (d.instances.empty() && !d.source_error) {
            d.source_error = true;
            d.source_error_detail = "内联声明的 `instances` 为空(显式给了声明却解析出 0 个实例)";
        }
        return d;
    }
    if (pi.is_array()) {
        d = parse_channel_declaration(nlohmann::json{{"instances", pi}});
        if (d.instances.empty() && !d.source_error) {
            d.source_error = true;
            d.source_error_detail = "内联实例数组为空(显式给了声明却解析出 0 个实例)";
        }
        return d;
    }
    d.source_error = true;
    d.source_error_detail =
        "cep.protocol_instances 必须是 string(路径)/ object(内联声明)/ array(实例表),实际为 " +
        std::string(pi.type_name());
    return d;
}

// ─────────────────────────────────────────────────────────────────────
// 判据
// ─────────────────────────────────────────────────────────────────────
struct VerdictLine {
    std::string instance;
    std::string network;
    unsigned vc = 0;
    GateVerdict verdict = GateVerdict::PASS;
    std::string detail;
};

struct GateReport {
    GateVerdict verdict = GateVerdict::SKIP;
    GraphStats stats{};  // 全域合计
    std::map<std::string, GraphStats> per_instance;  // 逐实例(环只在实例内部,统计也分开给)
    std::vector<VerdictLine> verdicts;
    std::vector<Vertex> cycle;
    std::vector<std::string> errors;
    std::string skip_reason;
    std::string log;

    const std::string& to_log() const { return log; }
};

inline GateReport evaluate_channel_dependency_gate(const ChannelDeclaration& decl) {
    GateReport r;

    // 配置错误优先于「无可检查对象」—— 声明源写错不是 SKIP 的理由
    if (decl.source_error) {
        r.verdict = GateVerdict::FAIL;
        r.errors.push_back(decl.source_error_detail);
        r.log = "[FAIL] cep channel dependency gate: 声明源配置错误 —— " +
                decl.source_error_detail + '\n';
        return r;
    }

    if (decl.instances.empty()) {
        r.verdict = GateVerdict::SKIP;
        r.skip_reason = "no declared protocol instance";
        r.log = "[SKIP] cep channel dependency gate: " + r.skip_reason + '\n';
        if (!decl.skip_hint.empty())
            r.log += "  (" + decl.skip_hint + ")\n";
        return r;
    }

    // 图**按实例分别建** —— 边不跨实例,环也只可能落在单个实例内部;
    // 分开建图才能给出逐实例统计(如「moesi_gpu 8-8-0」)而不被实例数掩盖。
    for (const auto& inst : decl.instances) {
        ChannelDependencyGraph g;

        // ① 顶点:逐网络 × 该网络被绑定的 vc(边不造顶点)
        for (const auto& [net, classes] : inst.vc_binding) {
            std::set<unsigned> vcs;
            for (const auto& [cls, vc] : classes) {
                if (vc < 0 || static_cast<unsigned>(vc) > kMaxVcId) {
                    std::ostringstream os;
                    os << "实例 " << inst.id << " 的 " << net << '.' << cls << " 绑定 vc=" << vc
                       << " 越界(允许 0.." << kMaxVcId << ')';
                    r.errors.push_back(os.str());
                    continue;
                }
                vcs.insert(static_cast<unsigned>(vc));
            }
            for (const unsigned vc : vcs)
                g.add_vertex(Vertex{inst.id, net, vc});
        }

        // ② 边:由**声明边表**导出;端点必须已被绑定(否则声明自相矛盾 ⇒ FAIL)
        for (const auto& [net, edges] : decl.per_network_edges) {
            if (inst.vc_binding.count(net) == 0)
                continue;  // 该实例不参与这张网
            for (const auto& [from, to] : edges) {
                const Vertex a{inst.id, net, from};
                const Vertex b{inst.id, net, to};
                if (!g.has_vertex(a)) {
                    r.errors.push_back("边端点未绑定:" + vertex_text(a));
                    continue;
                }
                if (!g.has_vertex(b)) {
                    r.errors.push_back("边端点未绑定:" + vertex_text(b));
                    continue;
                }
                g.add_edge(a, b);
            }
        }

        const GraphStats st = g.stats();
        r.per_instance[inst.id] = st;
        r.stats.vertices += st.vertices;
        r.stats.edges += st.edges;
        r.stats.cross_net_edges += st.cross_net_edges;

        const std::vector<Vertex> cycle = g.find_cycle();
        std::set<std::string> in_cycle;
        for (const auto& v : cycle)
            in_cycle.insert(vertex_key(v));
        if (!cycle.empty() && r.cycle.empty())
            r.cycle = cycle;

        for (const auto& v : g.vertices()) {
            VerdictLine line;
            line.instance = v.instance;
            line.network = v.network;
            line.vc = v.vc;
            if (in_cycle.count(vertex_key(v)) != 0) {
                line.verdict = GateVerdict::FAIL;
                line.detail = "in cycle";
            } else {
                line.verdict = GateVerdict::PASS;
                line.detail = "acyclic";
            }
            r.verdicts.push_back(line);
        }
    }

    if (!r.cycle.empty()) {
        std::ostringstream os;
        os << "存在环:";
        for (std::size_t i = 0; i < r.cycle.size(); ++i) {
            if (i != 0)
                os << " -> ";
            os << vertex_text(r.cycle[i]);
        }
        r.errors.push_back(os.str());
    }

    r.verdict = r.errors.empty() ? GateVerdict::PASS : GateVerdict::FAIL;

    std::ostringstream os;
    os << '[' << gate_verdict_name(r.verdict) << "] cep channel dependency gate: "
       << r.stats.vertices << " vertices / " << r.stats.edges << " edges / "
       << r.stats.cross_net_edges << " cross-network edges";
    if (!decl.derived_from.empty())
        os << " (derived_from: " << decl.derived_from << ')';
    os << '\n';
    for (const auto& [id, st] : r.per_instance) {
        os << "[per-instance] instance=" << id << " vertices=" << st.vertices
           << " edges=" << st.edges << " cross_network_edges=" << st.cross_net_edges << '\n';
    }
    for (const auto& e : r.errors)
        os << "[error] " << e << '\n';
    for (const auto& v : r.verdicts) {
        os << "[verdict] net=" << v.network << " vc=" << v.vc << ' '
           << gate_verdict_name(v.verdict) << " (instance=" << v.instance << ", " << v.detail
           << ")\n";
    }
    r.log = os.str();
    return r;
}

/// module_factory 挂点入口(无 `cep` 段 ⇒ SKIP)
inline GateReport run_channel_gate(const nlohmann::json& config) {
    return evaluate_channel_dependency_gate(parse_channel_declaration_from_config(config));
}

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_CHANNEL_DEPENDENCY_GRAPH_HH
