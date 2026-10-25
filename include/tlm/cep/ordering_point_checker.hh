// include/tlm/cep/ordering_point_checker.hh
// Ordering Point 三判据 + E 态静默升级组合检查(ADR-CEP-08 决策 3 / 决策 4 / INV-CEP-08-IC2)
//
// 功能:把「装配期静态可判」的部分机器化;把**不可静态判定**的部分(J3 通用形式)
//       显式降级并标记 `vacuous`,不允许静默当作已实现。
// 作者: CppTLM Team / 日期: 2026-10-25
//
// ── 三判据的机器化边界(per `ADR-CEP-08 §2 决策 3 / 决策 7`)────────────
//   J1 单点性       —— 机器可判:冲突事务的**响应向**路径必须声明 `p2p_ordered`
//   J2 全序可观测性 —— 机器可判:`completeness == PARTIAL` 且无广播兜底 ⇒ fail
//   J3 唯一性保持   —— **通用形式不可静态判定**(HeteroGen §VI-E E1):
//                      本文件只给**单点 vacuous** 机器判据,多点情况显式 SKIP 并登记
//
// ── 关于 J2 的 vacuous 事实(必须随报告一起读)──────────────────────────
//   冻结 schema `protocol_instance.schema.json` 的 `completeness` enum **只有
//   `EXACT` / `CONSERVATIVE`** —— `PARTIAL` 声明本身就是非法声明。⇒ 在**合法**声明上
//   J2 恒 PASS(vacuous);它的判别力由**合成声明**的负向用例承担。
//   这一点在报告 detail 里逐实例写明,防止把 vacuous PASS 误读成「已实证无漏 snoop」。
//
// ── IC2(E 态静默升级,组合必查项)───────────────────────────────────────
//   E 态可静默升 M(`perm` 含 `UNIQUE`)且**层叠组合**被声明为可组合
//   (`combination_capabilities.hierarchical == true`)时,该组合才是可判的;
//   否则两个都允许静默升级的层会破坏全局 SWMR。⇒ 检查项**结构性存在**,
//   结果按实例出 PASS/FAIL 并逐行打印。
#ifndef CPPTLM_CEP_ORDERING_POINT_CHECKER_HH
#define CPPTLM_CEP_ORDERING_POINT_CHECKER_HH

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/channel_dependency_graph.hh"  // DeclaredInstance / ChannelDeclaration / JSON 读取

namespace tlm::cep {

enum class CriterionId : std::uint8_t {
    J1_SinglePoint = 0,
    J2_TotalOrderObservability = 1,
    J3_UniquenessVacuous = 2,
    IC2_ESilentUpgrade = 3,
};

inline const char* criterion_name(CriterionId id) {
    switch (id) {
        case CriterionId::J1_SinglePoint: return "J1";
        case CriterionId::J2_TotalOrderObservability: return "J2";
        case CriterionId::J3_UniquenessVacuous: return "J3";
        case CriterionId::IC2_ESilentUpgrade: return "INV-CEP-08-IC2";
    }
    return "UNKNOWN";
}

struct CriterionResult {
    CriterionId criterion = CriterionId::J1_SinglePoint;
    std::string instance;
    GateVerdict verdict = GateVerdict::PASS;
    std::string detail;
};

struct OrderingReport {
    std::vector<CriterionResult> results;
    GateVerdict verdict = GateVerdict::SKIP;
    bool ic2_item_present = false;
    std::string log;

    const std::string& to_log() const { return log; }

    bool has(CriterionId id) const {
        for (const auto& r : results) {
            if (r.criterion == id)
                return true;
        }
        return false;
    }

    std::size_t count(GateVerdict v) const {
        std::size_t n = 0;
        for (const auto& r : results) {
            if (r.verdict == v)
                ++n;
        }
        return n;
    }
};

// ─────────────────────────────────────────────────────────────────────
// 单实例四判据
// ─────────────────────────────────────────────────────────────────────
inline std::vector<CriterionResult> evaluate_ordering_point_instance(const DeclaredInstance& inst) {
    std::vector<CriterionResult> out;

    // ---- J1:响应向路径必须声明 p2p_ordered ----
    {
        CriterionResult c;
        c.criterion = CriterionId::J1_SinglePoint;
        c.instance = inst.id;
        c.verdict = GateVerdict::PASS;
        c.detail = "vacuous(无响应向消息类绑定)";

        std::size_t checked = 0;
        std::ostringstream fails;
        for (const auto& [net, classes] : inst.vc_binding) {
            for (const char* cls : {"response", "snoop_resp"}) {
                const auto cls_it = classes.find(cls);
                if (cls_it == classes.end())
                    continue;
                const int vc = cls_it->second;
                if (vc < 0)
                    continue;
                ++checked;

                const auto net_it = inst.ordering.find(net);
                if (net_it == inst.ordering.end()) {
                    fails << net << "(无 ordering_assumption) ";
                    c.verdict = GateVerdict::FAIL;
                    continue;
                }
                const auto vc_it = net_it->second.find(static_cast<unsigned>(vc));
                if (vc_it == net_it->second.end()) {
                    fails << net << ".vc" << vc << "(未声明) ";
                    c.verdict = GateVerdict::FAIL;
                    continue;
                }
                if (vc_it->second != "p2p_ordered") {
                    fails << net << ".vc" << vc << "(=" << vc_it->second << ") ";
                    c.verdict = GateVerdict::FAIL;
                }
            }
        }
        if (checked != 0 && c.verdict == GateVerdict::PASS)
            c.detail = "响应向 vc 全部 p2p_ordered(" + std::to_string(checked) + " 处)";
        if (c.verdict == GateVerdict::FAIL)
            c.detail = "响应向路径未落在有序点:" + fails.str();
        out.push_back(c);
    }

    // ---- J2:PARTIAL 必须声明广播兜底 ----
    {
        CriterionResult c;
        c.criterion = CriterionId::J2_TotalOrderObservability;
        c.instance = inst.id;
        if (inst.completeness == "PARTIAL") {
            if (inst.broadcast_fallback) {
                c.verdict = GateVerdict::PASS;
                c.detail = "PARTIAL + broadcast fallback(决策 2 的最小合法组合)";
            } else {
                c.verdict = GateVerdict::FAIL;
                c.detail = "PARTIAL 且无广播兜底 ⇒ 漏 sharer ⇒ 全序不可观测(决策 2 禁止)";
            }
        } else {
            c.verdict = GateVerdict::PASS;
            c.detail = "completeness=" + inst.completeness +
                       "(冻结 schema 的 enum 不含 PARTIAL ⇒ 合法声明上本判据 vacuous PASS)";
        }
        out.push_back(c);
    }

    // ---- J3:唯一性保持(单点 vacuous;通用形式降级)----
    {
        CriterionResult c;
        c.criterion = CriterionId::J3_UniquenessVacuous;
        c.instance = inst.id;

        std::size_t points = 0;
        for (const auto& [net, by_vc] : inst.ordering) {
            for (const auto& [vc, assumption] : by_vc) {
                if (assumption == "p2p_ordered") {
                    ++points;
                    break;  // 一张网算一个点
                }
            }
        }

        if (points == 0) {
            c.verdict = GateVerdict::SKIP;
            c.detail = "无 ordering point ⇒ 无 J3 可判";
        } else if (points == 1) {
            c.verdict = GateVerdict::PASS;
            c.detail = "vacuous PASS(单点:迁移后不存在第二个点,不可能与自身定出冲突序);"
                       "通用形式不可静态判定 ⇒ 降级为文档判据";
        } else {
            c.verdict = GateVerdict::SKIP;
            c.detail = "多点(" + std::to_string(points) +
                       ")需通用形式,不可静态判定(ADR-CEP-08 决策 7)⇒ 降级至队列 #6";
        }
        out.push_back(c);
    }

    // ---- IC2:E 态静默升级组合必查项 ----
    {
        CriterionResult c;
        c.criterion = CriterionId::IC2_ESilentUpgrade;
        c.instance = inst.id;
        if (!inst.exposes_unique_perm) {
            c.verdict = GateVerdict::PASS;
            c.detail = "perm 不含 UNIQUE ⇒ 无静默升级入口(vacuous PASS)";
        } else if (inst.hierarchical) {
            c.verdict = GateVerdict::PASS;
            c.detail = "UNIQUE(E 可静默升 M)+ hierarchical 组合已声明";
        } else {
            c.verdict = GateVerdict::FAIL;
            c.detail = "UNIQUE 允许静默升级,但未声明 hierarchical 组合能力 ⇒ 全局 SWMR 可能被破坏";
        }
        out.push_back(c);
    }

    return out;
}

inline OrderingReport evaluate_ordering_points(const ChannelDeclaration& decl) {
    OrderingReport r;

    if (decl.instances.empty()) {
        r.verdict = GateVerdict::SKIP;
        r.log = "[SKIP] cep ordering point checker: no declared protocol instance\n";
        return r;
    }

    for (const auto& inst : decl.instances) {
        const auto res = evaluate_ordering_point_instance(inst);
        r.results.insert(r.results.end(), res.begin(), res.end());
    }
    r.ic2_item_present = r.has(CriterionId::IC2_ESilentUpgrade);

    const std::size_t n_fail = r.count(GateVerdict::FAIL);
    r.verdict = (n_fail == 0) ? GateVerdict::PASS : GateVerdict::FAIL;

    std::ostringstream os;
    os << '[' << gate_verdict_name(r.verdict) << "] cep ordering point checker: "
       << decl.instances.size() << " instance(s), " << r.results.size() << " criterion result(s), "
       << n_fail << " failed\n";
    for (const auto& c : r.results) {
        os << "[criterion] instance=" << c.instance << ' ' << criterion_name(c.criterion) << '='
           << gate_verdict_name(c.verdict) << " (" << c.detail << ")\n";
    }
    r.log = os.str();
    return r;
}

inline OrderingReport run_ordering_gate(const nlohmann::json& config) {
    return evaluate_ordering_points(parse_channel_declaration_from_config(config));
}

// ─────────────────────────────────────────────────────────────────────
// S1-7 (β) 裁决 —— **交付物**(不是代码契约)
// ─────────────────────────────────────────────────────────────────────
// 裁决问题(per `ADR-CEP-05 §13.1` + `ADR-CEP-02 CEN-Ib11`):
//   Stub 的 `perm` 4 值锁(NONE/READ/UNIQUE/WRITEBACK)在**组合**语义下是否需要第 5 个判别位
//   来表达「UNIQUE 可静默升 WRITEBACK」?
//
// 结论:**不需要**。`UNIQUE` 已经是「E 态、可写」的编码;静默升级能不能被组合,
//   是**组合能力**问题而不是**权限值**问题 —— 冻结 schema 的
//   `combination_capabilities.hierarchical` 正是它的载体。扩 perm 为 5 值会
//   破 `CEN-Ib11` 的 4 值锁,且不能多回答任何一个问题。
//   ⇒ `perm` 判别位**不新增**;`perm` 形态的 schema 演进留队列 #6 复核(S1-7 的 perm 分支)。
struct S1_7BetaVerdict {
    bool needs_fifth_perm_bit = false;
    const char* decision = "";
    const char* rationale = "";
    const char* registered_in = "";
};

inline constexpr S1_7BetaVerdict kS1_7BetaVerdict{
    false,
    "判定:4 值 perm 足够,不新增第 5 判别位",
    "UNIQUE 已编码 E 态可写;静默升级的**可组合性**由 combination_capabilities.hierarchical "
    "承载(冻结 schema 已有)。扩 perm 会破 ADR-CEP-02 CEN-Ib11 的 4 值锁,且不增加判别力。",
    "openspec/changes/cpptlm-cep-directory-ordering-plane/design.md",
};

}  // namespace tlm::cep

#endif  // CPPTLM_CEP_ORDERING_POINT_CHECKER_HH
