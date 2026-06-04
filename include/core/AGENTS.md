# include/core/ — 核心框架

**域**: 仿真框架核心基类与基础设施（23 文件）
**作用**: SimObject 层次结构、ModuleFactory、Port 端口、ChStream 协议、插件加载

## 类层次结构

```
SimObject (基类 — 所有仿真模块)
├── ChStreamModuleBase (ChStream 模块基类 — TLM 模块统一标识)
│   ├── CacheTLM      (include/tlm/)
│   ├── CrossbarTLM   (include/tlm/)
│   └── MemoryTLM     (include/tlm/)
├── CPUSim            (include/modules/legacy/)
├── CacheSim          (include/modules/legacy/)
├── MemorySim         (include/modules/legacy/)
└── ...
```

## 关键文件

| 文件 | 行数 | 作用 |
|------|------|------|
| `sim_object.hh` | 300+ | SimObject 基类: tick/init/reset/层次结构/PortManager/TxnInfo |
| `module_factory.hh` | 168 | ModuleFactory: 双注册表, instantiateAll, StreamAdapter 注入 |
| `chstream_module.hh` | 56 | ChStreamModuleBase: set_stream_adapter(), num_ports() |
| `chstream_port.hh` | — | ChStream 端口: MasterPort/SlavePort/ChStream initiator/target |
| `simple_port.hh` | — | 简单端口实现（Legacy Port 回调机制） |
| `sim_module.hh` | — | SimModule 基类（带 Layout 特性的模块） |
| `sim_core.hh` | — | SimCore: 事件调度核心 |
| `event_queue.hh` | — | 事件队列 |
| `connection_resolver.hh` | — | 连接解析器（模块间拓扑连线） |
| `port_manager.hh` | — | 端口管理（上下行端口注册） |
| `plugin_loader.hh` + `dynamic_loader.cc` | — | 动态插件加载（SO/DLL） |
| `packet.hh` | — | 数据包定义（trans_id/addr/data/vc_id） |
| `packet_pool.hh` | — | 对象池 |
| `error_category.hh` | — | 错误分类 |
| `cmd.hh` | — | 仿真命令 |
| `tlm_module.hh` | — | TLM 模块基类 |

## 约定

- **双注册表**: `getObjectRegistry()` vs `getModuleRegistry()` — SimObject 与 SimModule 分离
- **create 函数签名**: `CreateSimObjectFunc = std::function<SimObject*(const std::string&, EventQueue*)>` vs `CreateSimModuleFunc`
- **实例存储**: `std::unordered_map<std::string, SimObject*> instances` — 按名称索引
- **StreamAdapter 存储**: `std::vector<std::unique_ptr<cpptlm::StreamAdapterBase>>` — 自动生命周期管理

## 注意事项

- `module_factory.hh` 中 `stream_adapters_`/`ch_initiator_ports_`/`ch_target_ports_` 成员由 Step 7 注入阶段使用
- `parsePortSpec(full_name)` — 解析 `"xbar.0"` → `("xbar", "0")` 端口索引语法
- PluginLoader 使用 `dlopen/dlsym` 动态加载共享库

## 架构层次说明

### core/ext/ 子目录（TLM 扩展桥接层）

`include/core/ext/` 是 `core/` 层的特殊子目录，**允许直接依赖 `tlm/` 层**。这是有意设计：

- **原因**：`core/ext/cmd_exts.hh` 定义了 TLM 扩展宏（`GEMSC_TLM_EXTENSION_DEF`），必须继承 `tlm::tlm_extension<T>`（C++ 模板继承需要完整类型定义）
- **使用方**：`core/ext/packet_to_payload.hh`（Packet → Payload 转换器，直接 `#include "cmd_exts.hh"`）；`core/ext/payload_to_packet.hh`（Payload → Packet 转换器，使用 `ext/mem_exts.hh` 中的对应类型）
- **替代方案**：将 `cmd_exts.hh` 移至 `ext/` 目录，但因其被 `core/ext/*` 使用，会增加循环依赖
- **判断标准**：仅当文件直接包含 `tlm/tlm_stub.hh` 或 `tlm.h` 时，才允许放在 `core/ext/` 下

### ext/ 层依赖

`include/ext/` 层（`transaction_context_ext.hh`、`error_context_ext.hh`、`mem_exts.hh`、`credit_stream.hh`）也直接依赖 `tlm/` 层——这些是 TLM 扩展的实现，按设计需要继承 `tlm::tlm_extension<T>`。

### core/packet.hh 的依赖链

`include/core/packet.hh` 不再直接包含 `tlm/tlm_stub.hh` 或 `tlm.h`，依赖通过下列传递性 include 解决：

```
core/packet.hh
  ├─ core/error_category.hh
  ├─ ext/transaction_context_ext.hh  → tlm/tlm_stub.hh
  └─ ext/error_context_ext.hh       → tlm/tlm_stub.hh
```

**结论**：`core/ext/` 和 `ext/` 是 TLM 扩展桥接层，是架构的有意例外，不是分层违规。任何新增文件若需直接依赖 `tlm/` 层，应优先放在 `ext/`，仅在 `core/ext/` 的适配器需要时才放入 `core/ext/`。
