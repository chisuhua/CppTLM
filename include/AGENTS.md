# include/ — 头文件总览

**域**: C++ 头文件（C++17）
**约定**: 所有 `.hh` 头文件在此，`src/` 仅放 `.cc` 实现
**子域**: core(核心框架), tlm(V2.1+ 模块), framework(流适配器), bundles(消息定义), utils(工具), ext(插件), sc_core(SimC 兼容)

## 关键入口

| 文件 | 作用 |
|------|------|
| `core/sim_object.hh` | 所有模块基类（Packet, TransactionInfo, SimObject, ResetConfig） |
| `core/module_factory.hh` | 模块工厂：双注册表(registerObject/registerModule), instantiateAll |
| `core/chstream_module.hh` | ChStreamModuleBase — TLM 模块的统一类型标识 |
| `chstream_register.hh` | REGISTER_CHSTREAM 宏（注册 TLM 模块 + StreamAdapter + 多端口适配器） |
| `modules.hh` | REGISTER_OBJECT 宏（v2.2 起为 no-op, CPUSim 已退场）+ REGISTER_MODULE 宏（参数化, P2-T2.4 起支持 9 个 SimModule 集中注册） |
| `modules_cluster.hh` | P2-T2.4: 集中注册 9 个 SimModule 派生类 (CpuCluster + 4 GPU 端 + 3 基础设施 + 1 顶层) |

## 注册宏体系

### 宏清单

| 宏 | 入口文件 | 派生自 | 注册到 | Build flag |
|----|----------|--------|--------|------------|
| `REGISTER_OBJECT` | `include/modules.hh` | `SimObject`（v2.2 起为 no-op, 见宏体注释） | `getObjectRegistry()` | **Always ON**（v2.2: 宏体为空, CPUSim 已退场） |
| `REGISTER_MODULE(T)` | `include/modules.hh` | `SimModule`（参数化宏, P2-T2.4 起支持任意 SimModule 派生类） | `getModuleRegistry()` | **Always ON** |
| `REGISTER_CHSTREAM` | `include/chstream_register.hh` | `ChStreamModuleBase`（如 `CacheTLM`/`CrossbarTLM`/`MemoryTLM`） | `getObjectRegistry()` + `ChStreamAdapterFactory` | **Always ON**（推荐） |
| `REGISTER_ALL` | `include/chstream_register.hh` | 复合宏 | `REGISTER_OBJECT; REGISTER_CHSTREAM` + 末尾 `modules_cluster.hh`（触发 9 个 SimModule 集中注册） | **Always ON** |
| `modules_cluster.hh` | `include/modules_cluster.hh` | 集中注册 | `REGISTER_MODULE(CpuCluster/ComputeCluster/TpcCluster/GpcCluster/GpuCluster/CacheCluster/MemoryCluster/GpuNoC/ApuSoC)` 全 9 个 SimModule 派生类 | **Always ON**（P2-T2.4 引入, P5 扩展至 9） |

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
5. **P2-T2.4 起 `REGISTER_MODULE` 参数化**：v2.2 时 `REGISTER_MODULE` 是无参宏（硬编码 `CpuCluster`）；P2-T2.4 (2026-06-19) 改为 `#define REGISTER_MODULE(T) ModuleFactory::registerModule<T>(#T), true`（expression 形式, 末尾带 `, true` 允许全局作用域变量初始化）。9 个 SimModule 派生类（`CpuCluster` + `ComputeCluster` + `TpcCluster` + `GpcCluster` + `GpuCluster` + `CacheCluster` + `MemoryCluster` + `GpuNoC` + `ApuSoC`）集中在 `include/modules_cluster.hh` 注册，通过 `chstream_register.hh` 末尾的 `#include "modules_cluster.hh"` 自动触发。
6. **全局作用域 hack**：`REGISTER_MODULE` 宏体返回 `, true`（逗号表达式），允许 `const bool _reg_x = (REGISTER_MODULE(X), true);` 在全局作用域作为变量初始化器使用——因为 C++ 全局作用域禁止 expression statement。

### 使用顺序

```cpp
#include "modules.hh"            // REGISTER_OBJECT + REGISTER_MODULE(T) 宏
#include "chstream_register.hh"  // REGISTER_CHSTREAM + REGISTER_ALL + 自动 include modules_cluster.hh
```

`REGISTER_ALL` = `REGISTER_OBJECT; REGISTER_CHSTREAM`（`chstream_register.hh:103`），末尾追加 `#include "modules_cluster.hh"`，一键注册 Object + ChStream + 9 个 SimModule 派生类全量。

## 约定差异

- **头文件后缀**: `.hh` 而非 `.h`
- **include 路径**: `include/` 直接作为 PUBLIC include dir，支持 `#include "core/xxx.hh"` 和 `#include "xxx.hh"`（无 core/ 前缀兼容旧代码）
- **命名空间**: 大部分代码在全局命名空间（非 cpptlm），仅 StreamAdapter 在 `cpptlm::`
- **DPRINTF 宏**: 编译期 `-DDEBUG_PRINT` 控制的日志，`DPRINTF(MODULE, "fmt", args...)`

## 注意事项

- `include/core/` 同时作为 include 目录添加到 CMake target，允许无 `core/` 前缀 `#include "packet.hh"` — 新旧代码共存
- 添加新 TLM 模块时：`include/tlm/*.hh` + `REGISTER_CHSTREAM` 宏更新
- 添加新 SimModule 容器（如 MemoryCluster/CacheCluster）时：`include/tlm/cluster/*.hh` + `include/modules.hh` 的 `REGISTER_MODULE` 宏更新
- `include/ext/` 多 extension 并存：`tlm::tlm_generic_payload` 通过 `tlm_extension_registry` + `tlm_array<T>` 支持多个不同类型 extension 同时附加（Phase 1c 升级，详见 `docs/adr/ADR-X.13-stub-multi-extension.md`）
