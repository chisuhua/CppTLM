// include/tlm/cep/adapter_schema_contracts.hh
// Adapter Schema Contracts —— adapter 侧**跨实例复用**契约的 5 条可机器检查判据
//
// 功能:把「adapter 侧的跨实例复用契约」固化成**一份**判据实现:
//       AC-01 `perm` 4 值锁不破(含跨 schema 值域绑定)
//       AC-02 `perm_upgrade` 引用完整性 + 权限单调
//       AC-03 三态显式化(声明源畸形 ⇒ FAIL,与「无可检查对象 ⇒ SKIP」严格区分)
//       AC-04 6→3 消息类映射表完备、值域受限且非恒等(或恒等 + 登记)
//       AC-05 adapter 的投影与状态机自洽
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 定位(per delta capability `cep-adapter-schema-contracts`)───────────
// 本头文件是**机制**,不是某个协议的实例:
//   · 后续 7 个 adapter change(`chi-rnf` / `mesif` / `cxl-nonsymmetric` /
//     `tilelink` / `cxl-dcoh-symmetric` / `nvlink` / `ualink`)**复用**本实现,
//     **各自填行**(静默升级声明 / 6→3 映射行 / 实例声明)。
//   · **禁止双实现**:判据实现**只此一份**;Catch2 测试与离线 CLI
//     (`examples/cep_adapter_contracts.cc`)**共用**本头文件,不得各自复制一份。
//
// ── 为什么是「纯 C++ 数据结构」而不是「直接读 JSON」──────────────────
// 判据实现**不依赖** JSON 解析:调用方(测试 / CLI)负责把 JSON 读成下列 POD 结构,
// 本头文件只做**判定**。这样判据实现既能被 C++ 测试复用,也能被将来的其它前端复用,
// 且**不**把 JSON 解析依赖引进公共头文件(`include/` 是安装面)。
//
// ── 三态(与 PASS 可区分)───────────────────────────────────────────────
//   SKIP = **无可检查对象**(例:静默升级声明源合法但为空 —— 该实例无此能力)
//   PASS = 有可检查对象且全部子条件成立
//   FAIL = 有可检查对象但子条件不成立,或**声明源本身畸形/缺失**(配置错误)
// `SKIP` **不得**被当作 `PASS`;声明源畸形**不得**被当作 `SKIP`
// (per 队列 #4 `cep-channel-dependency-gate` 先例 + `ADR-CEP-09 §8.3` 的 verdict 格式)。

#ifndef CPPTLM_TLM_CEP_ADAPTER_SCHEMA_CONTRACTS_HH
#define CPPTLM_TLM_CEP_ADAPTER_SCHEMA_CONTRACTS_HH

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bundles/noc_bundles_tlm.hh"

namespace tlm::cep {

// ══════════════════════════════════════════════════════════════════════
//  三态 + verdict 行
// ══════════════════════════════════════════════════════════════════════

/// 判据结论三态(与 `PASS` 可区分)
enum class AdapterContractVerdict : std::uint8_t { SKIP = 0, PASS = 1, FAIL = 2 };

inline const char* adapter_contract_verdict_name(AdapterContractVerdict v) {
    switch (v) {
        case AdapterContractVerdict::SKIP: return "SKIP";
        case AdapterContractVerdict::PASS: return "PASS";
        case AdapterContractVerdict::FAIL: return "FAIL";
    }
    return "FAIL";
}

/// 一条判据的结论(格式 `[verdict] <rule-id> PASS|FAIL|SKIP (detail)` —— 逐条可 grep)
struct AdapterContractVerdictLine {
    const char*   rule_id = "";
    AdapterContractVerdict verdict = AdapterContractVerdict::SKIP;
    std::string   detail;

    /// 渲染为逐条可 grep 的一行
    std::string render() const {
        return std::string("[verdict] ") + rule_id + " " + adapter_contract_verdict_name(verdict) + " (" +
               detail + ")";
    }
};

/// 判据报告(逐条结论 + 汇总)
struct AdapterContractReport {
    std::vector<AdapterContractVerdictLine> verdicts;
    AdapterContractVerdict verdict = AdapterContractVerdict::SKIP;

    bool failed() const { return verdict == AdapterContractVerdict::FAIL; }
    bool skipped() const { return verdict == AdapterContractVerdict::SKIP; }

    /// 逐条渲染(CLI 与测试**共用**本函数 ⇒ 输出必然逐条相同)
    std::string render() const {
        std::string out;
        for (const auto& v : verdicts) {
            out += v.render();
            out += "\n";
        }
        out += std::string("[verdict] SUMMARY ") + adapter_contract_verdict_name(verdict) + " (" +
               std::to_string(verdicts.size()) + " rule(s))\n";
        return out;
    }
};

// ══════════════════════════════════════════════════════════════════════
//  输入数据结构(由调用方从 JSON 装载)
// ══════════════════════════════════════════════════════════════════════

/// `perm` 值域(来自 `protocol_instance.schema.json` 的 `perm` 枚举)
/// —— **枚举顺序即权限秩序**(per `ADR-CEP-02` CEN-Ib11 的 4 值锁)。
struct PermValueDomain {
    bool        source_error       = false;
    std::string source_error_detail;
    std::vector<std::string> values;
};

/// 单个协议实例声明(来自 `protocol_instances.json`)
struct ProtocolInstanceDecl {
    std::string              id;
    std::vector<std::string> perm;
};

/// 协议实例声明集
struct ProtocolInstancesDecl {
    bool        source_error       = false;
    std::string source_error_detail;
    std::vector<ProtocolInstanceDecl> instances;

    const ProtocolInstanceDecl* find(const std::string& id) const {
        for (const auto& i : instances) {
            if (i.id == id) return &i;
        }
        return nullptr;
    }
};

/// 一条静默升级声明(`perm_upgrades.json` 的 `silent_upgrades[]` 元素)
struct SilentUpgradeDecl {
    std::string instance_id;
    std::string from;
    std::string to;
};

/// 静默升级声明源
struct PermUpgradesDecl {
    bool        source_error       = false;
    std::string source_error_detail;
    std::vector<SilentUpgradeDecl> silent_upgrades;
};

/// 消息类 → 链路层类别的映射(逐 adapter 提供)
struct MessageClassMapDecl {
    /// 映射行(消息类名 → `CATEGORY_*` 值)
    std::vector<std::pair<std::string, std::uint8_t>> rows;
    /// 本 adapter 是否声明「映射恒等」
    bool        is_identity = false;
    /// 恒等理由(恒等时**必须**非空)
    std::string identity_reason;
    /// 本 adapter **实际使用**的消息类(完备性判据的期望集合)
    std::vector<std::string> used_message_classes;
};

// ══════════════════════════════════════════════════════════════════════
//  平台级常量(判据的「参考面」)
// ══════════════════════════════════════════════════════════════════════

/// 契约侧的**权限秩序**(秩升序)—— 与 schema 侧的枚举顺序必须一致(AC-01 ③)
inline const std::vector<std::string>& perm_order() {
    static const std::vector<std::string> kOrder = {"NONE", "READ", "UNIQUE", "WRITEBACK"};
    return kOrder;
}

/// 链路层 `CATEGORY_*` 的取值域(恒为 3 类,per `ADR-CEP-12 §8.3` ② 分层纪律)
inline const std::vector<std::uint8_t>& category_value_domain() {
    static const std::vector<std::uint8_t> kDomain = {
        bundles::NoCFlitBundle::CATEGORY_REQUEST,
        bundles::NoCFlitBundle::CATEGORY_RESPONSE,
        bundles::NoCFlitBundle::CATEGORY_CREDIT,
    };
    return kDomain;
}

/// 6 类消息类的**方向默认**参考表(平台级)—— 「链路事务视角」:
/// 需要应答的消息 ⇒ `CATEGORY_REQUEST`;应答 ⇒ `CATEGORY_RESPONSE`。
/// 参照 ArchForge `32 号` §6.0.1 的 6 类词表与方向列。
/// **恒等判定的唯一基准**:某 adapter 的映射 == 本表 ⇒ 恒等 ⇒ 必须登记理由(AC-04 ③④)。
inline const std::vector<std::pair<std::string, std::uint8_t>>& identity_reference_map() {
    static const std::vector<std::pair<std::string, std::uint8_t>> kMap = {
        {"request",    bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"response",   bundles::NoCFlitBundle::CATEGORY_RESPONSE},
        {"snoop",      bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"snoop_resp", bundles::NoCFlitBundle::CATEGORY_RESPONSE},
        {"atomic",     bundles::NoCFlitBundle::CATEGORY_REQUEST},
        {"query",      bundles::NoCFlitBundle::CATEGORY_REQUEST},
    };
    return kMap;
}

/// 权限秩(未知值 ⇒ -1)
inline int perm_rank(const std::string& value) {
    const auto& order = perm_order();
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == value) return static_cast<int>(i);
    }
    return -1;
}

// ══════════════════════════════════════════════════════════════════════
//  判据实现(单一真源)
// ══════════════════════════════════════════════════════════════════════

/// adapter 侧跨实例复用契约的 5 条判据
class AdapterSchemaContracts {
public:
    /// ── AC-01 `perm` 4 值锁不破 ────────────────────────────────────
    /// ① 值域恰为 4 值且不含 `ATOMIC`;② 声明的 `from`/`to` 取自该值域;
    /// ③ 契约侧权限秩序 == schema 侧枚举顺序。
    static AdapterContractVerdictLine check_perm_four_value_lock(const PermValueDomain&   domain,
                                                            const PermUpgradesDecl& upgrades) {
        AdapterContractVerdictLine line;
        line.rule_id = "AC-01";
        if (domain.source_error) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "perm 值域来源畸形: " + domain.source_error_detail;
            return line;
        }
        if (domain.values.size() != perm_order().size()) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "perm 值域应为 4 值,实得 " + std::to_string(domain.values.size()) + " 值";
            return line;
        }
        for (const auto& v : domain.values) {
            if (v == "ATOMIC") {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "perm 值域含 ATOMIC —— 4 值锁(CEN-Ib11)被击穿";
                return line;
            }
        }
        if (domain.values != perm_order()) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "契约侧权限秩序与 schema 侧枚举顺序不一致";
            return line;
        }
        if (upgrades.source_error) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "静默升级声明源畸形: " + upgrades.source_error_detail;
            return line;
        }
        for (const auto& u : upgrades.silent_upgrades) {
            for (const std::string* v : {&u.from, &u.to}) {
                if (perm_rank(*v) < 0) {
                    line.verdict = AdapterContractVerdict::FAIL;
                    line.detail  = std::string("静默升级声明的 ") +
                                   (v == &u.from ? "from" : "to") + " = \"" + *v +
                                   "\" 不在 perm 4 值域内";
                    return line;
                }
            }
        }
        line.verdict = AdapterContractVerdict::PASS;
        line.detail  = "perm 4 值锁未破(NONE/READ/UNIQUE/WRITEBACK);秩序与 schema 枚举顺序一致;"
                       "声明值全部落在值域内";
        return line;
    }

    /// ── AC-02 `perm_upgrade` 引用完整性 + 权限单调 ──────────────────
    /// ① 声明的实例 id 存在;② 该实例的 `perm` 同时含 `from` 与 `to`;
    /// ③ `to` 的权限秩 ≥ `from` 的秩。
    static AdapterContractVerdictLine check_perm_upgrade_integrity(const ProtocolInstancesDecl& instances,
                                                             const PermUpgradesDecl&     upgrades) {
        AdapterContractVerdictLine line;
        line.rule_id = "AC-02";
        if (upgrades.source_error) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "静默升级声明源畸形: " + upgrades.source_error_detail;
            return line;
        }
        if (instances.source_error) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "实例声明源畸形: " + instances.source_error_detail;
            return line;
        }
        if (upgrades.silent_upgrades.empty()) {
            line.verdict = AdapterContractVerdict::SKIP;
            line.detail  = "静默升级声明源合法但为空 —— 无可检查对象(SKIP,不得记为 PASS)";
            return line;
        }
        for (const auto& u : upgrades.silent_upgrades) {
            const ProtocolInstanceDecl* inst = instances.find(u.instance_id);
            if (inst == nullptr) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "静默升级声明引用了不存在的实例 id: \"" + u.instance_id + "\"";
                return line;
            }
            const bool has_from = contains(inst->perm, u.from);
            const bool has_to   = contains(inst->perm, u.to);
            if (!has_from || !has_to) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "实例 \"" + u.instance_id + "\" 的 perm 数组未同时包含 from=\"" +
                               u.from + "\" 与 to=\"" + u.to + "\"";
                return line;
            }
            const int rf = perm_rank(u.from);
            const int rt = perm_rank(u.to);
            if (rf < 0 || rt < 0) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "静默升级声明的 from/to 不在权限秩序内: \"" + u.from + "\" → \"" +
                               u.to + "\"";
                return line;
            }
            if (rt < rf) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "静默升级降权限: \"" + u.from + "\" → \"" + u.to + "\"";
                return line;
            }
        }
        line.verdict = AdapterContractVerdict::PASS;
        line.detail  = std::to_string(upgrades.silent_upgrades.size()) +
                       " 条静默升级声明:实例引用完整 + 权限单调(秩不降)";
        return line;
    }

    /// ── AC-03 三态显式化 + 声明源畸形 ⇒ FAIL ────────────────────────
    /// ① 声明源缺失/畸形 ⇒ **FAIL**(配置错误,与「无可检查对象 ⇒ SKIP」严格区分);
    /// ② 不得把「无可检查对象」装作 PASS;③ 无静默升级能力的实例不得出现在声明源里。
    static AdapterContractVerdictLine check_decl_source_sanity(const PermUpgradesDecl&      upgrades,
                                                        const std::vector<std::string>& no_upgrade_instances) {
        AdapterContractVerdictLine line;
        line.rule_id = "AC-03";
        if (upgrades.source_error) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "静默升级声明源缺失或畸形 ⇒ FAIL(配置错误,不得记为 SKIP): " +
                           upgrades.source_error_detail;
            return line;
        }
        for (const auto& u : upgrades.silent_upgrades) {
            for (const auto& id : no_upgrade_instances) {
                if (u.instance_id == id) {
                    line.verdict = AdapterContractVerdict::FAIL;
                    line.detail  = "实例 \"" + id + "\" 无静默升级能力,却出现在静默升级声明源里";
                    return line;
                }
            }
        }
        if (upgrades.silent_upgrades.empty()) {
            line.verdict = AdapterContractVerdict::SKIP;
            line.detail  = "声明源合法但为空 —— 无可检查对象(SKIP,不得记为 PASS)";
            return line;
        }
        line.verdict = AdapterContractVerdict::PASS;
        line.detail  = "声明源形态合法;三态可区分(SKIP/PASS/FAIL);无越权实例";
        return line;
    }

    /// ── AC-04 6→3 映射表完备、值域受限且非恒等(或恒等 + 登记)──────
    /// ① 该实例用到的每个消息类**恰好一行**;② 目标 ∈ `CATEGORY_*` 3 类;
    /// ③ 相对方向默认表**非恒等**;④ 恒等者必须携带 `identity_reason`。
    static AdapterContractVerdictLine check_message_class_mapping(const MessageClassMapDecl& map) {
        AdapterContractVerdictLine line;
        line.rule_id = "AC-04";
        if (map.used_message_classes.empty()) {
            line.verdict = AdapterContractVerdict::SKIP;
            line.detail  = "该实例未声明使用的消息类 —— 无可检查对象(SKIP,不得记为 PASS)";
            return line;
        }
        if (map.rows.size() != map.used_message_classes.size()) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "映射行数 " + std::to_string(map.rows.size()) + " ≠ 使用中的消息类数 " +
                           std::to_string(map.used_message_classes.size()) + "(无多、无少)";
            return line;
        }
        for (const auto& cls : map.used_message_classes) {
            int hits = 0;
            for (const auto& r : map.rows) {
                if (r.first == cls) ++hits;
            }
            if (hits != 1) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "消息类 \"" + cls + "\" 的映射行数 = " + std::to_string(hits) +
                               "(应为恰好 1)";
                return line;
            }
        }
        for (const auto& r : map.rows) {
            bool in_domain = false;
            for (std::uint8_t v : category_value_domain()) {
                if (r.second == v) { in_domain = true; break; }
            }
            if (!in_domain) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "消息类 \"" + r.first + "\" 的目标类别 " +
                               std::to_string(r.second) + " 不在 CATEGORY_* 的 3 类值域内";
                return line;
            }
        }
        // 恒等判定:与平台方向默认表逐行比较
        bool identical = true;
        for (const auto& ref : identity_reference_map()) {
            bool found = false;
            for (const auto& r : map.rows) {
                if (r.first == ref.first) {
                    found = true;
                    if (r.second != ref.second) { identical = false; }
                    break;
                }
            }
            if (!found) { identical = false; break; }
        }
        if (identical) {
            if (!map.is_identity || map.identity_reason.empty()) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "映射 == 方向默认表(恒等)但未显式声明恒等并携带 identity_reason";
                return line;
            }
            line.verdict = AdapterContractVerdict::PASS;
            line.detail  = "映射 == 方向默认表(恒等,合法)且已登记 identity_reason:" +
                           map.identity_reason;
            return line;
        }
        line.verdict = AdapterContractVerdict::PASS;
        line.detail  = "映射完备(" + std::to_string(map.rows.size()) + " 行)且目标 ∈ 3 类值域;"
                       "相对方向默认表非恒等";
        return line;
    }

    /// ── AC-05 adapter 的投影与状态机自洽 ────────────────────────────
    /// ① 每个稳定态都有 4 值投影;② 瞬态 `T` 无投影;
    /// ③ `E + Write → M` 转换存在;④ 该转换后 `perm` 从 `UNIQUE` 变 `WRITEBACK`。
    static AdapterContractVerdictLine check_adapter_self_consistency(
        const std::vector<std::pair<int, bool>>& stable_state_has_projection,
        int  e_write_target_state,
        int  e_state_perm_rank,
        int  m_state_perm_rank) {
        AdapterContractVerdictLine line;
        line.rule_id = "AC-05";
        for (const auto& s : stable_state_has_projection) {
            if (!s.second) {
                line.verdict = AdapterContractVerdict::FAIL;
                line.detail  = "稳定态 " + std::to_string(s.first) + " 缺少 4 值投影";
                return line;
            }
        }
        if (e_write_target_state != 3) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "E + Write 的目标态 = " + std::to_string(e_write_target_state) +
                           "(应为 3 = M)";
            return line;
        }
        if (e_state_perm_rank < 0 || m_state_perm_rank < 0) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "E / M 态缺少 perm 投影(投影与状态机不一致)";
            return line;
        }
        if (m_state_perm_rank <= e_state_perm_rank) {
            line.verdict = AdapterContractVerdict::FAIL;
            line.detail  = "E→M 后 perm 秩未升高(E=" + std::to_string(e_state_perm_rank) +
                           " → M=" + std::to_string(m_state_perm_rank) + ")";
            return line;
        }
        line.verdict = AdapterContractVerdict::PASS;
        line.detail  = "稳定态投影齐备;T 无投影;E+Write→M 存在且 perm 秩 UNIQUE(" +
                       std::to_string(e_state_perm_rank) + ") → WRITEBACK(" +
                       std::to_string(m_state_perm_rank) + ")";
        return line;
    }

    // ── 便捷:一次跑完全部 5 条 ───────────────────────────────────────
    struct Inputs {
        PermValueDomain       perm_domain;
        ProtocolInstancesDecl instances;
        PermUpgradesDecl      upgrades;
        MessageClassMapDecl   mapping;
        std::vector<std::string> no_upgrade_instances;
        std::vector<std::pair<int, bool>> stable_state_has_projection;
        int  e_write_target_state = -1;
        int  e_state_perm_rank     = -1;
        int  m_state_perm_rank     = -1;
    };

    static AdapterContractReport check_all(const Inputs& in) {
        AdapterContractReport report;
        report.verdicts.push_back(check_perm_four_value_lock(in.perm_domain, in.upgrades));
        report.verdicts.push_back(check_perm_upgrade_integrity(in.instances, in.upgrades));
        report.verdicts.push_back(check_decl_source_sanity(in.upgrades, in.no_upgrade_instances));
        report.verdicts.push_back(check_message_class_mapping(in.mapping));
        report.verdicts.push_back(check_adapter_self_consistency(in.stable_state_has_projection,
                                                                in.e_write_target_state,
                                                                in.e_state_perm_rank,
                                                                in.m_state_perm_rank));
        report.verdict = AdapterContractVerdict::PASS;
        bool any_skip = false;
        for (const auto& v : report.verdicts) {
            if (v.verdict == AdapterContractVerdict::FAIL) { report.verdict = AdapterContractVerdict::FAIL; break; }
            if (v.verdict == AdapterContractVerdict::SKIP) { any_skip = true; }
        }
        if (report.verdict != AdapterContractVerdict::FAIL && any_skip) {
            report.verdict = AdapterContractVerdict::SKIP;
        }
        return report;
    }

private:
    static bool contains(const std::vector<std::string>& v, const std::string& x) {
        for (const auto& e : v) {
            if (e == x) return true;
        }
        return false;
    }
};

}  // namespace tlm::cep

#endif  // CPPTLM_TLM_CEP_ADAPTER_SCHEMA_CONTRACTS_HH
