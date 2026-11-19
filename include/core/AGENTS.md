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
└── SimModule (多层容器基类 — 见 include/tlm/cluster/)
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

### 双注册表动机

`ModuleFactory` 维护两个独立的 `static unordered_map`：`getObjectRegistry()`（`std::function<SimObject*>`）与 `getModuleRegistry()`（`std::function<SimModule*>`）。这一"不对称"设计是**类型系统强制约束**，而非冗余或历史遗留：

1. **构造签名不兼容**：`SimModule` 派生类（如 `CpuCluster`）的构造函数签名可能与 `SimObject` 不同（`SimModule` 引入 Layout/拓扑属性）。合并注册表将迫使 `register<T>` 接受所有类型，丢失编译期类型检查。
2. **`static_assert` 边界保护**：`registerModule<T>` 实现包含 `static_assert(std::is_base_of_v<SimModule, T>, "T must derive from SimModule")`。`ChStreamModuleBase` 继承 `SimObject` 而非 `SimModule`，因此**必须**用 `registerObject` 注册，无法混入 `registerModule`。
3. **职责分离**：`SimObject` 通用调度（tick/init/reset）由仿真核心统一管理；`SimModule` 引入层次化 Layout 语义，需要独立构造路径。合并后 `unregister/clear/getRegisteredTypes/listRegisteredTypes` 都要做联合遍历，增加调用点判断开销。
4. **StreamAdapter 解耦**：`ChStreamModuleBase` 派生类通过 `ChStreamAdapterFactory::get().registerAdapter/...` 单独注册 StreamAdapter（与对象注册解耦），通过 `REGISTER_CHSTREAM` 复合宏保证两者原子注册。`REGISTER_CHSTREAM` 内部仍调用 `ModuleFactory::registerObject<T>`，**复用**对象注册表而非另起炉灶。
5. **REGISTER_OBJECT 与 REGISTER_MODULE 互斥**：当前 `include/modules.hh` 同时定义两者，调用方按需选择其一。若合并为单一 `REGISTER`，将丧失 `SimModule` 子树与 `SimObject` 子树的语义边界。

**结论**：双注册表是有意设计，受 `static_assert` 与继承层次保护，任何"统一为单注册表"的提议都会破坏类型安全与 Layout 语义。详见 `include/AGENTS.md` "注册宏体系" 与 `include/modules.hh` 文件头注释。

## 注意事项

- `module_factory.hh` 中 `stream_adapters_`/`ch_initiator_ports_`/`ch_target_ports_` 成员由 Step 7 注入阶段使用
- **Step 7c（`cpptlm-simmodule-exposed-port-wiring` / ADR-X.15）**：`src/core/module_factory.cc` 新增隔离连接阶段，把 `SimModule` 暴露端口（`outputs`/`inputs`）与顶层 ChStream 端点建成真实 `PortPair`。方向按内部端口名后缀推导（`req_in`↔对端`req_out`、`resp_in`↔`resp_out`、`req_out`↔`req_in`、`resp_out`↔`resp_in`），单向、无自动响应镜像；失败一律硬错误；仅支持**单层** exposed（嵌套暴露报错）；`SimplePort::isBound()` 用于防 PortPair 静默覆盖。⚠️ 未声明 exposed 的 SimModule 的 `<实例>.<端口>` 引用（如 `pcie_ep.axi_slave_in`）维持既有普通端口语义，不受影响。
- `parsePortSpec(full_name)` — 解析 `"xbar.0"` → `("xbar", "0")` 端口索引语法
- PluginLoader 使用 `dlopen/dlsym` 动态加载共享库

## 架构层次说明

### core/ext/ 子目录

`include/core/ext/` 现**仅** `packet_pool.hh`。曾在此的 `cmd_exts.hh`（TLM 扩展宏 `GEMSC_TLM_EXTENSION_DEF`）已于 **2026-06-10 删除**（随之 `packet_to_payload.hh` / `payload_to_packet.hh` 也已移除）。

⚠️ 因此「`core/ext/` 允许直接依赖 `tlm/` 层」这条旧规则**已失效** —— 该目录现已无任何跨层依赖；TLM 扩展宏统一走 `include/ext/`（见下）。

### ext/ 层依赖

`include/ext/` 层（`transaction_context_ext.hh`、`error_context_ext.hh`、`mem_exts.hh`、`credit_stream.hh`）也直接依赖 `tlm/` 层——这些是 TLM 扩展的实现，按设计需要继承 `tlm::tlm_extension<T>`。

### core/packet.hh 的依赖链

`include/core/packet.hh` 不再直接包含 `tlm/tlm_stub.hh` 或 SystemC 的 `tlm.h`，依赖通过下列传递性 include 解决：

```
core/packet.hh
  ├─ core/error_category.hh
  ├─ ext/transaction_context_ext.hh  → tlm/tlm_stub.hh
  └─ ext/error_context_ext.hh       → tlm/tlm_stub.hh
```

**结论**：`core/ext/` 和 `ext/` 是 TLM 扩展桥接层，是架构的有意例外，不是分层违规。任何新增文件若需直接依赖 `tlm/` 层，应优先放在 `ext/`，仅在 `core/ext/` 的适配器需要时才放入 `core/ext/`。
