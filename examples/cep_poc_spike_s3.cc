// examples/cep_poc_spike_s3.cc
//
// CEP PoC Spike 场景 S3 —— 死锁自由(通道依赖图在 per-network 顶点下无环)
//
// 功能/作者/日期:实测执行 S3 判定场景 / CppTLM build agent / 2026-10-24
// 权威规格: docs/implementation/2026-10-22-cep-poc-spike-spec.md §3.3
//
// 硬边界(per 规格 §1.2,不可违反):
//   1. 单文件可丢弃 spike —— 不进 ModuleFactory / 不注册 ABI / 无公共 API
//      (守门先例: test/test_cep_dummy_signals.cc「替身不得入 include/tlm/cep/」)
//   2. 只验证「可组合性 + 契约自洽」,不验证正确性,不是 conformance test
//   3. 零冻结产物改动 —— 只**读** fabric.json 与 protocol_instance.schema.json
//
// 语义依据(全部引已裁定文本;批 2 待裁定单元格一律避开):
//   · 逐网络 VC 分配矩阵(权威): ArchForge 32 号 §6.0.2(:365-371)+ moesi_gpu 实例
//     声明(:434-442)—— 逐字镜像,**非 spike 自造**:
//       compute_noc  : request 0 / response 1 / snoop 2 / snoop_resp 3
//       ualink_fabric: request 0 / atomic 1 / query 2 / response 3 / snoop_resp 3
//   · 逐网络通道依赖边表(权威): ArchForge 32 号 §6.0.3(:383-384)
//       compute_noc  : VC0→VC1, VC0→VC2, VC0→VC3, VC1→VC3, VC3→VC2(无环)
//       ualink_fabric: VC0→VC3, VC1→VC3, VC2→VC3(无环)
//   · 顶点定义(取代后文本): docs/adr/ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md
//     §8.2 —— 顶点 = 每个 **(协议实例, network, vc_id)** 三元组;来源 = 逐网络的
//     `vc_binding` + `ordering_assumption`
//   · 放行禁令: 同上 :221 —— 「在图顶点按 `(协议实例, network, vc_id)` **建立**之前,
//     不得用本 ADR 的无环判据放行任何协议实例」→ 本 spike 的结论**不得**用作放行依据
//   · 合并边的双向风险: 同上 :223-224 —— 「合并边既可能制造**假环**,也可能掩盖真环」
//     ⇒ verdict 必须**逐 (network, vc) 标注**,不得只给全局结论
//   · 网络命名空间与槽位上界: include/tlm/cep/fabric.json(运行时**读**;
//     该文件自陈「只承载网络命名空间与槽位上界,不承载消息类 → vc_id 的分配表」)
//
// 观测纪律(承 S1/S2 先例):
//   ① 顶点集与边集**恰好**等于机器可导出的集合(8 顶点 / 8 边 / 跨网边 0)
//   ② 「降级后报环」的成因 SHALL 标注为「**网络别名造成的假环**」——
//      权威边表跨网边 = 0 ⇒ 该环**不是**「存在真实的跨网依赖」
//   ③ verdict **逐 (network, vc)** 标注,不得只给全局结论
//
// 红绿分离(per design.md D7-a):
//   exit 0 = 判据**工作正常**(无环结论本身可能是「有环」;结论由 stdout 报告)
//   exit 1 = **判据退化**(降级后不报环 / 注入环检不出)或其他内部失败
//   exit 2 = 读不到冻结文件(CWD 不符)
//
// 运行: cmake 构建后 ./build/bin/examples/cep_poc_spike_s3(**必须在仓库根运行**)
// ═══════════════════════════════════════════

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

    // ────────────────────────────────────────────
    // ① 权威数据(逐字镜像;来源行号见文件头)
    // ────────────────────────────────────────────
    constexpr const char* kInstance = "moesi_gpu"; // 协议实例标识(32 号 :434-442)

    struct VcBinding {
        struct Net {
            std::map<std::string, int> cls_to_vc;
        };
        std::map<std::string, Net> nets;
        static VcBinding authority() {
            VcBinding b;
            b.nets["compute_noc"].cls_to_vc = {
                {"request", 0}, {"response", 1}, {"snoop", 2}, {"snoop_resp", 3}};
            b.nets["ualink_fabric"].cls_to_vc = {
                {"request", 0}, {"atomic", 1}, {"query", 2}, {"response", 3}, {"snoop_resp", 3}};
            return b;
        }
    };

    // 逐网络通道依赖边(32 号 :383-384)
    struct NetEdgeTable {
        std::string net;
        std::vector<std::pair<int, int>> edges;
    };
    const std::vector<NetEdgeTable> kNetEdges{
        {"compute_noc", {{0, 1}, {0, 2}, {0, 3}, {1, 3}, {3, 2}}},
        {"ualink_fabric", {{0, 3}, {1, 3}, {2, 3}}},
    };

    // ────────────────────────────────────────────
    // ② 图与环检测
    // ────────────────────────────────────────────
    using Graph = std::map<std::string, std::vector<std::string>>;

    std::string up_vertex(const std::string& net, int vc) {
        return std::string(kInstance) + "|" + net + "|" + std::to_string(vc);
    }
    std::string flat_vertex(int vc) {
        return std::string(kInstance) + "|" + std::to_string(vc);
    }

    // 环检测(DFS 三色):返回找到的**第一个**环的顶点序列;无环则空
    std::vector<std::string> find_cycle(const Graph& g) {
        std::map<std::string, int> color; // 0=white 1=gray 2=black
        std::vector<std::string> stack;
        std::vector<std::string> cyc;
        std::function<bool(const std::string&)> dfs = [&](const std::string& v) -> bool {
            color[v] = 1;
            stack.push_back(v);
            auto it = g.find(v);
            if (it != g.end()) {
                for (const auto& w : it->second) {
                    if (color[w] == 1) { // 回边 → 环
                        cyc.clear();
                        bool started = false;
                        for (const auto& s : stack) {
                            if (s == w)
                                started = true;
                            if (started)
                                cyc.push_back(s);
                        }
                        cyc.push_back(w);
                        return true;
                    }
                    if (color[w] == 0 && dfs(w))
                        return true;
                }
            }
            stack.pop_back();
            color[v] = 2;
            return false;
        };
        for (const auto& [v, _] : g) {
            if (color[v] == 0 && dfs(v)) {
                // ⚠️ 必须返回**重建的环**(`cyc`),不是 DFS 路径(`stack`)
                //    —— 路径可能比环长(前面还有无关前缀)。
                return cyc;
            }
        }
        return {};
    }

    // ────────────────────────────────────────────
    // ③ 读冻结文件(失败 → 调用方 exit 2)
    // ────────────────────────────────────────────
    std::string read_file(const char* path) {
        std::ifstream in(path);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    int pass_count = 0;
    int fail_count = 0;
    void expect(bool cond, const std::string& what) {
        std::printf("      %s %s\n", cond ? "✓" : "✗", what.c_str());
        cond ? ++pass_count : ++fail_count;
    }

    // 顶点集(升级):∪_{n ∈ fabric.json.networks} {(实例, n, vc) : vc ∈ vc_binding[n].values()}
    Graph build_upgraded(const std::vector<std::string>& nets) {
        Graph g;
        const VcBinding binding = VcBinding::authority();
        for (const auto& net : nets) {
            std::set<int> vcs;
            for (const auto& [cls, vc] : binding.nets.at(net).cls_to_vc)
                (void)cls;
            for (const auto& [cls, vc] : binding.nets.at(net).cls_to_vc)
                vcs.insert(vc);
            for (int vc : vcs)
                g[up_vertex(net, vc)] = {}; // 顶点显式登记
        }
        return g;
    }

    // 边集(升级):逐网络边表 → 顶点级边
    void add_upgraded_edges(Graph& g) {
        for (const auto& ne : kNetEdges) {
            for (const auto& [from, to] : ne.edges)
                g[up_vertex(ne.net, from)].push_back(up_vertex(ne.net, to));
        }
    }

    // 降级:顶点去掉 network 分量 → 边按 vc 合并
    Graph flatten(const Graph& up) {
        Graph flat;
        for (const auto& [v, outs] : up) {
            const std::size_t p1 = v.rfind('|');
            const std::string vc = v.substr(p1 + 1);
            const std::string fv = flat_vertex(std::stoi(vc));
            flat[fv];
            for (const auto& w : outs) {
                const std::size_t q1 = w.rfind('|');
                flat[fv].push_back(flat_vertex(std::stoi(w.substr(q1 + 1))));
            }
        }
        return flat;
    }

    std::size_t edge_count(const Graph& g) {
        std::size_t n = 0;
        for (const auto& [_, outs] : g)
            n += outs.size();
        return n;
    }

    std::size_t cross_net_edges(const Graph& g) {
        std::size_t n = 0;
        for (const auto& [v, outs] : g) {
            const std::size_t p1 = v.rfind('|');
            const std::string net =
                v.substr(v.rfind('|', p1 - 1) + 1, p1 - v.rfind('|', p1 - 1) - 1);
            for (const auto& w : outs) {
                const std::size_t q1 = w.rfind('|');
                const std::string wnet =
                    w.substr(w.rfind('|', q1 - 1) + 1, q1 - w.rfind('|', q1 - 1) - 1);
                if (wnet != net)
                    ++n;
            }
        }
        return n;
    }

} // namespace

int main() {
    // 0. 读冻结声明(只读;失败即明示退出,不 terminate)
    const auto fabric_text = read_file("include/tlm/cep/fabric.json");
    const auto schema_text = read_file("include/tlm/cep/protocol_instance.schema.json");
    if (fabric_text.empty() || schema_text.empty()) {
        std::fprintf(stderr, "无法读取 include/tlm/cep/{fabric.json,protocol_instance.schema.json}"
                             " —— 必须在仓库根运行(exit 2)\n");
        return 2;
    }
    const nlohmann::json fabric = nlohmann::json::parse(fabric_text);
    const nlohmann::json schema = nlohmann::json::parse(schema_text);
    std::vector<std::string> nets;
    std::map<std::string, int> vc_count;
    for (auto& [net, spec] : fabric.at("networks").items()) {
        nets.push_back(net);
        vc_count[net] = spec.at("vc_count").get<int>();
    }
    std::sort(nets.begin(), nets.end());
    const auto perm_names =
        schema["properties"]["perm"]["items"]["enum"].get<std::vector<std::string>>();

    const VcBinding binding = VcBinding::authority();

    // 声明自洽:fabric.json 的网络清单 == 权威 vc_binding 的网络清单
    expect(nets.size() == binding.nets.size() &&
               std::all_of(nets.begin(), nets.end(),
                           [&](const std::string& n) { return binding.nets.count(n) > 0; }),
           "fabric.json 网络清单与权威 vc_binding 网络清单一致(32 号 :365-371)");
    expect(perm_names.size() == 4, "perm 4 值锁与冻结 schema 一致(读冻结 schema 比对)");

    // ── 顶点集与边集(机器可导出等值)────────────────────────────────────────
    Graph up = build_upgraded(nets);
    add_upgraded_edges(up);
    const std::size_t n_vert = up.size();
    const std::size_t n_edge = edge_count(up);
    const std::size_t n_cross = cross_net_edges(up);
    std::printf("\n[S3][DECL] 顶点=%zu 边=%zu 跨网边=%zu(网络:%zu)\n", n_vert, n_edge, n_cross,
                nets.size());
    expect(n_vert == 8, "顶点集**恰好** 8 个(2 网络 × 4 VC,按 fabric.json + vc_binding 导出)");
    expect(n_edge == 8, "依赖边集**恰好** 8 条(32 号 :383-384 逐网络边表并集)");
    expect(n_cross == 0, "跨网边 = 0(所有边均为**网内**边)");
    // 槽位上界自洽:每个 vc_id < 该网络 vc_count
    {
        bool ok = true;
        for (const auto& net : nets)
            for (const auto& [cls, vc] : binding.nets.at(net).cls_to_vc) {
                (void)cls;
                if (vc < 0 || vc >= vc_count.at(net))
                    ok = false;
            }
        expect(ok, "每个 vc_id < 该网络 vc_count(槽位上界自洽,fabric.json)");
    }

    // ── 正向:升级顶点下无环 ───────────────────────────────────────────────
    const auto up_cycle = find_cycle(up);
    std::printf("\n[S3][MAIN] 升级顶点 (实例, network, vc) → 环检测\n");
    expect(up_cycle.empty(), "升级顶点下依赖图**无环**(ADR-CEP-09 §8.2 顶点升级后判据仍成立)");

    // verdict 逐 (network, vc) 标注(不得只给全局结论)
    std::set<std::string> in_cycle;
    for (const auto& v : up_cycle)
        in_cycle.insert(v);
    for (const auto& net : nets) {
        for (int vc = 0; vc < vc_count.at(net); ++vc) {
            const std::string v = up_vertex(net, vc);
            const bool bad = in_cycle.count(v) > 0;
            std::printf("[S3][VERDICT_VC] %s = %s\n", v.c_str(), bad ? "IN-CYCLE" : "ACYCLIC");
        }
    }
    {
        bool all_acyclic = true;
        for (const auto& net : nets)
            for (int vc = 0; vc < vc_count.at(net); ++vc)
                if (in_cycle.count(up_vertex(net, vc)) > 0)
                    all_acyclic = false;
        expect(all_acyclic, "verdict **逐 (network, vc)** 标注完毕且全部 ACYCLIC(非仅全局结论)");
    }

    // ── 反向:降级顶点后**必须**报环(「有牙」保证)──────────────────────────
    const Graph flat = flatten(up);
    const auto flat_cycle = find_cycle(flat);
    std::printf("\n[S3][REVERSE] 降级顶点 (实例, vc) → 环检测\n");
    if (!flat_cycle.empty()) {
        std::printf("      环: ");
        for (const auto& v : flat_cycle)
            std::printf("%s ", v.c_str());
        std::printf("\n");
    }
    expect(!flat_cycle.empty(), "降级到 (实例, vc_id) 后**必须**报出环(顶点升级是实质需求)");
    // 环必须含 2↔3 这一对(由权威边表推导)
    {
        bool has2 = false, has3 = false;
        for (const auto& v : flat_cycle) {
            if (v == flat_vertex(2))
                has2 = true;
            if (v == flat_vertex(3))
                has3 = true;
        }
        expect(has2 && has3, "降级后的环含 vc2 与 vc3(由 32 号 :383-384 边表推导的 2↔3)");
    }
    // 成因标注:跨网边 = 0 ⇒ 该环是「网络别名造成的假环」,**不是**「真实跨网依赖」
    expect(n_cross == 0, "报环成因 = **网络别名造成的假环**(跨网边 0 ⇒ 网内边被按 vc 合并;"
                         "per ADR-CEP-09:223-224「合并边既可能制造假环,也可能掩盖真环」)");

    // ── 判据敏感度自检(「有牙」)───────────────────────────────────────────
    // ① 反向:降级后不报环 → 检测器恒无环 → 判据退化
    // ② 正向:向升级图**注入**一个环 → 检测器必须检出 → 否则它对环不敏感
    int selfcheck_ok = 1;
    if (flat_cycle.empty()) {
        selfcheck_ok = 0;
        std::printf("[S3][SELFCHECK] 降级后未报环 → 环检测器恒无环 → 判据退化\n");
    }
    {
        Graph injected = up;
        // 注入:compute_noc 的 vc0 → ualink_fabric 的 vc1(制造一条跨网边 + 回边)
        const std::string a = up_vertex("compute_noc", 0);
        const std::string b = up_vertex("ualink_fabric", 1);
        injected[a].push_back(b);
        injected[b].push_back(a);
        if (find_cycle(injected).empty()) {
            selfcheck_ok = 0;
            std::printf("[S3][SELFCHECK] 注入环后未检出 → 环检测器对环不敏感 → 判据退化\n");
        }
    }
    std::printf("[S3][SELFCHECK] S3_SELFCHECK=%s\n", selfcheck_ok ? "OK" : "DEGENERATE");
    expect(selfcheck_ok == 1, "自检:降级必报环 + 注入环必检出(检测器非恒真)");

    // ── 结论报告(红绿分离:结论不影响 exit code)────────────────────────────
    const bool s3_acyclic = up_cycle.empty();
    std::printf("\n[S3][VERDICT] %s\n", s3_acyclic ? "ACYCLIC" : "CYCLIC");
    std::printf("[S3][VERDICT] S3_VERDICT=%s\n", s3_acyclic ? "ACYCLIC" : "CYCLIC");
    std::printf("[S3][VERDICT] S3_VERTICES=%zu\n", n_vert);
    std::printf("[S3][VERDICT] S3_EDGES=%zu\n", n_edge);
    std::printf("[S3][VERDICT] S3_CROSSNET_EDGES=%zu\n", n_cross);
    std::printf("[S3][VERDICT] S3_DOWNGRADED_CYCLE=%s\n",
                flat_cycle.empty() ? "NONE" : "vc2<->vc3");
    std::printf("[S3][VERDICT] S3_CAUSE=alias-false-cycle(网络别名造成的假环;"
                "跨网边 0 ⇒ 非「真实跨网依赖」)\n");
    std::printf("[S3][VERDICT] S3_REVERSE_VALIDITY=until-3rd-network-appears"
                "(第 3 张网后扁平化可能**掩盖真环** → 该反向断言须重新设计)\n");
    std::printf("[S3][VERDICT] S3_RELEASE_USE=forbidden(ADR-CEP-09:221 放行禁令;"
                "spike 结论不得用作协议实例放行依据)\n");

    std::printf("\n[S3][SUMMARY] pass=%d fail=%d\n", pass_count, fail_count);
    if (fail_count > 0) {
        std::printf("[S3][SUMMARY] 判据自身失效(非判定结论)→ exit 1\n");
        return 1;
    }
    std::printf("[S3][SUMMARY] 判据工作正常(判定结论见 S3_VERDICT)→ exit 0\n");
    return 0;
}
