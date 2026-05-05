// src/module_factory.cc
#include "module_factory.hh"
#include "sim_module.hh"
#include "core/connection_resolver.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "core/chstream_port.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "tlm/router_tlm.hh"
#include "utils/config_utils.hh"
#include "utils/json_includer.hh"
#include "utils/wildcard.hh"
#include "utils/regex_matcher.hh"
#include "utils/module_group.hh"
#include "core/plugin_load_exception.hh"
#include "core/plugin_loader.hh"
#include "core/load_policy.hh"
#include <fstream>
#include <set>
#include <algorithm>

using json = nlohmann::json;

bool ModuleFactory::_debug_config = false;

std::pair<std::string, std::string> parsePortSpec(const std::string& full_name) {
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) {
        return {full_name, ""};
    }
    return {full_name.substr(0, dot_pos), full_name.substr(dot_pos + 1)};
}

// ============================================================================
// Config Extends Processing (Task 2.1-2.5)
// ============================================================================

static json mergeConfigs(const json& base, const json& child, int depth = 0) {
    if (depth > 10) {
        printf("[CONFIG ERROR] extends depth limit exceeded (possible circular reference)\n");
        return child;
    }

    json result = base;

    // Deep merge modules by name
    if (child.contains("modules")) {
        std::unordered_map<std::string, json> module_map;
        if (result.contains("modules")) {
            for (const auto& mod : result["modules"]) {
                module_map[mod["name"].get<std::string>()] = mod;
            }
        }
        for (const auto& mod : child["modules"]) {
            std::string name = mod["name"].get<std::string>();
            if (module_map.count(name)) {
                // Merge params
                json merged = module_map[name];
                if (mod.contains("params") && merged.contains("params")) {
                    for (const auto& [key, val] : mod["params"].items()) {
                        merged["params"][key] = val;
                    }
                } else if (mod.contains("params")) {
                    merged["params"] = mod["params"];
                }
                module_map[name] = merged;
            } else {
                module_map[name] = mod;
            }
        }
        result["modules"] = json::array();
        for (const auto& [name, mod] : module_map) {
            result["modules"].push_back(mod);
        }
    }

    // Append connections (not merge)
    if (child.contains("connections") && result.contains("connections")) {
        for (const auto& conn : child["connections"]) {
            result["connections"].push_back(conn);
        }
    } else if (child.contains("connections")) {
        result["connections"] = child["connections"];
    }

    if (child.contains("groups")) {
        if (!result.contains("groups")) {
            result["groups"] = json::object();
        }
        for (const auto& [group_name, members] : child["groups"].items()) {
            if (result["groups"].contains(group_name)) {
                for (const auto& m : members) {
                    result["groups"][group_name].push_back(m);
                }
            } else {
                result["groups"][group_name] = members;
            }
        }
    }

    for (const auto& [key, val] : child.items()) {
        if (key == "modules" || key == "connections" || key == "groups" || key == "extends") {
            continue;
        }
        if (!val.is_object() && !val.is_array()) {
            result[key] = val;
        }
    }

    return result;
}

static json processExtends(const json& config, int depth = 0) {
    if (depth > 10) {
        printf("[CONFIG ERROR] extends depth limit exceeded (possible circular reference)\n");
        return json::object();
    }

    if (!config.contains("extends")) {
        return config;
    }

    std::string extends_path = config["extends"].get<std::string>();
    if (ModuleFactory::debug_config()) {
        for (int i = 0; i < depth; ++i) printf("  ");
        printf("[DEBUG] Processing extends: %s (depth: %d)\n", extends_path.c_str(), depth);
    }

    std::ifstream f(extends_path);
    if (!f.is_open()) {
        printf("[CONFIG ERROR] Cannot open extends file: %s\n", extends_path.c_str());
        return json::object();
    }

    json base_config;
    try {
        base_config = json::parse(f);
    } catch (const json::parse_error& e) {
        printf("[CONFIG ERROR] Failed to parse extends file '%s': %s\n", extends_path.c_str(), e.what());
        return json::object();
    }
    f.close();

    // Recursively process extends in base config
    json processed_base = processExtends(base_config, depth + 1);
    if (processed_base.is_object() && processed_base.empty()) {
        return json::object();
    }

    // Merge child over base
    json result = mergeConfigs(processed_base, config, depth);

    if (result.contains("extends")) {
        result.erase("extends");
    }

    return result;
}

// ============================================================================
// JSON Schema 验证器（CFG-08）
// ============================================================================
bool ModuleFactory::validateConfig(const json& config) {
    // 1. 检查顶层必需字段
    if (!config.contains("modules")) {
        printf("[CONFIG ERROR] Missing required field 'modules'\n");
        return false;
    }
    if (!config["modules"].is_array()) {
        printf("[CONFIG ERROR] Field 'modules' must be an array\n");
        return false;
    }

    if (!config.contains("connections")) {
        printf("[CONFIG ERROR] Missing required field 'connections'\n");
        return false;
    }
    if (!config["connections"].is_array()) {
        printf("[CONFIG ERROR] Field 'connections' must be an array\n");
        return false;
    }

    // version 字段可选，缺失时警告
    if (!config.contains("version")) {
        DPRINTF(MODULE, "[CONFIG WARN] Missing optional field 'version'\n");
    }

    // 2. 检查每个模块的必需字段
    for (const auto& mod : config["modules"]) {
        // name 字段检查
        if (!mod.contains("name")) {
            printf("[CONFIG ERROR] Module missing required field 'name'\n");
            return false;
        }
        if (!mod["name"].is_string()) {
            printf("[CONFIG ERROR] Module field 'name' must be a string\n");
            return false;
        }
        std::string name = mod["name"].get<std::string>();

        // type 字段检查
        if (!mod.contains("type")) {
            printf("[CONFIG ERROR] Module '%s' missing required field 'type'\n", name.c_str());
            return false;
        }
        if (!mod["type"].is_string()) {
            printf("[CONFIG ERROR] Module '%s' field 'type' must be a string\n", name.c_str());
            return false;
        }
        std::string type = mod["type"].get<std::string>();

const json* params_src = nullptr;
        if (mod.contains("params")) params_src = &mod["params"];
        else if (type == "RouterTLM" && mod.contains("node_x")) params_src = &mod;
        else if (type == "NICTLM" && mod.contains("node_id")) params_src = &mod;

        if (params_src) {
            const auto& params = *params_src;
            if (type == "RouterTLM") {
                for (auto p : {"node_x", "node_y", "mesh_x", "mesh_y"}) {
                    if (!params.contains(p) || !params[p].is_number_integer()) {
                        printf("[CONFIG ERROR] Module '%s' missing/invalid '%s'\n", name.c_str(), p);
                        return false;
                    }
                }
            }
            if (type == "NICTLM") {
                if (!params.contains("node_id") || !params["node_id"].is_number_integer()) {
                    printf("[CONFIG ERROR] Module '%s' missing/invalid 'node_id'\n", name.c_str());
                    return false;
                }
            }
        } else if (type == "RouterTLM" || type == "NICTLM") {
            printf("[CONFIG ERROR] Module '%s' missing required params\n", name.c_str());
            return false;
        }
    }

    DPRINTF(MODULE, "[CONFIG] Schema validation passed\n");
    return true;
}

bool ModuleFactory::instantiateAll(const json& config) {
    json extended_config = processExtends(config);
    if (extended_config.is_object() && extended_config.empty()) {
        DPRINTF(MODULE, "[CONFIG ERROR] extends processing failed\n");
        return false;
    }
    json final_config = JsonIncluder::loadAndInclude(extended_config);

    // ========================
    // 0. JSON Schema 验证（CFG-08）
    // ========================
    if (!validateConfig(final_config)) {
        DPRINTF(MODULE, "[CONFIG ERROR] Schema validation failed, aborting instantiation\n");
        return false;
    }

    // 使用 PluginLoader 加载所有插件
    PluginLoader loader;
    if (final_config.contains("plugin")) {
        for (auto& plugin_path : final_config["plugin"]) {
            if (!PluginLoader{}.loadPlugin(plugin_path.get<std::string>(), LoadPolicy::CRITICAL_ONLY, true)) {
                printf("[ERROR] Failed to load plugin: %s\n", plugin_path.get<std::string>().c_str());
            }
        }
    }

    // ========================
    // 2. 创建所有模块实例
    // ========================
    std::unordered_map<std::string, SimObject*> object_instances;
    std::unordered_map<std::string, SimModule*> module_instances;

    for (auto& mod : final_config["modules"]) {
        if (!mod.contains("name") || !mod.contains("type")) continue;
        std::string name = mod["name"];
        std::string type = mod["type"];

        // 尝试在 SimModule 注册表中查找
        auto& module_registry = ModuleFactory::getModuleRegistry();
        auto module_it = module_registry.find(type);
        if (module_it != module_registry.end()) {
            // 这是一个 SimModule
            SimModule* new_module = module_it->second(name, event_queue);
            object_instances[name] = new_module;
            module_instances[name] = new_module;
        } else {
            // 在 SimObject 注册表中查找
            auto& object_registry = ModuleFactory::getObjectRegistry();
            auto object_it = object_registry.find(type);
            if (object_it != object_registry.end()) {
                object_instances[name] = object_it->second(name, event_queue);
            } else {
                printf("[ERROR] Unknown or unregistered type: %s\n", type.c_str());
            }
        }

        // 处理 layout
        if (mod.contains("layout")) {
            auto& l = mod["layout"];
            double x = l.value("x", -1);
            double y = l.value("y", -1);
            if (x >= 0 && y >= 0) {
                object_instances[name]->setLayout(x, y);
            }
        }

        const json* cfg_src = mod.contains("params") ? &mod["params"] : nullptr;
        if (!cfg_src && type == "RouterTLM" && mod.contains("node_x")) cfg_src = &mod;
        if (!cfg_src && type == "NICTLM" && mod.contains("node_id")) cfg_src = &mod;
        if (cfg_src) {
            auto* obj = object_instances[name];
            if (obj) {
                obj->set_config(*cfg_src);
                obj->on_config_loaded();
                DPRINTF(MODULE, "[CONFIG] Set params for module: %s\n", name.c_str());
            }
        }

        // 注册实例到 ModuleGroup（供通配符展开使用）
        if (object_instances[name]) {
            ModuleGroup::registerInstance(name, object_instances[name]);
        }
    }

    // ========================
    // 3. 解析 groups
    // ========================
    if (final_config.contains("groups")) {
        for (auto& [group_name, members] : final_config["groups"].items()) {
            std::vector<std::string> member_list;
            for (auto& m : members) {
                member_list.push_back(m.get<std::string>());
            }
            ModuleGroup::define(group_name, member_list);
        }
    }

    // ========================
    // 4. 实例化 SimModule 内部配置
    // ========================
    for (auto& mod : final_config["modules"]) {
        if (mod.contains("config")) {
            std::string name = mod["name"];
            auto* sim_mod = module_instances[name];
            if (sim_mod) {
                std::string config_file = mod["config"];
                std::ifstream f(config_file);
                if (f.is_open()) {
                    json internal_cfg = json::parse(f);
                    sim_mod->instantiate(internal_cfg);
                } else {
                    printf("[ERROR] Cannot open config: %s\n", config_file.c_str());
                }
            }
        }
    }

// ========================
    // 5. 使用 ConnectionResolver 处理 connections
    // ========================

    // DEF-02: 在 ConnectionResolver 之前去重 connections
    json deduplicated_connections = json::array();
    std::set<std::string> seen_connections;
    std::map<std::string, int> connection_latencies;
    for (const auto& conn : final_config["connections"]) {
        if (!conn.contains("src") || !conn.contains("dst")) continue;
        std::string conn_key = conn["src"].get<std::string>() + "->" + conn["dst"].get<std::string>();
        if (seen_connections.count(conn_key)) {
            int existing_latency = connection_latencies[conn_key];
            int this_latency = conn.value("latency", 0);
            if (this_latency != existing_latency) {
                DPRINTF(CONN, "[WARN] Duplicate connection %s has conflicting latency (first=%d, this=%d) - using first\n",
                        conn_key.c_str(), existing_latency, this_latency);
            } else {
                DPRINTF(CONN, "[CONN] Skipped duplicate connection at resolver stage: %s\n", conn_key.c_str());
            }
            continue;
        }
        seen_connections.insert(conn_key);
        connection_latencies[conn_key] = conn.value("latency", 0);
        deduplicated_connections.push_back(conn);
    }

    ConnectionResolver resolver;
    
    // 简化的端口创建函数
    auto createPortFunc = [&object_instances](const std::string& owner, const std::string& port, 
                                               size_t buffer_size, bool is_upstream) -> bool {
        auto it = object_instances.find(owner);
        if (it != object_instances.end() && it->second->hasPortManager()) {
            auto& pm = it->second->getPortManager();
            if (is_upstream) {
                pm.addUpstreamPort(it->second, {buffer_size}, {}, port);
            } else {
                pm.addDownstreamPort(it->second, {buffer_size}, {}, port);
            }
            return true;
        }
        return false;
    };
    
    auto port_creations = resolver.resolveConnections(
        deduplicated_connections,
        module_instances,
        createPortFunc
    );
    
    // 创建端口
    for (const auto& info : port_creations) {
        auto it = object_instances.find(info.owner_name);
        if (it != object_instances.end() && it->second->hasPortManager()) {
            auto& pm = it->second->getPortManager();
            
            if (info.is_upstream) {
                pm.addUpstreamPort(it->second, info.buffer_sizes, info.priorities, info.port_name);
            } else {
                pm.addDownstreamPort(it->second, info.buffer_sizes, info.priorities, info.port_name);
            }
        }
    }

    // ========================
    // 6. 建立连接
    // ========================
    std::unordered_map<std::string, size_t> src_indices;
    std::unordered_map<std::string, size_t> dst_indices;
    std::set<std::pair<std::string, std::string>> processed_connections;

    for (auto& conn : deduplicated_connections) {
        if (!conn.contains("src") || !conn.contains("dst")) continue;

        std::string src_spec = conn["src"];
        std::string dst_spec = conn["dst"];
        int latency = conn.value("latency", 0);
        json exclude_list = conn.value("exclude", json::array());

        std::vector<std::string> src_names, dst_names;

        // 处理通配符和组连接
        if (ModuleGroup::isGroupReference(src_spec)) {
            src_names = ModuleGroup::resolve(src_spec);
        } else if (RegexMatcher::isRegexPattern(src_spec) || Wildcard::match("*", src_spec)) {
            for (auto& [name, obj] : object_instances) {
                if (RegexMatcher::match(src_spec, name)) {
                    src_names.push_back(name);
                }
            }
        } else {
            src_names.push_back(src_spec);
        }

        if (ModuleGroup::isGroupReference(dst_spec)) {
            dst_names = ModuleGroup::resolve(dst_spec);
        } else if (RegexMatcher::isRegexPattern(dst_spec) || Wildcard::match("*", dst_spec)) {
            for (auto& [name, obj] : object_instances) {
                if (RegexMatcher::match(dst_spec, name)) {
                    dst_names.push_back(name);
                }
            }
        } else {
            dst_names.push_back(dst_spec);
        }

        src_names = filterExcluded(src_names, exclude_list);
        dst_names = filterExcluded(dst_names, exclude_list);

        for (const std::string& src_full : src_names) {
            auto [src_module_name, src_port_name] = parsePortSpec(src_full);
            
            MasterPort* src_port = nullptr;
            if (auto mod_it = module_instances.find(src_module_name); 
                mod_it != module_instances.end() && !src_port_name.empty()) {
                
                std::string internal_path = mod_it->second->findInternalPath(src_port_name);
                if (!internal_path.empty()) {
                    auto [internal_owner, internal_port] = parsePortSpec(internal_path);
                    auto obj_it = object_instances.find(internal_owner);
                    if (obj_it != object_instances.end() && obj_it->second->hasPortManager()) {
                        src_port = dynamic_cast<MasterPort*>(
                            obj_it->second->getPortManager().getDownstreamPort(internal_port));
                    }
                }
            } else if (!src_port_name.empty()) {
                if (auto obj_it = object_instances.find(src_module_name);
                    obj_it != object_instances.end()) {
                    src_port = dynamic_cast<MasterPort*>(
                        obj_it->second->getPortManager().getDownstreamPort(src_port_name));
                }
            } else if (auto obj_it = object_instances.find(src_module_name);
                       obj_it != object_instances.end()) {
                // Wildcard/group expansion: create default downstream port
                src_port = obj_it->second->getPortManager().addDownstreamPort(
                    obj_it->second, {4}, {}, src_module_name);
            }

            for (const std::string& dst_full : dst_names) {
                auto [dst_module_name, dst_port_name] = parsePortSpec(dst_full);
                
                SlavePort* dst_port = nullptr;
                if (auto mod_it = module_instances.find(dst_module_name); 
                    mod_it != module_instances.end() && !dst_port_name.empty()) {
                    
                    std::string internal_path = mod_it->second->findInternalPath(dst_port_name);
                    if (!internal_path.empty()) {
                        auto [internal_owner, internal_port] = parsePortSpec(internal_path);
                        auto obj_it = object_instances.find(internal_owner);
                        if (obj_it != object_instances.end() && obj_it->second->hasPortManager()) {
                            dst_port = dynamic_cast<SlavePort*>(
                                obj_it->second->getPortManager().getUpstreamPort(internal_port));
                        }
                    }
                } else if (!dst_port_name.empty()) {
                    if (auto obj_it = object_instances.find(dst_module_name);
                        obj_it != object_instances.end()) {
                        dst_port = dynamic_cast<SlavePort*>(
                            obj_it->second->getPortManager().getUpstreamPort(dst_port_name));
                    }
                } else if (auto obj_it = object_instances.find(dst_module_name);
                           obj_it != object_instances.end()) {
                    // Wildcard/group expansion: create default upstream port
                    dst_port = obj_it->second->getPortManager().addUpstreamPort(
                        obj_it->second, {4}, {}, dst_module_name);
                }

                if (src_port && dst_port) {
                    auto conn_key = std::make_pair(src_full, dst_full);
                    if (processed_connections.count(conn_key)) {
                        DPRINTF(CONN, "[CONN] Skipped duplicate connection %s -> %s\n",
                                src_full.c_str(), dst_full.c_str());
                    } else {
                        processed_connections.insert(conn_key);
                        new PortPair(src_port, dst_port);
                        src_port->setDelay(latency);
                        DPRINTF(CONN, "[CONN] Connected %s -> %s (latency=%d)\n",
                                src_full.c_str(), dst_full.c_str(), latency);
                    }
                } else if (!src_port) {
                    DPRINTF(CONN, "[WARN] Source port not found: %s\n", src_full.c_str());
                } else if (!dst_port) {
                    DPRINTF(CONN, "[WARN] Destination port not found: %s\n", dst_full.c_str());
                }
            }
        }
    }
    
    // ========================
    // 7. 为 ChStream 模块注入 StreamAdapter（多端口感知）
    // ========================
    // 7a. 为每个 ChStream 模块创建适配器和端口
    using ChStreamInitiatorPtr = cpptlm::ChStreamInitiatorPort*;
    using ChStreamTargetPtr = cpptlm::ChStreamTargetPort*;
    std::unordered_map<std::string, cpptlm::StreamAdapterBase*> ch_adapters;
    std::unordered_map<std::string, std::vector<ChStreamInitiatorPtr>> ch_req_out;
    std::unordered_map<std::string, std::vector<ChStreamTargetPtr>>    ch_resp_in;
    std::unordered_map<std::string, std::vector<ChStreamTargetPtr>>    ch_req_in;
    std::unordered_map<std::string, std::vector<ChStreamInitiatorPtr>> ch_resp_out;

    auto& factory = ChStreamAdapterFactory::get();
    std::unordered_map<std::string, std::string> module_types;
    for (auto& mod : final_config["modules"]) {
        if (mod.contains("name") && mod.contains("type"))
            module_types[mod["name"]] = mod["type"];
    }

    for (auto& [name, obj] : object_instances) {
        if (!obj) continue;
        auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj);
        if (!ch_mod) continue;

        const std::string& type = module_types[name];
        bool is_multi = factory.isMultiPort(type);
        bool is_dual = factory.isDualPort(type);
        unsigned n_ports = is_multi || is_dual ? factory.getPortCount(type) : 1;

        if (!factory.knows(type)) {
            DPRINTF(MODULE, "[ERROR] No adapter factory for ChStream type: %s (%s)\n", type.c_str(), name.c_str());
            continue;
        }

        auto adapter = factory.create(type, obj);
        if (!adapter) {
            DPRINTF(MODULE, "[ERROR] Failed to create adapter for %s (type: %s)\n", name.c_str(), type.c_str());
            continue;
        }

        // 创建 N 组端口
        auto& req_out_vec  = ch_req_out[name];
        auto& resp_in_vec  = ch_resp_in[name];
        auto& req_in_vec   = ch_req_in[name];
        auto& resp_out_vec = ch_resp_out[name];
        req_out_vec.resize(n_ports);
        resp_in_vec.resize(n_ports);
        req_in_vec.resize(n_ports);
        resp_out_vec.resize(n_ports);

        for (unsigned i = 0; i < n_ports; i++) {
            char suffix[16];
            snprintf(suffix, sizeof(suffix), "[%u]", i);
            
            req_out_vec[i]  = new cpptlm::ChStreamInitiatorPort(name + ".req_out"  + (n_ports > 1 ? suffix : ""), event_queue);
            resp_in_vec[i]  = new cpptlm::ChStreamTargetPort(name + ".resp_in"  + (n_ports > 1 ? suffix : ""), adapter, event_queue);
            req_in_vec[i]   = new cpptlm::ChStreamTargetPort(name + ".req_in"   + (n_ports > 1 ? suffix : ""), adapter, event_queue);
            resp_out_vec[i] = new cpptlm::ChStreamInitiatorPort(name + ".resp_out" + (n_ports > 1 ? suffix : ""), event_queue);

            ch_initiator_ports_.emplace_back(req_out_vec[i]);
            ch_target_ports_.emplace_back(resp_in_vec[i]);
            ch_target_ports_.emplace_back(req_in_vec[i]);
            ch_initiator_ports_.emplace_back(resp_out_vec[i]);
        }

        // 注入 StreamAdapter（区分单端口 / 多端口 / 双端口）
        if (is_dual) {
            // 双端口非对称：组 0 = PE 侧，组 1 = Network 侧
            auto* dual = static_cast<cpptlm::DualPortStreamAdapter<ChStreamModuleBase,
                bundles::CacheReqBundle, bundles::CacheRespBundle,
                bundles::CacheReqBundle, bundles::CacheRespBundle>*>(adapter);
            if (dual) {
                dual->bind_pe_ports(req_out_vec[0], resp_in_vec[0], resp_out_vec[0], req_in_vec[0]);
                dual->bind_net_ports(req_out_vec[1], resp_in_vec[1], resp_out_vec[1], req_in_vec[1]);
            }
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created DualPort adapter for %s (type: %s, PE+Net)\n", name.c_str(), type.c_str());
        } else if (is_multi) {
            if (type == "RouterTLM") {
                auto* bi_adapter = static_cast<cpptlm::BidirectionalPortAdapter<tlm::RouterTLM,
                    bundles::NoCFlitBundle, tlm::RouterTLM::NUM_PORTS>*>(adapter);
                for (unsigned i = 0; i < n_ports; i++) {
                    bi_adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i], resp_out_vec[i], req_in_vec[i]);
                }
            }
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created BidirectionalPortAdapter for %s (%u ports, type: %s)\n", name.c_str(), n_ports, type.c_str());
        } else {
            adapter->bind_ports(req_out_vec[0], resp_in_vec[0], resp_out_vec[0], req_in_vec[0]);
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created SinglePort adapter for %s (type: %s)\n", name.c_str(), type.c_str());
        }

        ch_adapters[name] = adapter;
        stream_adapters_.emplace_back(adapter);
    }

    // 7b. 创建 PortPairs（支持端口索引语法：xbar.0 → xbar.req_in[0]）
    // DEF-02: 使用同一个 processed_connections 集合去重（Step 6 已填充）
    for (auto& conn : final_config["connections"]) {
        if (!conn.contains("src") || !conn.contains("dst")) continue;
        std::string src_full = conn["src"];
        std::string dst_full = conn["dst"];

        auto conn_key = std::make_pair(src_full, dst_full);
        if (processed_connections.count(conn_key)) {
            DPRINTF(CONN, "[ChStream] Skipped duplicate connection %s -> %s\n",
                    src_full.c_str(), dst_full.c_str());
            continue;
        }

        int latency = conn.value("latency", 0);
        auto [src_name, src_spec] = parsePortSpec(src_full);
        auto [dst_name, dst_spec] = parsePortSpec(dst_full);

        unsigned src_idx = 0, dst_idx = 0;
        if (!src_spec.empty() && std::isdigit(src_spec[0])) {
            bool all_digits = std::all_of(src_spec.begin(), src_spec.end(), ::isdigit);
            if (all_digits) src_idx = std::stoul(src_spec);
        }
        if (!dst_spec.empty() && std::isdigit(dst_spec[0])) {
            bool all_digits = std::all_of(dst_spec.begin(), dst_spec.end(), ::isdigit);
            if (all_digits) dst_idx = std::stoul(dst_spec);
        }

        // 单端口模块忽略端口索引
        if (ch_adapters.count(src_name) && !factory.isMultiPort(module_types[src_name])) src_idx = 0;
        if (ch_adapters.count(dst_name) && !factory.isMultiPort(module_types[dst_name])) dst_idx = 0;

        bool src_ch = (ch_adapters.count(src_name) > 0 && ch_req_out.count(src_name) && ch_req_out[src_name].size() > src_idx);
        bool dst_ch = (ch_adapters.count(dst_name) > 0 && ch_req_in.count(dst_name) && ch_req_in[dst_name].size() > dst_idx);

        if (!src_ch || !dst_ch) continue;

        processed_connections.insert(conn_key);

        // 请求路径: src → dst
        auto* pp_req = new PortPair(ch_req_out[src_name][src_idx], ch_req_in[dst_name][dst_idx]);
        (void)pp_req;  // suppress unused warning
        ch_req_out[src_name][src_idx]->setDelay(latency);
        DPRINTF(CONN, "[ChStream] Connected %s.req_out[%u] -> %s.req_in[%u] (latency=%d)\n", src_name.c_str(), src_idx, dst_name.c_str(), dst_idx, latency);

        // 响应路径: dst → src
        if (ch_resp_out.count(dst_name) > dst_idx && ch_resp_in.count(src_name) > src_idx) {
            auto* pp_resp = new PortPair(ch_resp_out[dst_name][dst_idx], ch_resp_in[src_name][src_idx]);
            (void)pp_resp;  // suppress unused warning
            ch_resp_out[dst_name][dst_idx]->setDelay(latency);
            DPRINTF(CONN, "[ChStream] Connected %s.resp_out[%u] -> %s.resp_in[%u] (latency=%d)\n", dst_name.c_str(), dst_idx, src_name.c_str(), src_idx, latency);
        }
    }
    
    // 保存所有实例
    instances = object_instances;
    return true;
}

void ModuleFactory::startAllTicks() {
    for (auto& [name, obj] : instances) {
        obj->initiate_tick();
        DPRINTF(MODULE, "[MODULE] Started tick for %s\n", name.c_str());
    }
}
