// src/module_factory.cc
#include "module_factory.hh"
#include "sim_module.hh"
#include "core/connection_resolver.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "core/chstream_port.hh"
#include "core/chstream_adapter_factory.hh"
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
                auto obj_it = object_instances.find(src_module_name);
                if (obj_it != object_instances.end() && obj_it->second->hasPortManager()) {
                    src_port = dynamic_cast<MasterPort*>(
                        obj_it->second->getPortManager().getDownstreamPort(src_port_name));
                }
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
                    auto obj_it = object_instances.find(dst_module_name);
                    if (obj_it != object_instances.end() && obj_it->second->hasPortManager()) {
                        dst_port = dynamic_cast<SlavePort*>(
                            obj_it->second->getPortManager().getUpstreamPort(dst_port_name));
                    }
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
    // 7. 为 ChStream 模块注入 StreamAdapter
    // ========================
    // 7a. Create adapter + 4 ports per ChStream module
    std::unordered_map<std::string, cpptlm::StreamAdapterBase*> ch_adapters;
    std::unordered_map<std::string, cpptlm::ChStreamInitiatorPort*> ch_req_out;
    std::unordered_map<std::string, cpptlm::ChStreamTargetPort*>    ch_resp_in;
    std::unordered_map<std::string, cpptlm::ChStreamTargetPort*>    ch_req_in;
    std::unordered_map<std::string, cpptlm::ChStreamInitiatorPort*> ch_resp_out;

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
        auto adapter = ChStreamAdapterFactory::get().create(type, obj);
        if (!adapter) {
            DPRINTF(MODULE, "[ERROR] No adapter factory for ChStream type: %s (%s)\n", type.c_str(), name.c_str());
            continue;
        }

        auto* req_out  = new cpptlm::ChStreamInitiatorPort(name + ".req_out", event_queue);
        auto* resp_in  = new cpptlm::ChStreamTargetPort(name + ".resp_in", adapter, event_queue);
        auto* req_in   = new cpptlm::ChStreamTargetPort(name + ".req_in", adapter, event_queue);
        auto* resp_out = new cpptlm::ChStreamInitiatorPort(name + ".resp_out", event_queue);

        adapter->bind_ports(req_out, resp_in, resp_out, req_in);
        ch_mod->set_stream_adapter(adapter);

        ch_adapters[name] = adapter;
        ch_req_out[name]  = req_out;
        ch_resp_in[name]  = resp_in;
        ch_req_in[name]   = req_in;
        ch_resp_out[name] = resp_out;

        ch_initiator_ports_.emplace_back(req_out);
        ch_target_ports_.emplace_back(resp_in);
        ch_target_ports_.emplace_back(req_in);
        ch_initiator_ports_.emplace_back(resp_out);

        DPRINTF(MODULE, "[ChStream] Created adapter + 4 ports for %s (%s)\n", name.c_str(), type.c_str());
    }

    // 7b. Create PortPairs for ChStream-to-ChStream connections
    for (auto& conn : final_config["connections"]) {
        if (!conn.contains("src") || !conn.contains("dst")) continue;
        std::string src = conn["src"];
        std::string dst = conn["dst"];
        int latency = conn.value("latency", 0);

        if (src.find('.') != std::string::npos) continue;
        if (dst.find('.') != std::string::npos) continue;

        bool src_ch = (ch_adapters.count(src) > 0);
        bool dst_ch = (ch_adapters.count(dst) > 0);

        if (!src_ch || !dst_ch) {
            if (src_ch || dst_ch)
                DPRINTF(CONN, "[WARN] Mixed ChStream/legacy connection: %s -> %s (skipped)\n", src.c_str(), dst.c_str());
            continue;
        }

        auto* pp_req = new PortPair(ch_req_out[src], ch_req_in[dst]);
        ch_req_out[src]->setDelay(latency);
        DPRINTF(CONN, "[ChStream] Connected %s.req_out -> %s.req_in (latency=%d)\n", src.c_str(), dst.c_str(), latency);

        auto* pp_resp = new PortPair(ch_resp_out[dst], ch_resp_in[src]);
        ch_resp_out[dst]->setDelay(latency);
        DPRINTF(CONN, "[ChStream] Connected %s.resp_out -> %s.resp_in (latency=%d)\n", dst.c_str(), src.c_str(), latency);
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
