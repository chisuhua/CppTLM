// include/tlm/cep/instance_assembly_gate.hh
// **实例级装配闸** —— 兑现「声明 ≠ 装配」(per `ADR-CEP-12 §5.1.1` / 台账 Entry #240)。
//
// 三层证据(缺一即「未装配」):
//   ① **声明层**:实例在 `protocol_instances.json` 的 `instances[]` 中;
//   ② **拓扑层**:实例出现在某配置的 `cep.bridges[].protocol_instance`(⇒ 被挂进可运行拓扑);
//   ③ **平面层**:实例在 `collective_planes.json` 的 `plane_binding` 中的绑定**非空**且**已登记**
//      (⇒ collective 能力有确定的承载实现)。
//
// ⚠️ **本闸只判「装配完整性」,不判协议语义** —— 语义判据在 collective plane 契约面
//    (CP-03 的 C-1′ / C-2 证书)与 channel dependency 闸。
//
// 归因 tag 形态:`[IAG-01]` ~ `[IAG-07]`(与既有 `[CP-03-…]` 风格一致 ⇒ 反向 fixture 可精确命中)。
#ifndef CPPT_LM_CEP_INSTANCE_ASSEMBLY_GATE_HH
#define CPPT_LM_CEP_INSTANCE_ASSEMBLY_GATE_HH

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/bridge_module_membership.hh"
#include "tlm/cep/protocol_family.hh"

namespace tlm::cep {

enum class InstanceAssemblyVerdict : std::uint8_t { SKIP = 0, PASS = 1, FAIL = 2 };

inline const char* instance_assembly_verdict_name(InstanceAssemblyVerdict v) {
    switch (v) {
        case InstanceAssemblyVerdict::SKIP:
            return "SKIP";
        case InstanceAssemblyVerdict::PASS:
            return "PASS";
        case InstanceAssemblyVerdict::FAIL:
            return "FAIL";
    }
    return "UNKNOWN";
}

/// 装配闸报告(形态照 `channel_dependency_graph.hh` 的 `GateReport`)。
struct InstanceAssemblyReport {
    InstanceAssemblyVerdict verdict = InstanceAssemblyVerdict::SKIP;
    std::vector<std::string> mounted;  ///< 挂载成功的实例 id
    std::vector<std::string> errors;   ///< 失败项(含 `[IAG-0x]` 归因 tag)
    std::string log;
    std::string skip_reason;

    /// 是否命中某个归因 tag(反向 fixture 用)。
    bool has_error(std::string_view tag) const {
        for (const auto& e : errors) {
            if (e.find(tag) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

namespace detail {

inline const nlohmann::json* find_instance_decl(const nlohmann::json& instances_doc,
                                                 std::string_view id) {
    if (!instances_doc.contains("instances") || !instances_doc.at("instances").is_array()) {
        return nullptr;
    }
    for (const auto& inst : instances_doc.at("instances")) {
        if (inst.contains("id") && inst.at("id").is_string() &&
            inst.at("id").get<std::string>() == id) {
            return &inst;
        }
    }
    return nullptr;
}

inline bool module_declared(const nlohmann::json& config, std::string_view name) {
    if (!config.contains("modules") || !config.at("modules").is_array()) {
        return false;
    }
    for (const auto& m : config.at("modules")) {
        if (m.contains("name") && m.at("name").is_string() &&
            m.at("name").get<std::string>() == name) {
            return true;
        }
    }
    return false;
}

}  // namespace detail

/// 跑装配闸。
/// @param config      拓扑配置(含 `cep.bridges[]`)
/// @param instances_doc `protocol_instances.json` 解析结果
/// @param planes_doc  `collective_planes.json` 解析结果
inline InstanceAssemblyReport run_instance_assembly_gate(
    const nlohmann::json& config, const nlohmann::json& instances_doc,
    const nlohmann::json& planes_doc,
    const std::vector<std::string>& required_plane_instances = {}) {
    InstanceAssemblyReport rep;

    if (!config.contains("cep") || !config.at("cep").contains("bridges") ||
        !config.at("cep").at("bridges").is_array()) {
        rep.verdict = InstanceAssemblyVerdict::SKIP;
        rep.skip_reason = "config has no cep.bridges";
        rep.log = "[IAG] SKIP — no cep.bridges\n";
        return rep;
    }

    const auto& bridges = config.at("cep").at("bridges");
    if (bridges.empty()) {
        rep.verdict = InstanceAssemblyVerdict::FAIL;
        rep.errors.emplace_back("[IAG-01] no bridge mounted — 实例未挂进拓扑");
        rep.log = "[IAG] FAIL — no bridge mounted\n";
        return rep;
    }

    const nlohmann::json* binding = nullptr;
    if (planes_doc.contains("plane_binding") && planes_doc.at("plane_binding").is_object()) {
        binding = &planes_doc.at("plane_binding");
    }

    std::vector<long long> seen_bridge_ids;
    for (const auto& bridge : bridges) {
        if (!bridge.contains("protocol_instance") || !bridge.at("protocol_instance").is_string()) {
            rep.errors.emplace_back("[IAG-02] bridge missing protocol_instance");
            continue;
        }
        const std::string id = bridge.at("protocol_instance").get<std::string>();

        // ① 声明层
        const nlohmann::json* decl = detail::find_instance_decl(instances_doc, id);
        if (decl == nullptr) {
            rep.errors.emplace_back("[IAG-02] instance not declared: " + id);
            continue;
        }

        // ② family **必须存在且可解析**(缺键 / 非 string / 不可解析 ⇒ 一律红)
        if (!decl->contains("family") || !decl->at("family").is_string()) {
            rep.errors.emplace_back("[IAG-03] family missing: " + id);
        } else {
            ProtocolFamily fam{};
            if (!protocol_family_from_name(decl->at("family").get<std::string>(), fam)) {
                rep.errors.emplace_back("[IAG-03] family unresolvable: " + id);
            }
        }

        // ③ 拓扑层:bridge 的 `modules` **必须存在且非空**,且引用的模块必须真的在拓扑里
        if (!bridge.contains("modules") || !bridge.at("modules").is_array() ||
            bridge.at("modules").empty()) {
            rep.errors.emplace_back("[IAG-06] bridge has no modules: " + id);
        } else {
            for (const auto& m : bridge.at("modules")) {
                if (!m.is_string()) {
                    rep.errors.emplace_back("[IAG-06] module entry not a string (bridge " + id + ")");
                    continue;
                }
                const std::string mod = m.get<std::string>();
                if (!detail::module_declared(config, mod)) {
                    rep.errors.emplace_back("[IAG-06] module not in topology: " + mod + " (bridge " +
                                            id + ")");
                }
            }
        }

        // ④ bridge_id **必须存在**且唯一
        if (!bridge.contains("bridge_id") || !bridge.at("bridge_id").is_number_integer()) {
            rep.errors.emplace_back("[IAG-07] bridge_id missing: " + id);
        } else {
            const auto bid = bridge.at("bridge_id").get<long long>();
            if (std::find(seen_bridge_ids.begin(), seen_bridge_ids.end(), bid) !=
                seen_bridge_ids.end()) {
                rep.errors.emplace_back("[IAG-07] duplicate bridge_id: " + std::to_string(bid));
            }
            seen_bridge_ids.push_back(bid);
        }

        // ⑤ 平面层:
        //    ⚠️ `plane_binding` 的 **null/空值 = 该实例不启用 collective**(合法状态,
        //    如 `moesi_gpu` / `mesif`)⇒ **不判红**;
        //    只有**显式要求绑定**的实例(required_plane_instances)才强制非空 ⇒ `[IAG-04]`;
        //    非空但**未登记**的 plane 名一律判红 ⇒ `[IAG-05]`。
        const bool plane_required =
            std::find(required_plane_instances.begin(), required_plane_instances.end(), id) !=
            required_plane_instances.end();
        const bool has_binding = binding != nullptr && binding->contains(id);
        if (has_binding) {
            const auto& v = binding->at(id);
            const std::string plane = v.is_string() ? v.get<std::string>() : std::string();
            if (plane.empty()) {
                if (plane_required) {
                    rep.errors.emplace_back("[IAG-04] plane binding empty: " + id);
                } else {
                    rep.log += "[IAG] instance " + id + " 不启用 collective(plane 绑定为空)\n";
                }
            } else if (find_collective_plane(plane.c_str()) == nullptr) {
                rep.errors.emplace_back("[IAG-05] plane not registered: " + plane + " (" + id + ")");
            }
        } else if (plane_required) {
            rep.errors.emplace_back("[IAG-04] plane binding missing: " + id);
        }

        rep.mounted.push_back(id);
    }

    // ⑥ 桥单槽兜底(per W4):同一模块属**多个** bridge entry ⇒ 模块的 `bridge_` 单槽
    //    会被**后挂的桥静默覆盖** ⇒ 判红。
    //    ⚠️ **判据实现在 `tlm/cep/bridge_module_membership.hh`**(**唯一实现**)——
    //       本闸与 `ModuleFactory` 装配路径**共用**同一份 ⇒ 禁双实现
    //       (per `src/core/module_factory.cc:173` 的「禁双实现」纪律;
    //        per 台账 Entry #319 ④ S-1 / S-2)。
    //    ⚠️ **口径**:**无 entry 内去重** ⇒ 两个子情形都命中:① 跨 entry 重叠
    //       ② 单 entry 内重复列同一模块(如 `["w","w"]`)(per Entry #319 ②)。
    {
        for (const auto& [mod_name, n] :
             tlm::cep::find_bridge_module_conflicts(bridges)) {
            rep.errors.emplace_back(tlm::cep::bridge_module_conflict_message(mod_name, n));
        }
    }

    rep.verdict = rep.errors.empty() ? InstanceAssemblyVerdict::PASS
                                      : InstanceAssemblyVerdict::FAIL;
    std::string log = "[IAG] ";
    log += instance_assembly_verdict_name(rep.verdict);
    log += " — mounted=";
    for (std::size_t i = 0; i < rep.mounted.size(); ++i) {
        log += (i != 0 ? "," : "");
        log += rep.mounted[i];
    }
    log += "\n";
    for (const auto& e : rep.errors) {
        log += "[IAG] " + e + "\n";
    }
    rep.log = log;
    return rep;
}

}  // namespace tlm::cep

#endif  // CPPT_LM_CEP_INSTANCE_ASSEMBLY_GATE_HH
