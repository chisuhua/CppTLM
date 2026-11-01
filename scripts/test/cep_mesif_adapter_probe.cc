// scripts/test/cep_mesif_adapter_probe.cc
//   —— MESIF 实现层判据的**探针**(反向验证用)
//
// 功能:把 `cpptlm-cep-mesif-adapter` 的判据(MA-01 ~ MA-05 / MA-08 / HD-01 ~ HD-05)
//       以**运行期谓词**跑一遍,失败时**逐条打印判据 id 与所属层**。
// 作者: CppTLM Team / 日期: 2026-10-30
//
// ── 为什么需要这个探针(与 `test_cep_mesif_*` 的分工)──────────────────
// ctest 里的 Catch2 用例给出**通过/失败**;但反向验证要回答的是**另一个问题**:
// 「毒化之后,红的是不是**我以为的那一条**判据?」
// ⇒ 本探针把每条判据的失败输出成 `FAIL [<layer>] <criterion-id>`,
//    脚本据此 grep **期望 id**,而不是只断言「整体红了」。
//
// ── 两层 id 的分工(归因不模糊)────────────────────────────────────────
//   · `[spec]` —— 规约载体(`mesif_state_spec.hh`)自身的自洽性;
//   · `[impl]` —— 实现载体(adapter / tracker)对规约的**消费**是否正确;
//   · `[decl]` —— 声明源(`protocol_instances.json` / `perm_upgrades.json`)与 fabric 的约束。
// ⚠️ 本仓的规约载体带**编译期锁**(14 条 `static_assert`)⇒ 毒化规约载体**通常**在编译期
//    就红,`[impl]` 层根本不会运行。这是**层序**而不是漏判:脚本对 `compile-lock` 型
//    fixture 断言的是**编译报错里的 static_assert 文案**,并显式记录「impl 层未运行」。
//
// 用法:cep_mesif_adapter_probe <repo_root> [cep_json_dir] [adapter_src]
//   退出码 0 = 全部判据通过;1 = 至少一条失败(失败明细在 stdout)。
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/adapter_schema_contracts.hh"
#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/mesif_state_spec.hh"
#include "tlm/cep/sharer_tracker.hh"

namespace {

    namespace cep = tlm::cep;
    using json = nlohmann::json;
    using MesifSpec = cep::MesifStateSpec;
    using Adapter = cep::MesifAdapter;

    int g_failures = 0;
    int g_checks = 0;

    void check(bool ok, const char* layer, const char* id, const std::string& detail = "") {
        ++g_checks;
        if (ok)
            return;
        ++g_failures;
        std::printf("FAIL [%s] %s%s%s\n", layer, id, detail.empty() ? "" : " — ",
                    detail.c_str());
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        if (!in.good())
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去掉 `//` 与 `/* */` 注释(静态扫描的前提)
    std::string strip_comments(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool line_comment = false, block_comment = false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            const char n = (i + 1 < s.size()) ? s[i + 1] : '\0';
            if (line_comment) {
                if (c == '\n') {
                    line_comment = false;
                    out.push_back(c);
                }
                continue;
            }
            if (block_comment) {
                if (c == '*' && n == '/') {
                    block_comment = false;
                    ++i;
                }
                continue;
            }
            if (c == '/' && n == '/') {
                line_comment = true;
                ++i;
                continue;
            }
            if (c == '/' && n == '*') {
                block_comment = true;
                ++i;
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <repo_root> [cep_json_dir]\n", argv[0]);
        return 2;
    }
    const std::string root = argv[1];
    const std::string cep_dir = (argc >= 3) ? argv[2] : (root + "/include/tlm/cep");
    // ⚠️ 静态扫描必须读**被毒化的那一份**载体(影子副本) —— 编译用的是影子头,
    //    但 `slurp()` 读的是磁盘路径 ⇒ 不给覆盖参数时,毒化对扫描**不可见**(假绿)。
    const std::string adapter_src =
        (argc >= 4) ? argv[3] : (root + "/include/tlm/cep/mesif_adapter.hh");

    // ══════════════════════════════════════════════════════════════════
    //  [spec] 规约载体自身的自洽性
    // ══════════════════════════════════════════════════════════════════
    check(MesifSpec::table_is_complete(MesifSpec::kTable), "spec", "MS-SPEC-TABLE");
    check(MesifSpec::projection_is_complete(MesifSpec::kTable), "spec", "MS-SPEC-PROJECTION");
    check(MesifSpec::basis_is_consistent(MesifSpec::kTable), "spec", "MS-SPEC-BASIS");
    check(MesifSpec::write_rule_holds(MesifSpec::kTable), "spec", "MS-SPEC-WRITE-RULE");
    check(MesifSpec::rank_rule_holds(MesifSpec::kTable), "spec", "MS-SPEC-RANK-RULE");
    check(MesifSpec::deviations_are_consistent(MesifSpec::kTable), "spec", "MS-SPEC-DEVIATIONS");

    // ══════════════════════════════════════════════════════════════════
    //  [impl] MA-01 —— 零自有表副本 + 35 格 pass-through
    // ══════════════════════════════════════════════════════════════════
    {
        const std::string raw = slurp(adapter_src);
        check(!raw.empty(), "impl", "MA-01-ZERO-COPY", "载体文件读不到");
        const std::string code = strip_comments(raw);
        for (const char* tok : {"State::", "Event::", "Perm::", "kTable"}) {
            check(code.find(tok) == std::string::npos, "impl", "MA-01-ZERO-COPY",
                  std::string("载体出现自有表记号: ") + tok);
        }
    }
    {
        bool passthrough = true;
        for (std::size_t s = 0; s < Adapter::kStateCount; ++s) {
            for (std::size_t e = 0; e < Adapter::kEventCount; ++e) {
                const auto st = static_cast<Adapter::State>(s);
                const auto ev = static_cast<Adapter::Event>(e);
                if (Adapter::transition(st, ev) != MesifSpec::transition(st, ev))
                    passthrough = false;
            }
        }
        for (std::size_t s = 0; s < Adapter::kStateCount; ++s) {
            Adapter::Perm a = Adapter::Perm::NONE, b = Adapter::Perm::NONE;
            const auto st = static_cast<Adapter::State>(s);
            if (Adapter::perm_of(st, a) != MesifSpec::perm_of(st, b))
                passthrough = false;
            else if (static_cast<int>(a) != static_cast<int>(b))
                passthrough = false;
        }
        check(passthrough, "impl", "MA-01-PASSTHROUGH");
    }

    // ══════════════════════════════════════════════════════════════════
    //  [impl] MA-02 —— 5→4 投影 / 瞬态 / 语义损失措辞
    // ══════════════════════════════════════════════════════════════════
    {
        // 4 值锁 + 秩单调(I ≤ S ≤ E ≤ M)+ `F` 与 `M` **同投影**(语义损失 ①)
        auto rank = [](Adapter::Perm p) {
            switch (p) {
                case Adapter::Perm::NONE:
                    return 0;
                case Adapter::Perm::READ:
                    return 1;
                case Adapter::Perm::UNIQUE:
                    return 2;
                case Adapter::Perm::WRITEBACK:
                    return 3;
            }
            return -1;
        };
        Adapter::Perm pi, ps, pe, pm, pf;
        const bool has = Adapter::perm_of(Adapter::State::I, pi) &&
                         Adapter::perm_of(Adapter::State::S, ps) &&
                         Adapter::perm_of(Adapter::State::E, pe) &&
                         Adapter::perm_of(Adapter::State::M, pm) &&
                         Adapter::perm_of(Adapter::State::F, pf);
        check(has, "impl", "MA-02-PROJECTION", "5 态并非全部有投影");
        if (has) {
            check(rank(pi) <= rank(ps) && rank(ps) <= rank(pe) && rank(pe) <= rank(pm), "impl",
                  "MA-02-PROJECTION", "投影秩不单调");
            check(rank(pf) == rank(pm), "impl", "MA-02-PROJECTION", "F 与 M 投影不同(损失 ① 不成立)");
        }
    }
    check(Adapter::kTransientStateCount == 0u, "impl", "MA-02-TRANSIENT",
          "本规约 5 态全稳定 ⇒ 瞬态列表必须为空");
    {
        const std::string note = Adapter::kPermMergeNote;
        const std::string contrast = Adapter::kPermMergeNoteChiRnfContrast;
        const std::string wb = Adapter::kPermMergeWritebackPathNote;
        check(note.find("不可补偿") == std::string::npos, "impl", "MA-02-LOSS-WORDING",
              "主体登记出现「不可补偿」");
        check(contrast.find("不可补偿") != std::string::npos, "impl", "MA-02-LOSS-WORDING",
              "对照说明缺「不可补偿」");
        check(note.find("可由 Sharer Tracker 补偿") != std::string::npos, "impl",
              "MA-02-LOSS-WORDING", "主体登记缺「可补偿」措辞");
        check(wb.find("不可分辨") != std::string::npos, "impl", "MA-02-LOSS-WORDING",
              "损失 ③ 的登记缺「不可分辨」");
    }

    // ══════════════════════════════════════════════════════════════════
    //  [impl] MA-03 —— 6→3 消息类映射
    // ══════════════════════════════════════════════════════════════════
    {
        std::set<std::string> seen;
        bool ok_rows = (Adapter::kMessageClassCount == 6u);
        for (std::size_t i = 0; i < Adapter::kMessageClassCount; ++i) {
            const auto& row = Adapter::kMessageClassMap[i];
            const std::string mc = row.message_class ? row.message_class : "";
            if (!seen.insert(mc).second)
                ok_rows = false;  // 重复行
            if (row.category > 1)
                ok_rows = false;  // CATEGORY_* 只有 2 个接受值
        }
        check(ok_rows, "impl", "MA-03-MAPPING-COMPLETE");

        // 与平台方向默认表逐行对拍
        bool identity = true;
        for (const auto& ref : cep::identity_reference_map()) {
            std::uint8_t got = 0xFF;
            if (!Adapter::category_of(ref.first.c_str(), got) || got != ref.second)
                identity = false;
        }
        check(identity, "impl", "MA-03-IDENTITY", "映射与平台方向默认表不一致");

        std::uint8_t cat = 0xFF;
        check(!Adapter::category_of("forward_data", cat), "impl", "MA-03-FORWARD-DATA-ABSENT",
              "forward_data 不得进入 6 类词表");
        check(Adapter::kMappingIsIdentity == identity, "impl", "MA-03-IDENTITY-REASON",
              "恒等登记与实际映射不符");
        check(Adapter::kIdentityReason != nullptr &&
                  std::string(Adapter::kIdentityReason).size() > 20,
              "impl", "MA-03-IDENTITY-REASON", "恒等理由缺失或过短");
    }

    // ══════════════════════════════════════════════════════════════════
    //  [impl] HD-01 ~ HD-05 —— 第 6 种 SharerTracker 实现
    // ══════════════════════════════════════════════════════════════════
    {
        const json decl = json::parse(slurp(cep_dir + "/protocol_instances.json"));
        const json* mesif = nullptr;
        for (const auto& i : decl.at("instances"))
            if (i.value("id", "") == "mesif")
                mesif = &i;
        check(mesif != nullptr, "decl", "MA-05-REQUIRED", "声明里缺少 mesif");
        if (mesif != nullptr) {
            const auto& st = mesif->at("sharer_tracker");
            const auto* traits = cep::find_tracker_traits(cep::TrackerImpl::HierarchicalDirectory);
            cep::HierarchicalDirectory dir;
            const bool caps_ok =
                traits != nullptr && st.at("impl") == "HierarchicalDirectory" &&
                st.at("caps").at("scope") == "SYSTEM" && st.at("caps").at("completeness") == "EXACT" &&
                st.at("caps").at("authority") == "GLOBAL" &&
                st.at("caps").at("aggregation_at") == "HIERARCHICAL" &&
                static_cast<int>(traits->caps.scope) == static_cast<int>(cep::Scope::SYSTEM) &&
                static_cast<int>(traits->caps.aggregation_at) ==
                    static_cast<int>(cep::AggregationAt::HIERARCHICAL) &&
                static_cast<int>(dir.caps.scope) == static_cast<int>(traits->caps.scope) &&
                static_cast<int>(dir.caps.aggregation_at) ==
                    static_cast<int>(traits->caps.aggregation_at);
            check(caps_ok, "impl", "HD-01-CAPS", "caps 三方不一致");
            check(static_cast<int>(traits->behavior.hazard_under_contention) ==
                      static_cast<int>(cep::LookupHazard::STALL) &&
                      traits->behavior.broadcast_fallback == false &&
                      static_cast<int>(dir.behavior.hazard_under_contention) ==
                          static_cast<int>(traits->behavior.hazard_under_contention),
                  "impl", "HD-01-CAPS", "behavior 两族不一致");
        }
    }
    {
        // 多级聚合:存在**严格更窄**的级内视图 ⇒ 不是 FullMap 克隆
        cep::HierarchicalDirectory dir;
        constexpr std::uint64_t addr = 0x4000;
        dir.set_sharer(addr, cep::kNodeIdSdma, 0, cep::SnoopState::S);
        dir.set_sharer(addr, cep::kNodeIdGmmu, 1, cep::SnoopState::M);
        const auto v0 = dir.aggregate_view_at(addr, 0);
        const auto vr = dir.view_of(addr);
        check(v0.sharers != vr.sharers && (v0.sharers & vr.sharers) == v0.sharers, "impl",
              "HD-02-AGGREGATION", "L0 视图未严格窄于根级(聚合退化为单级)");
        check(v0.valid && vr.valid, "impl", "HD-02-AGGREGATION", "聚合视图无效");
    }
    {
        // contention ⇒ STALL + view 无效;无 contention ⇒ NONE + view 有效
        cep::HierarchicalDirectory dir;
        constexpr std::uint64_t addr = 0x5000;
        dir.set_sharer(addr, cep::kNodeIdSdma, 0, cep::SnoopState::M);
        cep::LookupResult r = cep::LookupResult::ANSWER_HERE;
        cep::LocalView v;
        cep::LookupHazard h = cep::LookupHazard::NONE;
        dir.lookup(addr, cep::Intent::want_read, r, v, h);
        check(h == cep::LookupHazard::NONE && v.valid, "impl", "HD-03-STALL",
              "无 contention 时出错");

        cep::snoop_handle_t handle = 0;
        dir.snoop_begin(addr, cep::responder_mask_of(cep::kNodeIdGmmu), handle);
        dir.lookup(addr, cep::Intent::want_read, r, v, h);
        check(handle != 0 && h == cep::LookupHazard::STALL && !v.valid, "impl", "HD-03-STALL",
              "contention 下未返回 STALL 或 view 仍有效");
    }
    {
        check(cep::HierarchicalDirectory::behavior_decisions_are_consistent(), "impl",
              "HD-04-DECISIONS", "行为决策表不自洽(字段缺失/编号断裂)");
        bool all_pending = true;
        const std::size_t n = cep::HierarchicalDirectory::kBehaviorDecisionCount;
        for (std::size_t i = 0; i < n; ++i)
            if (!cep::HierarchicalDirectory::kBehaviorDecisions[i].pending)
                all_pending = false;
        check(n == 5u && all_pending, "impl", "HD-04-DECISIONS",
              "决策条数非 5 或存在「已裁决」标记");
    }
    {
        const std::string note = cep::HierarchicalDirectory::kForwardPointGapNote;
        check(cep::HierarchicalDirectory::kForwardPointRoleRepresentable == false &&
                  note.find("转发点保真度缺口") != std::string::npos &&
                  note.find("不可分辨") != std::string::npos,
              "impl", "HD-05-GAP", "F 态(转发点)缺口未登记");
    }

    // ══════════════════════════════════════════════════════════════════
    //  [decl] MA-04 / MA-05 / MA-08
    // ══════════════════════════════════════════════════════════════════
    {
        const json pu = json::parse(slurp(cep_dir + "/perm_upgrades.json"));
        std::size_t hits = 0;
        for (const auto& u : pu.at("perm_upgrade").at("silent_upgrades")) {
            if (u.value("instance_id", "") == "mesif") {
                ++hits;
                check(u.value("from", "") == "UNIQUE" && u.value("to", "") == "WRITEBACK", "decl",
                      "MA-04-UPGRADE-ROW", "mesif 行不是 UNIQUE→WRITEBACK");
            }
        }
        check(hits == 1u, "decl", "MA-04-UPGRADE-ROW", "mesif 行缺失或重复");

        const std::string cs = pu.at("derived_from").at("current_state").get<std::string>();
        // ⚠️ 2026-11-01(队列 #10 实现层):静默升级声明 **3 → 4** 条(新增 `cxl_2_0`)
        //    ⇒ 计数串同步为「4 个实例全部」(per 台账 Entry #131)
        check(cs.find("无该能力") == std::string::npos && cs.find("4 个实例全部") != std::string::npos,
              "decl", "MA-04-CURRENT-STATE", "current_state 仍否认 mesif 的能力");
    }
    {
        const json schema = json::parse(slurp(cep_dir + "/protocol_instance.schema.json"));
        const json decl = json::parse(slurp(cep_dir + "/protocol_instances.json"));
        const json* mesif = nullptr;
        for (const auto& i : decl.at("instances"))
            if (i.value("id", "") == "mesif")
                mesif = &i;
        bool required_ok = (mesif != nullptr);
        if (mesif != nullptr) {
            for (const auto& k : schema.at("required"))
                if (mesif->count(k.get<std::string>()) != 1u)
                    required_ok = false;
        }
        check(required_ok, "decl", "MA-05-REQUIRED", "13 个 required 属性不齐");
        const std::string st = (mesif != nullptr) ? mesif->value("status", "") : "";
        check(st == "planned" || st == "implemented", "decl", "MA-05-STATUS-DOMAIN",
              "status 越域: " + st);
    }
    {
        const json fabric = json::parse(slurp(cep_dir + "/fabric.json"));
        const json decl = json::parse(slurp(cep_dir + "/protocol_instances.json"));
        const json* mesif = nullptr;
        for (const auto& i : decl.at("instances"))
            if (i.value("id", "") == "mesif")
                mesif = &i;
        bool bound_ok = (mesif != nullptr);
        if (mesif != nullptr) {
            for (const auto& net : mesif->at("vc_binding").items()) {
                if (fabric.at("networks").count(net.key()) != 1u) {
                    bound_ok = false;
                    continue;
                }
                const unsigned limit = fabric.at("networks").at(net.key()).at("vc_count").get<unsigned>();
                for (const auto& mc : net.value().items())
                    if (mc.value().get<unsigned>() >= limit)
                        bound_ok = false;
            }
        }
        check(bound_ok, "decl", "MA-08-VC-BOUND", "vc_binding 越界或网络不存在");
    }

    std::printf("%s: %d 项判据,失败 %d 项\n", g_failures == 0 ? "PASS" : "FAIL", g_checks,
                g_failures);
    return (g_failures == 0) ? 0 : 1;
}
