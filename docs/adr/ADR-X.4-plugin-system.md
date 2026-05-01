# ADR-X.4: 插件系统

> **版本**: 3.0
> **日期**: 2026-04-09
> **状态**: ✅ 已实施
> **影响**: v2.0 - 模块注册与动态加载机制

---

## 1. 核心决策

**v2.0 采用编译时静态注册方案**，通过 `ModuleFactory` 的模板方法实现模块类型注册。

**结论**: 增量编译（~30 秒）已满足快速迭代需求，完整的动态插件系统（v2.1）按需实现。

---

## 2. 行业调研（参考）

### 2.1 方案对比

| 方案 | 类型安全 | 性能 | 灵活性 | 复杂度 |
|------|---------|------|--------|--------|
| **A) 静态链接** | ✅ 编译期检查 | 高 | 低（需重编译） | 低 |
| **B) 动态库** | ⚠️ 运行时检查 | 高 | 高（运行时加载） | 中 |
| **C) 脚本扩展** | ❌ 无类型检查 | 低 | 最高 | 中 |
| **D) 混合方案** | ✅ 部分保证 | 中 | 高 | 高 |

**推荐**: v2.0 采用 A) + B) 可选（按需）

---

## 3. 需求场景分析

| 场景 | 频率 | 当前满足度 | 推荐 |
|------|------|-----------|------|
| 用户自定义模块 | 每周 1-2 次 | 90%（继承 + 增量编译） | ✅ 不需要插件 |
| 第三方 IP 集成 | 每月 1-2 次 | 80%（静态链接黑盒） | ⏳ v2.1 可选 |
| 实验性模块 | 每周数次 | 85%（独立可执行文件 + IPC） | ⏳ v2.1 可选 |
| 多版本共存 | 偶尔 | 95%（命名空间隔离） | ✅ 不需要插件 |

---

## 4. 实际实现：模块注册机制（v2.0）

### 4.1 核心架构

代码中存在**三套注册机制**，分别承担不同职责：

```
┌─────────────────────────────────────────────────────────────┐
│  1. ModuleFactory (核心)                                     │
│     ├── registerObject<T>(name)    → SimObject 派生类         │
│     └── registerModule<T>(name)    → SimModule 派生类         │
│     位置: include/core/module_factory.hh                     │
├─────────────────────────────────────────────────────────────┤
│  2. modules.hh (Legacy 宏)                                   │
│     ├── REGISTER_OBJECT            → 注册 CPUSim               │
│     └── REGISTER_MODULE           → 注册 CpuCluster            │
│     位置: include/modules.hh                                  │
├─────────────────────────────────────────────────────────────┤
│  3. chstream_register.hh (ChStream 宏)                      │
│     ├── REGISTER_CHSTREAM        → 注册 10 个 TLM 模块        │
│     └── REGISTER_ALL             → REGISTER_OBJECT + REGISTER_CHSTREAM │
│     位置: include/chstream_register.hh                       │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 API 参考

#### 4.2.1 ModuleFactory 模板方法

```cpp
// include/core/module_factory.hh

// 注册 SimObject 派生类
template<typename T>
static void registerObject(const std::string& name) {
    auto& registry = getObjectRegistry();
    registry[name] = [](const std::string& n, EventQueue* eq) -> SimObject* {
        return new T(n, eq);
    };
}

// 注册 SimModule 派生类
template<typename T>
static void registerModule(const std::string& name) {
    static_assert(std::is_base_of_v<SimModule, T>, "T must derive from SimModule");
    auto& registry = getModuleRegistry();
    registry[name] = [](const std::string& n, EventQueue* eq) -> SimModule* {
        return new T(n, eq);
    };
}
```

#### 4.2.2 便捷宏

```cpp
// include/modules.hh - Legacy 模块
REGISTER_OBJECT;    // 注册 CPUSim
REGISTER_MODULE;     // 注册 CpuCluster

// include/chstream_register.hh - ChStream 模块
REGISTER_CHSTREAM;   // 注册 10 个 TLM 模块 + StreamAdapter
REGISTER_ALL;       // REGISTER_OBJECT + REGISTER_CHSTREAM
```

#### 4.2.3 使用示例

```cpp
// main.cpp
#include "modules.hh"
#include "chstream_register.hh"

REGISTER_ALL;  // 注册全部模块类型

// JSON 配置创建
json config = load_config("system.json");
ModuleFactory factory(event_queue);
factory.instantiateAll(config);  // 批量实例化
```

#### 4.2.4 配置格式

```json
{
  "modules": [
    {"type": "CacheTLM", "name": "l1_cache", "params": {...}},
    {"type": "CrossbarTLM", "name": "noc", "params": {...}},
    {"type": "MemoryTLM", "name": "dram", "params": {...}}
  ]
}
```

### 4.3 双注册表机制

`ModuleFactory` 内部维护两个独立的注册表：

```cpp
// SimObject 注册表 → Legacy 模块（CacheSim, CPUSim, MemorySim 等）
static std::unordered_map<std::string, CreateSimObjectFunc>& getObjectRegistry();

// SimModule 注册表 → 带 Layout 特性的模块（CpuCluster 等）
static std::unordered_map<std::string, CreateSimModuleFunc>& getModuleRegistry();
```

两者的**区别**在于 create 函数的返回类型：
- `CreateSimObjectFunc` → `SimObject*`
- `CreateSimModuleFunc` → `SimModule*`

---

## 5. 实际实现：动态加载（部分）

### 5.1 当前状态

`PluginLoader` 支持基本的 dlopen 加载，但**缺少标准化的 IPlugin 接口**：

| 特性 | ADR 设计 | 实际实现 |
|------|---------|---------|
| 插件接口 | `IPlugin` 抽象基类 | ❌ 不存在 |
| 插件元数据 | `PluginMeta` 结构体 | ❌ 不存在 |
| 导出宏 | `PLUGIN_EXPORT` + `DEFINE_PLUGIN` | ❌ 不存在 |
| 加载能力 | dlopen + dlsym | ✅ dlopen（仅检查 `registerType`） |
| 卸载能力 | `unload()` + cleanup | ❌ 不支持 |
| 策略控制 | 无 | ✅ `LoadPolicy` (BEST_EFFORT/STRICT/CRITICAL_ONLY) |

### 5.2 v2.1 可选扩展方案

如果未来需要完整的动态插件支持，建议采用**最小化 C 函数入口方案**：

```cpp
// include/plugin/plugin_interface.hh (候选 v2.1)
extern "C" {
    // 插件唯一入口：插件 .so 的初始化函数
    // 调用方：ModuleFactory::loadPlugin()
    // 返回值：0 成功，非 0 失败
    using PluginInitFunc = int(*)(ModuleFactory*);
}

// 插件侧实现示例
extern "C" int plugin_init(ModuleFactory* factory) {
    factory->registerObject<CustomCache>("CustomCache");
    return 0;
}
```

**优点**：
- 插件不需要定义 C++ 类，只需导出一个 C 函数
- 直接复用 `ModuleFactory` 的注册 API，无新接口
- ABI 稳定（C 函数签名不随 C++ 编译器变化）

---

## 6. 实际实现 vs 文档设计差异

| 组件 | 文档旧设计 | 实际实现 | 状态 |
|------|-----------|---------|------|
| **模块注册器** | `ModuleRegistry` 单例 | `ModuleFactory` 内部 static 注册表 | ✅ 已实现 |
| **注册 API** | 函数指针 `register_module()` | 模板 `registerObject<T>()` / `registerModule<T>()` | ✅ 已实现 |
| **双注册表** | 未设计 | SimObject + SimModule 分离 | ✅ 已实现 |
| **注册宏** | `REGISTER_MODULE(type, class)` | `REGISTER_ALL` / `REGISTER_CHSTREAM` | ✅ 已实现 |
| **批量创建** | 逐个 `create_module()` | JSON 批量 `instantiateAll()` | ✅ 已实现 |
| **IPlugin 接口** | C++ 抽象基类 | 不存在 | ⏳ v2.1 可选 |
| **版本兼容检查** | `PluginMeta` 版本字段 | 不存在 | ⏳ v2.2 候选 |

---

## 7. 决策汇总

**v2.0 决策**:
- ✅ 采用静态链接
- ✅ `ModuleFactory` 双注册表（SimObject + SimModule）
- ✅ `registerObject<T>()` / `registerModule<T>()` 模板方法
- ✅ `REGISTER_ALL` / `REGISTER_CHSTREAM` 便捷宏
- ✅ JSON 配置批量实例化

**v2.1 决策**（按需）:
- ⏳ 可选动态库支持（基于 C 函数入口方案）
- ⏳ 完整插件系统暂缓

**不需要插件的理由**:
- 增量编译 ~30 秒已足够快
- 当前无明确的第三方 IP 集成需求
- v2.1 动态加载可以基于 C 函数入口实现，无需 IPlugin C++ 接口

---

## 8. 相关文档

| 文档 | 位置 | 说明 |
|------|------|------|
| 模块工厂 | `include/core/module_factory.hh` | 核心注册 API |
| Legacy 宏 | `include/modules.hh` | REGISTER_OBJECT/REGISTER_MODULE |
| ChStream 宏 | `include/chstream_register.hh` | REGISTER_CHSTREAM/REGISTER_ALL |
| 动态加载器 | `include/core/plugin_loader.hh` | dlopen 基础支持 |
| 动态加载实现 | `src/utils/dynamic_loader.cc` | 实际加载逻辑 |

---

## 9. 架构优势

| 特性 | 说明 |
|------|------|
| **类型安全** | 模板 `registerObject<T>()` 在编译期检查 T 是否派生自 SimObject |
| **注册与适配器一体化** | `REGISTER_CHSTREAM` 同时注册模块类型和 StreamAdapter |
| **分阶段实例化** | `instantiateAll()` 的 7 阶段流程（验证→加载→创建→分组→内部配置→连接→适配器注入） |
| **策略化动态加载** | `LoadPolicy` (BEST_EFFORT/STRICT/CRITICAL_ONLY) 支持不同加载策略 |

---

## 10. 长期演进方向（v3.0+ 候选）

| 方向 | 触发条件 | 复杂度 |
|------|---------|--------|
| 热加载/热切换 | 仿真运行时替换模块 | 高 |
| 插件版本管理 | 多团队开发，ABI 兼容需求 | 中 |
| 插件依赖图 | 插件之间有依赖关系 | 中 |
| Python 脚本扩展 | 配置/测试层脚本化 | 低 |
| 插件市场/分发 | 第三方生态 | 高 |

---

**状态**: ✅ 已实施（v2.0）<br>
**最后更新**: 2026-05-01<br>
**下次评审**: v2.1 需求明确时
