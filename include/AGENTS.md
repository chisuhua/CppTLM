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

```
REGISTER_OBJECT    → 注册 Legacy SimObject 派生类（modules.hh 定义）
REGISTER_MODULE    → 注册 SimModule 派生类（modules.hh 定义）
REGISTER_CHSTREAM  → 注册 ChStreamModuleBase 派生类 + StreamAdapter（chstream_register.hh）
REGISTER_ALL       → REGISTER_OBJECT + REGISTER_CHSTREAM
```

## 约定差异

- **头文件后缀**: `.hh` 而非 `.h`
- **include 路径**: `include/` 直接作为 PUBLIC include dir，支持 `#include "core/xxx.hh"` 和 `#include "xxx.hh"`（无 core/ 前缀兼容旧代码）
- **命名空间**: 大部分代码在全局命名空间（非 cpptlm），仅 StreamAdapter 在 `cpptlm::`
- **DPRINTF 宏**: 编译期 `-DDEBUG_PRINT` 控制的日志，`DPRINTF(MODULE, "fmt", args...)`

## 注意事项

- `include/core/` 同时作为 include 目录添加到 CMake target，允许无 `core/` 前缀 `#include "packet.hh"` — 新旧代码共存
- 添加新 TLM 模块时：`include/tlm/*.hh` + `REGISTER_CHSTREAM` 宏更新
- 添加新 Legacy 模块时：`include/modules/legacy/*.hh` + `modules.hh` 更新
