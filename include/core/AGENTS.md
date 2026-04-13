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
| `chstream_adapter_factory.hh` | — | StreamAdapter 工厂: registerAdapter/registerMultiPortAdapter |
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
