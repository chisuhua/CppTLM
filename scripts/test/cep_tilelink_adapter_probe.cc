// =============================================================================
// scripts/test/cep_tilelink_adapter_probe.cc
//   —— 队列 #11 `cpptlm-cep-tilelink-tree-manager` 的**反向验证探针**
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: scripts/test/cep_tilelink_adapter_reverse_verify.sh(驱动本探针)
//       openspec/changes/cpptlm-cep-tilelink-tree-manager/(delta spec TMA-01 ~ TMA-10)
//
// ── 用途 ──────────────────────────────────────────────────────────────────
// 本探针在**影子目录**(被毒化的源文件副本)下编译并运行 ⇒ 验证判据的**鉴别力**:
//   · **干净载体 ⇒ 必须通过**(正控)
//   · **毒化载体 ⇒ 必须失败**(且失败原因须命中期望的判据 id)
//
// 用法:probe <仓根目录>     (退出码 0 = 全部谓词成立;1 = 有谓词不成立)
// =============================================================================

#include "tlm/cep/tilelink_c_adapter.hh"
#include "tlm/cep/tilelink_tree_manager.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Adapter = tlm::cep::TileLinkCAdapter;
using TreeMgr = tlm::cep::TreeManager;
using Spec = tlm::cep::TileLinkStateSpec;
using State = Spec::State;
using Event = Spec::Event;
using tlm::cep::Intent;
using tlm::cep::LookupHazard;
using tlm::cep::LookupResult;
using tlm::cep::LocalView;
using tlm::cep::node_id_t;

int g_fail = 0;

void expect(bool cond, const std::string& id, const std::string& what) {
    if (!cond) {
        std::cout << "PROBE:FAIL " << id << " —— " << what << "\n";
        ++g_fail;
    }
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// 去注释(行 + 块)
std::string strip_comments(std::string code) {
    std::string out;
    for (std::size_t i = 0; i < code.size();) {
        if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            while (i < code.size() && code[i] != '\n') ++i;
            continue;
        }
        if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            i += 2;
            while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) ++i;
            i = (i + 1 < code.size()) ? i + 2 : code.size();
            out.push_back(' ');
            continue;
        }
        out.push_back(code[i]);
        ++i;
    }
    return out;
}

/// P1 —— adapter 零自有表副本(静态扫描)
void p1_zero_copy(const std::string& root) {
    const std::string code =
        strip_comments(read_file(root + "/include/tlm/cep/tilelink_c_adapter.hh"));
    if (code.empty()) {
        expect(false, "impl:TMA-01-ZERO-COPY", "adapter 源不可读");
        return;
    }
    for (const char* tok : {"State::", "Event::", "Perm::", "kTable", "kTransitions", "kPermProjection"}) {
        expect(code.find(tok) == std::string::npos, "impl:TMA-01-ZERO-COPY",
               std::string("adapter 含违禁记号 ") + tok);
    }
}

/// P2 —— 6→3 映射:4 行 · 非恒等 · 理由必填
void p2_mapping() {
    expect(Adapter::kMessageClassCount == 4u, "impl:TMA-03-MAP6TO3", "6→3 映射行数 ≠ 4");
    expect(!Adapter::kMappingIsIdentity, "impl:TMA-03-MAP6TO3", "恒等标志应为 false");
    expect(std::string(Adapter::kIdentityReason).size() > 0u, "impl:TMA-03-MAP6TO3", "恒等/非恒等理由为空");
    std::uint8_t cat = 0xFF;
    expect(Adapter::category_of("request", cat) && cat == 0u, "impl:TMA-03-MAP6TO3", "request → REQUEST 失效");
    expect(Adapter::category_of("snoop_resp", cat) && cat == 1u, "impl:TMA-03-MAP6TO3", "snoop_resp → RESPONSE 失效");
    expect(Adapter::category_of("response", cat) && cat == 1u, "impl:TMA-03-MAP6TO3", "response → RESPONSE 失效");
    expect(!Adapter::category_of("writeback", cat), "impl:TMA-03-MAP6TO3", "writeback 被误收为消息类");
}

/// P3 —— 逐消息 → 6 类:10 行 · 逐条 R-T3
void p3_protocol_messages() {
    expect(Adapter::kProtocolMessageCount == 10u, "impl:TMA-03-PROTOMSG", "逐消息表行数 ≠ 10");
    struct E { const char* m; const char* c; };
    const E kExp[] = {
        {"Acquire", "request"}, {"Probe", "snoop"},
        {"ProbeAck", "snoop_resp"}, {"ProbeAckData", "snoop_resp"},
        {"Release", "snoop_resp"}, {"ReleaseData", "snoop_resp"},
        {"Grant", "response"}, {"GrantData", "response"},
        {"ReleaseAck", "response"}, {"GrantAck", "response"},
    };
    for (const auto& e : kExp) {
        const char* cls = nullptr;
        const bool ok = Adapter::message_class_of(e.m, cls);
        expect(ok && cls && std::string(cls) == std::string(e.c), "impl:TMA-03-PROTOMSG",
               std::string("逐消息归属错误: ") + e.m);
    }
}

/// P4 —— `TreeManager` 的 caps/behavior 逐字段等于 `kTrackerTraits[3]`
void p4_tracker_caps() {
    const auto& t = tlm::cep::kTrackerTraits[3];
    TreeMgr tm(tlm::cep::kNodeIdPcie);
    expect(tm.caps.scope == t.caps.scope, "impl:TMA-04-CAPS", "scope 漂移");
    expect(tm.caps.completeness == t.caps.completeness, "impl:TMA-04-CAPS", "completeness 漂移");
    expect(tm.caps.authority == t.caps.authority, "impl:TMA-04-CAPS", "authority 漂移");
    expect(tm.caps.aggregation_at == t.caps.aggregation_at, "impl:TMA-04-CAPS", "aggregation_at 漂移");
    expect(tm.behavior.hazard_under_contention == t.behavior.hazard_under_contention, "impl:TMA-04-CAPS", "hazard 漂移");
    expect(tm.behavior.broadcast_fallback == t.behavior.broadcast_fallback, "impl:TMA-04-CAPS", "broadcast_fallback 漂移");
}

/// P5 —— `ESCALATE_UP` / `CHAIN` 真实使用
void p5_escalate_and_chain() {
    const node_id_t kParent = tlm::cep::kNodeIdPcie;
    const node_id_t kSelf = tlm::cep::kNodeIdSdma;
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x100, kSelf, State::Nothing);
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard h = LookupHazard::NONE;
        tm.lookup(0x100, Intent::want_read, r, v, h);
        expect(r == LookupResult::ESCALATE_UP, "impl:TMA-05-ESCALATE", "无副本时未返 ESCALATE_UP");
        expect(v.forward_to == kParent, "impl:TMA-05-ESCALATE", "forward_to 未指向树父");
    }
    {
        TreeMgr tm(kParent);
        tm.set_sharer(0x200, kSelf, State::Tip);
        tlm::cep::snoop_handle_t h = 0;
        tm.snoop_begin(0x200, tlm::cep::responder_mask_of(kSelf), h);
        expect(h != 0, "impl:TMA-05-CHAIN", "snoop_begin 未成功");
        LookupResult r = LookupResult::ANSWER_HERE;
        LocalView v;
        LookupHazard hz = LookupHazard::NONE;
        tm.lookup(0x200, Intent::want_read, r, v, hz);
        expect(hz == LookupHazard::CHAIN, "impl:TMA-05-CHAIN", "contention 未返 CHAIN");
    }
}

/// P6 —— `snoop_join_core` 的 4 步语义
void p6_join_core() {
    const node_id_t kA = tlm::cep::kNodeIdSdma;
    const node_id_t kB = tlm::cep::kNodeIdGmmu;
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0xFFFF;
        m.snoop_join_core(0x1, tlm::cep::responder_mask_of(kA), h);
        expect(h == 0, "impl:TMA-06-JOIN", "无 in-flight 时应返 0");
    }
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0;
        m.snoop_begin_core(0x2, tlm::cep::responder_mask_of(kA), h);
        expect(h != 0, "impl:TMA-06-JOIN", "snoop_begin 未成功");
        tlm::cep::snoop_handle_t h2 = 0xFFFF;
        m.snoop_join_core(0x2, tlm::cep::responder_mask_of(kA) | (tlm::cep::responder_set_t{1} << 40), h2);
        expect(h2 == 0, "impl:TMA-06-JOIN", "未映射位应被拒");
    }
    {
        tlm::cep::SnoopMechanics m;
        tlm::cep::snoop_handle_t h = 0;
        m.snoop_begin_core(0x3, tlm::cep::responder_mask_of(kA), h);
        expect(h != 0, "impl:TMA-06-JOIN", "snoop_begin 未成功");
        tlm::cep::snoop_handle_t h2 = 0;
        m.snoop_join_core(0x3, tlm::cep::responder_mask_of(kA), h2);
        expect(h2 == h, "impl:TMA-06-JOIN", "幂等 join 未返既有 handle");
        tlm::cep::snoop_handle_t h3 = 0;
        m.snoop_join_core(0x3, tlm::cep::responder_mask_of(kB), h3);
        expect(h3 == h, "impl:TMA-06-JOIN", "加入未并入既有 in-flight");
        tlm::cep::responder_set_t remaining = 0;
        tlm::cep::MergedState merged{};
        m.snoop_fold_core(h, kA, tlm::cep::SnoopState::I, false, false, remaining, merged);
        expect((remaining & tlm::cep::responder_mask_of(kB)) != 0, "impl:TMA-06-JOIN",
               "加入位未并入 remaining ⇒ 归零逻辑被破");
    }
}

/// P7 —— 实例声明:13 键 · perm 3 值 · vc_binding 逐类 · 不含 writeback
void p7_instance_decl(const std::string& root) {
    const std::string text =
        read_file(root + "/include/tlm/cep/protocol_instances.json");
    if (text.empty()) {
        expect(false, "decl:TMA-08-INSTANCE", "实例声明源不可读");
        return;
    }
    nlohmann::json doc = nlohmann::json::parse(text);
    const nlohmann::json* inst = nullptr;
    for (const auto& i : doc.at("instances")) {
        if (i.value("id", "") == "tilelink_c") inst = &i;
    }
    expect(inst != nullptr, "decl:TMA-08-INSTANCE", "缺 tilelink_c 实例");
    if (!inst) return;
    const auto& perm = inst->at("perm");
    expect(perm.size() == 3u, "decl:TMA-08-INSTANCE", "perm 行数 ≠ 3");
    bool has_unique = false;
    for (const auto& p : perm) {
        if (p.get<std::string>() == "UNIQUE") has_unique = true;
    }
    expect(!has_unique, "decl:TMA-08-INSTANCE", "perm 含 UNIQUE ⇒ 语义为假的绿灯");
    const auto& vc = inst->at("vc_binding").at("compute_noc");
    expect(vc.value("request", -1) == 0, "decl:TMA-08-INSTANCE", "vc_binding.request ≠ 0");
    expect(vc.value("snoop", -1) == 1, "decl:TMA-08-INSTANCE", "vc_binding.snoop ≠ 1");
    expect(vc.value("snoop_resp", -1) == 2, "decl:TMA-08-INSTANCE", "vc_binding.snoop_resp ≠ 2");
    expect(vc.value("response", -1) == 3, "decl:TMA-08-INSTANCE", "vc_binding.response ≠ 3");
    expect(!vc.contains("writeback"), "decl:TMA-08-INSTANCE", "vc_binding 含 writeback(流名非类名)");
    expect(inst->at("sharer_tracker").value("impl", "") == "TreeManager", "decl:TMA-08-INSTANCE",
           "sharer_tracker.impl ≠ TreeManager");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "用法:probe <仓根目录>\n";
        return 2;
    }
    const std::string root = argv[1];
    p1_zero_copy(root);
    p2_mapping();
    p3_protocol_messages();
    p4_tracker_caps();
    p5_escalate_and_chain();
    p6_join_core();
    p7_instance_decl(root);
    if (g_fail > 0) {
        std::cout << "PROBE:FAIL 共 " << g_fail << " 项谓词不成立\n";
        return 1;
    }
    std::cout << "PROBE:OK 全部谓词成立\n";
    return 0;
}
