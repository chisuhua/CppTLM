# src/core/ — 核心实现

**域**: ModuleFactory + ConnectionResolver + PluginLoader（3 文件，745 行）
**作用**: 仿真框架核心逻辑：双注册表、实例化、拓扑连接、插件加载

## 文件

| 文件 | 行数 | 作用 |
|------|------|------|
| `module_factory.cc` | 528 | ModuleFactory 双注册表、instantiateAll、Step 7 StreamAdapter 注入 |
| `connection_resolver.cc` | 137 | ConnectionResolver 拓扑连线解析 |
| `plugin_loader.cc` | 80 | PluginLoader SO/DLL 动态加载 |

## ModuleFactory 核心流程

```
Step 1: registerAllObjects()      → 从 REGISTRY 全局注册表注入所有对象
Step 2: registerAllModules()      → 从 REGISTRY 全局注册表注入所有模块
Step 3: loadPlugins()             → dlopen 动态加载 .so
Step 4: instantiateAll(config)    → JSON 配置实例化所有模块
Step 5: resolveConnections()      → 根据 connections 数组连接端口
Step 6: loadGroupTopology()       → 处理 module_groups 层级
Step 7: injectStreamAdapters()    → ChStreamAdapterFactory 创建并注入适配器
Step 8: startAllTicks()           → 启动所有模块 tick() 循环
```

## 关键符号（module_factory.cc）

- `getObjectRegistry()` / `getModuleRegistry()` — 双注册表
- `parsePortSpec(name)` — 解析 `"xbar.0"` → `("xbar", "0")`
- `stream_adapters_` — `vector<unique_ptr<StreamAdapterBase>>`
- `ChStreamAdapterFactory::createAdapter(type)` — 创建适配器

## 约定

- **显式源文件**: CMakeLists.txt 使用 `set(CORE_SOURCES ...)` 显式列举
- **历史遗留**: `packet.cc` 和 `noc/*.cc` 暂不编译（与当前头文件不兼容）
- **JSON Schema 验证**: `validateConfig()` 检查 modules/connections 必需字段

## 注意事项

- `module_factory.cc` 中 `instantiateAll` 是主入口，~528 行包含完整流程
- ConnectionResolver 处理 `"dst": "xbar.0"` 端口索引语法
- PluginLoader 使用 `dlopen`/`dlsym` 加载共享库