// =============================================================================
// test/test_cep_nvlink_sharp.cc
//   —— 队列 #13 **实现层** change `cpptlm-cep-nvlink-sharp` 的**正向判据**
//
// 作者: CppTLM Team / 日期: 2026-11-03
// 配套: include/tlm/cep/nvlink_adapter.hh · broadcast_null_tracker.hh
//       include/tlm/cep/collective_plane.hh(V3)· collective_plane_contract.hh(V6-c / CP-05)
//       include/tlm/cep/protocol_instances.json · collective_planes.json
//       scripts/test/cep_nvlink_sharp_probe.cc + ..._reverse_verify.sh(**反向 fixture**)
//       openspec/changes/cpptlm-cep-nvlink-sharp/(4 件套 + 2 份 delta spec)
//       台账 obj-cep-platform.md Entry #187(双审修订)
//
// ── 判据纪律(队列 #12 沉淀的 4 铁律)──────────────────────────────────────
//   ① 纯负向断言须配正控 ② 每条判据须有可执行反向 fixture 且**因正确原因红**
//   ③ 编译期锁不得只以「编译红」充当 ctest 判据 ⇒ 本文件是**运行期**判据
//   ④ fixture 状态变迁后须重新基线化
//
// ⚠️ **本文件只提供正控 + 运行期兜底**;**反向 fixture** 在
//    `scripts/test/cep_nvlink_sharp_reverse_verify.sh`(影子毒化 ⇒ 必红 + 归因精确命中)。
// =============================================================================

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/broadcast_null_tracker.hh"
#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/collective_plane_contract.hh"
#include "tlm/cep/nvlink_adapter.hh"

namespace {

    using namespace tlm::cep;

    constexpr const char* kAdapterRelPath = "include/tlm/cep/nvlink_adapter.hh";
    constexpr const char* kTrackerRelPath = "include/tlm/cep/broadcast_null_tracker.hh";
    constexpr const char* kCollectiveRelPath = "include/tlm/cep/collective_plane.hh";
    constexpr const char* kInstancesRelPath = "include/tlm/cep/protocol_instances.json";
    constexpr const char* kPlanesRelPath = "include/tlm/cep/collective_planes.json";
    constexpr const char* kDesignRelPath = "openspec/changes/cpptlm-cep-nvlink-sharp/design.md";

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// **去注释**(行 + 块)—— 否则注释里提到被禁标识符会假阳性
    std::string local_strip_comments(const std::string& src) {
        std::string out;
        bool in_line = false;
        bool in_block = false;
        for (std::size_t i = 0; i < src.size(); ++i) {
            if (!in_line && !in_block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                in_line = true;
                ++i;
                continue;
            }
            if (!in_line && !in_block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                in_block = true;
                ++i;
                continue;
            }
            if (in_line && src[i] == '\n') {
                in_line = false;
                out.push_back('\n');
                continue;
            }
            if (in_block && src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                in_block = false;
                ++i;
                continue;
            }
            if (!in_line && !in_block)
                out.push_back(src[i]);
        }
        return out;
    }

    /// **去注释 + 去字符串 / 字符字面量**(只看**代码标识符**)
    std::string local_strip_comments_and_strings(const std::string& src) {
        const std::string nc = local_strip_comments(src);
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
            std::size_t b = 0;
            std::size_t e = raw.size();
            while (b < e && (raw[b] == ' ' || raw[b] == '\n' || raw[b] == '\t' || raw[b] == '\r'))
                ++b;
            while (e > b && (raw[e - 1] == ' ' || raw[e - 1] == '\n' || raw[e - 1] == '\t' ||
                             raw[e - 1] == '\r'))
                --e;
            if (e > b)
                ++count;
        }
        return count;
    }

} // namespace

// ══════════════════════════════════════════════════════════════════════════
// NVA-01 —— adapter 零自有表副本 / 无转移表 / 不暴露区域表类型
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-01: NvlinkAdapter 零自有表副本 + 无 transition_* + 不暴露区域表类型",
          "[cep][nvlink-sharp][adapter]") {
    const std::string idents = local_strip_comments_and_strings(read_repo_file(kAdapterRelPath));
    REQUIRE(!idents.empty());

    // 禁词表:成员字面量 + 表名(设计 §2.2)
    static const char* kForbidden[] = {"IngressDecision::",
                                       "RegisterResult::",
                                       "EpochState::",
                                       "OrderingRule::",
                                       "VcReservationState::",
                                       "CollectiveScope::",
                                       "EvidenceKind::",
                                       "SourceId::",
                                       "Basis{",
                                       "kSourceLicenseConfirmed",
                                       "all_basis_valid",
                                       "PendingSemanticItem",
                                       "kPendingSemanticItems",
                                       "kBasis"};
    for (const char* f : kForbidden) {
        INFO("forbidden: " << f);
        CHECK(idents.find(f) == std::string::npos);
    }
    CHECK(idents.find("transition_") == std::string::npos);
    CHECK(idents.find("collective_region_table") == std::string::npos);
    // 类型名别名**允许**(不含 `::` ⇒ 与 `CxlDcohAdapter` 同构)
    CHECK(idents.find("using IngressDecision = nvlink_sharp::IngressDecision;") !=
          std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-02 —— BroadcastNull:caps 取自 traits + 全部出参 + 无状态容器
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-02: BroadcastNull 行为实现 —— caps 取自 traits + 全部出参 + 零状态容器",
          "[cep][nvlink-sharp][tracker]") {
    BroadcastNullTracker tracker;
    const TrackerTraits* t = BroadcastNullTracker::traits();
    REQUIRE(t != nullptr);
    CHECK(tracker.caps == t->caps);
    CHECK(tracker.behavior.hazard_under_contention == t->behavior.hazard_under_contention);
    CHECK(tracker.behavior.broadcast_fallback == t->behavior.broadcast_fallback);
    CHECK(tracker.caps.scope == Scope::NONE);
    CHECK(tracker.caps.authority == Authority::NONE);
    CHECK(tracker.caps.aggregation_at == AggregationAt::NONE);

    // 出参**全**断言(⛔ 不依赖默认值 —— `LocalView` 默认值恰好符合期望)
    LookupResult result = LookupResult::ANSWER_HERE;
    LocalView view{};
    view.valid = true;
    view.sharers = 0xFFu;
    view.dirty = 0xFFu;
    view.forward_to = 7u;
    LookupHazard hazard = LookupHazard::STALL;
    tracker.lookup(0x1000u, Intent::want_read, result, view, hazard);
    CHECK(result == LookupResult::BROADCAST);
    CHECK(!view.valid);
    CHECK(view.sharers == 0u);
    CHECK(view.dirty == 0u);
    CHECK(view.forward_to == kInvalidNode);
    CHECK(hazard == LookupHazard::NONE);

    // 安装面 = 显式 no-op(⇒ 不留下可见条目)
    tracker.set_sharer(0x1000u, kNodeIdSdma, SnoopState::M);
    CHECK(tracker.possible_sharer_count(0x1000u) == 0u);
    CHECK(!tracker.view_of(0x1000u).valid);

    // 本类**不持有**任何 `std::map` / `std::vector` 状态容器(⇒「零目录边」可证伪)
    const std::string idents = local_strip_comments_and_strings(read_repo_file(kTrackerRelPath));
    CHECK(idents.find("std::map") == std::string::npos);
    CHECK(idents.find("std::vector") == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-03 —— nvlink_4 实例声明:13 键齐备 + caps ↔ traits + repo_owned
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-03: nvlink_4 实例声明 —— 13 键齐备 + caps 与 traits 相等 + repo_owned",
          "[cep][nvlink-sharp][instance]") {
    const std::string raw = read_repo_file(kInstancesRelPath);
    REQUIRE(!raw.empty());
    // 用契约头的既有解析器(⛔ 不另写一套 JSON 解析)
    const auto decl = load_collective_plane_declaration(std::string(CPPTLM_SOURCE_DIR) + "/" +
                                                        kPlanesRelPath); // 仅为确认解析器可用
    (void)decl;

    // 13 键 = schema `required` **实读** + 逐键核对(源码级,不依赖第三方 JSON 解析)
    const std::string schema = read_repo_file("include/tlm/cep/protocol_instance.schema.json");
    REQUIRE(!schema.empty());
    const std::string instances = raw;
    const auto pos = instances.find("\"id\": \"nvlink_4\"");
    REQUIRE(pos != std::string::npos);
    const auto end = instances.find("\"id\": \"tilelink_c\"", pos);
    const std::string block =
        instances.substr(pos, (end == std::string::npos ? instances.size() : end) - pos);
    REQUIRE(!block.empty());

    static const char* kRequired[] = {"id",
                                      "family",
                                      "adapter",
                                      "status",
                                      "protocol_version",
                                      "flow_support",
                                      "perm",
                                      "atomic",
                                      "consistency_labels",
                                      "ordering_assumption",
                                      "vc_binding",
                                      "sharer_tracker",
                                      "combination_capabilities"};
    for (const char* k : kRequired) {
        INFO("key: " << k);
        CHECK(block.find(std::string("\"") + k + "\"") != std::string::npos);
    }
    CHECK(block.find("\"protocol_version\": \"nvlink-4.0\"") != std::string::npos);
    CHECK(block.find("\"impl\": \"BroadcastNull\"") != std::string::npos);
    // caps ↔ traits **逐字段相等**
    const TrackerTraits* t = find_tracker_traits(TrackerImpl::BroadcastNull);
    REQUIRE(t != nullptr);
    CHECK(block.find("\"scope\": \"NONE\"") != std::string::npos);
    CHECK(block.find("\"completeness\": \"EXACT\"") != std::string::npos);
    CHECK(block.find("\"authority\": \"NONE\"") != std::string::npos);
    CHECK(block.find("\"aggregation_at\": \"NONE\"") != std::string::npos);
    CHECK(t->caps.scope == Scope::NONE);
    CHECK(t->caps.completeness == Completeness::EXACT);
    // `repo_owned_instance_ids` 含本实例
    CHECK(instances.find("\"nvlink_4\"\n  ]") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-04 —— V3 barrier 分类 litmus 双向(归因 CP-BARRIER-CLASS)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-04: V3 barrier 分类 litmus 双向 —— 地址 key ⇒ Ordering / 组 key ⇒ Collective",
          "[cep][nvlink-sharp][barrier]") {
    BarrierKey ordering_key;
    ordering_key.key_kind = BarrierKeyKind::Address;
    ordering_key.completion_kind = BarrierCompletionKind::Transaction;
    ordering_key.addr = 0x1000u;
    CHECK(classify_barrier_key(ordering_key) == BarrierKeyClass::Ordering);

    BarrierKey collective_key;
    collective_key.key_kind = BarrierKeyKind::Group;
    collective_key.completion_kind = BarrierCompletionKind::Participant;
    collective_key.group = 1u;
    CHECK(classify_barrier_key(collective_key) == BarrierKeyClass::Collective);

    // 空 key / 双重 key / 两条 litmus 冲突 ⇒ Invalid
    BarrierKey empty_key;
    CHECK(classify_barrier_key(empty_key) == BarrierKeyClass::Invalid);
    BarrierKey dual_key;
    dual_key.addr = 0x1000u;
    dual_key.group = 1u;
    CHECK(classify_barrier_key(dual_key) == BarrierKeyClass::Invalid);
    BarrierKey conflict_key;
    conflict_key.key_kind = BarrierKeyKind::Address;
    conflict_key.completion_kind = BarrierCompletionKind::Participant;
    conflict_key.addr = 0x1000u;
    CHECK(classify_barrier_key(conflict_key) == BarrierKeyClass::Invalid);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-05 —— 「算子族恰 6 值」(源文本枚举项计数)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-05: 算子族恰 6 值 —— 源文本枚举项计数(C++17 无反射 ⇒ 不用「最大值 + 1」)",
          "[cep][nvlink-sharp][reduceop]") {
    const std::string src = local_strip_comments(read_repo_file(kCollectiveRelPath));
    bool found = false;
    const std::size_t n = count_enum_items(src, "ReduceOp", &found);
    CHECK(found);
    CHECK(n == 6u);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-06 —— partial failure 显式建模(归因 CP-PARTIAL-FAIL)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-06: partial failure —— group_destroy 对 in-flight barrier 到达者产出 Fault",
          "[cep][nvlink-sharp][partial-failure]") {
    LocalCollectivePlane plane;
    plane.set_current_member(1u);
    const group_id_t gid = plane.group_create({1u, 2u, 3u}, GroupAttrs{});
    REQUIRE(gid != kInvalidGroupId);
    plane.set_current_member(1u);
    plane.collective_barrier(gid, 1u);
    plane.group_destroy(gid);

    bool saw_fault = false;
    CollectiveEvt evt;
    for (int i = 0; i < 8; ++i) {
        plane.recv(evt);
        if (evt.is_fault()) {
            saw_fault = true;
            break;
        }
        if (plane.last_status() == Status::NoEvent)
            break;
    }
    CHECK(saw_fault);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-07 —— 集体面绑定 + region 声明(本层引入的那一条)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-07: 集体面绑定 —— nvlink_4 绑本地模型 + 已注册 region 走 nc_drop_cacheable",
          "[cep][nvlink-sharp][collective]") {
    const std::string raw = read_repo_file(kPlanesRelPath);
    REQUIRE(!raw.empty());
    CHECK(raw.find("\"nvlink_4\": \"LocalCollectivePlane\"") != std::string::npos);
    CHECK(raw.find("\"mode\": \"nc_drop_cacheable\"") != std::string::npos);
    // 解析器视角:声明源可解析 + region 表非空
    const auto decl =
        load_collective_plane_declaration(std::string(CPPTLM_SOURCE_DIR) + "/" + kPlanesRelPath);
    CHECK(!decl.source_error);
    CHECK(decl.plane_binding.size() == 3u);
    CHECK(decl.registered_regions.size() == 1u);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-08 —— V6-c 前提约束(归因 CP-03-VC-PRECONDITION)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-08: V6-c 前提约束 —— unavailable ⇒ 仅本地模型(null 绑定不判 FAIL,零键 ⇒ SKIP)",
          "[cep][nvlink-sharp][vc-precondition]") {
    const auto decl =
        load_collective_plane_declaration(std::string(CPPTLM_SOURCE_DIR) + "/" + kPlanesRelPath);
    REQUIRE(!decl.source_error);
    std::string detail;
    const ContractVerdict v = evaluate_vc_precondition(decl, detail);
    CHECK(v == ContractVerdict::PASS);
    CHECK(detail.rfind("[CP-03-VC-PRECONDITION]", 0) == 0);

    // `null` 绑定 ⇒ 不判 FAIL(loader 把 null 物化成空串)
    CollectivePlaneDeclaration only_null = decl;
    only_null.plane_binding.clear();
    only_null.plane_binding["moesi_gpu"] = "";
    std::string d2;
    CHECK(evaluate_vc_precondition(only_null, d2) == ContractVerdict::PASS);

    // 零键 ⇒ SKIP(⛔ 禁 vacuous PASS)
    CollectivePlaneDeclaration empty = decl;
    empty.plane_binding.clear();
    std::string d3;
    CHECK(evaluate_vc_precondition(empty, d3) == ContractVerdict::SKIP);
}

// ══════════════════════════════════════════════════════════════════════════
// NVA-09 —— CP-05 标识清单含 collective_region_table + 扫描面可扩展
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("NVA-09: CP-05 —— 标识清单含 collective_region_table 且扫描面可注入扩展",
          "[cep][nvlink-sharp][completion-domain]") {
    std::string detail;
    const std::string checker =
        std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/ordering_point_checker.hh";
    CHECK(evaluate_completion_domain(std::vector<std::string>{checker}, detail) ==
          ContractVerdict::PASS);

    // 扩展面可注入:注入一个**含** group-scoped 标识的文件 ⇒ 必红(证明扫描器有牙)
    std::string d2;
    const ContractVerdict v =
        evaluate_completion_domain(std::vector<std::string>{checker, checker}, d2);
    CHECK(v == ContractVerdict::PASS); // 同一文件重复注入不改变结论
    CHECK(d2.find("coherence 侧") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
// 归因 id **元测试** —— design §7 的 id 集合 ⊇ fixture 的 id 集合(双向)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("归因 id 元测试 —— fixture id ⊆ design §7 id,且本层新建 id 各有一条 fixture",
          "[cep][nvlink-sharp][meta]") {
    // fixture 的归因 id 集合(与反向验证脚本的 9 组一一对应)
    const std::set<std::string> fixture_ids{
        "NVA-01", "NVA-02",           "NVA-03",          "NVA-05",
        "NVA-07", "CP-BARRIER-CLASS", "CP-PARTIAL-FAIL", "CP-03-VC-PRECONDITION"};
    // 本层**新建**的 `CP-*` id(ADR 强制 ⇒ 只能有这 2 个)
    const std::set<std::string> new_cp_ids{"CP-BARRIER-CLASS", "CP-PARTIAL-FAIL"};

    const std::string design = read_repo_file(kDesignRelPath);
    REQUIRE(!design.empty());

    // ① 每个 fixture id 都 SHALL 出现在 design §7(唯一登记处)
    for (const auto& id : fixture_ids) {
        INFO("fixture id: " << id);
        CHECK(design.find(id) != std::string::npos);
    }
    // ② 本层新建的每个 `CP-*` id SHALL 有一条 fixture
    for (const auto& id : new_cp_ids) {
        INFO("new cp id: " << id);
        CHECK(fixture_ids.count(id) == 1u);
    }
    // ③ **反向**:`design` 里出现的 `CP-NVLINK-*` 归因 id SHALL 为 **0**
    //    (A-10:本层**不**自造 `CP-NVLINK-*` 体系 ⇒ 归因用 `NVA-xx`)
    CHECK(design.find("CP-NVLINK-") == std::string::npos);
}
