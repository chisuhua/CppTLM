# include/ — 头文件总览

**域**: C++ 头文件（C++17）
**约定**: 所有 `.hh` 头文件在此，`src/` 仅放 `.cc` 实现
**子域**: core(核心框架), tlm(V2.1 模块), framework(流适配器), bundles(消息定义), modules(Legacy), utils(工具), ext(插件), sc_core(SimC 兼容)

## 关键入口

| 文件 | 作用 |
|------|------|
| `core/sim_object.hh` | 所有模块基类（Packet, TransactionInfo, SimObject, ResetConfig） |
| `core/module_factory.hh` | 模块工厂：双注册表(registerObject/registerModule), instantiateAll |
| `core/chstream_module.hh` | ChStreamModuleBase — TLM 模块的统一类型标识 |
| `chstream_register.hh` | REGISTER_CHSTREAM 宏（注册 TLM 模块 + StreamAdapter + 多端口适配器） |
| `modules.hh` | REGISTER_OBJECT 宏（注册 Legacy 模块）+ REGISTER_MODULE 宏 |

## 注册宏体系

### 宏清单

| 宏 | 入口文件 | 派生自 | 注册到 |
|----|----------|--------|--------|
| `REGISTER_OBJECT` | `include/modules.hh` | `SimObject`（含 Legacy `CPUSim` 与 `ChStreamModuleBase` 子树） | `getObjectRegistry()` |
| `REGISTER_MODULE` | `include/modules.hh` | `SimModule`（如 `CpuCluster`） | `getModuleRegistry()` |
| `REGISTER_CHSTREAM` | `include/chstream_register.hh` | `ChStreamModuleBase`（如 `CacheTLM`/`CrossbarTLM`/`MemoryTLM`） | `getObjectRegistry()` + `ChStreamAdapterFactory` |
| `REGISTER_ALL` | `include/chstream_register.hh` | 复合宏 | `REGISTER_OBJECT; REGISTER_CHSTREAM` |

### 设计意图

注册宏体系是模块工厂（`ModuleFactory`）与具体模块类之间的唯一耦合点。设计动机：

1. **解耦类注册与实例化**：模块类只需在 `*.hh` 中通过对应宏注册一次，`ModuleFactory::instantiateAll()` 即可从 JSON 拓扑配置按名称构造实例，模块类无需依赖工厂实现。
2. **静态多态 + 动态分发**：宏内 lambda `[](const std::string& n, EventQueue* eq) -> SimObject* { return new T(n, eq); }` 把具体类 `T` 的构造点烧入注册表，运行时通过字符串名查表创建，避免每个模块写显式 if/else 分支。
3. **双注册表分离（SimObject vs SimModule）**：两种基类的构造签名、生命周期、布局语义不同，强行合并会导致：
   - `registerModule<T>` 必须 `static_assert(std::is_base_of_v<SimModule, T>)`，而 ChStream 模块（继承 `ChStreamModuleBase` → `SimObject`）不满足；
   - `SimModule` 子树引入 Layout 特性，与 `SimObject` 通用调度器职责混淆；
   - `unregister/clear/getRegisteredTypes` 必须做联合遍历，增加迭代开销。
   因此采用 `getObjectRegistry()` 与 `getModuleRegistry()` 两个独立 `unordered_map`，看似"不对称"，实为类型系统强制约束，非设计冗余。
4. **ChStreamModuleBase 必须用 `REGISTER_OBJECT`**：因 `ChStreamModuleBase` 继承 `SimObject`（非 `SimModule`），其 StreamAdapter 通过 `ChStreamAdapterFactory` 单独注册，两者解耦但同一对象通过 `REGISTER_CHSTREAM` 复合宏一次性完成。
5. **Legacy 模块编译守卫**：`REGISTER_OBJECT`/`REGISTER_MODULE` 当前总是展开为 `CPUSim`/`CpuCluster` 的注册代码；计划在 `BUILD_LEGACY_MODULES=OFF`（CMake 默认）时退化为 no-op（详见 wave0-t04 §6.3，本任务仅文档化意图，不改宏体）。

### 使用顺序

```cpp
#include "modules.hh"            // REGISTER_OBJECT + REGISTER_MODULE
#include "chstream_register.hh"  // REGISTER_CHSTREAM + REGISTER_ALL
```

`REGISTER_ALL` = `REGISTER_OBJECT; REGISTER_CHSTREAM`（`chstream_register.hh:91`），一键注册 Legacy + ChStream 全量。

## 约定差异

- **头文件后缀**: `.hh` 而非 `.h`
- **include 路径**: `include/` 直接作为 PUBLIC include dir，支持 `#include "core/xxx.hh"` 和 `#include "xxx.hh"`（无 core/ 前缀兼容旧代码）
- **命名空间**: 大部分代码在全局命名空间（非 cpptlm），仅 StreamAdapter 在 `cpptlm::`
- **DPRINTF 宏**: 编译期 `-DDEBUG_PRINT` 控制的日志，`DPRINTF(MODULE, "fmt", args...)`

## 注意事项

- `include/core/` 同时作为 include 目录添加到 CMake target，允许无 `core/` 前缀 `#include "packet.hh"` — 新旧代码共存
- 添加新 TLM 模块时：`include/tlm/*.hh` + `REGISTER_CHSTREAM` 宏更新
- 添加新 Legacy 模块时：`include/modules/legacy/*.hh` + `modules.hh` 更新
- `include/ext/` 多 extension 并存：`tlm::tlm_generic_payload` 通过 `tlm_extension_registry` + `tlm_array<T>` 支持多个不同类型 extension 同时附加（Phase 1c 升级，详见 `docs/adr/ADR-X.13-stub-multi-extension.md`）
