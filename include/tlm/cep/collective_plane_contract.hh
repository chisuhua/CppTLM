// include/tlm/cep/collective_plane_contract.hh
// Fabric Collective Plane 的**跨平面契约判据** —— 把 ADR 的「说辞」变成「机器可判」
//
// 功能:5 条可机器检查的契约(零调用边 / canonical 10 流零 diff / collective VC 预留不冲突 /
//       区域互斥(同源)/ 抓取点可测性登记)+ 独立声明源的加载与校验 + 三态 SKIP/PASS/FAIL。
// 作者: CppTLM Team / 日期: 2026-10-26
//
// ── 定位(per delta capability `cep-collective-plane-contract`)────────
// 本文件是**判据层**,与行为面(`collective_plane.hh`)**分离**但**共享**同一判据实现 ——
// 区域互斥尤其:判据与 `LocalCollectivePlane` 走**同一条**代码路径(禁双实现,per 队列 #4 纪律)。
//
// ── 红绿(per 项目铁律)────────────────────────────────────────────────
// 「判据实现退化」= 用例红;「判定结论为红」= 用例**仍绿**且结论作为交付物登记。
//
// ── 三态(per 队列 #4 `cep-channel-dependency-gate` 先例)──────────────
//   · PASS = 契约成立
//   · FAIL = 契约**被违反**(**含**「配置错误」:声明源缺失/畸形 ⇒ FAIL,而非 SKIP)
//   · SKIP = **无可检查对象**(如未提供声明源路径 ⇒ 不检查)—— 与 FAIL 严格区分
//
// ── 禁写恒真 / 恒 SKIP 判据(per design Non-Goals)─────────────────────
// 本 Stage 物理不可测的条款(跨平面 ordering A/B/E、fabric 组播投递单实现)SHALL **不**进本文件。
//
// ── 声明源(诞生即不冻结)──────────────────────────────────────────────
// `include/tlm/cep/collective_planes.json` + 配套 schema。

#ifndef INCLUDE_TLM_CEP_COLLECTIVE_PLANE_CONTRACT_HH_
#define INCLUDE_TLM_CEP_COLLECTIVE_PLANE_CONTRACT_HH_

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/collective_plane.hh"

namespace tlm::cep {

// ─────────────────────────────────────────────────────────────────────
// 判据三态 + verdict 行(格式与队列 #4 同源:`[verdict] <rule-id> PASS|FAIL|SKIP (detail)`)
// ─────────────────────────────────────────────────────────────────────
enum class ContractVerdict : std::uint8_t {
    PASS = 0,
    FAIL = 1,
    SKIP = 2,
};

inline const char* contract_verdict_name(ContractVerdict v) {
    switch (v) {
        case ContractVerdict::PASS:
            return "PASS";
        case ContractVerdict::FAIL:
            return "FAIL";
        case ContractVerdict::SKIP:
            return "SKIP";
    }
    return "?";
}

/// 判据规则编号(与 `ADR-CEP-01 §3.3` 抓取点一一对应)
enum class ContractRule : std::uint8_t {
    /// 哨兵:**无覆盖**(该抓取点不由任何判据覆盖 —— 推迟 / 已满足 / 行为面覆盖)
    /// ⚠️ 不能拿某条真实规则兼作哨兵 —— 那样「未绑定」与「绑定到该规则」无法区分。
    None = 0,
    /// 抓取点 1 —— 零调用边(include-graph 闸)
    ZeroCallEdge = 1,
    /// 抓取点 4 —— canonical 10 流零 diff
    CanonicalFlows = 2,
    /// 抓取点 3 —— collective VC 预留与 coherence VC 绑定不冲突(声明层)
    VcReservation = 3,
    /// CEP-13 —— 区域互斥(与行为面同源)
    RegionExclusion = 4,
    /// 完成域判别 —— `group_id` 等 group-scoped 字段不得出现在 coherence 侧
    CompletionDomain = 5,
    /// 抓取点可测性登记(逐条核对 ADR-CEP-01 的 Status Update 表格)
    CatchpointCoverage = 6,
};

inline constexpr std::size_t kContractRuleCount = 6;

inline const char* contract_rule_id(ContractRule r) {
    switch (r) {
        case ContractRule::None:
            return "??";
        case ContractRule::ZeroCallEdge:
            return "CP-01";
        case ContractRule::CanonicalFlows:
            return "CP-02";
        case ContractRule::VcReservation:
            return "CP-03";
        case ContractRule::RegionExclusion:
            return "CP-04";
        case ContractRule::CompletionDomain:
            return "CP-05";
        case ContractRule::CatchpointCoverage:
            return "CP-06";
    }
    return "??";
}

struct ContractVerdictLine {
    ContractRule rule = ContractRule::ZeroCallEdge;
    ContractVerdict verdict = ContractVerdict::SKIP;
    std::string detail;
};

/// 契约报告
struct ContractReport {
    ContractVerdict verdict = ContractVerdict::SKIP;
    std::vector<ContractVerdictLine> lines;
    std::vector<std::string> errors;
    std::string skip_reason;
    std::string log;

    const std::string& to_log() const { return log; }

    std::size_t count(ContractVerdict v) const {
        std::size_t n = 0;
        for (const auto& l : lines)
            if (l.verdict == v)
                ++n;
        return n;
    }
    bool has(ContractRule r) const {
        for (const auto& l : lines)
            if (l.rule == r)
                return true;
        return false;
    }
    ContractVerdict verdict_of(ContractRule r) const {
        for (const auto& l : lines)
            if (l.rule == r)
                return l.verdict;
        return ContractVerdict::SKIP;
    }
};

// ─────────────────────────────────────────────────────────────────────
// 声明源(集体平面声明)
// ─────────────────────────────────────────────────────────────────────
struct CollectivePlaneDeclaration {
    /// 协议实例 → 绑定的 plane(空串 / 未列 = 不启用)
    std::map<std::string, std::string> plane_binding;
    /// 已注册 region:range → (gid, mode)
    std::vector<std::pair<AddrRange, std::pair<group_id_t, AccessMode>>> registered_regions;
    /// collective VC 预留:network → vc_id
    std::vector<std::pair<std::string, unsigned>> vc_reservation;

    /// **配置错误**(声明源缺失 / 畸形 / 类型非法)—— 与「无可检查对象 ⇒ SKIP」严格区分
    bool source_error = false;
    std::string source_error_detail;

    bool empty() const {
        return plane_binding.empty() && registered_regions.empty() && vc_reservation.empty();
    }
};

inline AccessMode parse_access_mode(const std::string& s, bool& ok) {
    ok = true;
    if (s == "normal")
        return AccessMode::Normal;
    if (s == "nc_drop_cacheable")
        return AccessMode::NcDropCacheable;
    ok = false;
    return AccessMode::Normal;
}

inline CollectivePlaneDeclaration parse_collective_plane_declaration(const nlohmann::json& doc) {
    CollectivePlaneDeclaration d;
    if (!doc.is_object()) {
        d.source_error = true;
        d.source_error_detail = "声明源根不是对象";
        return d;
    }
    if (doc.contains("plane_binding")) {
        if (!doc["plane_binding"].is_object()) {
            d.source_error = true;
            d.source_error_detail = "plane_binding 不是对象";
        } else {
            for (auto it = doc["plane_binding"].begin(); it != doc["plane_binding"].end(); ++it) {
                if (it.value().is_null()) {
                    // 显式 null = 该实例**不启用** collective —— 必须**保留该键**
                    // (跳过会让 map 为空,与「未声明此实例」无法区分)
                    d.plane_binding[it.key()] = {};
                    continue;
                }
                if (!it.value().is_string()) {
                    d.source_error = true;
                    d.source_error_detail = "plane_binding[" + it.key() + "] 既非字符串也非 null";
                    continue;
                }
                d.plane_binding[it.key()] = it.value().get<std::string>();
            }
        }
    }
    if (doc.contains("registered_regions")) {
        if (!doc["registered_regions"].is_array()) {
            d.source_error = true;
            d.source_error_detail = "registered_regions 不是数组";
        } else {
            for (const auto& r : doc["registered_regions"]) {
                if (!r.is_object() || !r.contains("lo") || !r.contains("hi") ||
                    !r.contains("group") || !r.contains("mode")) {
                    d.source_error = true;
                    d.source_error_detail = "registered_regions 某项缺 lo/hi/group/mode";
                    continue;
                }
                AddrRange range;
                range.lo = r["lo"].get<std::uint64_t>();
                range.hi = r["hi"].get<std::uint64_t>();
                if (!range.valid()) {
                    d.source_error = true;
                    d.source_error_detail = "registered_regions 某项区间非法(lo > hi)";
                    continue;
                }
                bool ok = false;
                const AccessMode mode = parse_access_mode(r["mode"].get<std::string>(), ok);
                if (!ok) {
                    d.source_error = true;
                    d.source_error_detail = "registered_regions 某项 mode 非法";
                    continue;
                }
                d.registered_regions.emplace_back(
                    range, std::make_pair(r["group"].get<group_id_t>(), mode));
            }
        }
    }
    if (doc.contains("collective_vc_reservation")) {
        if (!doc["collective_vc_reservation"].is_array()) {
            d.source_error = true;
            d.source_error_detail = "collective_vc_reservation 不是数组";
        } else {
            for (const auto& v : doc["collective_vc_reservation"]) {
                if (!v.is_object() || !v.contains("network") || !v.contains("vc_id")) {
                    d.source_error = true;
                    d.source_error_detail = "collective_vc_reservation 某项缺 network/vc_id";
                    continue;
                }
                d.vc_reservation.emplace_back(v["network"].get<std::string>(),
                                             v["vc_id"].get<unsigned>());
            }
        }
    }
    return d;
}

/// 加载声明源文件
inline CollectivePlaneDeclaration load_collective_plane_declaration(const std::string& path) {
    CollectivePlaneDeclaration d;
    std::ifstream in(path);
    if (!in) {
        d.source_error = true;
        d.source_error_detail = "声明源不可读: " + path;
        return d;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    try {
        d = parse_collective_plane_declaration(nlohmann::json::parse(ss.str()));
    } catch (const std::exception& exc) {
        CollectivePlaneDeclaration bad;
        bad.source_error = true;
        bad.source_error_detail = std::string("声明源 JSON 畸形: ") + path + " (" + exc.what() + ')';
        return bad;
    }
    return d;
}

/// 声明源缺省路径(相对仓库根;与 CLI 共用同一常量)
inline constexpr const char* kDefaultCollectiveDeclarationPath =
    "include/tlm/cep/collective_planes.json";

// ─────────────────────────────────────────────────────────────────────
// 判据实现(逐条)
// ─────────────────────────────────────────────────────────────────────

/// **CP-01 零调用边** —— 扫 collective 侧头文件的直接 include,禁 directory / ordering / sharer 侧。
///
/// ⚠️ **两条口径必须同时成立**(per critic BLOCKER 复评):
///   1. **去注释后**再扫 —— 否则 `// #include "tlm/cep/sharer_tracker.hh"` 这类**注释行**误报;
///   2. 按 **include 路径**判定(而非整行子串)—— 否则 `#include <directory>`(C23 标准头)
///      这类**字面命中**也误报。
/// 与行为面的静态扫描**正交**:行为面查「include 指令里出现禁用**关键字**」,
/// 本判据查「include 指令解析出的**路径**是否命中禁用**文件名/目录**」—— 两份实现不同。
inline std::string strip_cpp_comments(const std::string& src) {
    std::string out;
    bool in_line = false, in_block = false;
    for (std::size_t i = 0; i < src.size(); ++i) {
        if (!in_block && !in_line && i + 1 < src.size() && src[i] == '/' && src[i + 1] == '/') {
            in_line = true;
            ++i;
            continue;
        }
        if (!in_block && !in_line && i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
            in_block = true;
            ++i;
            continue;
        }
        if (in_line && src[i] == '\n') {
            in_line = false;
            out.push_back('\n');
            continue;
        }
        if (in_block && i + 1 < src.size() && src[i] == '*' && src[i + 1] == '/') {
            in_block = false;
            ++i;
            continue;
        }
        if (!in_line && !in_block)
            out.push_back(src[i]);
    }
    return out;
}

/// 从源码里抽出全部 `#include` 的**路径**(尖括号 / 引号两种)
inline std::vector<std::string> extract_include_paths(const std::string& src) {
    std::vector<std::string> paths;
    std::istringstream in(src);
    std::string line;
    while (std::getline(in, line)) {
        const auto pos = line.find("#include");
        if (pos == std::string::npos)
            continue;
        const auto open = line.find_first_of("<\"", pos);
        if (open == std::string::npos)
            continue;
        const char close_ch = (line[open] == '<') ? '>' : '"';
        const auto close = line.find(close_ch, open + 1);
        if (close == std::string::npos)
            continue;
        paths.push_back(line.substr(open + 1, close - open - 1));
    }
    return paths;
}

/// 某 include 路径是否指向**禁用侧**(directory / ordering / sharer)
inline bool include_path_is_forbidden(const std::string& path) {
    static const char* kForbidden[] = {"/sharer_tracker", "/address_resolver", "/ordering_point",
                                       "/channel_dependency", "/stub_cache_core", "/directory"};
    for (const char* f : kForbidden) {
        if (path.find(f) != std::string::npos)
            return true;
    }
    return false;
}

/// 供测试用的单文件形态:该头文件(去注释后)是否含**指向禁用侧**的 include
inline bool header_include_is_forbidden(const std::string& header_text) {
    const std::string stripped = strip_cpp_comments(header_text);
    for (const auto& p : extract_include_paths(stripped)) {
        if (include_path_is_forbidden(p))
            return true;
    }
    return false;
}

inline ContractVerdict evaluate_zero_call_edge(const std::vector<std::string>& header_paths,
                                                std::string& detail) {
    for (const auto& hp : header_paths) {
        std::ifstream in(hp);
        if (!in) {
            detail = "collective 侧头文件不可读: " + hp;
            return ContractVerdict::FAIL;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string stripped = strip_cpp_comments(ss.str());
        for (const auto& p : extract_include_paths(stripped)) {
            if (include_path_is_forbidden(p)) {
                detail = "collective 侧头文件 " + hp + " include 了禁用侧的头 `" + p +
                         "` ⇒ 层级纠缠(per ADR-CEP-01 §3.3 抓取点 1)";
                return ContractVerdict::FAIL;
            }
        }
    }
    detail = "collective 侧头文件(" + std::to_string(header_paths.size()) +
             " 个)未 include 任何 directory / ordering / sharer 侧的头";
    return ContractVerdict::PASS;
}

/// **CP-02 canonical 10 流零 diff** —— 解析 `ADR-CEP-02 §2` 的流清单表。
/// canonical 流名**不得**出现在 collective 侧(collective 是**旁路**,不扩事务集)。
inline std::set<std::string> parse_canonical_flow_names(const std::string& adr_text) {
    std::set<std::string> names;
    std::istringstream in(adr_text);
    std::string line;
    while (std::getline(in, line)) {
        const auto s = line.find('|');
        if (s == std::string::npos)
            continue;
        // 表格行形如: | N | `flow_name` | ... |
        const auto b1 = line.find('|', s + 1);
        if (b1 == std::string::npos)
            continue;
        const auto b2 = line.find('|', b1 + 1);
        if (b2 == std::string::npos)
            continue;
        std::string num = line.substr(s + 1, b1 - s - 1);
        std::string name = line.substr(b1 + 1, b2 - b1 - 1);
        // 去空格
        const auto trim = [](std::string& v) {
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t'))
                v.erase(v.begin());
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r'))
                v.pop_back();
        };
        trim(num);
        trim(name);
        if (num.empty() || name.empty())
            continue;
        bool all_digit = true;
        for (char c : num)
            if (c < '0' || c > '9')
                all_digit = false;
        if (!all_digit)
            continue;
        if (name.size() >= 2 && name.front() == '`' && name.back() == '`')
            name = name.substr(1, name.size() - 2);
        names.insert(name);
    }
    return names;
}

inline ContractVerdict evaluate_canonical_flows(const std::string& adr_path,
                                                const std::string& collective_header_path,
                                                std::string& detail) {
    std::ifstream adr(adr_path);
    if (!adr) {
        detail = "canonical 流权威源不可读: " + adr_path;
        return ContractVerdict::FAIL;
    }
    std::ostringstream as;
    as << adr.rdbuf();
    const std::set<std::string> flows = parse_canonical_flow_names(as.str());
    if (flows.size() != 10) {
        detail = "canonical 流清单解析出 " + std::to_string(flows.size()) + " 条(期望 10)—— 权威源格式已变";
        return ContractVerdict::FAIL;
    }
    std::ifstream hdr(collective_header_path);
    if (!hdr) {
        detail = "collective 头文件不可读: " + collective_header_path;
        return ContractVerdict::FAIL;
    }
    std::ostringstream hs;
    hs << hdr.rdbuf();
    const std::string text = strip_cpp_comments(hs.str());
    for (const auto& f : flows) {
        // collective 侧**不得**出现 canonical 流名作为接口标识符。
        // ⚠️ 匹配面必须覆盖**多种形态**:`name(` / `name (` / `struct name` / `typedef … name` /
        //    变量名 —— 只查 `name(` 会漏检 `struct request {…}` 这类用法(per critic)。
        // ⚠️ 同时必须**词边界**:裸子串会把 `collective_barrier(` 误判成 `barrier(`、
        //    把 `make_completion(` 误判成 `completion(` —— 二者是 collective 平面**自有**标识符。
        std::size_t pos = 0;
        while ((pos = text.find(f, pos)) != std::string::npos) {
            const bool left_ok =
                (pos == 0) || !(std::isalnum(static_cast<unsigned char>(text[pos - 1])) ||
                                 text[pos - 1] == '_');
            const std::size_t after = pos + f.size();
            const bool right_ok =
                (after >= text.size()) || !(std::isalnum(static_cast<unsigned char>(text[after])) ||
                                            text[after] == '_');
            if (left_ok && right_ok) {
                detail = std::string("collective 侧出现 canonical 流名 `") + f +
                         "` ⇒ 旁路平面不得扩 Coherence Interface 事务集";
                return ContractVerdict::FAIL;
            }
            pos = after;
        }
    }
    detail = "canonical 10 流解析成功(" + std::to_string(flows.size()) +
             " 条),collective 侧未出现任何 canonical 流名";
    return ContractVerdict::PASS;
}

/// **CP-03 collective VC 预留** —— 值域 0-3 + `< 该网络 vc_count` + 与既有 coherence
/// `vc_binding` **不冲突**。
///
/// ⚠️ **权威位置**:既有 `vc_binding` 在 `protocol_instances.json` 的 `instances[].vc_binding`
/// (**不是** `fabric.json` —— 后者只有 `networks[].vc_count`)。per planner 产出的实施期校准 T0-4。
struct VcBindingSnapshot {
    /// network → 该网络上已被 coherence 协议占用的 VC 集合
    std::map<std::string, std::set<unsigned>> used;
    /// network → vc_count
    std::map<std::string, unsigned> vc_count;
    bool source_error = false;
    std::string source_error_detail;
};

inline VcBindingSnapshot load_vc_binding_snapshot(const std::string& protocol_instances_path,
                                                   const std::string& fabric_path) {
    VcBindingSnapshot snap;
    std::ifstream fab(fabric_path);
    if (!fab) {
        snap.source_error = true;
        snap.source_error_detail = "fabric 声明源不可读: " + fabric_path;
        return snap;
    }
    try {
        const auto fdoc = nlohmann::json::parse(fab);
        if (fdoc.contains("networks") && fdoc["networks"].is_object()) {
            for (auto it = fdoc["networks"].begin(); it != fdoc["networks"].end(); ++it) {
                if (it.value().is_object() && it.value().contains("vc_count")) {
                    snap.vc_count[it.key()] = it.value()["vc_count"].get<unsigned>();
                }
            }
        }
    } catch (const std::exception& exc) {
        snap.source_error = true;
        snap.source_error_detail = std::string("fabric 声明源 JSON 畸形: ") + exc.what();
        return snap;
    }
    std::ifstream proto(protocol_instances_path);
    if (!proto) {
        snap.source_error = true;
        snap.source_error_detail = "协议实例声明源不可读: " + protocol_instances_path;
        return snap;
    }
    try {
        const auto pdoc = nlohmann::json::parse(proto);
        if (pdoc.contains("instances") && pdoc["instances"].is_array()) {
            for (const auto& inst : pdoc["instances"]) {
                if (!inst.is_object() || !inst.contains("vc_binding"))
                    continue;
                const auto& vb = inst["vc_binding"];
                if (!vb.is_object())
                    continue;
                for (auto net = vb.begin(); net != vb.end(); ++net) {
                    if (!net.value().is_object())
                        continue;
                    for (auto cls = net.value().begin(); cls != net.value().end(); ++cls) {
                        if (cls.value().is_number_integer())
                            snap.used[net.key()].insert(cls.value().get<unsigned>());
                    }
                }
            }
        }
    } catch (const std::exception& exc) {
        snap.source_error = true;
        snap.source_error_detail = std::string("协议实例声明源 JSON 畸形: ") + exc.what();
        return snap;
    }
    return snap;
}

inline ContractVerdict evaluate_vc_reservation(const CollectivePlaneDeclaration& decl,
                                                const VcBindingSnapshot& snap,
                                                std::string& detail) {
    if (decl.vc_reservation.empty()) {
        detail = "未预留任何 collective VC(本仓当前拓扑无 collective 可用隔离 VC,per 抓取点 3)";
        return ContractVerdict::PASS;
    }
    for (const auto& [net, vc] : decl.vc_reservation) {
        if (vc > 3u) {
            detail = net + ".vc" + std::to_string(vc) + " 超出值域 0-3(per noc_bundles_tlm.hh)";
            return ContractVerdict::FAIL;
        }
        auto cit = snap.vc_count.find(net);
        if (cit == snap.vc_count.end()) {
            detail = net + " 未在 fabric 声明源登记(NS-R1:单点权威在 fabric.json)";
            return ContractVerdict::FAIL;
        }
        if (vc >= cit->second) {
            detail = net + ".vc" + std::to_string(vc) + " 越界(该网络 vc_count = " +
                     std::to_string(cit->second) + ")";
            return ContractVerdict::FAIL;
        }
        auto uit = snap.used.find(net);
        if (uit != snap.used.end() && uit->second.count(vc) != 0) {
            detail = net + ".vc" + std::to_string(vc) +
                     " 已被 coherence 协议的 vc_binding 占用 ⇒ 死锁通道隔离被破坏(抓取点 3)";
            return ContractVerdict::FAIL;
        }
    }
    detail = "collective VC 预留(" + std::to_string(decl.vc_reservation.size()) +
             " 条)与 coherence vc_binding 不冲突";
    return ContractVerdict::PASS;
}

/// **CP-04 区域互斥** —— 与行为面**同源**:判据直接调 `LocalCollectivePlane` 的注册路径,
/// 而不是**另写一份**重叠检测(禁双实现,per 队列 #4 纪律)。
inline ContractVerdict evaluate_region_exclusion(const CollectivePlaneDeclaration& decl,
                                                 std::string& detail) {
    LocalCollectivePlane plane;
    std::size_t ok_count = 0;
    // 按**声明的 group 编号**建 group(per critic)—— 每个 gid 一个 group,
    // 同 gid 的多个 region 走**同一个** group ⇒ 与真实场景(group 持有多 region)一致。
    std::map<group_id_t, group_id_t> gid_map;
    for (const auto& [range, gm] : decl.registered_regions) {
        auto git = gid_map.find(gm.first);
        if (git == gid_map.end()) {
            const group_id_t created = plane.group_create({1, 2, 3}, GroupAttrs{});
            git = gid_map.emplace(gm.first, created).first;
        }
        const group_id_t gid = git->second;
        const Status st = plane.group_register_region(gid, range, gm.second);
        if (st == Status::Ok) {
            ++ok_count;
            continue;
        }
        if (st == Status::RegionOverlap) {
            detail = "registered_regions 存在重叠区间 ⇒ 违反区域互斥(CEP-13)";
            return ContractVerdict::FAIL;
        }
        detail = std::string("registered_regions 某项注册失败: ") + status_name(st);
        return ContractVerdict::FAIL;
    }
    detail = "已注册 region " + std::to_string(ok_count) + " 条,无重叠(CEP-13 成立)";
    return ContractVerdict::PASS;
}

/// **CP-05 完成域判别** —— group-scoped 字段**不得**出现在 coherence 侧接口/契约。
inline ContractVerdict evaluate_completion_domain(const std::string& coherence_side_path,
                                                   std::string& detail) {
    std::ifstream in(coherence_side_path);
    if (!in) {
        detail = "coherence 侧契约不可读: " + coherence_side_path;
        return ContractVerdict::FAIL;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    // ⚠️ **去注释**后再查 —— 否则 coherence 侧注释里提及 `group_id` 即假阳性 FAIL
    const std::string text = strip_cpp_comments(ss.str());
    static const char* kGroupScoped[] = {"group_id", "member_set", "CollectiveReq", "CollectiveEvt",
                                         "group_create", "group_register_region",
                                         "collective_barrier"};
    for (const char* g : kGroupScoped) {
        if (text.find(g) != std::string::npos) {
            detail = std::string("coherence 侧出现 group-scoped 标识 `") + g +
                     "` ⇒ 违反完成域判别规则(requester-scoped → Coherence / group-scoped → Collective)";
            return ContractVerdict::FAIL;
        }
    }
    detail = "coherence 侧未出现任何 group-scoped 标识";
    return ContractVerdict::PASS;
}

/// **CP-06 抓取点可测性登记** —— 解析 `ADR-CEP-01` 的 Status Update 表格,
/// 逐条核对 7 个抓取点的覆盖状态。
///
/// ⚠️ 权威源 = ADR 文本里的**表格**(per planner 产出的 S4 修);
/// 禁「文档里有这张表就通过」与「测试自建期望集 vs 自建实际集」的恒真写法。
struct CatchpointCoverage {
    /// 抓取点编号 → 该抓取点的可测性裁决文本
    std::map<std::string, std::string> verdicts;
    /// 解析到的抓取点条数
    std::size_t parsed = 0;
    bool source_error = false;
    std::string source_error_detail;
};

/// 该段内是否含**抓取点登记表**的表格形态(行首 `| **N**(`)—— 与标题措辞解耦
inline bool segment_has_catchpoint_table(const std::string& seg) {
    std::istringstream in(seg);
    std::string line;
    while (std::getline(in, line)) {
        std::size_t k = 0;
        while (k < line.size() && (line[k] == ' ' || line[k] == '\t' || line[k] == '>'))
            ++k;
        if (k >= line.size() || line[k] != '|')
            continue;
        // 行内出现 `**N**(` 形态
        std::size_t pos = 0;
        while ((pos = line.find("**", pos)) != std::string::npos) {
            const auto e = line.find("**", pos + 2);
            if (e == std::string::npos)
                break;
            const std::string token = line.substr(pos + 2, e - pos - 2);
            bool all_digit = !token.empty();
            for (char c : token)
                if (c < '0' || c > '9')
                    all_digit = false;
            if (all_digit && token.size() == 1 && line.find('(', e) != std::string::npos &&
                line.find('(', e) < line.find('|', e))
                return true;
            pos = e + 2;
        }
    }
    return false;
}

inline CatchpointCoverage parse_catchpoint_coverage(const std::string& adr_text) {
    CatchpointCoverage cov;
    // ⚠️ **必须限定在**「Status Update 2026-10-26 —— §3.3 抓取点的可测性裁决」段内:
    //    ADR 正文里的加粗数字(如 `**7 条**`、`**1**`)会污染全文级解析。
    // ⚠️ **不得硬编码日期 / 不得依赖标题关键词** —— 本仓铁律是「ADR 不可变 = 追加新 Status Update」,
    //    未来对抓取点的**重判**会写进**新**段 ⇒ 硬编码旧日期、或靠标题含「抓取点」三字
    //    (标题可能写成「§3.3 评审检查单更新」)⇒ 重判不可见、闸仍绿(per critic)。
    // ⇒ 判据 = **该段内含抓取点登记表的表格形态**(行首 `| **N**(`)⇒ 与标题措辞解耦。
    std::string scope;
    std::size_t search_from = 0;
    while (true) {
        const auto marker = adr_text.find("## Status Update", search_from);
        if (marker == std::string::npos)
            break;
        const auto next_h2 = adr_text.find("\n## ", marker + 4);
        const std::string seg = (next_h2 == std::string::npos)
                                    ? adr_text.substr(marker)
                                    : adr_text.substr(marker, next_h2 - marker);
        if (segment_has_catchpoint_table(seg))
            scope = seg;  // 保留**最后**一个含抓取点登记表的段
        search_from = (next_h2 == std::string::npos) ? adr_text.size() : next_h2 + 1;
    }
    if (scope.empty())
        scope = adr_text;
    // 抓取点编号在 ADR 里形如 `**1**(高)` / `**2**(高)` / … / `**7**(低)`
    // 裁决文本在同一表格行内。逐行取「首个 **N**(…) 后的单元格」。
    std::istringstream in(scope);
    std::string line;
    while (std::getline(in, line)) {
        if (line.find('|') == std::string::npos)
            continue;
        // 只认**表格行** —— 行首(可含 `> ` blockquote 前缀)必须是 `|`;
        // 正文里的 `**N**` 不算。
        // ⚠️ 必须容忍 `>` 前缀:ADR 的 Status Update 段把表格包在 blockquote 里
        //    (`> | 抓取点 | … |`)⇒ 只认 `|` 会把 7 条抓取点**全部**漏掉(实测 parsed=0)。
        {
            std::size_t k = 0;
            while (k < line.size() &&
                   (line[k] == ' ' || line[k] == '\t' || line[k] == '>'))
                ++k;
            if (k >= line.size() || line[k] != '|')
                continue;
        }
        // 定位 `**N**` 形态
        std::size_t pos = 0;
        while (true) {
            const auto b = line.find("**", pos);
            if (b == std::string::npos)
                break;
            const auto e = line.find("**", b + 2);
            if (e == std::string::npos)
                break;
            const std::string token = line.substr(b + 2, e - b - 2);
            bool all_digit = !token.empty();
            for (char c : token)
                if (c < '0' || c > '9')
                    all_digit = false;
            if (all_digit && token.size() == 1) {
                if (cov.verdicts.count(token) == 0) {
                    // 按**列**取「本 Stage 可测性」列(第 3 列):
                    // | 抓取点 | 内容 | 本 Stage 可测性 | 处置 |
                    // 逐行取整行文本会把第 2 列的「内容」也混进来 ⇒ 关键词匹配失真。
                    std::string rest = line.substr(e + 2);
                    std::string cell3;
                    int col = 0;
                    std::string cur;
                    for (char c : rest) {
                        if (c == '|') {
                            ++col;
                            // ⚠️ 列计数:`rest` 从 `**N**` **之后**开始 ⇒ 第 1 个 `|` 结束的是
                            //    **第 1 列**(如 `**1**(高) |`),第 2 个结束第 2 列(内容),
                            //    **第 3 个**才结束第 3 列(本 Stage 可测性)⇒ 取 col == 3。
                            if (col == 3) {
                                cell3 = cur;
                                break;
                            }
                            cur.clear();
                            continue;
                        }
                        cur.push_back(c);
                    }
                    // 去掉首尾空白
                    const auto trim = [](std::string& v) {
                        while (!v.empty() && (v.front() == ' ' || v.front() == '\t'))
                            v.erase(v.begin());
                        while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r'))
                            v.pop_back();
                    };
                    trim(cell3);
                    cov.verdicts[token] = cell3;
                }
            }
            pos = e + 2;
        }
    }
    cov.parsed = cov.verdicts.size();
    return cov;
}

/// 抓取点 → 判据的**显式映射表**。
///
/// ⚠️ 禁「测试自建期望集 vs 自建实际集」的恒真写法:本表是**代码里的显式数据**,
/// 被测的是「**ADR 声称的覆盖 ⊆ 本表的覆盖** 且 判据**实跑通过**」——
/// 即 ADR 若把某个抓取点改判为「可机器检查」而本表未绑定规则 ⇒ 报红。
struct CatchpointBinding {
    const char* id;       // 抓取点编号("1".."7")
    ContractRule rule;    // 覆盖它的判据(ZeroCallEdge 兼作「无覆盖」哨兵)
    bool behavior_side;   // true = 由**行为面**测试覆盖(不在判据清单里)
    bool declared_measurable;  // ADR 声称该抓取点「本 Stage 可机器检查 / 仅声明层可测」
};

inline const std::vector<CatchpointBinding>& catchpoint_bindings() {
    static const std::vector<CatchpointBinding> v = {
        {"1", ContractRule::ZeroCallEdge, false, true},   // 零调用边
        {"2", ContractRule::None, false, false},          // 组播投递单实现 —— 推迟
        {"3", ContractRule::VcReservation, false, true},  // 死锁通道隔离(仅声明层)
        {"4", ContractRule::CanonicalFlows, false, true}, // canonical 10 流零 diff
        {"5", ContractRule::None, false, false},          // barrier 分类规则已在正文 —— 已满足
        {"6", ContractRule::None, true, true},            // partial failure —— 行为面覆盖
        {"7", ContractRule::None, false, false},          // 复合流编排 —— 推迟
    };
    return v;
}

inline ContractVerdict evaluate_catchpoint_coverage(const CatchpointCoverage& cov,
                                                    const ContractReport& self,
                                                    std::string& detail) {
    if (cov.source_error) {
        detail = "抓取点可测性登记解析失败: " + cov.source_error_detail;
        return ContractVerdict::FAIL;
    }
    if (cov.parsed != 7) {
        detail = "抓取点可测性登记解析出 " + std::to_string(cov.parsed) + " 条(期望 7)";
        return ContractVerdict::FAIL;
    }
    // 映射表必须**穷举 7 条**(缺一条 ⇒ 映射表本身失守)
    if (catchpoint_bindings().size() != 7) {
        detail = "抓取点映射表条目数 " + std::to_string(catchpoint_bindings().size()) + " ≠ 7";
        return ContractVerdict::FAIL;
    }
    for (const auto& binding : catchpoint_bindings()) {
        const auto vit = cov.verdicts.find(binding.id);
        if (vit == cov.verdicts.end()) {
            detail = std::string("抓取点 ") + binding.id + " 未在 ADR 登记中解析到";
            return ContractVerdict::FAIL;
        }
        const std::string& verdict_text = vit->second;
        // ADR 的可测性裁决 → 分类
        const bool says_measurable =
            verdict_text.find("可机器检查") != std::string::npos ||
            verdict_text.find("仅声明层可测") != std::string::npos;
        const bool says_deferred = verdict_text.find("不可测") != std::string::npos ||
                                   verdict_text.find("推迟") != std::string::npos;
        const bool says_satisfied = verdict_text.find("已满足") != std::string::npos;
        // ⚠️ **无法分类 ⇒ FAIL**(per critic):三类措辞全不命中时**不得**静默通过 ——
        //   否则「ADR 把措辞改成无法识别的说法」即可绕过闸(判据失效却仍绿)。
        if (!says_measurable && !says_deferred && !says_satisfied) {
            detail = std::string("抓取点 ") + binding.id +
                     " 的可测性裁决**无法分类**(三类措辞均不命中): \"" + verdict_text +
                     "\" ⇒ 判红(避免「换个措辞即绕过闸」)";
            return ContractVerdict::FAIL;
        }

        // ⚠️ **交叉核对**:映射表声明的「本 change 承诺覆盖」必须与 ADR 的措辞分类**一致** ——
        //    不一致(ADR 说可测而本 change 没承诺 / 本 change 承诺而 ADR 说不测)⇒ 判红。
        //    该字段原先**只写不读**(与已删除的 `collective_enabled` 死字段同类)——
        //    现在它承担「承诺 vs 声明」的对账,不再是死字段。
        if (says_measurable != binding.declared_measurable) {
            detail = std::string("抓取点 ") + binding.id + " 的覆盖声明与 ADR 措辞不一致:ADR " +
                     (says_measurable ? "声称可机器检查" : "标为推迟/已满足") + ",但本 change 的映射表 " +
                     (binding.declared_measurable ? "承诺覆盖" : "未承诺覆盖") +
                     " ⇒ 判红(避免「改了 ADR 措辞而闸无感」)";
            return ContractVerdict::FAIL;
        }

        if (says_measurable) {
            // ADR 声称可测 ⇒ 映射表必须绑定规则(判据侧或行为面侧),且判据须实跑通过
            if (binding.behavior_side)
                continue;  // 由行为面测试覆盖
            if (binding.rule == ContractRule::None) {
                detail = std::string("抓取点 ") + binding.id +
                         " 在 ADR 声称可机器检查,但映射表未绑定任何判据";
                return ContractVerdict::FAIL;
            }
            const ContractVerdict v = self.verdict_of(binding.rule);
            if (v == ContractVerdict::SKIP) {
                detail = std::string("抓取点 ") + binding.id + " 声称可机器检查,但对应判据 " +
                         contract_rule_id(binding.rule) + " 未出现在判据清单里";
                return ContractVerdict::FAIL;
            }
            if (v != ContractVerdict::PASS) {
                detail = std::string("抓取点 ") + binding.id + " 声称可机器检查,但对应判据 " +
                         contract_rule_id(binding.rule) + " 实跑为 " +
                         contract_verdict_name(v);
                return ContractVerdict::FAIL;
            }
        }
        if (says_deferred || says_satisfied) {
            // ADR 说「推迟 / 已满足」⇒ **不得**有判据绑定(否则是「禁写恒真判据」的破口)
            if (!binding.behavior_side && binding.rule != ContractRule::None) {
                detail = std::string("抓取点 ") + binding.id +
                         " 在 ADR 标为推迟/已满足,却有判据 " +
                         contract_rule_id(binding.rule) + " 绑定 ⇒ 禁写恒真判据";
                return ContractVerdict::FAIL;
            }
        }
    }
    detail = "7 条抓取点登记解析成功;声称可测的条目均有对应判据且实跑通过,推迟/已满足项无判据绑定";
    return ContractVerdict::PASS;
}

// ─────────────────────────────────────────────────────────────────────
// 判据编排
// ─────────────────────────────────────────────────────────────────────

/// 契约判据的输入路径集合(相对仓库根;与 CLI 共用同一常量组)
inline constexpr const char* kCollectiveHeaderPath = "include/tlm/cep/collective_plane.hh";
inline constexpr const char* kCollectiveContractHeaderPath =
    "include/tlm/cep/collective_plane_contract.hh";
inline constexpr const char* kCanonicalFlowsAdrPath =
    "docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md";
inline constexpr const char* kCatchpointAdrPath = "docs/adr/ADR-CEP-01-fabric-collective-plane.md";
inline constexpr const char* kCoherenceSidePath = "include/tlm/cep/ordering_point_checker.hh";
inline constexpr const char* kProtocolInstancesPath = "include/tlm/cep/protocol_instances.json";
inline constexpr const char* kFabricPath = "include/tlm/cep/fabric.json";

struct ContractPaths {
    std::string root = ".";
    std::string collective_header = kCollectiveHeaderPath;
    std::string collective_contract_header = kCollectiveContractHeaderPath;
    std::string canonical_flows_adr = kCanonicalFlowsAdrPath;
    std::string catchpoint_adr = kCatchpointAdrPath;
    std::string coherence_side = kCoherenceSidePath;
    std::string protocol_instances = kProtocolInstancesPath;
    std::string fabric = kFabricPath;
    std::string declaration = kDefaultCollectiveDeclarationPath;
};

inline ContractReport run_collective_plane_contract(const ContractPaths& paths) {
    ContractReport r;
    const std::string root = paths.root.empty() ? std::string(".") : paths.root;
    const auto at = [&](const std::string& rel) {
        // 绝对路径直接用(否则 `root + "/" + "/tmp/x"` 指向不存在的路径 ⇒ 报「不可读」而非真因)
        if (!rel.empty() && rel.front() == '/')
            return rel;
        return root + "/" + rel;
    };

    // 声明源:缺失 / 畸形 ⇒ **FAIL**(配置错误),不是 SKIP
    const CollectivePlaneDeclaration decl = load_collective_plane_declaration(at(paths.declaration));
    if (decl.source_error) {
        r.verdict = ContractVerdict::FAIL;
        r.errors.push_back(decl.source_error_detail);
        r.log = "[FAIL] cep collective plane contract: 声明源配置错误 —— " +
                decl.source_error_detail + '\n';
        return r;
    }

    const auto add = [&](ContractRule rule, ContractVerdict v, std::string detail) {
        ContractVerdictLine line;
        line.rule = rule;
        line.verdict = v;
        line.detail = std::move(detail);
        r.lines.push_back(line);
    };

    // ⚠️ 每条判据都拆成「先算 verdict,再 move detail」两步 ——
    //    `add(rule, evaluate(d), std::move(d))` 的**函数实参求值顺序未指定**,
    //    std::move(d) 可能先于 evaluate 执行 ⇒ detail 被移空(实测 CP-02/CP-06 detail 为空)。

    // CP-01 零调用边 —— 扫**两个** collective 侧头文件(行为面 + 判据面);
    //   只扫一个 ⇒ 判据面自己 include 禁用侧的头不会被抓到(per critic)
    {
        std::string d;
        const ContractVerdict v = evaluate_zero_call_edge(
            {at(paths.collective_header), at(paths.collective_contract_header)}, d);
        add(ContractRule::ZeroCallEdge, v, std::move(d));
    }
    // CP-02 canonical 10 流零 diff
    {
        std::string d;
        const ContractVerdict v =
            evaluate_canonical_flows(at(paths.canonical_flows_adr), at(paths.collective_header), d);
        add(ContractRule::CanonicalFlows, v, std::move(d));
    }
    // CP-03 collective VC 预留
    {
        std::string d;
        const VcBindingSnapshot snap =
            load_vc_binding_snapshot(at(paths.protocol_instances), at(paths.fabric));
        if (snap.source_error) {
            add(ContractRule::VcReservation, ContractVerdict::FAIL, snap.source_error_detail);
        } else {
            const ContractVerdict v = evaluate_vc_reservation(decl, snap, d);
            add(ContractRule::VcReservation, v, std::move(d));
        }
    }
    // CP-04 区域互斥(与行为面同源)
    {
        std::string d;
        const ContractVerdict v = evaluate_region_exclusion(decl, d);
        add(ContractRule::RegionExclusion, v, std::move(d));
    }
    // CP-05 完成域判别
    {
        std::string d;
        const ContractVerdict v = evaluate_completion_domain(at(paths.coherence_side), d);
        add(ContractRule::CompletionDomain, v, std::move(d));
    }
    // CP-06 抓取点可测性登记(须在其它判据跑完后核对 ⇒ 放最后)
    {
        std::ifstream in(at(paths.catchpoint_adr));
        if (!in) {
            std::string d = "抓取点登记权威源不可读: " + paths.catchpoint_adr;
            add(ContractRule::CatchpointCoverage, ContractVerdict::FAIL, std::move(d));
        } else {
            std::ostringstream ss;
            ss << in.rdbuf();
            const CatchpointCoverage cov = parse_catchpoint_coverage(ss.str());
            std::string d;
            const ContractVerdict v = evaluate_catchpoint_coverage(cov, r, d);
            add(ContractRule::CatchpointCoverage, v, std::move(d));
        }
    }

    std::size_t n_fail = 0;
    for (const auto& l : r.lines)
        if (l.verdict == ContractVerdict::FAIL)
            ++n_fail;
    r.verdict = (n_fail == 0) ? ContractVerdict::PASS : ContractVerdict::FAIL;

    std::ostringstream os;
    os << '[' << contract_verdict_name(r.verdict) << "] cep collective plane contract: "
       << r.lines.size() << " rule(s), " << n_fail << " failed\n";
    for (const auto& l : r.lines) {
        os << "[verdict] rule=" << contract_rule_id(l.rule) << ' '
           << contract_verdict_name(l.verdict) << " (" << l.detail << ")\n";
    }
    for (const auto& e : r.errors)
        os << "[error] " << e << '\n';
    r.log = os.str();
    return r;
}

}  // namespace tlm::cep

#endif  // INCLUDE_TLM_CEP_COLLECTIVE_PLANE_CONTRACT_HH_
