# ADR-X.4: 插件系统

> **版本**: 2.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0/v2.1 - 模块扩展机制

---

## 1. 核心问题

在混合仿真系统中，是否需要支持动态加载模块（插件系统）？

**需要考虑的场景**:
1. 用户自定义模块（无需重新编译核心框架）
2. 第三方模块集成（商业 IP、开源模块）
3. 实验性模块（快速迭代，不稳定）
4. 多版本共存（同一模块的多个版本）

---

## 2. 行业调研

### 2.1 Gem5 的模块扩展机制

**Gem5 方式**: 编译时配置（Python 配置脚本）

```python
# Gem5 配置脚本（Python）
from m5.objects import *

system = System()
system.cpu = [O3CPU()]
system.cache = L1Cache()
system.memory = DRAM()

# 所有模块在编译时链接
# 不支持运行时动态加载
```

**特点**:
- ✅ 类型安全（编译时检查）
- ✅ 性能高（无运行时开销）
- ❌ 需要重新编译
- ❌ 不支持动态加载

---

### 2.2 SystemC 的模块机制

**SystemC 方式**: 静态链接 + 动态库（可选）

```cpp
// SystemC 模块
SC_MODULE(MyModule) {
    SC_CTOR(MyModule) {
        SC_METHOD(process);
        sensitive << clk;
    }
    
    void process() {
        // ...
    }
};

// 动态库加载（用户自行实现）
void* handle = dlopen("libmymodule.so", RTLD_NOW);
MyModule* module = (MyModule*)dlsym(handle, "create_module");
```

**特点**:
- ✅ 支持动态库
- ⚠️ 无标准插件接口
- ⚠️ 用户自行实现加载机制

---

### 2.3 LLVM 的插件系统

**LLVM 方式**: Pass 管理器 + 动态加载

```cpp
// LLVM Pass 插件
struct MyPass : public Pass {
    static char ID;
    MyPass() : Pass(ID) {}
    
    bool runOnFunction(Function &F) override {
        // ...
    }
};

// 注册 Pass
static RegisterPass<MyPass> X("mypass", "My Pass");

// 动态加载
opt -load libMyPass.so -mypass input.bc
```

**特点**:
- ✅ 标准插件接口
- ✅ 支持动态加载
- ✅ 版本兼容检查
- ⚠️ 复杂度较高

---

### 2.4 VSCode 的插件系统

**VSCode 方式**: 扩展 API + 市场

```typescript
// VSCode 扩展
export function activate(context: ExtensionContext) {
    let disposable = vscode.commands.registerCommand(
        'extension.hello',
        () => {
            vscode.window.showInformationMessage('Hello World!');
        }
    );
    context.subscriptions.push(disposable);
}
```

**特点**:
- ✅ 完整插件 API
- ✅ 依赖管理
- ✅ 版本控制
- ❌ 运行时开销大
- ❌ 实现复杂

---

## 3. 需求场景分析

### 场景 1: 用户自定义模块（高频）

```
需求：用户希望添加自定义 Cache 替换策略，无需修改核心框架

当前方案:
- 继承 CacheV2 基类
- 重写 replace_policy() 方法
- 重新编译

插件方案:
- 实现 ICachePolicy 接口
- 编译为动态库
- 配置文件加载

收益分析:
- 开发频率：每周 1-2 次
- 重新编译时间：~30 秒（增量编译）
- 插件收益：避免重新编译核心框架
- 复杂度增加：中等
```

**评估**: 增量编译已足够快，插件系统收益有限

---

### 场景 2: 第三方模块集成（中频）

```
需求：集成商业 IP 模型（如 ARM NIC-400），保护知识产权

当前方案:
- 提供黑盒模块接口
- 静态链接二进制

插件方案:
- 动态库加载
- 接口标准化

收益分析:
- 开发频率：每月 1-2 次
- IP 保护需求：中等
- 插件收益：便于分发
- 复杂度增加：中等
```

**评估**: 静态链接黑盒模块已可满足需求

---

### 场景 3: 实验性模块（中频）

```
需求：快速迭代实验性模块（如新的 coherence 协议）

当前方案:
- 继承基类，快速原型
- 编译为独立可执行文件
- 与主框架通过 socket/共享内存通信

插件方案:
- 动态加载
- 热切换（无需重启仿真）

收益分析:
- 开发频率：每周数次
- 热切换需求：低（通常重启仿真）
- 插件收益：中等
- 复杂度增加：高
```

**评估**: 独立可执行文件 + IPC 已可满足需求

---

### 场景 4: 多版本共存（低频）

```
需求：同一模块的多个版本共存（如 CacheV1 和 CacheV2 对比）

当前方案:
- 命名空间隔离
- 配置文件选择版本

插件方案:
- 动态库版本管理
- 运行时加载指定版本

收益分析:
- 开发频率：偶尔
- 多版本需求：低
- 插件收益：低
- 复杂度增加：高
```

**评估**: 命名空间隔离已可满足需求

---

## 4. 方案对比

| 方案 | 设计 | 优点 | 缺点 | 适用场景 |
|------|------|------|------|---------|
| **A) 无插件** ✅ | 编译时静态链接 | 简单、性能高、类型安全 | 需要重新编译 | ✅ 推荐（v2.0） |
| **B) 动态库** | .so/.dll 加载 | 支持运行时加载 | 跨平台兼容性、版本管理 | 第三方 IP |
| **C) 脚本扩展** | Python/Lua 脚本 | 极灵活、快速迭代 | 性能开销大、类型不安全 | 配置/测试 |
| **D) 混合方案** | 核心静态 + 可选动态 | 平衡灵活性与性能 | 实现复杂 | v2.1+ |

---

## 5. 推荐方案：无插件（v2.0）+ 可选动态库（v2.1）

### 5.1 核心设计

```
┌─────────────────────────────────────────────────────────────┐
│  v2.0: 编译时静态链接                                        │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  所有模块编译为静态库                                 │   │
│  │  - 类型安全                                          │   │
│  │  - 性能高                                            │   │
│  │  - 增量编译快（~30 秒）                               │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  v2.1: 可选动态库支持                                       │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  ModuleFactory + 动态加载                            │   │
│  │  - 第三方 IP 分发                                     │   │
│  │  - 实验性模块                                         │   │
│  │  - 需要时启用                                         │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

### 5.2 v2.0: 静态链接实现

#### 模块注册机制

```cpp
// include/core/module_registry.hh
#ifndef MODULE_REGISTRY_HH
#define MODULE_REGISTRY_HH

#include "sim_object.hh"
#include <map>
#include <functional>

// 模块工厂函数
using ModuleFactoryFunc = std::function<SimObject*(const std::string&)>;

// 模块注册器（编译时注册）
class ModuleRegistry {
private:
    std::map<std::string, ModuleFactoryFunc> factories_;
    
    ModuleRegistry() = default;
    
public:
    static ModuleRegistry& instance() {
        static ModuleRegistry registry;
        return registry;
    }
    
    // 注册模块（宏调用）
    void register_module(const std::string& name, ModuleFactoryFunc factory) {
        factories_[name] = factory;
    }
    
    // 创建模块
    SimObject* create_module(const std::string& name, const std::string& instance_name) {
        if (factories_.count(name)) {
            return factories_[name](instance_name);
        }
        return nullptr;
    }
    
    // 列出所有可用模块
    std::vector<std::string> list_modules() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }
};

// 模块注册宏
#define REGISTER_MODULE(type_name, class_name) \
    static struct type_name##_registrar { \
        type_name##_registrar() { \
            ModuleRegistry::instance().register_module( \
                type_name, \
                [](const std::string& name) { return new class_name(name); } \
            ); \
        } \
    } type_name##_instance;

#endif // MODULE_REGISTRY_HH
```

#### 模块使用示例

```cpp
// include/modules/cache_v2.hh
class CacheV2 : public TLMModule {
public:
    CacheV2(const std::string& n) : TLMModule(n) {}
    // ...
};

// src/modules/cache_v2.cc
#include "cache_v2.hh"
#include "../core/module_registry.hh"

REGISTER_MODULE(cache_v2, CacheV2)
```

#### 配置文件创建模块

```cpp
// main.cpp
int sc_main() {
    // 从配置创建模块
    json config = load_config("system.json");
    
    for (const auto& module_cfg : config["modules"]) {
        std::string type = module_cfg["type"];
        std::string name = module_cfg["name"];
        
        SimObject* module = ModuleRegistry::instance().create_module(type, name);
        if (module) {
            root_modules.push_back(module);
        }
    }
    
    // ...
}
```

```json
// system.json
{
  "modules": [
    {"type": "cache_v2", "name": "l1_cache"},
    {"type": "crossbar_v2", "name": "noc"},
    {"type": "memory_v2", "name": "dram"}
  ]
}
```

---

### 5.3 v2.1: 可选动态库支持

#### 插件接口定义

```cpp
// include/plugin/plugin_interface.hh
#ifndef PLUGIN_INTERFACE_HH
#define PLUGIN_INTERFACE_HH

#include "../core/sim_object.hh"

// 插件元数据
struct PluginMeta {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> dependencies;
};

// 插件接口
class IPlugin {
public:
    virtual ~IPlugin() = default;
    
    // 获取元数据
    virtual PluginMeta get_meta() const = 0;
    
    // 初始化插件
    virtual void init() = 0;
    
    // 创建模块
    virtual SimObject* create_module(const std::string& name) = 0;
    
    // 清理插件
    virtual void cleanup() = 0;
};

// 插件导出宏
#define PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))

// 插件入口函数
#define DEFINE_PLUGIN(meta_struct) \
    PLUGIN_EXPORT IPlugin* create_plugin() { \
        return new meta_struct(); \
    } \
    PLUGIN_EXPORT void destroy_plugin(IPlugin* plugin) { \
        delete plugin; \
    }

#endif // PLUGIN_INTERFACE_HH
```

#### 插件加载器

```cpp
// include/plugin/plugin_loader.hh
#ifndef PLUGIN_LOADER_HH
#define PLUGIN_LOADER_HH

#include "plugin_interface.hh"
#include <vector>
#include <dlfcn.h>

class PluginLoader {
private:
    struct LoadedPlugin {
        void* handle;
        IPlugin* plugin;
        PluginMeta meta;
    };
    
    std::vector<LoadedPlugin> plugins_;
    
public:
    // 加载插件
    bool load(const std::string& path) {
        // 打开动态库
        void* handle = dlopen(path.c_str(), RTLD_NOW);
        if (!handle) {
            DPRINTF(PLUGIN, "Failed to load %s: %s\n", path.c_str(), dlerror());
            return false;
        }
        
        // 获取创建函数
        using CreateFunc = IPlugin*(*)();
        CreateFunc create = (CreateFunc)dlsym(handle, "create_plugin");
        if (!create) {
            DPRINTF(PLUGIN, "No create_plugin symbol in %s\n", path.c_str());
            dlclose(handle);
            return false;
        }
        
        // 创建插件实例
        IPlugin* plugin = create();
        PluginMeta meta = plugin->get_meta();
        
        // 初始化插件
        plugin->init();
        
        // 记录已加载插件
        plugins_.push_back({handle, plugin, meta});
        
        DPRINTF(PLUGIN, "Loaded plugin: %s v%s\n", meta.name.c_str(), meta.version.c_str());
        return true;
    }
    
    // 卸载插件
    void unload(const std::string& plugin_name) {
        for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
            if (it->meta.name == plugin_name) {
                it->plugin->cleanup();
                
                using DestroyFunc = void(*)(IPlugin*);
                DestroyFunc destroy = (DestroyFunc)dlsym(it->handle, "destroy_plugin");
                if (destroy) {
                    destroy(it->plugin);
                }
                
                dlclose(it->handle);
                plugins_.erase(it);
                return;
            }
        }
    }
    
    // 创建模块
    SimObject* create_module(const std::string& plugin_name, const std::string& module_name) {
        for (const auto& loaded : plugins_) {
            if (loaded.meta.name == plugin_name) {
                return loaded.plugin->create_module(module_name);
            }
        }
        return nullptr;
    }
    
    // 列出已加载插件
    std::vector<PluginMeta> list_plugins() const {
        std::vector<PluginMeta> metas;
        for (const auto& loaded : plugins_) {
            metas.push_back(loaded.meta);
        }
        return metas;
    }
    
    ~PluginLoader() {
        // 清理所有插件
        for (auto& loaded : plugins_) {
            loaded.plugin->cleanup();
            dlclose(loaded.handle);
        }
    }
};

#endif // PLUGIN_LOADER_HH
```

#### 插件示例

```cpp
// plugins/custom_cache/custom_cache_plugin.hh
#ifndef CUSTOM_CACHE_PLUGIN_HH
#define CUSTOM_CACHE_PLUGIN_HH

#include "../../include/plugin/plugin_interface.hh"
#include "custom_cache.hh"

struct CustomCachePlugin : public IPlugin {
    PluginMeta get_meta() const override {
        PluginMeta meta;
        meta.name = "custom_cache";
        meta.version = "1.0.0";
        meta.author = "Your Name";
        meta.description = "Custom Cache Replacement Policy";
        return meta;
    }
    
    void init() override {
        // 初始化插件
    }
    
    SimObject* create_module(const std::string& name) override {
        return new CustomCache(name);
    }
    
    void cleanup() override {
        // 清理插件
    }
};

// 导出插件
DEFINE_PLUGIN(CustomCachePlugin)
```

```cpp
// plugins/custom_cache/custom_cache.hh
#ifndef CUSTOM_CACHE_HH
#define CUSTOM_CACHE_HH

#include "../../include/modules/cache_v2.hh"

// 自定义 Cache（LRU 替换策略）
class CustomCache : public CacheV2 {
public:
    CustomCache(const std::string& n) : CacheV2(n) {}
    
    // 重写替换策略
    uint64_t choose_victim() override {
        // LRU 实现
        // ...
    }
};

#endif // CUSTOM_CACHE_HH
```

#### 插件配置

```json
// plugins.json
{
  "plugins": [
    {
      "path": "libcustom_cache.so",
      "enabled": true,
      "config": {
        "replacement_policy": "lru"
      }
    }
  ],
  "modules": [
    {
      "type": "plugin:custom_cache/custom_cache_v2",
      "name": "l1_cache",
      "plugin_config": {
        "replacement_policy": "lru"
      }
    }
  ]
}
```

---

## 6. 实施建议

### 6.1 v2.0: 静态链接（推荐）

**实现内容**:
```cpp
// ModuleRegistry（编译时注册）
REGISTER_MODULE(cache_v2, CacheV2)
REGISTER_MODULE(crossbar_v2, CrossbarV2)
REGISTER_MODULE(memory_v2, MemoryV2)

// 配置文件创建
SimObject* module = ModuleRegistry::instance().create_module(type, name);
```

**优点**:
- ✅ 类型安全（编译时检查）
- ✅ 性能高（无运行时开销）
- ✅ 增量编译快（~30 秒）
- ✅ 跨平台兼容

**缺点**:
- ❌ 需要重新编译（但增量编译快）

**验收标准**:
- [ ] 模块注册机制正常工作
- [ ] 配置文件创建模块
- [ ] 增量编译时间 <30 秒

**预计工期**: 2 天

---

### 6.2 v2.1: 可选动态库（按需实现）

**触发条件**:
- 有第三方 IP 集成需求
- 需要保护知识产权
- 用户强烈要求热加载

**实现内容**:
```cpp
// PluginLoader（运行时加载）
PluginLoader::instance().load("libcustom_cache.so");
SimObject* module = PluginLoader::instance().create_module("custom_cache", "l1_cache");
```

**优点**:
- ✅ 支持运行时加载
- ✅ 第三方 IP 分发方便
- ✅ 热切换（可选）

**缺点**:
- ❌ 跨平台兼容性复杂
- ❌ 版本管理困难
- ❌ 实现复杂度高

**预计工期**: 7 天

---

## 7. 需求优先级评估

| 需求 | 频率 | 当前方案满足度 | 插件系统收益 | 推荐 |
|------|------|---------------|-------------|------|
| 用户自定义模块 | 每周 1-2 次 | 90%（继承 + 增量编译） | 低 | ❌ 不需要 |
| 第三方 IP 集成 | 每月 1-2 次 | 80%（静态链接黑盒） | 中 | ⏳ v2.1 可选 |
| 实验性模块 | 每周数次 | 85%（独立可执行文件） | 中 | ⏳ v2.1 可选 |
| 多版本共存 | 偶尔 | 95%（命名空间隔离） | 低 | ❌ 不需要 |

---

## 8. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: v2.0 是否需要插件？ | A) 需要 / B) 不需要 | **B) 不需要** |
| **Q2**: v2.1 是否添加动态库支持？ | A) 需要 / B) 不需要 / C) 可选 | **C) 可选** |
| **Q3**: 插件接口复杂度？ | A) 简单 / B) 中等 / C) 完整 | **B) 中等** |
| **Q4**: 第三方 IP 需求？ | A) 迫切 / B) 未来 / C) 不需要 | **B) 未来** |
| **Q5**: 实施时机？ | A) v2.0 / B) v2.1 / C) v2.2 | **B) v2.1** |

---

## 9. 与现有架构整合

| 架构 | 整合方式 |
|------|---------|
| **模块系统** | ModuleRegistry 统一管理（静态 + 动态） |
| **配置系统** | JSON 配置支持插件加载 |
| **复位系统** | 插件模块支持 reset()/save_snapshot()/load_snapshot() |

---

## 10. 相关文档

| 文档 | 位置 |
|------|------|
| 模块注册机制 | `include/core/module_registry.hh` |
| 插件接口（v2.1） | `include/plugin/plugin_interface.hh` |
| 插件加载器（v2.1） | `include/plugin/plugin_loader.hh` |

---

## 11. 决策汇总

**v2.0 决策**:
- ✅ 采用静态链接
- ✅ ModuleRegistry 编译时注册
- ✅ 配置文件创建模块
- ❌ 不实现动态加载

**v2.1 决策**（按需）:
- ⏳ 可选动态库支持
- ⏳ 标准插件接口
- ⏳ PluginLoader 运行时加载

---

**下一步**: 请老板确认插件系统方案
