// include/core/port_types.hh
// SPDX-License-Identifier: Apache-2.0
// Phase 3.2: 端口类型系统定义

#pragma once

#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

namespace cpptlm {

// 端口角色枚举 (L1 方向检查)
enum class PortRole {
    INITIATOR,
    TARGET,
    BI_DIRECTIONAL,
    NETWORK,
    PE
};

NLOHMANN_JSON_SERIALIZE_ENUM(PortRole, {
    {PortRole::INITIATOR, "initiator"},
    {PortRole::TARGET, "target"},
    {PortRole::BI_DIRECTIONAL, "bi_directional"},
    {PortRole::NETWORK, "network"},
    {PortRole::PE, "pe"},
})

// Bundle 类型枚举 (L2 匹配检查)
enum class BundleType {
    CACHE_REQ,
    CACHE_RESP,
    NOC_FLIT,
    GENERIC
};

NLOHMANN_JSON_SERIALIZE_ENUM(BundleType, {
    {BundleType::CACHE_REQ, "cache_req"},
    {BundleType::CACHE_RESP, "cache_resp"},
    {BundleType::NOC_FLIT, "noc_flit"},
    {BundleType::GENERIC, "generic"},
})

// 端口组 bundle 类型 (m2 共识)
enum class PortGroupBundleType {
    SINGLE,
    BUNDLE_MASTER,
    BUNDLE_SLAVE
};

NLOHMANN_JSON_SERIALIZE_ENUM(PortGroupBundleType, {
    {PortGroupBundleType::SINGLE, "single"},
    {PortGroupBundleType::BUNDLE_MASTER, "bundle_master"},
    {PortGroupBundleType::BUNDLE_SLAVE, "bundle_slave"}
})

// 端口规格结构体
struct PortSpec {
    std::string name;
    PortRole role = PortRole::BI_DIRECTIONAL;
    BundleType bundle = BundleType::GENERIC;
    unsigned width = 64;
    bool is_multi = false;
    unsigned port_count = 1;
    std::string layout_hint;

    static const std::map<std::string, unsigned>& deprecated_names() {
        static const std::map<std::string, unsigned> names = {
            {"NORTH", 0}, {"EAST", 1}, {"SOUTH", 2}, {"WEST", 3}, {"LOCAL", 4},
            {"N_in", 0}, {"N_out", 0}, {"E_in", 1}, {"E_out", 1},
            {"S_in", 2}, {"S_out", 2}, {"W_in", 3}, {"W_out", 3}
        };
        return names;
    }
};

inline void to_json(nlohmann::json& j, const PortSpec& p) {
    j = nlohmann::json{{"name", p.name}, {"role", p.role}, {"bundle", p.bundle},
                       {"width", p.width}, {"is_multi", p.is_multi},
                       {"port_count", p.port_count}, {"layout_hint", p.layout_hint}};
}

inline void from_json(const nlohmann::json& j, PortSpec& p) {
    j.at("name").get_to(p.name);
    if (j.contains("role")) j.at("role").get_to(p.role);
    if (j.contains("bundle")) j.at("bundle").get_to(p.bundle);
    if (j.contains("width")) j.at("width").get_to(p.width);
    if (j.contains("is_multi")) j.at("is_multi").get_to(p.is_multi);
    if (j.contains("port_count")) j.at("port_count").get_to(p.port_count);
    if (j.contains("layout_hint")) j.at("layout_hint").get_to(p.layout_hint);
}

// 端口组成员（引用已有端口 by index）
struct PortGroupMember {
    unsigned index = 0;
    PortRole role = PortRole::BI_DIRECTIONAL;
    BundleType bundle = BundleType::GENERIC;
};

inline void to_json(nlohmann::json& j, const PortGroupMember& m) {
    j = nlohmann::json{{"index", m.index}, {"role", m.role}, {"bundle", m.bundle}};
}

inline void from_json(const nlohmann::json& j, PortGroupMember& m) {
    j.at("index").get_to(m.index);
    if (j.contains("role")) j.at("role").get_to(m.role);
    if (j.contains("bundle")) j.at("bundle").get_to(m.bundle);
}

// 端口组规格 (m2 共识)
struct PortGroupSpec {
    std::string name;
    PortGroupBundleType bundle_type = PortGroupBundleType::SINGLE;
    std::vector<PortGroupMember> ports;
};

inline void to_json(nlohmann::json& j, const PortGroupSpec& g) {
    j = nlohmann::json{{"name", g.name}, {"bundle_type", g.bundle_type}, {"ports", g.ports}};
}

inline void from_json(const nlohmann::json& j, PortGroupSpec& g) {
    j.at("name").get_to(g.name);
    if (j.contains("bundle_type")) j.at("bundle_type").get_to(g.bundle_type);
    if (j.contains("ports")) j.at("ports").get_to(g.ports);
}

// 模块端口规格配置（用于 JSON 反序列化）
struct ModulePortSpec {
    std::string module_name;
    std::vector<PortSpec> ports;
    std::vector<PortGroupSpec> port_groups;
    std::map<std::string, std::string> aliases;
};

inline void to_json(nlohmann::json& j, const ModulePortSpec& m) {
    j = nlohmann::json{{"module_name", m.module_name}, {"ports", m.ports},
                       {"port_groups", m.port_groups}, {"aliases", m.aliases}};
}

inline void from_json(const nlohmann::json& j, ModulePortSpec& m) {
    j.at("module_name").get_to(m.module_name);
    if (j.contains("ports")) j.at("ports").get_to(m.ports);
    if (j.contains("port_groups")) j.at("port_groups").get_to(m.port_groups);
    if (j.contains("aliases")) j.at("aliases").get_to(m.aliases);
}

} // namespace cpptlm