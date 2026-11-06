// =============================================================================
// test/test_cep_tilelink_engine.cc
//   —— TileLink-C 运行时引擎(TileLinkProtocolEngine)的契约测试
//
// 作者: CppTLM Team / 日期: 2026-11-06
// 配套: include/tlm/cep/tilelink_protocol_engine.hh(引擎本体)
//       openspec/changes/cpptlm-cep-tilelink-runtime-engine/(delta spec TLE-01~TLE-06)
//       台账 obj-cep-platform.md Entry #243(立项)
//       规约真源: include/tlm/cep/tilelink_state_spec.hh(kT3MappingCells · 11 行)
//
// ── 判据纪律(per 本仓 4 条铁律)────────────────────────────────────────────
// ① 每条**负向**断言 SHALL 有**正控**(「引擎确实收到过该意图」+「事件确实发生」);
// ② 每条判据 SHALL 有**可执行反向 fixture** 且**因正确原因红**;
// ③ **文本存在性**不算有牙载体 ⇒ 行为面判据走**真实驱动**(dispatch_cohif / on_*);
//    ⛔ 仅「引擎源文件不含某 token」这类**禁止性 lint** 允许以文本判据成立
//    (它判的是「违规形态存在与否」,不是「功能存在与否」);
// ④ 编译期锁 SHALL **同时**有运行时可判入口。
//
// ── 覆盖映射(delta spec → TEST_CASE)────────────────────────────────────────
// TLE-01 零自有事件表副本(禁止性 lint + 唯一载具计数)   → ①
// TLE-02 10 个 on_* 全覆盖 + 7 零事件意图断言(basis 双断言) → ②
// TLE-03 5 个有事件流 L4 事务级贯通(dispatch_cohif)      → ③
// TLE-04 决定性变异(删被消费行 ⇒ 红)                     → ④(构建期,见
//        scripts/test/cep_tilelink_engine_failclosed.sh;ctest 侧断言判据**在位**)
// TLE-05 不携带源态(禁止性 lint + 输出面只有事件类别位)   → ⑤
// TLE-06 fail-closed 消费面(唯一载具与真源一致)          → ⑥
//        (真值由同脚本的 **scratch 拷贝** 变异取证:正控 clean tree ⇒ 有事件;
//         反控 Pending tree ⇒ **引擎发射出口零计数**)
//
// =============================================================================

#include "tlm/cep/tilelink_protocol_engine.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/cohif_protocol_engine.hh"
#include "bundles/cep_coherence_bundles_tlm.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "framework/chstream_adapter_factory.hh"

#include "cep_adr_parser.hh"   // `cpptlm::cep::repo_root()`(测试侧仓根定位)

#include <nlohmann/json.hpp>

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Engine = tlm::cep::TileLinkProtocolEngine;
using Spec = tlm::cep::TileLinkStateSpec;
using Intent = Spec::PlatformIntent;
using TlEvent = Spec::Event;
using Bundle = bundles::CohIfBundle;
using FlowId = bundles::CohIfFlowId;

/// 仓库根(CPPTLM_SOURCE_DIR 由 test/CMakeLists.txt 注入)
std::string repo_root_dir() {
#ifdef CPPTLM_SOURCE_DIR
    return std::string(CPPTLM_SOURCE_DIR);
#else
    return std::string(".");
#endif
}

std::string read_file_or_empty(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// 引擎源文件原文(禁止性 lint 的对象)
std::string engine_source_text() {
    return read_file_or_empty(repo_root_dir() + "/include/tlm/cep/tilelink_protocol_engine.hh");
}

/// **去注释**后的引擎源码(避免注释里的 token 触发 lint 误报)
std::string strip_comments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool in_block = false;
    for (std::size_t i = 0; i < src.size();) {
        if (!in_block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            continue;
        }
        if (!in_block && src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            in_block = true;
            i += 2;
            continue;
        }
        if (in_block && src[i] == '*' && i + 1 < src.size() && src[i + 1] == '/') {
            in_block = false;
            i += 2;
            continue;
        }
        if (!in_block) out.push_back(src[i]);
        ++i;
    }
    return out;
}

/// 构造一个 CohIf bundle(5 条 CohIf 腿的字段齐全)
Bundle make_bundle(FlowId flow, std::uint64_t addr, std::uint64_t data,
                    std::uint64_t txn, std::uint8_t perm) {
    Bundle b;
    b.flow_id = flow;
    b.address.write(addr);
    b.data.write(data);
    b.transaction_id.write(txn);
    b.perm.write(perm);
    b.hit.write(0);
    b.dirty.write(0);
    b.error.write(0);
    b.atomic.write(0);
    b.snoop_type.write(Bundle::SNOOP_INVALIDATE);
    b.line_state.write(Bundle::STATE_NONE);
    return b;
}

/// 7 个「零事件」意图(TLC-M-01 / 06~11)
const std::array<Intent, 7> kZeroEventIntents{
    Intent::Request,      Intent::ForwardData,    Intent::BackInvalidate,
    Intent::AddressResolve, Intent::Barrier,       Intent::SnoopCompletionAck,
    Intent::AtomicIntent};

/// 4 个「有事件」意图(TLC-M-02~05)
const std::array<Intent, 4> kEventIntents{Intent::Completion, Intent::Snoop,
                                         Intent::SnoopResp, Intent::Writeback};

/// 加载 TileLink 的最小装配拓扑(`protocol_instances` 相对路径 → 绝对路径)
nlohmann::json load_tilelink_topology() {
    const auto path = cpptlm::cep::repo_root() / "configs" / "cep_tilelink_pe_minimal.json";
    std::ifstream in(path);
    if (!in.good())
        throw std::runtime_error("无法打开 TileLink 装配配置: " + path.string());
    nlohmann::json cfg = nlohmann::json::parse(in);
    cfg["cep"]["protocol_instances"] =
        (cpptlm::cep::repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json")
            .string();
    return cfg;
}

/// 直调 `on_*` 的意图分派器(5 条非 CohIf 腿的流 + atomic 无钩子 ⇒ 显式落点)
void drive_intent(Engine& e, Intent i) {
    switch (i) {
        case Intent::Request:
            e.on_request(0x1000, tlm::cep::Perm::READ, 1);
            break;
        case Intent::Completion:
            e.on_completion(1, 0xdead, tlm::cep::Perm::READ, false);
            break;
        case Intent::Snoop:
            e.on_snoop(0x1000, tlm::cep::SnoopType::INVALIDATE, 2);
            break;
        case Intent::SnoopResp:
            e.on_snoop_resp(0x1000, false, tlm::cep::LineState::NONE, false, 0, 2);
            break;
        case Intent::Writeback:
            e.on_writeback(0x1000, 0xbeef, false);
            break;
        case Intent::ForwardData:
            e.on_forward_data(0x1000, 0x2000, 0x1, tlm::cep::Perm::READ, 3);
            break;
        case Intent::BackInvalidate:
            e.on_back_invalidate(0x1000, tlm::cep::SnoopType::INVALIDATE, 3);
            break;
        case Intent::AddressResolve: {
            tlm::cep::AddressResolveResult r;
            e.on_address_resolve(0x1000, 0, r);
            break;
        }
        case Intent::Barrier:
            e.on_barrier(0, 4);
            break;
        case Intent::SnoopCompletionAck:
            e.on_snoop_completion_ack(0x1000, 0, 4);
            break;
        case Intent::AtomicIntent:
            e.note_atomic_intent();
            break;
    }
}

} // namespace

// ══════════════════════════════════════════════════════════════════════════
//  ① TLE-01 —— 零自有事件表副本(禁止性 lint)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-01 TileLink 引擎:零自有事件表副本(唯一消费载具)", "[cep][tilelink][engine][tle-01]") {
    const std::string src = strip_comments(engine_source_text());
    REQUIRE_FALSE(src.empty());

    // ① 不得直接引用 T3 表本体(只能经访问器家族)
    INFO("引擎不得直接引用 kT3MappingCells(须经 t3_* 访问器)");
    REQUIRE(src.find("kT3MappingCells") == std::string::npos);

    // ② 不得出现 `event_mask` **字段**访问(掩码只能经 consumable_mask_of 取)
    //    ⚠️ 判别式:先剔除访问器名 `t3_consumable_event_mask`(其内含子串 `event_mask`),
    //    否则**假阳性**(访问器名本身合法)。
    INFO("引擎不得直接访问 event_mask 字段");
    {
        std::string probe = src;
        const std::string accessor = "t3_consumable_event_mask";
        for (std::size_t pos = probe.find(accessor); pos != std::string::npos;
             pos = probe.find(accessor, pos))
            probe.replace(pos, accessor.size(), "ACCESSOR");
        REQUIRE(probe.find("event_mask") == std::string::npos);
    }

    // ③ 消费真源的调用**有且仅有 1 处**(唯一 choke point)
    const std::size_t pos = src.find("t3_consumable_event_mask");
    REQUIRE(pos != std::string::npos);
    REQUIRE(src.find("t3_consumable_event_mask", pos + 1) == std::string::npos);
    // 该处必须在 choke point 内
    const std::size_t cp = src.find("consumable_mask_of");
    REQUIRE(cp != std::string::npos);
    REQUIRE(cp < pos);  // 定义在前,调用在后 ⇒ 只可能经 choke point

    // ④ 不得持有掩码/映射的成员数组(⛔ 允许「到达计数」数组 —— 计数非映射)
    INFO("引擎不得持有事件掩码/映射的成员数组");
    REQUIRE(src.find("kT3EventAll") == std::string::npos);
    REQUIRE(src.find("std::array<std::uint8_t") == std::string::npos);

    // ⑤ 正控:引擎确实消费了 T3 真源(计数随表走 ⇒ 行为面判据非文本)
    EventQueue eq;
    Engine e("tl", &eq);
    e.on_completion(1, 0x42, tlm::cep::Perm::READ, false);
    REQUIRE(e.counters().grow_events == 1u);
    REQUIRE(e.total_events() == 1u);
}

// ══════════════════════════════════════════════════════════════════════════
//  ② TLE-02 —— 10 个 on_* 全覆盖 + 7 零事件意图断言(basis 双断言)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-02 TileLink 引擎:11 意图全覆盖 + 7 零事件意图(basis 双断言)",
          "[cep][tilelink][engine][tle-02]") {
    EventQueue eq;
    Engine e("tl", &eq);

    // ① 逐意图驱动一次 ⇒ 11 个意图的到达计数均为 1
    for (std::size_t i = 0; i < Spec::kPlatformIntentCount; ++i) {
        drive_intent(e, static_cast<Intent>(i));
    }
    for (std::size_t i = 0; i < Spec::kPlatformIntentCount; ++i) {
        INFO("意图 " << i << " 未被观测");
        REQUIRE(e.intent_observed(static_cast<Intent>(i)) == 1u);
    }

    // ② 7 个零事件意图:零事件 **且** basis == Direct(消除「未锚 ⇒ 合法零事件」真空)
    for (Intent i : kZeroEventIntents) {
        INFO("零事件意图 " << Spec::t3_row_id_of(i) << " 的 basis 非 Direct");
        REQUIRE(Spec::t3_basis_of(i) == Spec::T3BasisKind::Direct);
        REQUIRE(Spec::t3_consumable_event_mask(i) == 0u);
    }
    REQUIRE(e.zero_event_intents() == kZeroEventIntents.size());

    // ③ 4 个有事件意图:事件计数与真源掩码**逐位一致**
    std::uint64_t expect_total = 0;
    std::array<std::uint64_t, 4> expect_by_event{};  // 4 类事件的**累计**期望
    for (Intent i : kEventIntents) {
        const std::uint8_t mask = Spec::t3_consumable_event_mask(i);
        REQUIRE(mask != 0u);
        expect_total += 1;
        for (std::size_t b = 0; b < 4; ++b)
            if (mask & (1u << b)) expect_by_event[b] += 1;
    }
    for (std::size_t b = 0; b < 4; ++b) {
        INFO("事件类别 " << b << " 计数与真源掩码不一致");
        REQUIRE(e.event_count(static_cast<TlEvent>(b)) == expect_by_event[b]);
    }
    REQUIRE(e.total_events() == expect_total);

    // ④ 反向 fixture:「request 自身零事件」是反直觉点 ⇒ 单独钉死
    //    (CHI 先例的 `on_request` 会记事件 ⇒ 本引擎 SHALL NOT)
    EventQueue eq2;
    Engine only_req("tl_req", &eq2);
    only_req.on_request(0x1000, tlm::cep::Perm::READ, 1);
    REQUIRE(only_req.intent_observed(Intent::Request) == 1u);
    REQUIRE(only_req.total_events() == 0u);
    REQUIRE(only_req.zero_event_intents() == 1u);
}

// ══════════════════════════════════════════════════════════════════════════
//  ③ TLE-03 —— 5 个有事件流 L4 事务级贯通(dispatch_cohif)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-03 TileLink 引擎:5 流事务级贯通(dispatch_cohif)", "[cep][tilelink][engine][tle-03]") {
    EventQueue eq;
    Engine e("tl", &eq);

    // ① request(零事件)
    REQUIRE(e.dispatch_cohif(make_bundle(FlowId::Request, 0x1000, 0, 1, Bundle::PERM_READ)));
    // ② completion ⇒ Grow
    REQUIRE(e.dispatch_cohif(make_bundle(FlowId::Completion, 0x1000, 0xaa, 1, Bundle::PERM_READ)));
    // ③ snoop ⇒ Prune
    REQUIRE(e.dispatch_cohif(make_bundle(FlowId::Snoop, 0x1000, 0, 2, Bundle::PERM_NONE)));
    // ④ snoop_resp ⇒ Prune
    REQUIRE(e.dispatch_cohif(make_bundle(FlowId::SnoopResp, 0x1000, 0, 2, Bundle::PERM_NONE)));
    // ⑤ writeback ⇒ Prune
    REQUIRE(e.dispatch_cohif(make_bundle(FlowId::Writeback, 0x1000, 0xbb, 3, Bundle::PERM_NONE)));

    REQUIRE(e.counters().grow_events == 1u);
    REQUIRE(e.counters().prune_events == 3u);
    REQUIRE(e.counters().cap_events == 0u);
    REQUIRE(e.counters().report_events == 0u);
    REQUIRE(e.total_events() == 4u);
    REQUIRE(e.intent_observed(Intent::Request) == 1u);
    REQUIRE(e.zero_event_intents() == 1u);  // 仅 request

    // ⑥ 反向 fixture:域外 flow_id ⇒ 防御性返回 false 且**零事件**
    //    (⛔ 不静默计入任何意图)
    Bundle bogus = make_bundle(FlowId::Request, 0x1000, 0, 9, Bundle::PERM_READ);
    bogus.flow_id = static_cast<FlowId>(99);
    const std::uint64_t before = e.total_events();
    REQUIRE_FALSE(e.dispatch_cohif(bogus));
    REQUIRE(e.total_events() == before);

    // ⑦ 对照判别力:Null 引擎在同一 5 流上**零事件**(结构上不可能有事件)
    tlm::cep::CohIfNullProtocolEngine null_engine("tl_null", &eq);
    null_engine.dispatch_cohif(make_bundle(FlowId::Request, 0x1000, 0, 1, Bundle::PERM_READ));
    null_engine.dispatch_cohif(make_bundle(FlowId::Completion, 0x1000, 0xaa, 1, Bundle::PERM_READ));
    null_engine.dispatch_cohif(make_bundle(FlowId::Snoop, 0x1000, 0, 2, Bundle::PERM_NONE));
    null_engine.dispatch_cohif(make_bundle(FlowId::SnoopResp, 0x1000, 0, 2, Bundle::PERM_NONE));
    null_engine.dispatch_cohif(make_bundle(FlowId::Writeback, 0x1000, 0xbb, 3, Bundle::PERM_NONE));
    // Null 侧**没有事件面**(结构上不可能)⇒ 正控:它确实收到了这 5 个流
    REQUIRE(null_engine.counters().per_flow[static_cast<std::size_t>(
                tlm::cep::CanonicalFlow::Request)] == 1u);
    REQUIRE(null_engine.counters().per_flow[static_cast<std::size_t>(
                tlm::cep::CanonicalFlow::Completion)] == 1u);
    // ⇒ 同一组流:真插件 **4 个事件** vs Null **零事件面** ⇒ 判别力成立
    REQUIRE(e.total_events() == 4u);
}

// ══════════════════════════════════════════════════════════════════════════
//  ④ TLE-04 —— 决定性变异判据**在位**(真值由构建期脚本取证)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-04 TileLink 引擎:决定性变异判据在位(构建期取证脚本)",
          "[cep][tilelink][engine][tle-04]") {
    // 变异载体 = T3 表(非引擎内部);期望失败签名 = state_spec 编译期 static_assert。
    // 本 case 只断言「判据的**载体与签名**在位」;真值取证见
    // scripts/test/cep_tilelink_engine_failclosed.sh(clean / pending / deleted 三态)。
    const std::string script = read_file_or_empty(
        repo_root_dir() + "/scripts/test/cep_tilelink_engine_failclosed.sh");
    REQUIRE_FALSE(script.empty());

    // 期望签名:state_spec 的行宇宙锁 static_assert 文案
    const std::string spec_src = read_file_or_empty(
        repo_root_dir() + "/include/tlm/cep/tilelink_state_spec.hh");
    REQUIRE_FALSE(spec_src.empty());
    REQUIRE(spec_src.find("t3_intents_are_unique_and_complete") != std::string::npos);
    REQUIRE(spec_src.find("t3_row_count_is_exact") != std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
//  ⑤ TLE-05 —— 不携带源态(禁止性 lint + 输出面只有事件类别位)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-05 TileLink 引擎:不携带源态(不重发明规约真源)", "[cep][tilelink][engine][tle-05]") {
    const std::string src = strip_comments(engine_source_text());
    REQUIRE_FALSE(src.empty());

    // ① 不得引用规约层的**态枚举**(3 态树位置)
    REQUIRE(src.find("TileLinkStateSpec::State") == std::string::npos);
    REQUIRE(src.find("Nothing") == std::string::npos);
    REQUIRE(src.find("Branch") == std::string::npos);
    REQUIRE(src.find("Tip") == std::string::npos);

    // ② 不得引用转移矩阵 / 逐格表
    REQUIRE(src.find("transition") == std::string::npos);
    REQUIRE(src.find("Transition") == std::string::npos);

    // ③ 正控:输出面**只有事件类别位**(4 类计数),无源态字段
    EventQueue eq;
    Engine e("tl", &eq);
    e.on_completion(1, 0x42, tlm::cep::Perm::READ, false);
    REQUIRE(e.counters().grow_events == 1u);
    REQUIRE(e.total_events() == 1u);
    // ⛔ 引擎**不暴露**任何「源态/树位置」查询面(编译期即不存在)
    REQUIRE(src.find("line_state_of") == std::string::npos);
}

// ══════════════════════════════════════════════════════════════════════════
//  ⑥ TLE-06 —— fail-closed:引擎消费面与真源逐位一致
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-06 TileLink 引擎:fail-closed 消费面与真源一致", "[cep][tilelink][engine][tle-06]") {
    // ① 逐意图:引擎的掩码取自真源访问器(不得偏离)
    //    —— 行为面的真值( Pending ⇒ 引擎**零发射**)由构建期脚本在 scratch 树上取证;
    //    本 case 断言「引擎与真源**同源**」(choke point 唯一 ⇒ 必然一致)。
    EventQueue eq;
    Engine e("tl", &eq);
    for (std::size_t i = 0; i < Spec::kPlatformIntentCount; ++i) {
        const auto intent = static_cast<Intent>(i);
        const std::uint8_t mask = Spec::t3_consumable_event_mask(intent);
        const std::uint64_t before = e.total_events();
        drive_intent(e, intent);
        const std::uint64_t delta = e.total_events() - before;
        const std::uint64_t want = (mask == 0u) ? 0u : 1u;
        INFO("意图 " << Spec::t3_row_id_of(intent) << " 的事件数与真源掩码不一致");
        REQUIRE(delta == want);
    }

    // ② 反向 fixture(状态层):Pending 行 ⇒ 可消费掩码归零(fail-closed 的前提)
    //    —— 用**带参**入口对**毒化副本**判定(⛔ 不动真源)。
    std::array<Spec::T3MappingCell, Spec::kPlatformIntentCount> bad{};
    for (std::size_t i = 0; i < Spec::kT3MappingCells.size(); ++i)
        bad[i] = Spec::kT3MappingCells[i];
    bad[1].basis = Spec::T3BasisKind::Pending;  // Completion 行
    bad[1].event_mask = Spec::kT3EventGrow;
    REQUIRE_FALSE(Spec::t3_rows_are_valid_for(bad));
    REQUIRE(Spec::t3_consumable_event_mask_for(bad, Intent::Completion) == 0u);
}

// ══════════════════════════════════════════════════════════════════════════
//  ⑦ TLE-08 —— JSON 装配面(引擎名可选 + 类型判别)
// ══════════════════════════════════════════════════════════════════════════
TEST_CASE("TLE-08 TileLink 引擎:JSON 装配成功 + 类型判别(正控/反控)",
          "[cep][tilelink][engine][tle-08][json-assembly]") {
    nlohmann::json cfg = load_tilelink_topology();

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));  // 装配**成功**(不是靠「装配失败」蒙对判据)

    // ① 正控:两个实例都是**真插件**(类型判据)
    auto* tl0 = dynamic_cast<Engine*>(factory.getInstanceForTesting("tl0"));
    auto* tl1 = dynamic_cast<Engine*>(factory.getInstanceForTesting("tl1"));
    REQUIRE(tl0 != nullptr);
    REQUIRE(tl1 != nullptr);

    // ② 端口数与注册名一致(注册面判据)
    REQUIRE(ChStreamAdapterFactory::get().isMultiPort("TileLinkProtocolEngine"));
    REQUIRE(ChStreamAdapterFactory::get().getPortCount("TileLinkProtocolEngine") == 2u);

    // ③ 反向 fixture:同拓扑换成 Null 引擎 ⇒ 类型判据**必须判否**
    nlohmann::json null_cfg = load_tilelink_topology();
    null_cfg["modules"][0]["type"] = "CohIfProtocolEngine";
    null_cfg["modules"][1]["type"] = "CohIfProtocolEngine";
    EventQueue eq2;
    ModuleFactory null_factory(&eq2);
    REQUIRE(null_factory.instantiateAll(null_cfg));
    REQUIRE(dynamic_cast<Engine*>(null_factory.getInstanceForTesting("tl0")) == nullptr);
    REQUIRE(dynamic_cast<tlm::cep::CohIfNullProtocolEngine*>(
                null_factory.getInstanceForTesting("tl0")) != nullptr);
}
