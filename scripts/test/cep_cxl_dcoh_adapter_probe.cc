// =============================================================================
// scripts/test/cep_cxl_dcoh_adapter_probe.cc
//   —— 队列 #12 `cpptlm-cep-cxl-dcoh-symmetric` 的**反向验证探针**
//
// 作者: CppTLM Team / 日期: 2026-11-02
// 配套: scripts/test/cep_cxl_dcoh_adapter_reverse_verify.sh(驱动本探针)
//       openspec/changes/cpptlm-cep-cxl-dcoh-symmetric/(delta spec CDX-01 ~ CDX-08)
//
// ── 用途 ────────────────────────────────────────────────────────────────────
// 本探针在**影子目录**(被毒化的源文件副本)下编译并运行 ⇒ 验证判据的**鉴别力**:
//   · **干净载体 ⇒ 必须通过**(正控)
//   · **毒化载体 ⇒ 必须失败**(且失败原因须命中期望的判据 id)
//
// ⚠️ **形态 = 「毒化 ⇒ 判据必红」**(鉴别力)⇒ 与队列 #11 修**误报**的那一组
//    (「毒化 ⇒ 判据仍绿」= 回归防护)**形态相反** ⇒ **两组都须登记**,避免后人混用。
//
// 用法:probe <仓根目录>     (退出码 0 = 全部谓词成立;1 = 有谓词不成立)
// =============================================================================

#include "tlm/cep/cxl_dcoh_adapter.hh"
#include "tlm/cep/cxl_dcoh_tracker.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Adapter = tlm::cep::CxlDcohAdapter;
using Tracker = tlm::cep::DcohSymmetric;
using Intent = tlm::cep::Intent;
using LookupHazard = tlm::cep::LookupHazard;
using LookupResult = tlm::cep::LookupResult;
using LocalView = tlm::cep::LocalView;
using node_id_t = tlm::cep::node_id_t;

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
    std::size_t i = 0;
    while (i < code.size()) {
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

/// CDX-01 的**零副本扫描器**(与 `test/test_cep_cxl_dcoh_adapter.cc` 同款判据 ⇒ 禁双实现)
std::vector<std::string> scan_zero_copy_violations(const std::string& code) {
    static const char* kMemberLiterals[] = {
        "DevCacheState::", "DcohViewState::", "HomeDirState::", "CxlMemChannel::",
        "CxlCacheChannel::", "ChannelDirection::", "Perm::"};
    static const char* kTableNames[] = {
        "kPermProjection", "kDeviceInitiatedSnoopSet", "kCxlMemChannelDirection",
        "kBasisDevCacheState", "kBasisDcohViewState", "kBasisHomeDirState",
        "kBasisCxlMemChannel", "kBasisCxlCacheChannel", "kBasisPermProjection",
        "kBasisSymmetricPredicate", "kPendingSemanticItems"};
    const std::string src = strip_comments(code);
    std::vector<std::string> hits;
    for (const char* w : kMemberLiterals) {
        if (src.find(w) != std::string::npos) hits.push_back(w);
    }
    for (const char* w : kTableNames) {
        if (src.find(w) != std::string::npos) hits.push_back(w);
    }
    return hits;
}

/// CDX-03 的接口面扫描:adapter **不得**暴露 `transition_*`
bool adapter_exposes_transition(const std::string& code) {
    const std::string src = strip_comments(code);
    return src.find("transition_") != std::string::npos;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "PROBE:FAIL usage —— 缺仓根目录参数\n";
        return 1;
    }
    const std::string root = argv[1];

    // ══ CDX-01 —— adapter 零自有表副本 ══════════════════════════════════════
    {
        const std::string code = read_file(root + "/include/tlm/cep/cxl_dcoh_adapter.hh");
        if (code.empty()) {
            std::cout << "PROBE:FAIL CDX-01-ADAPTER-UNREADABLE —— 读不到 adapter 源\n";
            ++g_fail;
        } else {
            const auto hits = scan_zero_copy_violations(code);
            expect(hits.empty(), "CDX-01-ZEROCOPY",
                   "adapter 含自有表副本/成员字面量:" + (hits.empty() ? std::string("?") : hits.front()));
        }
    }

    // ══ CDX-03 —— adapter 不暴露 transition API ═════════════════════════════
    {
        const std::string code = read_file(root + "/include/tlm/cep/cxl_dcoh_adapter.hh");
        expect(!adapter_exposes_transition(code), "CDX-03-NO-TRANSITION",
               "adapter 暴露了 transition_*(等于自造转移表)");
    }

    // ══ CDX-04 —— tracker 行为逐字段符合 traits ════════════════════════════
    {
        Tracker t({0x1000, 0x100000});
        expect(t.caps.scope == tlm::cep::Scope::LOCAL_MEMORY_REGION, "CDX-04-CAPS-SCOPE",
               "scope 不符 traits");
        expect(t.caps.completeness == tlm::cep::Completeness::CONSERVATIVE, "CDX-04-CAPS-COMPLETE",
               "completeness 不符 traits");
        expect(t.caps.authority == tlm::cep::Authority::LOCAL_HINT, "CDX-04-CAPS-AUTH",
               "authority 不符 traits");
        expect(t.caps.aggregation_at == tlm::cep::AggregationAt::HOME, "CDX-04-CAPS-AGG",
               "aggregation_at 不符 traits");
        expect(t.behavior.hazard_under_contention == LookupHazard::STALL, "CDX-04-HAZARD",
               "hazard 不符 traits");
        expect(t.behavior.broadcast_fallback, "CDX-04-BROADCAST-FALLBACK",
               "broadcast_fallback 不符 traits");

        // 行为:区域外 ⇒ BROADCAST(不可判定的「区域」⇒ 不得给本地结论)
        LookupResult res = LookupResult::ANSWER_HERE;
        LocalView view{};
        LookupHazard hz = LookupHazard::CHAIN;
        t.lookup(0x90000000, Intent::want_read, res, view, hz);
        expect(res == LookupResult::BROADCAST, "CDX-04-SCOPE-GATE",
               "区域外地址未被拒(应 BROADCAST)");
        expect(hz == LookupHazard::NONE, "CDX-04-SCOPE-GATE-HZ",
               "区域外地址 hazard 不应是 NONE 以外的值");
    }

    // ══ CDX-06 / CDX-07 —— 声明面(读影子目录的 JSON)══════════════════════
    {
        const std::string inst_txt =
            read_file(root + "/include/tlm/cep/protocol_instances.json");
        const std::string up_txt = read_file(root + "/include/tlm/cep/perm_upgrades.json");
        if (inst_txt.empty() || up_txt.empty()) {
            std::cout << "PROBE:FAIL DECL-UNREADABLE —— 读不到声明源\n";
            ++g_fail;
        } else {
            nlohmann::json inst_doc = nlohmann::json::parse(inst_txt);
            nlohmann::json up_doc = nlohmann::json::parse(up_txt);
            const nlohmann::json* inst = nullptr;
            for (const auto& i : inst_doc.at("instances")) {
                if (i.value("id", "") == "cxl_3_0_symmetric") {
                    inst = &i;
                    break;
                }
            }
            expect(inst != nullptr, "CDX-06-INST-EXISTS", "实例声明缺失");
            if (inst != nullptr) {
                expect(inst->at("perm").size() == 4u, "CDX-06-PERM-COUNT",
                       "perm 值数不为 4:" + std::to_string(inst->at("perm").size()));
                expect(inst->at("flow_support").at("back_invalidate").get<bool>(),
                       "CDX-06-BACK-INVALIDATE", "back_invalidate 非 true");
                expect(inst->at("sharer_tracker").at("impl").get<std::string>() == "DcohSymmetric",
                       "CDX-06-TRACKER-IMPL", "sharer_tracker.impl 非 DcohSymmetric");
                expect(inst->at("sharer_tracker").at("caps").at("aggregation_at").get<std::string>() ==
                           "HOME",
                       "CDX-06-AGG-HOME", "aggregation_at 非 HOME");
            }
            // CDX-07:本族**不得**出现在静默升级声明源里
            bool declared = false;
            for (const auto& u : up_doc.at("perm_upgrade").at("silent_upgrades")) {
                if (u.value("instance_id", "") == "cxl_3_0_symmetric") declared = true;
            }
            expect(!declared, "CDX-07-NO-UPGRADE-DECL", "本族出现在静默升级声明源里");
        }
    }

    if (g_fail == 0) {
        std::cout << "PROBE:OK —— 全部谓词成立\n";
        return 0;
    }
    std::cout << "PROBE:FAIL-COUNT " << g_fail << "\n";
    return 1;
}
