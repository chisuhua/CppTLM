// include/utils/module_group.hh
#ifndef MODULE_GROUP_HH
#define MODULE_GROUP_HH

#include <unordered_map>
#include <vector>
#include <string>
#include "core/sim_core.hh"
#include "core/sim_object.hh"
#include "utils/wildcard.hh"

class ModuleGroup {
private:
    static std::unordered_map<std::string, std::vector<std::string>>& getGroups() {
        static std::unordered_map<std::string, std::vector<std::string>> groups;
        return groups;
    }

    static std::unordered_map<std::string, SimObject*>& getInstanceRegistry() {
        static std::unordered_map<std::string, SimObject*> instances;
        return instances;
    }

public:
    // 注册模块实例（供通配符展开使用）
    static void registerInstance(const std::string& name, SimObject* obj) {
        getInstanceRegistry()[name] = obj;
        DPRINTF(GROUP, "[Group] Registered instance '%s'\n", name.c_str());
    }

    static void unregisterInstance(const std::string& name) {
        auto it = getInstanceRegistry().find(name);
        if (it != getInstanceRegistry().end()) {
            delete it->second;
            getInstanceRegistry().erase(it);
        }
        DPRINTF(GROUP, "[Group] Unregistered instance '%s'\n", name.c_str());
    }

    // Erase a single name from the instance registry without deleting the
    // SimObject (caller owns the object and is responsible for its lifetime).
    static void eraseInstance(const std::string& name) {
        getInstanceRegistry().erase(name);
    }

    // Clear all groups. Used by ModuleFactory destructor to reset shared
    // group state that may have been polluted by previous tests in the
    // same process.
    static void clearAllGroups() {
        getGroups().clear();
    }

    static bool isInstanceRegistered(const std::string& name) {
        return getInstanceRegistry().count(name) > 0;
    }

    static std::vector<std::string> getRegisteredInstanceNames() {
        std::vector<std::string> names;
        for (const auto& kv : getInstanceRegistry()) {
            names.push_back(kv.first);
        }
        return names;
    }

    // 定义组
    static void define(const std::string& name, const std::vector<std::string>& members) {
        getGroups()[name] = members;
        DPRINTF(GROUP, "[Group] Defined group '%s' with %zu members\n", name.c_str(), members.size());
    }

    // 添加成员
    static void addMember(const std::string& group_name, const std::string& member) {
        getGroups()[group_name].push_back(member);
    }

    // 获取组成员
    static std::vector<std::string> getMembers(const std::string& name) {
        auto it = getGroups().find(name);
        return it != getGroups().end() ? it->second : std::vector<std::string>{};
    }

    // 是否是组引用
    static bool isGroupReference(const std::string& str) {
        return str.rfind("group:", 0) == 0;
    }

    // 解析组名
    static std::string extractGroupName(const std::string& str) {
        if (isGroupReference(str)) {
            return str.substr(6);
        }
        return "";
    }

    // 根据组名获取模块名列表（带通配符展开）
    static std::vector<std::string> resolve(const std::string& group_ref) {
        if (!isGroupReference(group_ref)) return {};
        std::string name = extractGroupName(group_ref);
        auto members = getMembers(name);
        if (members.empty()) return {};

        std::vector<std::string> resolved;
        const auto& instance_map = getInstanceRegistry();

        for (const auto& pattern : members) {
            // 检查是否包含通配符
            if (pattern.find('*') != std::string::npos ||
                pattern.find('?') != std::string::npos) {
                // 通配符展开：遍历所有已注册实例
                for (const auto& [inst_name, obj] : instance_map) {
                    if (Wildcard::match(pattern, inst_name)) {
                        resolved.push_back(inst_name);
                    }
                }
            } else {
                // 无通配符：直接添加
                resolved.push_back(pattern);
            }
        }
        return resolved;
    }

    // clearAll: clears both registries. ModuleFactory::~ModuleFactory
    // is responsible for deleting its own SimObject* instances (not
    // ModuleGroup), so this method is a pure registry reset, not a
    // ownership destructor. Previously this method called `delete obj`
    // for every registered SimObject, which caused a double-free when
    // SimModule's internal_factory sub-factory also re-entered clearAll
    // during cascade destruction: the outer snapshot included child
    // instances that the inner sub-factory had already deleted.
    static void clearAll() {
        getInstanceRegistry().clear();
        getGroups().clear();
    }

    static std::vector<std::string> getAllGroupNames() {
        std::vector<std::string> names;
        for (const auto& kv : getGroups()) {
            names.push_back(kv.first);
        }
        return names;
    }

    // 列出所有组（调试用）
    static void listAllGroups() {
        printf("[ModuleGroup] Defined groups:\n");
        for (const auto& kv : getGroups()) {
            printf("  %s: ", kv.first.c_str());
            for (size_t i = 0; i < kv.second.size(); ++i) {
                printf("%s%s", kv.second[i].c_str(), i == kv.second.size()-1 ? "\n" : ", ");
            }
        }
    }
};
#endif
