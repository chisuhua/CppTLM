// =============================================================================
// scripts/test/cep_nvlink_sharp_probe.cc
//   —— 队列 #13 **实现层** change `cpptlm-cep-nvlink-sharp` 的**反向验证探针**
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: scripts/test/cep_nvlink_sharp_reverse_verify.sh(驱动本探针)
//       include/tlm/cep/nvlink_adapter.hh / broadcast_null_tracker.hh(被毒化的对象)
//       include/tlm/cep/collective_plane.hh / collective_plane_contract.hh(被毒化的对象)
//       include/tlm/cep/protocol_instances.json / collective_planes.json(被毒化的对象)
//
// ── 用途 ──────────────────────────────────────────────────────────────────
// 本探针在**影子目录**(被毒化的副本)下编译并运行 ⇒ 验证判据的**鉴别力**:
//   · **干净载体 ⇒ 必须通过**(基线正控)
//   · **毒化载体 ⇒ 必须失败**,且失败原因 SHALL 命中**期望的判据 id**
//
// ⚠️ **两类失败形态都要支持**:
//   ① **编译期红** ⇒ `static_assert` 触发 ⇒ 归因 = 编译日志里的 `<id>:`
//   ② **运行期红** ⇒ 探针的 `PROBE:FAIL <id>` 输出
//
// ⚠️ **归因必须「精确相等」**(⛔ 不用前缀匹配)—— 否则 `CP-03` 会命中
//    `CP-03-VC-PRECONDITION`(per 台账 Entry #187)。
//
// 用法:probe <shadow_root>
//   <shadow_root> = 影子根目录(其 `include/` 与 `include/tlm/cep/` 下是被毒化的副本)
//   退出码 0 = 全部谓词成立;1 = 有谓词不成立
// =============================================================================

#include "tlm/cep/broadcast_null_tracker.hh"
#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/collective_plane_contract.hh"
#include "tlm/cep/nvlink_adapter.hh"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    using json = nlohmann::json;

    int g_fail = 0;
    std::string g_root = ".";

    void expect(bool cond, const char* id, const std::string& what) {
        if (!cond) {
            std::printf("PROBE:FAIL %s —— %s\n", id, what.c_str());
            ++g_fail;
        }
    }

    std::string read_file(const std::string& rel) {
        std::ifstream in(g_root + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去注释(行 + 块)
    std::string strip_comments(const std::string& src) {
        std::string out;
        bool line = false;
        bool block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (!line && !block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                line = true;
                ++i;
                continue;
            }
            if (!line && !block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                block = true;
                ++i;
                continue;
            }
            if (line && src[i] == '\n') {
                line = false;
                out.push_back('\n');
                continue;
            }
            if (block && src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                block = false;
                ++i;
                continue;
            }
            if (!line && !block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// 去注释 + 去字符串 / 字符字面量(「某标识符不得出现」类判据只看**代码标识符**)
    std::string strip_comments_and_strings(const std::string& src) {
        const std::string nc = strip_comments(src);
        std::string out;
        bool in_str = false;
        bool in_chr = false;
        for (std::size_t i = 0; i < nc.size(); ++i) {
            const char c = nc[i];
            if (!in_str && !in_chr && (c == '"' || c == '\'')) {
                in_str = (c == '"');
                in_chr = (c == '\'');
                continue;
            }
            if (in_str || in_chr) {
                if (c == '\\' && i + 1 < nc.size()) {
                    ++i;
                    continue;
                }
                if ((in_str && c == '"') || (in_chr && c == '\'')) {
                    in_str = false;
                    in_chr = false;
                }
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

    /// 数 `enum class <name>` 块内的**枚举项个数**(⛔ 不是「最大值 + 1」——
    /// C++17 无反射 ⇒ 追加第 7 个值时「最大值 + 1」不变 ⇒ 无鉴别力)
    std::size_t count_enum_items(const std::string& src, const std::string& enum_name,
                                  bool* found) {
        *found = false;
        const std::string key = "enum class " + enum_name;
        const auto pos = src.find(key);
        if (pos == std::string::npos)
            return 0;
        const auto open = src.find('{', pos);
        if (open == std::string::npos)
            return 0;
        std::size_t depth = 0;
        std::size_t end = std::string::npos;
        for (std::size_t i = open; i < src.size(); ++i) {
            if (src[i] == '{')
                ++depth;
            else if (src[i] == '}') {
                --depth;
                if (depth == 0) {
                    end = i;
                    break;
                }
            }
        }
        if (end == std::string::npos)
            return 0;
        *found = true;
        const std::string body = src.substr(open + 1, end - open - 1);
        // 逐项:以逗号分隔;去掉尾逗号后计非空项
        std::size_t count = 0;
        std::string item;
        std::vector<std::string> items;
        for (char c : body) {
            if (c == ',') {
                items.push_back(item);
                item.clear();
                continue;
            }
            item.push_back(c);
        }
        items.push_back(item);
        for (const auto& raw : items) {
            // 去空白
            std::size_t b = 0;
            std::size_t e = raw.size();
            while (b < e && (raw[b] == ' ' || raw[b] == '\n' || raw[b] == '\t' || raw[b] == '\r'))
                ++b;
            while (e > b &&
                   (raw[e - 1] == ' ' || raw[e - 1] == '\n' || raw[e - 1] == '\t' || raw[e - 1] == '\r'))
                --e;
            if (e > b)
                ++count;
        }
        return count;
    }

    bool json_load(const std::string& rel, json& out) {
        const std::string raw = read_file(rel);
        if (raw.empty())
            return false;
        out = json::parse(raw);
        return true;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("PROBE:FAIL NVA-01 —— 用法: probe <shadow_root>\n");
        return 1;
    }
    g_root = argv[1];

    using namespace tlm::cep;

    // ══════════════════════════════════════════════════════════════════════
    // NVA-01 —— adapter 零自有表副本 / 无 transition_* / 不暴露区域表类型
    // ══════════════════════════════════════════════════════════════════════
    {
        const std::string idents = strip_comments_and_strings(read_file("include/tlm/cep/nvlink_adapter.hh"));
        expect(!idents.empty(), "NVA-01", "adapter 头不可读");
        static const char* kForbidden[] = {
            "IngressDecision::", "RegisterResult::",  "EpochState::",
            "OrderingRule::",   "VcReservationState::", "CollectiveScope::",
            "EvidenceKind::",    "SourceId::",         "Basis{",
            "kSourceLicenseConfirmed", "all_basis_valid", "PendingSemanticItem",
            "kPendingSemanticItems"};
        for (const char* f : kForbidden) {
            expect(idents.find(f) == std::string::npos, "NVA-01",
                   std::string("adapter 不得含成员字面量 / 表名 `") + f + "`");
        }
        expect(idents.find("kBasis") == std::string::npos, "NVA-01",
               "adapter 不得含 basis 表名 `kBasis*`");
        expect(idents.find("transition_") == std::string::npos, "NVA-01",
               "adapter SHALL NOT 暴露 `transition_*`");
        expect(idents.find("collective_region_table") == std::string::npos, "NVA-01",
               "adapter SHALL NOT 暴露 `collective_region_table`");
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-02 —— BroadcastNull:caps 逐字段取自 traits + 全部出参
    // ══════════════════════════════════════════════════════════════════════
    {
        BroadcastNullTracker tracker;  // ⛔ 非 const:`lookup` 是虚方法
        const TrackerTraits* t = BroadcastNullTracker::traits();
        expect(t != nullptr, "NVA-02", "traits 行 SHALL 存在");
        if (t != nullptr) {
            expect(tracker.caps == t->caps, "NVA-02",
                   "可见 caps SHALL 与 traits 行**逐字段相等**(毒化 traits ⇒ 可见 caps 必须跟随)");
            expect(tracker.behavior.hazard_under_contention == t->behavior.hazard_under_contention &&
                       tracker.behavior.broadcast_fallback == t->behavior.broadcast_fallback,
                   "NVA-02", "可见 behavior SHALL 与 traits 行逐字段相等");
        }
        // 出参**全**断言(⛔ 不依赖默认值 —— `LocalView` 默认值恰好符合期望)
        LookupResult result = LookupResult::ANSWER_HERE;
        LocalView view{};
        view.valid = true; // 故意置成「非期望值」⇒ 实现必须显式写 false
        view.sharers = 0xFFu;
        view.dirty = 0xFFu;
        view.forward_to = 7u;
        LookupHazard hazard = LookupHazard::STALL;
        tracker.lookup(0x1000u, Intent::want_read, result, view, hazard);
        expect(result == LookupResult::BROADCAST, "NVA-02", "lookup SHALL 恒返 BROADCAST");
        expect(!view.valid, "NVA-02", "lookup SHALL 置 view.valid == false");
        expect(view.sharers == 0u, "NVA-02", "lookup SHALL 置 view.sharers == 0");
        expect(view.dirty == 0u, "NVA-02", "lookup SHALL 置 view.dirty == 0");
        expect(view.forward_to == kInvalidNode, "NVA-02", "lookup SHALL 置 view.forward_to == 无效");
        expect(hazard == LookupHazard::NONE, "NVA-02", "lookup SHALL 置 hazard == NONE");
        // 本类**不持有**任何 `std::map` / `std::vector` 状态容器(源扫描 ⇒「零目录边」可证伪)
        const std::string idents = strip_comments_and_strings(read_file("include/tlm/cep/broadcast_null_tracker.hh"));
        expect(idents.find("std::map") == std::string::npos, "NVA-02",
               "tracker 不得持有 `std::map` 状态容器");
        expect(idents.find("std::vector") == std::string::npos, "NVA-02",
               "tracker 不得持有 `std::vector` 状态容器");
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-03 —— nvlink_4 实例声明:13 键齐备 + caps ↔ traits + repo_owned
    // ══════════════════════════════════════════════════════════════════════
    {
        json inst_doc;
        expect(json_load("include/tlm/cep/protocol_instances.json", inst_doc), "NVA-03",
               "protocol_instances.json 不可读 / 畸形");
        json schema;
        expect(json_load("include/tlm/cep/protocol_instance.schema.json", schema), "NVA-03",
               "protocol_instance.schema.json 不可读 / 畸形");
        if (!inst_doc.is_null() && !schema.is_null()) {
            // 13 键 = schema 的 `required` **实读**
            const json* inst = nullptr;
            if (inst_doc.contains("instances")) {
                for (const auto& i : inst_doc.at("instances")) {
                    if (i.contains("id") && i.at("id").get<std::string>() == "nvlink_4")
                        inst = &i;
                }
            }
            expect(inst != nullptr, "NVA-03", "`nvlink_4` 实例声明 SHALL 存在");
            if (inst != nullptr && schema.contains("required")) {
                for (const auto& k : schema.at("required")) {
                    expect(inst->contains(k.get<std::string>()), "NVA-03",
                           "`nvlink_4` 缺 schema required 键 `" + k.get<std::string>() + "`");
                }
                expect(inst->at("protocol_version").get<std::string>() == "nvlink-4.0", "NVA-03",
                       "`nvlink_4` 的 protocol_version SHALL 为 nvlink-4.0");
                expect(inst->at("sharer_tracker").at("impl").get<std::string>() == "BroadcastNull",
                       "NVA-03", "`nvlink_4` 的 sharer_tracker.impl SHALL 为 BroadcastNull");
                // caps ↔ traits **逐字段相等**
                const json& caps = inst->at("sharer_tracker").at("caps");
                const TrackerTraits* t = find_tracker_traits(TrackerImpl::BroadcastNull);
                expect(t != nullptr, "NVA-03", "traits 行 SHALL 存在");
                if (t != nullptr) {
                    expect(caps.at("scope").get<std::string>() ==
                               (t->caps.scope == Scope::NONE ? "NONE" : "?"),
                           "NVA-03", "caps.scope SHALL 与 traits 相等");
                    expect(caps.at("completeness").get<std::string>() ==
                               (t->caps.completeness == Completeness::EXACT ? "EXACT" : "CONSERVATIVE"),
                           "NVA-03", "caps.completeness SHALL 与 traits 相等");
                    expect(caps.at("authority").get<std::string>() ==
                               (t->caps.authority == Authority::NONE ? "NONE" : "?"),
                           "NVA-03", "caps.authority SHALL 与 traits 相等");
                    expect(caps.at("aggregation_at").get<std::string>() ==
                               (t->caps.aggregation_at == AggregationAt::NONE ? "NONE" : "?"),
                           "NVA-03", "caps.aggregation_at SHALL 与 traits 相等");
                }
            }
            // `repo_owned_instance_ids` 含本实例
            bool owned = false;
            if (inst_doc.contains("repo_owned_instance_ids")) {
                for (const auto& id : inst_doc.at("repo_owned_instance_ids")) {
                    if (id.get<std::string>() == "nvlink_4")
                        owned = true;
                }
            }
            expect(owned, "NVA-03", "`repo_owned_instance_ids` SHALL 含 `nvlink_4`");
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-04 —— V3 barrier 分类 litmus 双向(归因 CP-BARRIER-CLASS)
    // ══════════════════════════════════════════════════════════════════════
    {
        BarrierKey ordering_key;
        ordering_key.key_kind = BarrierKeyKind::Address;
        ordering_key.completion_kind = BarrierCompletionKind::Transaction;
        ordering_key.addr = 0x1000u;
        BarrierKey collective_key;
        collective_key.key_kind = BarrierKeyKind::Group;
        collective_key.completion_kind = BarrierCompletionKind::Participant;
        collective_key.group = 1u;
        expect(classify_barrier_key(ordering_key) == BarrierKeyClass::Ordering, "CP-BARRIER-CLASS",
               "地址 key + 数事务 ⇒ SHALL 判 Ordering");
        expect(classify_barrier_key(collective_key) == BarrierKeyClass::Collective,
               "CP-BARRIER-CLASS", "组 key + 数参与者 ⇒ SHALL 判 Collective");
        // 空 key / 双重 key / 两条 litmus 冲突 ⇒ Invalid
        BarrierKey empty_key;
        expect(classify_barrier_key(empty_key) == BarrierKeyClass::Invalid, "CP-BARRIER-CLASS",
               "空 key ⇒ SHALL 判 Invalid");
        BarrierKey dual_key;
        dual_key.addr = 0x1000u;
        dual_key.group = 1u;
        expect(classify_barrier_key(dual_key) == BarrierKeyClass::Invalid, "CP-BARRIER-CLASS",
               "双重 key ⇒ SHALL 判 Invalid");
        BarrierKey conflict_key;
        conflict_key.key_kind = BarrierKeyKind::Address;
        conflict_key.completion_kind = BarrierCompletionKind::Participant;
        conflict_key.addr = 0x1000u;
        expect(classify_barrier_key(conflict_key) == BarrierKeyClass::Invalid, "CP-BARRIER-CLASS",
               "两条 litmus 冲突 ⇒ SHALL 判 Invalid");
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-05 —— 「算子族恰 6 值」(**源文本枚举项计数**)
    // ══════════════════════════════════════════════════════════════════════
    {
        const std::string src = strip_comments(read_file("include/tlm/cep/collective_plane.hh"));
        bool found = false;
        const std::size_t n = count_enum_items(src, "ReduceOp", &found);
        expect(found, "NVA-05", "未找到 `enum class ReduceOp`");
        expect(n == 6u, "NVA-05",
               "`ReduceOp` 枚举项 SHALL 恰为 6(实测 " + std::to_string(n) + ")");
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-06 —— partial failure 显式建模(归因 CP-PARTIAL-FAIL)
    // ══════════════════════════════════════════════════════════════════════
    {
        LocalCollectivePlane plane;
        plane.set_current_member(1u);
        const group_id_t gid = plane.group_create({1u, 2u, 3u}, GroupAttrs{});
        expect(gid != kInvalidGroupId, "CP-PARTIAL-FAIL", "group_create 失败");
        plane.set_current_member(1u);
        plane.collective_barrier(gid, 1u); // 成员 1 到达
        plane.group_destroy(gid);           // ⇒ 对 in-flight barrier 的到达者发 Fault
        bool saw_fault = false;
        CollectiveEvt evt;
        // 非阻塞取事件:LocalCollectivePlane 的 recv 在无事件时返回 NoEvent
        for (int i = 0; i < 8; ++i) {
            plane.recv(evt);
            if (evt.is_fault()) {
                saw_fault = true;
                break;
            }
            if (plane.last_status() == Status::NoEvent)
                break;
        }
        expect(saw_fault, "CP-PARTIAL-FAIL",
               "group_destroy 时存在 in-flight barrier 到达者 ⇒ SHALL 产出 Fault 事件");
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-07 —— 集体面绑定 + region 声明(本层引入的那一条)
    // ══════════════════════════════════════════════════════════════════════
    {
        json planes_doc;
        expect(json_load("include/tlm/cep/collective_planes.json", planes_doc), "NVA-07",
               "collective_planes.json 不可读 / 畸形");
        if (!planes_doc.is_null()) {
            expect(planes_doc.contains("plane_binding"), "NVA-07", "缺 `plane_binding`");
            if (planes_doc.contains("plane_binding")) {
                const json& pb = planes_doc.at("plane_binding");
                expect(pb.contains("nvlink_4"), "NVA-07", "`plane_binding` SHALL 含 `nvlink_4`");
                if (pb.contains("nvlink_4")) {
                    expect(pb.at("nvlink_4").get<std::string>() == "LocalCollectivePlane", "NVA-07",
                           "`nvlink_4` SHALL 绑**本地模型** LocalCollectivePlane");
                }
            }
            expect(planes_doc.contains("registered_regions"), "NVA-07", "缺 `registered_regions`");
            if (planes_doc.contains("registered_regions")) {
                const json& regs = planes_doc.at("registered_regions");
                expect(regs.size() >= 1u, "NVA-07", "`registered_regions` SHALL 非空");
                bool mode_ok = true;
                for (const auto& r : regs) {
                    if (!r.contains("mode") || r.at("mode").get<std::string>() != "nc_drop_cacheable")
                        mode_ok = false;
                }
                expect(mode_ok, "NVA-07", "已注册 region 的 `mode` SHALL 为 nc_drop_cacheable(R3)");
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-08 —— V6-c 前提约束(归因 CP-03-VC-PRECONDITION,**精确相等**)
    // ══════════════════════════════════════════════════════════════════════
    {
        json planes_doc;
        if (json_load("include/tlm/cep/collective_planes.json", planes_doc) && !planes_doc.is_null()) {
            CollectivePlaneDeclaration decl = parse_collective_plane_declaration(planes_doc);
            std::string detail;
            const ContractVerdict v = evaluate_vc_precondition(decl, detail);
            expect(detail.rfind("[CP-03-VC-PRECONDITION]", 0) == 0, "CP-03-VC-PRECONDITION",
                   "V6-c 的 detail SHALL 以 `[CP-03-VC-PRECONDITION]` **开头**");
            expect(v == ContractVerdict::PASS, "CP-03-VC-PRECONDITION",
                   std::string("V6-c 前提约束 SHALL 成立(实测 ") + contract_verdict_name(v) + ": " + detail + ")");
        } else {
            expect(false, "CP-03-VC-PRECONDITION", "collective_planes.json 不可读 / 畸形");
        }
    }

    // ══════════════════════════════════════════════════════════════════════
    // NVA-09 —— CP-05 标识清单含 collective_region_table(复用 CP-05)
    // ══════════════════════════════════════════════════════════════════════
    {
        std::string detail;
        const ContractVerdict v =
            evaluate_completion_domain(std::vector<std::string>{g_root + "/include/tlm/cep/ordering_point_checker.hh"},
                                       detail);
        expect(v == ContractVerdict::PASS, "CP-05",
               "coherence 侧 SHALL 无 group-scoped 标识(含 collective_region_table)");
    }

    if (g_fail != 0) {
        std::printf("PROBE:SUMMARY %d 条谓词不成立\n", g_fail);
        return 1;
    }
    std::printf("PROBE:OK\n");
    return 0;
}
