/**
 * @file module_factory.cc
 * @brief ModuleFactory 核心实现 - 双注册表、实例化、拓扑连接、StreamAdapter 注入
 *
 * 本文件实现 ModuleFactory 类的核心流程，负责：
 * - **双注册表管理**：getObjectRegistry() / getModuleRegistry() 分离对象与模块注册
 * - **JSON 拓扑实例化**：instantiateAll() 从配置创建所有模块实例
 * - **端口连接解析**：resolveConnections() 处理 "module.port_index" 语法
 * - **StreamAdapter 注入**：Step 7 为 ChStreamModuleBase 模块注入适配器
 *
 * ## 核心流程（8 步）
 * 1. registerAllObjects()    → 从 REGISTRY 注入所有 SimObject
 * 2. registerAllModules()    → 从 REGISTRY 注入所有 SimModule
 * 3. loadPlugins()           → dlopen 动态加载 .so 插件
 * 4. instantiateAll()        → 从 JSON 配置实例化所有模块
 * 5. resolveConnections()    → 根据 connections 数组连接端口
 * 6. loadGroupTopology()     → 处理 module_groups 层级拓扑
 * 7. injectStreamAdapters()  → 为 ChStreamModuleBase 创建并注入 StreamAdapter
 * 8. startAllTicks()         → 启动所有模块 tick() 循环
 *
 * ## 关键 API
 * - `instantiateAll(config)` — 主入口，执行完整实例化流程
 * - `parsePortSpec(name)` — 解析 "xbar.0" → ("xbar", 0)
 * - `injectStreamAdapters()` — Step 7，创建 ChStreamAdapterFactory 适配器
 * - `stream_adapters_` — vector<unique_ptr<StreamAdapterBase>> 适配器生命周期管理
 *
 * ## 使用注意事项
 * - **显式源文件**：CMakeLists.txt 使用 set(CORE_SOURCES ...) 显式列举，禁用 GLOB
 * - **端口索引语法**：JSON 连接支持 "dst": "xbar.0" 表示模块 xbar 的第 0 端口
 * - **StreamAdapter 生命周期**：由 ModuleFactory::stream_adapters_ 管理，模块析构前自动清理
 * - **插件加载**：loadPlugins() 使用 dlopen/dlsym，需确保 .so 符合 REGISTER_OBJECT 约定
 *
 * @author CppTLM Team
 * @date 2024-05
 * @see module_factory.hh
 * @see core/connection_resolver.hh
 * @see framework/stream_adapter.hh
 */

#include "module_factory.hh"
#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <set>
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/chstream_port.hh"
#include "core/coherence_domain.hh"
#include "core/connection_resolver.hh"
#include "core/load_policy.hh"
#include "core/param_errors.hh"
#include "core/param_parser.hh"
#include "core/plugin_load_exception.hh"
#include "core/plugin_loader.hh"
#include "core/port_compatibility.hh"
#include "core/port_types.hh"
#include "core/topology_parser.hh"
#include "framework/chstream_adapter_factory.hh"
#include "framework/protocol_bridge.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats_manager.hh"
#include "sim_module.hh"
#include "tlm/cep/channel_dependency_graph.hh"
#include "tlm/cep/protocol_family.hh"
#include "tlm/cep/ordering_point_checker.hh"
#include "tlm/router_tlm.hh"
#include "utils/config_utils.hh"
#include "utils/json_includer.hh"
#include "utils/module_group.hh"
#include "utils/regex_matcher.hh"
#include "utils/var_resolver.hh"
#include "utils/wildcard.hh"

using json = nlohmann::json;

json processExtends(const json& config, int depth = 0);
bool validate_nic_pe_connection(const std::string& src_type, unsigned src_port,
                                const std::string& dst_type, unsigned dst_port);
bool check_port_compatibility(const std::string& src_name, const std::string& dst_name,
                              unsigned src_idx, unsigned dst_idx,
                              const std::map<std::string, cpptlm::ModulePortSpec>& port_specs);
std::map<std::string, cpptlm::ModulePortSpec> get_default_port_specs();

// P3.x ASan: clear StatsManager paths, then erase own entries from
// ModuleGroup global registry (so cascade destruction does not see
// stale pointers), then delete own SimObject* instances in a
// second-pass vector to avoid iterator invalidation when SimModule's
// internal_factory sub-factory re-enters ModuleGroup::clearAll() on
// its own destruction. Each factory owns its own SimObjects; the
// global ModuleGroup is just a name->pointer lookup, not an
// ownership structure.
ModuleFactory::~ModuleFactory() {
    for (auto& [name, obj_ptr] : instances) {
        if (!obj_ptr)
            continue;
        if (auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj_ptr.get())) {
            if (auto* sg = ch_mod->get_stats_group()) {
                std::vector<std::string> paths;
                for (const auto& kv : tlm_stats::StatsManager::instance().groups()) {
                    if (kv.second == sg)
                        paths.push_back(kv.first);
                }
                for (const auto& p : paths) {
                    tlm_stats::StatsManager::instance().unregister_group(p);
                }
            }
        }
    }
    // RAII: instances.clear() 触发 unique_ptr 析构 → SimObject 自动 delete,
    // 不再需要 to_release + 手动 delete 循环。
    // ModuleGroup::eraseInstance 必须在 SimObject delete 之前完成, 否则
    // ModuleGroup::clearAllGroups 会在 cascade destruction 中撞到 stale pointer。
    std::vector<std::string> names;
    names.reserve(instances.size());
    for (const auto& [name, _] : instances) {
        names.push_back(name);
    }
    instances.clear();
    for (const auto& name : names) {
        ModuleGroup::eraseInstance(name);
    }
    ModuleGroup::clearAllGroups();
}

std::pair<std::string, std::string> parsePortSpec(const std::string& full_name) {
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) {
        return {full_name, ""};
    }
    return {full_name.substr(0, dot_pos), full_name.substr(dot_pos + 1)};
}

// Phase 3.2: Resolve port alias (T3.2-07)
// Handles deprecated port names like NORTH/EAST/SOUTH/WEST/LOCAL
static std::string resolve_port_alias(const std::string& port_spec) {
    auto it = cpptlm::PortSpec::deprecated_names().find(port_spec);
    if (it != cpptlm::PortSpec::deprecated_names().end()) {
        DPRINTF(CONN, "[WARN] Deprecated port name '%s' resolved to index %u\n", port_spec.c_str(),
                it->second);
        return std::to_string(it->second);
    }
    return port_spec;
}

bool ModuleFactory::instantiateAll(const json& config) {
    json extended_config = processExtends(config);
    if (extended_config.is_object() && extended_config.empty()) {
        DPRINTF(MODULE, "[CONFIG ERROR] extends processing failed\n");
        return false;
    }
    json final_config = JsonIncluder::loadAndInclude(extended_config);

    // ========================
    // 1. 解析变量引用 ${path}
    // ========================
    cpptlm::VarResolver var_resolver(final_config);
    final_config = var_resolver.resolveAll(final_config);

    // ========================
    // 0. JSON Schema 验证（CFG-08）
    // ========================
    if (!validateConfig(final_config)) {
        DPRINTF(MODULE, "[CONFIG ERROR] Schema validation failed, aborting instantiation\n");
        return false;
    }

    // ========================
    // 0.25 CEP 装配期判据(ADR-CEP-09 Decision A + ADR-CEP-08 决策 3/4)
    // ========================
    // 只在 config 带 `cep` 段时触发;无该段 = 本仓绝大多数拓扑不涉及 CEP ⇒ 不打印噪声。
    // 两个挂点(此处 + `validate_topology` 跑的 CLI)调用**同一组**判据函数 —— 禁双实现。
    // 契约:不通过 = 配置 fail(`INV-CEP-09-IC1`),不得以运行时检查替代。
    if (final_config.is_object() && final_config.contains("cep")) {
        const tlm::cep::GateReport channel_gate = tlm::cep::run_channel_gate(final_config);
        const tlm::cep::OrderingReport ordering_gate = tlm::cep::run_ordering_gate(final_config);

        std::fputs(channel_gate.to_log().c_str(), stdout);
        std::fputs(ordering_gate.to_log().c_str(), stdout);
        std::fflush(stdout);

        if (channel_gate.verdict == tlm::cep::GateVerdict::FAIL ||
            ordering_gate.verdict == tlm::cep::GateVerdict::FAIL) {
            DPRINTF(MODULE, "[CEP GATE] assembly criteria failed, aborting instantiation\n");
            return false;
        }
    }

    // ========================
    // 0.5 Hierarchy 树解析 (TGMS v4.0 Phase 4.1)
    // ========================
    if (final_config.contains("hierarchy")) {
        DPRINTF(MODULE, "[HIERARCHY] Parsing hierarchy tree...\n");
        try {
            auto hierarchy_root = cpptlm::parse_hierarchy_tree_with_validation(
                final_config["hierarchy"], final_config.contains("coherence_domains")
                                               ? final_config["coherence_domains"]
                                               : json::array());
            (void)hierarchy_root;
            DPRINTF(MODULE, "[HIERARCHY] Hierarchy tree parsed successfully\n");
        } catch (const cpptlm::TopologyParseException& e) {
            DPRINTF(MODULE, "[HIERARCHY ERROR] %s\n", e.what());
            return false;
        }
    } else {
        DPRINTF(MODULE, "[HIERARCHY] No hierarchy section found in config\n");
    }

    for (auto& mod : final_config["modules"]) {
        std::string type = mod["type"];
        if (mod.contains("params")) {
            // D.3 (cpptlm-dgpu-realip-pin-contract Change D, 2026-10-08):
            // 泛化 RouterTLM 硬编码特例为 ParamRules 注册表查询。
            // 行为与原代码逐字一致 (required → return false; default_int → 注入);
            // RouterTLM 通过 src/tlm/router_tlm.cc 静态自注册。
            // 未注册 type 直接跳过 (原代码等效: `if type == "RouterTLM"` 否则整段跳过)。
            auto& rules_registry = ModuleFactory::getParamRulesRegistry();
            auto it = rules_registry.find(type);
            if (it != rules_registry.end()) {
                auto rules = it->second();
                for (const auto& [name, rule] : rules) {
                    if (!mod["params"].contains(name.c_str())) {
                        if (rule.required) {
                            DPRINTF(
                                MODULE,
                                "[PARAM ERROR] Required parameter '%s' missing for module '%s'\n",
                                name.c_str(), type.c_str());
                            return false;
                        }
                        if (rule.default_int.has_value()) {
                            mod["params"][name] = rule.default_int.value();
                        }
                    }
                }
            }
        }
    }

    // ========================
    // 1.5 解析 port_specs (Phase 3.2: T3.2-04)
    // ========================
    std::map<std::string, cpptlm::ModulePortSpec> port_specs;
    for (const auto& mod : final_config["modules"]) {
        if (!mod.contains("name") || !mod.contains("type"))
            continue;
        std::string name = mod["name"];
        if (mod.contains("port_spec")) {
            try {
                auto spec = mod["port_spec"].get<cpptlm::ModulePortSpec>();
                port_specs[name] = spec;
            } catch (const std::exception& e) {
                DPRINTF(MODULE, "[WARN] Failed to parse port_spec for module '%s': %s\n",
                        name.c_str(), e.what());
            }
        }
    }

    auto default_specs = get_default_port_specs();
    for (const auto& mod : final_config["modules"]) {
        if (!mod.contains("name") || !mod.contains("type"))
            continue;
        std::string name = mod["name"];
        std::string type = mod["type"];
        if (port_specs.find(name) == port_specs.end()) {
            if (default_specs.find(type) != default_specs.end()) {
                port_specs[name] = default_specs[type];
                DPRINTF(MODULE, "[PORT INFO] Using default port spec for module '%s' (type: %s)\n",
                        name.c_str(), type.c_str());
            }
        }
    }

    // 使用 PluginLoader 加载所有插件
    PluginLoader loader;
    if (final_config.contains("plugin")) {
        for (auto& plugin_path : final_config["plugin"]) {
            if (!PluginLoader{}.loadPlugin(plugin_path.get<std::string>(),
                                           LoadPolicy::CRITICAL_ONLY, true)) {
                DPRINTF(MODULE, "[ERROR] Failed to load plugin: %s\n",
                        plugin_path.get<std::string>().c_str());
            }
        }
    }

    // ========================
    // 2. 创建所有模块实例
    // ========================
    // RAII: unique_ptr 持有所有权, 防 Step 4.5 异常路径 (line 379-382 return false)
    // 跳过 line 830 transfer 时 local map 析构 → SimObject 孤儿泄漏。
    // 与 member `instances` 类型一致, line 830 `instances = object_instances`
    // 走 move-assign 即可。
    std::unordered_map<std::string, std::unique_ptr<SimObject>> object_instances;
    std::unordered_map<std::string, SimModule*> module_instances;

    for (auto& mod : final_config["modules"]) {
        if (!mod.contains("name") || !mod.contains("type"))
            continue;
        std::string name = mod["name"];
        std::string type = mod["type"];

        // 尝试在 SimModule 注册表中查找
        auto& module_registry = ModuleFactory::getModuleRegistry();
        auto module_it = module_registry.find(type);
        if (module_it != module_registry.end()) {
            // D.1 (Change D): strict_types 检查 — 若该 type 标 kCompositeOnly
            // (per Oracle bg_c60653e0 §4 PcieAxiBridgeIP precedent), JSON 顶层
            // 实例化视为配置错误, fail-fast (return false + DPRINTF ERROR)。
            // 防止 composite-only 类被误用为顶层模块, 进入半构造状态。
            auto& scope_registry = ModuleFactory::getModuleScopeRegistry();
            auto scope_it = scope_registry.find(type);
            if (scope_it != scope_registry.end() &&
                scope_it->second == ModuleFactory::ModuleInstScope::kCompositeOnly) {
                DPRINTF(MODULE,
                        "[STRICT_TYPES] Type '%s' is composite-only; "
                        "JSON 顶层实例化禁止 (kCompositeOnly, per Oracle bg_c60653e0 §4)\n",
                        type.c_str());
                return false;
            }
            // 这是一个 SimModule。RAII: unique_ptr 持有所有权, module_instances
            // 保存 raw 视图 (非所有权, 用于 Step 4.5/incorporate_parent 迭代)。
            auto new_module = std::unique_ptr<SimModule>(module_it->second(name, event_queue));
            module_instances[name] = new_module.get();
            object_instances[name] = std::move(new_module);
        } else {
            // 在 SimObject 注册表中查找
            auto& object_registry = ModuleFactory::getObjectRegistry();
            auto object_it = object_registry.find(type);
            if (object_it != object_registry.end()) {
                object_instances[name] =
                    std::unique_ptr<SimObject>(object_it->second(name, event_queue));
            } else {
                DPRINTF(MODULE, "[ERROR] Unknown or unregistered type: %s\n", type.c_str());
            }
        }

        // 处理 layout
        if (mod.contains("layout")) {
            auto& l = mod["layout"];
            double x = l.value("x", -1);
            double y = l.value("y", -1);
            if (x >= 0 && y >= 0) {
                auto it = object_instances.find(name);
                if (it != object_instances.end() && it->second) {
                    // unique_ptr::operator-> 返回 raw pointer, setLayout 直接调用
                    it->second->setLayout(x, y);
                }
            }
        }

        const json* cfg_src = mod.contains("params") ? &mod["params"] : nullptr;
        if (!cfg_src && type == "RouterTLM" && mod.contains("node_x"))
            cfg_src = &mod;
        if (!cfg_src && type == "NICTLM" && mod.contains("node_id"))
            cfg_src = &mod;
        if (cfg_src) {
            auto it = object_instances.find(name);
            if (it != object_instances.end() && it->second) {
                it->second->set_config(*cfg_src);
                it->second->on_config_loaded();
                DPRINTF(MODULE, "[CONFIG] Set params for module: %s\n", name.c_str());
            }
        }

        // 注册实例到 ModuleGroup（供通配符展开使用）——使用 find 避免 operator[]
        // 在类型未注册时隐式插入 nullptr 条目。.get() 转 raw pointer 给 registerInstance。
        auto reg_it = object_instances.find(name);
        if (reg_it != object_instances.end() && reg_it->second) {
            ModuleGroup::registerInstance(name, reg_it->second.get());
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
            // 防御性类型检查: config 字段必须是 string (file path)。
            // 如果是 object/array, 用户大概率是把参数 dict 错放在 config 字段
            // (canonical 字段名是 params, 见 openspec/changes/field-name-unification)。
            if (!mod["config"].is_string()) {
                DPRINTF(MODULE,
                        "[CONFIG WARN] module '%s' has 'config' field that is %s; "
                        "expected string (file path). Did you mean 'params' for "
                        "module configuration? Use 'params' field instead.\n",
                        mod["name"].get<std::string>().c_str(), mod["config"].type_name());
                continue;
            }
            std::string name = mod["name"];
            auto* sim_mod = module_instances[name];
            if (sim_mod) {
                std::string config_file = mod["config"];
                std::ifstream f(config_file);
                if (f.is_open()) {
                    json internal_cfg = json::parse(f);
                    sim_mod->instantiate(internal_cfg);
                } else {
                    DPRINTF(MODULE, "[ERROR] Cannot open config: %s\n", config_file.c_str());
                }
            }
        }
    }

    // ========================
    // 4.5. 触发顶层 SimModule 的 simulate_instantiate（处理 inline `modules` 嵌套）
    // ========================
    // Per design.md Decision 1: 顶层 instantiateAll 构造完顶层 SimObject 后,
    // 对每个顶层 SimModule 派生类调 simulate_instantiate(top_cfg),
    // 触发内部子模块 instantiateAll + 递归激活嵌套的孙子 SimModule.
    // 内部递归由 SimModule::simulate_instantiate 自管理(1 次 RTTI / 子模块).
    for (auto& mod : final_config["modules"]) {
        if (!mod.contains("name") || !mod.contains("type"))
            continue;
        std::string name = mod["name"];
        auto sim_it = module_instances.find(name);
        if (sim_it == module_instances.end())
            continue;
        SimModule* sim = sim_it->second;
        if (!sim)
            continue;
        try {
            sim->simulate_instantiate(mod);
        } catch (const std::exception& e) {
            DPRINTF(MODULE, "[ERROR] simulate_instantiate failed for '%s': %s\n", name.c_str(),
                    e.what());
            return false;
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
        if (!conn.contains("src") || !conn.contains("dst"))
            continue;
        std::string conn_key =
            conn["src"].get<std::string>() + "->" + conn["dst"].get<std::string>();
        if (seen_connections.count(conn_key)) {
            int existing_latency = connection_latencies[conn_key];
            int this_latency = conn.value("latency", 0);
            if (this_latency != existing_latency) {
                DPRINTF(CONN,
                        "[WARN] Duplicate connection %s has conflicting latency (first=%d, "
                        "this=%d) - using first\n",
                        conn_key.c_str(), existing_latency, this_latency);
            } else {
                DPRINTF(CONN, "[CONN] Skipped duplicate connection at resolver stage: %s\n",
                        conn_key.c_str());
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
        if (it != object_instances.end() && it->second && it->second->hasPortManager()) {
            auto& pm = it->second->getPortManager();
            SimObject* raw_obj = it->second.get();
            if (is_upstream) {
                pm.addUpstreamPort(raw_obj, {buffer_size}, {}, port);
            } else {
                pm.addDownstreamPort(raw_obj, {buffer_size}, {}, port);
            }
            return true;
        }
        return false;
    };

    auto port_creations =
        resolver.resolveConnections(deduplicated_connections, module_instances, createPortFunc);

    // 创建端口
    for (const auto& info : port_creations) {
        auto it = object_instances.find(info.owner_name);
        if (it != object_instances.end() && it->second && it->second->hasPortManager()) {
            auto& pm = it->second->getPortManager();
            SimObject* raw_obj = it->second.get();

            if (info.is_upstream) {
                auto* port =
                    pm.addUpstreamPort(raw_obj, info.buffer_sizes, info.priorities, info.port_name);
                if (port)
                    port->setDelay(info.latency);
            } else {
                auto* port = pm.addDownstreamPort(raw_obj, info.buffer_sizes, info.priorities,
                                                  info.port_name);
                if (port)
                    port->setDelay(info.latency);
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
        if (!conn.contains("src") || !conn.contains("dst"))
            continue;

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
                    if (obj_it != object_instances.end() && obj_it->second &&
                        obj_it->second->hasPortManager()) {
                        src_port = dynamic_cast<MasterPort*>(
                            obj_it->second->getPortManager().getDownstreamPort(internal_port));
                    }
                }
            } else if (!src_port_name.empty()) {
                if (auto obj_it = object_instances.find(src_module_name);
                    obj_it != object_instances.end() && obj_it->second) {
                    src_port = dynamic_cast<MasterPort*>(
                        obj_it->second->getPortManager().getDownstreamPort(src_port_name));
                }
            } else if (auto obj_it = object_instances.find(src_module_name);
                       obj_it != object_instances.end() && obj_it->second) {
                // Wildcard/group expansion: create default downstream port
                src_port = obj_it->second->getPortManager().addDownstreamPort(
                    obj_it->second.get(), {4}, {}, src_module_name);
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
                        if (obj_it != object_instances.end() && obj_it->second &&
                            obj_it->second->hasPortManager()) {
                            dst_port = dynamic_cast<SlavePort*>(
                                obj_it->second->getPortManager().getUpstreamPort(internal_port));
                        }
                    }
                } else if (!dst_port_name.empty()) {
                    if (auto obj_it = object_instances.find(dst_module_name);
                        obj_it != object_instances.end() && obj_it->second) {
                        dst_port = dynamic_cast<SlavePort*>(
                            obj_it->second->getPortManager().getUpstreamPort(dst_port_name));
                    }
                } else if (auto obj_it = object_instances.find(dst_module_name);
                           obj_it != object_instances.end() && obj_it->second) {
                    // Wildcard/group expansion: create default upstream port
                    dst_port = obj_it->second->getPortManager().addUpstreamPort(
                        obj_it->second.get(), {4}, {}, dst_module_name);
                }

                if (src_port && dst_port) {
                    auto conn_key = std::make_pair(src_full, dst_full);
                    if (processed_connections.count(conn_key)) {
                        DPRINTF(CONN, "[CONN] Skipped duplicate connection %s -> %s\n",
                                src_full.c_str(), dst_full.c_str());
                    } else {
                        processed_connections.insert(conn_key);
                        port_pairs_.push_back(std::make_unique<PortPair>(src_port, dst_port));
                        src_port->setDelay(latency);
                        DPRINTF(CONN, "[CONN] Connected %s -> %s (latency=%d)\n", src_full.c_str(),
                                dst_full.c_str(), latency);
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
    std::unordered_map<std::string, std::vector<ChStreamTargetPtr>> ch_resp_in;
    std::unordered_map<std::string, std::vector<ChStreamTargetPtr>> ch_req_in;
    std::unordered_map<std::string, std::vector<ChStreamInitiatorPtr>> ch_resp_out;

    auto& factory = ChStreamAdapterFactory::get();
    std::unordered_map<std::string, std::string> module_types;
    for (auto& mod : final_config["modules"]) {
        if (mod.contains("name") && mod.contains("type"))
            module_types[mod["name"]] = mod["type"];
    }

    for (auto& [name, obj_ptr] : object_instances) {
        if (!obj_ptr)
            continue;
        auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj_ptr.get());
        if (!ch_mod)
            continue;

        const std::string& type = module_types[name];
        bool is_multi = factory.isMultiPort(type);
        bool is_dual = factory.isDualPort(type);
        unsigned n_ports = is_multi || is_dual ? factory.getPortCount(type) : 1;

        if (!factory.knows(type)) {
            DPRINTF(MODULE, "[ERROR] No adapter factory for ChStream type: %s (%s)\n", type.c_str(),
                    name.c_str());
            continue;
        }

        auto adapter = factory.create(type, obj_ptr.get());
        if (!adapter) {
            DPRINTF(MODULE, "[ERROR] Failed to create adapter for %s (type: %s)\n", name.c_str(),
                    type.c_str());
            continue;
        }

        // 创建 N 组端口
        auto& req_out_vec = ch_req_out[name];
        auto& resp_in_vec = ch_resp_in[name];
        auto& req_in_vec = ch_req_in[name];
        auto& resp_out_vec = ch_resp_out[name];
        req_out_vec.resize(n_ports);
        resp_in_vec.resize(n_ports);
        req_in_vec.resize(n_ports);
        resp_out_vec.resize(n_ports);

        for (unsigned i = 0; i < n_ports; i++) {
            std::string suffix = std::format("[{}]", i);

            req_out_vec[i] = new cpptlm::ChStreamInitiatorPort(
                name + ".req_out" + (n_ports > 1 ? suffix : ""), event_queue);
            resp_in_vec[i] = new cpptlm::ChStreamTargetPort(
                name + ".resp_in" + (n_ports > 1 ? suffix : ""), adapter, event_queue, i);
            req_in_vec[i] = new cpptlm::ChStreamTargetPort(
                name + ".req_in" + (n_ports > 1 ? suffix : ""), adapter, event_queue, i);
            resp_out_vec[i] = new cpptlm::ChStreamInitiatorPort(
                name + ".resp_out" + (n_ports > 1 ? suffix : ""), event_queue);

            ch_initiator_ports_.emplace_back(req_out_vec[i]);
            ch_target_ports_.emplace_back(resp_in_vec[i]);
            ch_target_ports_.emplace_back(req_in_vec[i]);
            ch_initiator_ports_.emplace_back(resp_out_vec[i]);

            // P0 D.1 fix: 让 SimModule::getInternalOutputPort 能查到 ChStream 端口
            // ch_mod 已是 dynamic_cast<ChStreamModuleBase*> 结果, 这里 cast 回 SimObject*
            // 因 ChStreamModuleBase : public SimObject
            if (auto* ch_obj = dynamic_cast<SimObject*>(ch_mod); ch_obj) {
                // 注意: 单端口模块的 port_manager 尚未创建, 直接调用 getPortManager()
                // 触发 lazy-create; 多端口模块首次 mirror 后已存在, 后续调用复用同一实例.
                auto& pm = ch_obj->getPortManager();
                std::string suffix =
                    (n_ports > 1) ? (std::string("[") + std::to_string(i) + "]") : std::string("");
                pm.mirrorExistingDownstreamPort("req_out" + suffix, req_out_vec[i]);
                pm.mirrorExistingUpstreamPort("resp_in" + suffix, resp_in_vec[i]);
                pm.mirrorExistingUpstreamPort("req_in" + suffix, req_in_vec[i]);
                pm.mirrorExistingDownstreamPort("resp_out" + suffix, resp_out_vec[i]);
            }
        }

        // 注入 StreamAdapter（区分单端口 / 多端口 / 双端口）
        if (is_dual) {
            // 双端口非对称：组 0 = PE 侧，组 1 = Network 侧
            auto* dual = static_cast<cpptlm::DualPortStreamAdapter<
                ChStreamModuleBase, bundles::CacheReqBundle, bundles::CacheRespBundle,
                bundles::NoCFlitBundle, bundles::NoCFlitBundle>*>(adapter);
            if (dual) {
                dual->bind_pe_ports(req_out_vec[0], resp_in_vec[0], resp_out_vec[0], req_in_vec[0]);
                dual->bind_net_ports(req_out_vec[1], resp_in_vec[1], resp_out_vec[1],
                                     req_in_vec[1]);
            }
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created DualPort adapter for %s (type: %s, PE+Net)\n",
                    name.c_str(), type.c_str());
        } else if (is_multi) {
            for (unsigned i = 0; i < n_ports; i++) {
                adapter->bind_port_pair(i, req_out_vec[i], resp_in_vec[i], resp_out_vec[i],
                                        req_in_vec[i]);
            }
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created multi-port adapter for %s (%u ports, type: %s)\n",
                    name.c_str(), n_ports, type.c_str());
        } else {
            adapter->bind_ports(req_out_vec[0], resp_in_vec[0], resp_out_vec[0], req_in_vec[0]);
            ch_mod->set_stream_adapter(adapter);
            DPRINTF(MODULE, "[ChStream] Created SinglePort adapter for %s (type: %s)\n",
                    name.c_str(), type.c_str());
        }

        ch_adapters[name] = adapter;
        stream_adapters_.emplace_back(adapter);
    }

    // ========================
    // 7.5 协议桥装配(per `cpptlm-cep-cohif-carrier` Phase 3 / 判据 CC-05)
    // ========================
    //
    // 配置形态(挂在**既有** `cep` 段下 —— **零新增** JSON 顶层段):
    //   "cep": {
    //     "protocol_instances": "include/tlm/cep/protocol_instances.json",
    //     "bridges": [
    //       { "name": "moesi_gpu_cohif", "protocol_instance": "moesi_gpu",
    //         "bridge_id": 1, "modules": ["pe0", "pe1"] }
    //     ]
    //   }
    //
    // 校验(任一不过 ⇒ 装配失败):
    //   ① 配置引用的桥名**未注册**        ⇒ 红(CC-05 ② 的反面:引用了不存在的桥)
    //   ② `protocol_id_` 与实例声明的 `family` 编号**不符** ⇒ 红(CC-05 ④)
    //   ③ 两个被消费的桥 `bridge_id_` **重复**   ⇒ 红(CC-05 ④)
    //   ④ 桥元数据自检不过(`protocol_id_`/`bridge_id_` 为 0) ⇒ 红(CC-05 ④)
    //   ⑤ 目标模块不存在 / 不支持桥挂载        ⇒ 红
    //
    // ⚠️ **共享语义**(per CC-03 ③):一个 bridge 条目的 `modules` 可列**多个**模块
    //    ⇒ **同一个桥实例**被挂到多个 PE ⇒ 满足「同一 `ProtocolBridgeEr*` 可被
    //    多个具体桥实例共享」。`bridge_id_` 唯一性针对的是**桥条目**(桥实例)而非
    //    挂载点 ⇒ 两者不冲突。
    //
    // ⚠️ **「死桥」不判红**(per CC-05 反向 fixture 的形态说明):多拓扑共用同一
    //    进程级注册表时,某个拓扑不引用全部桥是**正常**的 ⇒ 强制失败会误伤。
    //    ⇒ 以**清单**(`unconsumed_bridges()`)暴露,由判据测试断言其非空。
    bool bridge_failed = false;
    {
        consumed_bridges_.clear();
        const bool has_bridge_section =
            final_config.is_object() && final_config.contains("cep") &&
            final_config["cep"].is_object() && final_config["cep"].contains("bridges");
        if (has_bridge_section) {
            const auto& bsec = final_config["cep"]["bridges"];
            if (!bsec.is_array()) {
                std::fprintf(stderr, "[ModuleFactory] `cep.bridges` 必须是数组(实际为 %s)\n",
                             std::string(bsec.type_name()).c_str());
                bridge_failed = true;
            } else {
                // 协议实例声明 —— 与 Step 0.25 的 gate **同一份**解析(不重复实现)
                const tlm::cep::ChannelDeclaration decl =
                    tlm::cep::parse_channel_declaration_from_config(final_config);

                auto& registry = get_bridges();
                std::vector<std::pair<std::uint16_t, std::string>> used_bridge_ids;

                for (const auto& b : bsec) {
                    if (!b.is_object()) {
                        std::fprintf(stderr, "[ModuleFactory] `cep.bridges` 元素必须是对象\n");
                        bridge_failed = true;
                        continue;
                    }
                    const std::string bname =
                        b.contains("name") && b["name"].is_string() ? b["name"].get<std::string>()
                                                                  : std::string();
                    if (bname.empty()) {
                        std::fprintf(stderr, "[ModuleFactory] `cep.bridges` 条目缺少非空的 `name`\n");
                        bridge_failed = true;
                        continue;
                    }
                    std::uint8_t expected_protocol_id = 0;  // 0 = 未声明 protocol_instance
                    const auto it = registry.find(bname);
                    if (it == registry.end()) {
                        std::fprintf(stderr,
                                     "[ModuleFactory] 桥 `%s` 未注册(引用了不存在的桥)\n",
                                     bname.c_str());
                        bridge_failed = true;
                        continue;
                    }
                    cpptlm::ProtocolBridgeEr* raw = it->second ? it->second() : nullptr;
                    if (!raw) {
                        std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 的 factory 返回空指针\n",
                                     bname.c_str());
                        bridge_failed = true;
                        continue;
                    }
                    std::unique_ptr<cpptlm::ProtocolBridgeEr> bridge(raw);

                    // ── `bridge_id`:JSON 显式给,否则自动分配未占用的最小正整数 ──
                    if (b.contains("bridge_id")) {
                        if (!b["bridge_id"].is_number_integer()) {
                            std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 的 `bridge_id` 不是整数\n",
                                         bname.c_str());
                            bridge_failed = true;
                            continue;
                        }
                        const long long v = b["bridge_id"].get<long long>();
                        if (v <= 0 || v > 0xFFFF) {
                            std::fprintf(stderr,
                                         "[ModuleFactory] 桥 `%s` 的 `bridge_id` 越界(须 1..65535)\n",
                                         bname.c_str());
                            bridge_failed = true;
                            continue;
                        }
                        bridge->set_bridge_id(static_cast<std::uint16_t>(v));
                    } else {
                        std::uint16_t cand = 1;
                        while (std::any_of(used_bridge_ids.begin(), used_bridge_ids.end(),
                                           [cand](const std::pair<std::uint16_t, std::string>& e) {
                                               return e.first == cand;
                                           }))
                            ++cand;
                        bridge->set_bridge_id(cand);
                    }

                    // ── `protocol_id` ← 实例声明的 `family` 编号(per CC-05 ④)──
                    if (b.contains("protocol_instance")) {
                        const std::string pid = b["protocol_instance"].is_string()
                                                    ? b["protocol_instance"].get<std::string>()
                                                    : std::string();
                        const tlm::cep::DeclaredInstance* inst = nullptr;
                        for (const auto& di : decl.instances) {
                            if (di.id == pid) {
                                inst = &di;
                                break;
                            }
                        }
                        if (!inst) {
                            std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 引用的协议实例 `%s` 不存在\n",
                                         bname.c_str(), pid.c_str());
                            bridge_failed = true;
                            continue;
                        }
                        tlm::cep::ProtocolFamily fam{};
                        if (!tlm::cep::protocol_family_from_name(inst->family, fam)) {
                            std::fprintf(stderr,
                                         "[ModuleFactory] 桥 `%s` 的实例 `%s` 的 `family` 非法或未声明: `%s`\n",
                                         bname.c_str(), pid.c_str(), inst->family.c_str());
                            bridge_failed = true;
                            continue;
                        }
                        expected_protocol_id = tlm::cep::protocol_family_id(fam);
                        // 桥自带了 `protocol_id_` 且与族编号不符 ⇒ 判据函数会报红;
                        // 未自带 ⇒ 由装配期按族编号填入(避免人肉填错)。
                        if (bridge->protocol_id() == 0)
                            bridge->set_protocol_id(expected_protocol_id);
                    }

                    // ── 元数据自检(per CC-05 ④)—— 调**同一份**判据函数 ──
                    std::string why;
                    if (!validate_bridge_metadata(bridge.get(), expected_protocol_id, &why)) {
                        std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 元数据不合规:%s\n", bname.c_str(),
                                     why.c_str());
                        bridge_failed = true;
                        continue;
                    }

                    // ── `bridge_id_` 唯一(同一 ModuleFactory 内不重复,per CC-05 ④)──
                    if (!check_bridge_id_collision(used_bridge_ids, bridge->bridge_id(), bname,
                                                   &why)) {
                        std::fprintf(stderr, "[ModuleFactory] %s\n", why.c_str());
                        bridge_failed = true;
                        continue;
                    }
                    used_bridge_ids.emplace_back(bridge->bridge_id(), bname);

                    // ── 注入到目标模块(可多个 ⇒ CC-03 ③ 的共享)──
                    if (!b.contains("modules") || !b["modules"].is_array() ||
                        b["modules"].empty()) {
                        std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 缺少非空的 `modules`\n",
                                     bname.c_str());
                        bridge_failed = true;
                        continue;
                    }
                    bool injected = false;
                    for (const auto& m : b["modules"]) {
                        if (!m.is_string()) {
                            std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 的 `modules` 元素不是字符串\n",
                                         bname.c_str());
                            bridge_failed = true;
                            break;
                        }
                        const std::string mname = m.get<std::string>();
                        // ⚠️ 用**局部** `object_instances` —— member `instances` 在本函数
                        //    末尾才 `= std::move(object_instances)`(Step 7.5 之前为空)。
                        const auto mit = object_instances.find(mname);
                        if (mit == object_instances.end()) {
                            std::fprintf(stderr, "[ModuleFactory] 桥 `%s` 的目标模块 `%s` 不存在\n",
                                         bname.c_str(), mname.c_str());
                            bridge_failed = true;
                            break;
                        }
                        auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(mit->second.get());
                        if (!ch_mod) {
                            std::fprintf(stderr,
                                         "[ModuleFactory] 桥 `%s` 的目标模块 `%s` 不是 ChStream 模块\n",
                                         bname.c_str(), mname.c_str());
                            bridge_failed = true;
                            break;
                        }
                        if (!ch_mod->attach_protocol_bridge(bridge.get())) {
                            std::fprintf(stderr,
                                         "[ModuleFactory] 桥 `%s` 的目标模块 `%s` 不支持桥挂载\n",
                                         bname.c_str(), mname.c_str());
                            bridge_failed = true;
                            break;
                        }
                        injected = true;
                    }
                    if (!injected)
                        continue;

                    consumed_bridges_.insert(bname);
                    protocol_bridges_.push_back(std::move(bridge));
                    DPRINTF(MODULE, "[Bridge] Mounted bridge %s (bridge_id=%u protocol_id=%u)\n",
                            bname.c_str(),
                            static_cast<unsigned>(protocol_bridges_.back()->bridge_id()),
                            static_cast<unsigned>(protocol_bridges_.back()->protocol_id()));
                }
            }
        }
    }
    if (bridge_failed) {
        std::fprintf(stderr, "[ModuleFactory] 协议桥装配失败(见上方诊断)\n");
        return false;
    }

    // ========================
    // 8. 自动注册 StatGroup 到 StatsManager（Phase 0 修复）
    // ========================
    for (auto& [name, obj_ptr] : object_instances) {
        if (!obj_ptr)
            continue;
        auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj_ptr.get());
        if (!ch_mod)
            continue;

        auto* stat_group = ch_mod->get_stats_group();
        if (stat_group) {
            std::string stats_path = ch_mod->get_stats_path();
            tlm_stats::StatsManager::instance().register_group(stat_group, stats_path);
            DPRINTF(MODULE, "[Stats] Registered %s -> %s\n", name.c_str(), stats_path.c_str());
        }
    }

    // 7b. 创建 PortPairs（支持端口索引语法：xbar.0 → xbar.req_in[0]）
    // DEF-02: 使用同一个 processed_connections 集合去重（Step 6 已填充）
    bool connection_failed = false;
    for (auto& conn : final_config["connections"]) {
        if (!conn.contains("src") || !conn.contains("dst"))
            continue;
        std::string src_full = conn["src"];
        std::string dst_full = conn["dst"];

        auto conn_key = std::make_pair(src_full, dst_full);
        if (processed_connections.count(conn_key)) {
            DPRINTF(CONN, "[ChStream] Skipped duplicate connection %s -> %s\n", src_full.c_str(),
                    dst_full.c_str());
            continue;
        }

        int latency = conn.value("latency", 0);
        auto [src_name, src_spec_raw] = parsePortSpec(src_full);
        auto [dst_name, dst_spec_raw] = parsePortSpec(dst_full);

        std::string src_spec = resolve_port_alias(src_spec_raw);
        std::string dst_spec = resolve_port_alias(dst_spec_raw);

        unsigned src_idx = 0, dst_idx = 0;
        if (!src_spec.empty() && std::isdigit(src_spec[0])) {
            bool all_digits = std::all_of(src_spec.begin(), src_spec.end(), ::isdigit);
            if (all_digits) {
                src_idx = std::stoul(src_spec);
            } else {
                DPRINTF(CONN,
                        "[WARN] Invalid port index '%s' (expected digits only), defaulting to 0\n",
                        src_spec.c_str());
            }
        }
        if (!dst_spec.empty() && std::isdigit(dst_spec[0])) {
            bool all_digits = std::all_of(dst_spec.begin(), dst_spec.end(), ::isdigit);
            if (all_digits) {
                dst_idx = std::stoul(dst_spec);
            } else {
                DPRINTF(CONN,
                        "[WARN] Invalid port index '%s' (expected digits only), defaulting to 0\n",
                        dst_spec.c_str());
            }
        }

        if (!validate_nic_pe_connection(module_types[src_name], src_idx, module_types[dst_name],
                                        dst_idx)) {
            connection_failed = true;
            continue;
        }

        // 单端口模块忽略端口索引
        if (ch_adapters.count(src_name) && !factory.isMultiPort(module_types[src_name]))
            src_idx = 0;
        if (ch_adapters.count(dst_name) && !factory.isMultiPort(module_types[dst_name]))
            dst_idx = 0;

        bool src_ch = (ch_adapters.count(src_name) > 0 && ch_req_out.count(src_name) &&
                       ch_req_out[src_name].size() > src_idx);
        bool dst_ch = (ch_adapters.count(dst_name) > 0 && ch_req_in.count(dst_name) &&
                       ch_req_in[dst_name].size() > dst_idx);

        if (!src_ch || !dst_ch)
            continue;

        // Phase 3.2: L1/L2/L3 port compatibility check
        if (!check_port_compatibility(src_name, dst_name, src_idx, dst_idx, port_specs)) {
            connection_failed = true;
            continue;
        }

        processed_connections.insert(conn_key);

        // 请求路径: src → dst
        port_pairs_.push_back(std::make_unique<PortPair>(ch_req_out[src_name][src_idx],
                                                         ch_req_in[dst_name][dst_idx]));
        ch_req_out[src_name][src_idx]->setDelay(latency);
        DPRINTF(CONN, "[ChStream] Connected %s.req_out[%u] -> %s.req_in[%u] (latency=%d)\n",
                src_name.c_str(), src_idx, dst_name.c_str(), dst_idx, latency);

        // 响应路径: dst → src
        // P0-5b fix: count(name) 是键存在性(0/1),不是 vector 长度。
        // 旧代码 `count(dst_name) > dst_idx` 在 dst_idx >= 1 时永远 false,
        // 导致 xbar.resp_out[1/2/3] 永远没绑到 cpu1/2/3.resp_in,响应被丢弃。
        if (ch_resp_out.count(dst_name) && ch_resp_out[dst_name].size() > dst_idx &&
            ch_resp_in.count(src_name) && ch_resp_in[src_name].size() > src_idx) {
            port_pairs_.push_back(std::make_unique<PortPair>(ch_resp_out[dst_name][dst_idx],
                                                             ch_resp_in[src_name][src_idx]));
            ch_resp_out[dst_name][dst_idx]->setDelay(latency);
            DPRINTF(CONN, "[ChStream] Connected %s.resp_out[%u] -> %s.resp_in[%u] (latency=%d)\n",
                    dst_name.c_str(), dst_idx, src_name.c_str(), src_idx, latency);
        }
    }

    // ========================
    // 9. P1: 触发 SimModule::incorporate_parent late-binding
    //    对每个顶层 SimModule 调用一次, parent 传 nullptr
    //    ApuSoC 等会重写 incorporate_parent 完成跨域 wiring
    //    (Step 7 已注入 StreamAdapter + D.1 mirror, peer cache req_out 此时可查)
    // ========================
    for (auto& [name, mod] : module_instances) {
        if (!mod)
            continue;
        mod->incorporate_parent(nullptr);
    }

    // 保存所有实例。unique_ptr 非可拷贝, 必须 std::move 触发 move-assign
    // (instances 与 object_instances 类型一致, move 后 object_instances 为空,
    //  栈展开自动析构空 map 无副作用)。
    instances = std::move(object_instances);
    return !connection_failed;
}

bool ModuleFactory::validate_bridge_metadata(const cpptlm::ProtocolBridgeEr* bridge,
                                               std::uint8_t expected_protocol_id,
                                               std::string* why) {
    if (!bridge) {
        if (why)
            *why = "桥指针为空";
        return false;
    }
    std::string local;
    if (!bridge->metadata_ok(&local)) {
        if (why)
            *why = local;
        return false;
    }
    if (expected_protocol_id != 0 && bridge->protocol_id() != expected_protocol_id) {
        if (why) {
            *why = "protocol_id_(" + std::to_string(static_cast<unsigned>(bridge->protocol_id())) +
                   ")与协议族编号(" +
                   std::to_string(static_cast<unsigned>(expected_protocol_id)) + ")不符";
        }
        return false;
    }
    return true;
}

bool ModuleFactory::check_bridge_id_collision(
    const std::vector<std::pair<std::uint16_t, std::string>>& assigned,
    std::uint16_t candidate_id, const std::string& candidate_name, std::string* why) {
    for (const auto& e : assigned) {
        if (e.first == candidate_id) {
            if (why) {
                *why = "bridge_id " + std::to_string(static_cast<unsigned>(candidate_id)) +
                       " 重复(桥 `" + candidate_name + "` 与 `" + e.second + "`)";
            }
            return false;
        }
    }
    return true;
}

std::vector<std::string> ModuleFactory::unconsumed_bridges() const {
    std::vector<std::string> dead;
    for (const auto& entry : get_bridges()) {
        if (!consumed_bridges_.count(entry.first))
            dead.push_back(entry.first);
    }
    std::sort(dead.begin(), dead.end());
    return dead;
}

void ModuleFactory::startAllTicks() {
    for (auto& [name, obj_ptr] : instances) {
        // unique_ptr::operator-> 返回 raw pointer, 直接调 initiate_tick
        obj_ptr->initiate_tick();
        DPRINTF(MODULE, "[MODULE] Started tick for %s\n", name.c_str());
    }
}
