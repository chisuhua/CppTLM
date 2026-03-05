// src/module_factory.cc
#include "module_factory.hh"
#include "sim_module.hh"
#include "utils/config_utils.hh"
#include "utils/json_includer.hh"
#include "utils/wildcard.hh"
#include "utils/regex_matcher.hh"
#include "utils/module_group.hh"
#include "core/plugin_load_exception.hh"
#include "core/plugin_loader.hh"
#include "core/load_policy.hh"

using json = nlohmann::json;

// 辅助函数：从 instance_name.port_name 中提取模块名和端口名
std::pair<std::string, std::string> parsePortSpec(const std::string& full_name) {
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) {
        return {full_name, ""}; // 没有 .，整个字符串都是模块名
    }
    return {full_name.substr(0, dot_pos), full_name.substr(dot_pos + 1)};
}

void ModuleFactory::instantiateAll(const json& config) {
    json final_config = JsonIncluder::loadAndInclude(config);

    // ========================
    // 1. 集中加载所有插件
    // ========================
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
            // 找到，这是一个 SimModule
            SimModule* new_module = module_it->second(name, event_queue);
            object_instances[name] = new_module;
            module_instances[name] = new_module;
        } else {
            // 否则，在 SimObject 注册表中查找
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
    // 5. 处理 connections 并创建端口
    // ========================
    // 记录需要创建端口的信息
    struct PortCreationInfo {
        std::string owner_name;
        std::string port_name;
        std::vector<size_t> buffer_sizes;
        std::vector<size_t> priorities;
        bool is_upstream; // true表示上游端口，false表示下游端口
        
        PortCreationInfo(const std::string& owner, const std::string& port, 
                        const std::vector<size_t>& sizes, const std::vector<size_t>& pri, bool upstream)
            : owner_name(owner), port_name(port), buffer_sizes(sizes), priorities(pri), is_upstream(upstream) {}
    };

    std::vector<PortCreationInfo> port_creations;

    // 处理普通连接和SimModule暴露端口的连接
    for (auto& conn : final_config["connections"]) {
        if (!conn.contains("src") || !conn.contains("dst")) continue;
        
        std::string src_spec = conn["src"];
        std::string dst_spec = conn["dst"];
        int latency = conn.value("latency", 0);

        // 解析源端口
        auto [src_module_name, src_port_name] = parsePortSpec(src_spec);
        
        // 检查源端口是否属于SimModule的暴露端口
        if (auto mod_it = module_instances.find(src_module_name); 
            mod_it != module_instances.end() && !src_port_name.empty()) {
            
            std::string internal_path = mod_it->second->findInternalPath(src_port_name);
            if (!internal_path.empty()) {
                // 这是一个暴露的输出端口，需要为其创建下游端口
                auto [internal_owner, internal_port] = parsePortSpec(internal_path);
                
                // 获取连接参数
                std::vector<size_t> out_sizes = {4}; // 默认值
                std::vector<size_t> priorities = {};
                
                if (conn.contains("output_buffer_sizes")) {
                    for (auto& size : conn["output_buffer_sizes"]) {
                        out_sizes.push_back(size.get<size_t>());
                    }
                }
                if (conn.contains("vc_priorities")) {
                    for (auto& pri : conn["vc_priorities"]) {
                        priorities.push_back(pri.get<size_t>());
                    }
                }
                
                port_creations.emplace_back(internal_owner, internal_port, out_sizes, priorities, false);
            }
        } else if (!src_port_name.empty()) {
            // 普通模块的下游端口
            std::vector<size_t> out_sizes = {4};
            std::vector<size_t> priorities = {};
            
            if (conn.contains("output_buffer_sizes")) {
                for (auto& size : conn["output_buffer_sizes"]) {
                    out_sizes.push_back(size.get<size_t>());
                }
            }
            if (conn.contains("vc_priorities")) {
                for (auto& pri : conn["vc_priorities"]) {
                    priorities.push_back(pri.get<size_t>());
                }
            }
            
            port_creations.emplace_back(src_module_name, src_port_name, out_sizes, priorities, false);
        }

        // 解析目标端口
        auto [dst_module_name, dst_port_name] = parsePortSpec(dst_spec);
        
        // 检查目标端口是否属于SimModule的暴露端口
        if (auto mod_it = module_instances.find(dst_module_name); 
            mod_it != module_instances.end() && !dst_port_name.empty()) {
            
            std::string internal_path = mod_it->second->findInternalPath(dst_port_name);
            if (!internal_path.empty()) {
                // 这是一个暴露的输入端口，需要为其创建上游端口
                auto [internal_owner, internal_port] = parsePortSpec(internal_path);
                
                // 获取连接参数
                std::vector<size_t> in_sizes = {4};
                std::vector<size_t> priorities = {};
                
                if (conn.contains("input_buffer_sizes")) {
                    for (auto& size : conn["input_buffer_sizes"]) {
                        in_sizes.push_back(size.get<size_t>());
                    }
                }
                if (conn.contains("vc_priorities")) {
                    for (auto& pri : conn["vc_priorities"]) {
                        priorities.push_back(pri.get<size_t>());
                    }
                }

                
                port_creations.emplace_back(internal_owner, internal_port, in_sizes, priorities, true);
            }
        } else if (!dst_port_name.empty()) {
            // 普通模块的上游端口
            std::vector<size_t> in_sizes = {4};
            std::vector<size_t> priorities = {};
            
            if (conn.contains("input_buffer_sizes")) {
                for (auto& size : conn["input_buffer_sizes"]) {
                    in_sizes.push_back(size.get<size_t>());
                }
            }
            
            port_creations.emplace_back(dst_module_name, dst_port_name, in_sizes, priorities, true);
        }
    }

    // 创建端口
    for (const auto& info : port_creations) {
        auto it = object_instances.find(info.owner_name);
        if (it != object_instances.end() && it->second->hasPortManager()) {
            auto& pm = it->second->getPortManager();
            
            if (info.is_upstream) {
                // 创建上游端口（接收请求）
                pm.addUpstreamPort(it->second, info.buffer_sizes, info.priorities, info.port_name);
                DPRINTF(CONN, "[CONN] Created upstream port %s.%s\n", 
                        info.owner_name.c_str(), info.port_name.c_str());
            } else {
                // 创建下游端口（发送响应）
                pm.addDownstreamPort(it->second, info.buffer_sizes, info.priorities, info.port_name);
                DPRINTF(CONN, "[CONN] Created downstream port %s.%s\n", 
                        info.owner_name.c_str(), info.port_name.c_str());
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
            
            // 查找实际的源端口
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
                
                // 查找实际的目标端口
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
}
// src/module_factory.cc
// 在 ModuleFactory 类定义中添加方法实现
void ModuleFactory::startAllTicks() {
    for (auto& [name, obj] : instances) {
        obj->initiate_tick();
        DPRINTF(MODULE, "[MODULE] Started tick for %s\n", name.c_str());
    }
}
