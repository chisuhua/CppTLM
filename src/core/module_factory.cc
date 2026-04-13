// src/module_factory.cc
#include "module_factory.hh"
#include "sim_module.hh"
#include "core/connection_resolver.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "core/chstream_port.hh"
#include "core/chstream_adapter_factory.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "utils/config_utils.hh"
#include "utils/json_includer.hh"
#include "utils/wildcard.hh"
#include "utils/regex_matcher.hh"
#include "utils/module_group.hh"
#include "core/plugin_load_exception.hh"
#include "core/plugin_loader.hh"
#include "core/load_policy.hh"
#include <fstream>

using json = nlohmann::json;

std::pair<std::string, std::string> parsePortSpec(const std::string& full_name) {
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) {
        return {full_name, ""};
    }
    return {full_name.substr(0, dot_pos), full_name.substr(dot_pos + 1)};
}

void ModuleFactory::instantiateAll(const json& config) {
    json final_config = JsonIncluder::loadAndInclude(config);

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
        final_config["connections"], 
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

    for (auto& conn : final_config["connections"]) {
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
                    new PortPair(src_port, dst_port);
                    src_port->setDelay(latency);
                    DPRINTF(CONN, "[CONN] Connected %s -> %s (latency=%d)\n",
                            src_full.c_str(), dst_full.c_str(), latency);
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
        unsigned n_ports = is_multi ? factory.getPortCount(type) : 1;

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

        // 注入 StreamAdapter
        if (is_multi) {
            std::vector<cpptlm::StreamAdapterBase*> adapter_vec(n_ports, adapter);
            ch_mod->set_stream_adapter(adapter_vec.data());
            for (unsigned i = 0; i < n_ports; i++) {
                auto* mp = static_cast<cpptlm::MultiPortStreamAdapter<void, bundles::CacheReqBundle, bundles::CacheRespBundle, 4>*>(adapter);
                if (mp) {
            // 多端口：通过 bind_port_pair 逐端口绑定（Phase 5 已验证）
        // 请求路径: ModuleFactory → ch_req_out[src_idx] → ch_req_in[dst_idx] → Module
        // 响应路径: Module → ch_resp_out[dst_idx] → ch_resp_in[src_idx] → upstream
                }
            }
            DPRINTF(MODULE, "[ChStream] Created MultiPort adapter for %s (%u ports, type: %s)\n", name.c_str(), n_ports, type.c_str());
        } else {
            adapter->bind_ports(req_out_vec[0], resp_in_vec[0], resp_out_vec[0], req_in_vec[0]);
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created SinglePort adapter for %s (type: %s)\n", name.c_str(), type.c_str());
        }

        ch_adapters[name] = adapter;
        stream_adapters_.emplace_back(adapter);
    }

    // 7b. 创建 PortPairs（支持端口索引语法：xbar.0 → xbar.req_in[0]）
    for (auto& conn : final_config["connections"]) {
        if (!conn.contains("src") || !conn.contains("dst")) continue;
        std::string src_full = conn["src"];
        std::string dst_full = conn["dst"];
        int latency = conn.value("latency", 0);
        auto [src_name, src_spec] = parsePortSpec(src_full);
        auto [dst_name, dst_spec] = parsePortSpec(dst_full);

        unsigned src_idx = 0, dst_idx = 0;
        if (!src_spec.empty() && std::isdigit(src_spec[0])) src_idx = std::stoul(src_spec);
        if (!dst_spec.empty() && std::isdigit(dst_spec[0])) dst_idx = std::stoul(dst_spec);

        // 单端口模块忽略端口索引
        if (ch_adapters.count(src_name) && !factory.isMultiPort(module_types[src_name])) src_idx = 0;
        if (ch_adapters.count(dst_name) && !factory.isMultiPort(module_types[dst_name])) dst_idx = 0;

        bool src_ch = (ch_adapters.count(src_name) > 0 && ch_req_out.count(src_name) && ch_req_out[src_name].size() > src_idx);
        bool dst_ch = (ch_adapters.count(dst_name) > 0 && ch_req_in.count(dst_name) && ch_req_in[dst_name].size() > dst_idx);

        if (!src_ch || !dst_ch) continue;

        // 请求路径: src → dst
        auto* pp_req = new PortPair(ch_req_out[src_name][src_idx], ch_req_in[dst_name][dst_idx]);
        ch_req_out[src_name][src_idx]->setDelay(latency);
        DPRINTF(CONN, "[ChStream] Connected %s.req_out[%u] -> %s.req_in[%u] (latency=%d)\n", src_name.c_str(), src_idx, dst_name.c_str(), dst_idx, latency);

        // 响应路径: dst → src
        if (ch_resp_out.count(dst_name) > dst_idx && ch_resp_in.count(src_name) > src_idx) {
            auto* pp_resp = new PortPair(ch_resp_out[dst_name][dst_idx], ch_resp_in[src_name][src_idx]);
            ch_resp_out[dst_name][dst_idx]->setDelay(latency);
            DPRINTF(CONN, "[ChStream] Connected %s.resp_out[%u] -> %s.resp_in[%u] (latency=%d)\n", dst_name.c_str(), dst_idx, src_name.c_str(), src_idx, latency);
        }
    }
    
    // 保存所有实例
    instances = object_instances;
}

void ModuleFactory::startAllTicks() {
    for (auto& [name, obj] : instances) {
        obj->initiate_tick();
        DPRINTF(MODULE, "[MODULE] Started tick for %s\n", name.c_str());
    }
}
