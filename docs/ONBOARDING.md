# CppTLM 新成员上手指南 (Onboarding Guide)

> 本文档基于知识图谱自动生成,适用于 CppTLM v2.1 分层融合架构。
> 最后更新:2026-06-04 · Commit:14acd9a1 · 285 个文件节点,532 个执行流,10 个社区

---

## 1. 项目概览 (Project Overview)

**CppTLM** 是一个 **TLM 2.0 周期精确片上网络 (NoC) 仿真框架**,采用 **C++ 核心 + Python 工具链** 的分层融合架构 v2.1。

| 维度 | 内容 |
|---|---|
| 主语言 | C++ (CMake 构建)、Python (Pydantic 配置) |
| 测试框架 | Catch2 v3.7.0(单二进制,预编译 amalgamated) |
| 仿真协议 | TLM 2.0 + 内部 ChStream 协议(Bundle→StreamAdapter→Module) |
| 可视化 | Dash/Plotly 后端 + Svelte 拓扑编辑器前端 |
| 持续集成 | GitHub Actions(Release/Debug × SystemC ON/OFF 矩阵) |
| 代码量级 | 285 文件节点,4 个可执行入口,77 个 C++ 测试,15 个 Python 测试 |

**关键设计决策**:
- JSON 驱动拓扑(`configs/*.json`)+ ModuleFactory 动态注入
- 双注册表分离:`SimObject`(对象)与 `SimModule`(模块),由 `instantiateAll` 统一装配
- ChStream 协议独立于 TLM,通过 `StreamAdapter` 桥接,默认使用内置 TLM stub (`USE_SYSTEMC_STUB=ON`)
- Phase 驱动开发(0-6 完成,3.x 子阶段覆盖端口/配置/验证)
- 零技术债原则:每个 Phase 完成即编译通过、测试覆盖、文档同步

---

## 2. 架构分层 (Architecture Layers)

知识图谱识别出 **25 个逻辑层**,按职能归并为 8 大领域:

### 2.1 代理协作文档 (Agent Docs)
- `AGENTS.md` — **首读!** 项目结构、约定、命令速查
- `CLAUDE.md` — Claude/AI 代理工作约定

### 2.2 架构设计文档 (Architecture Docs)
- `docs/adr/ADR-X.*.md` — 12 份架构决策记录(事务 ID、错误处理、端口类型系统、插件系统等)
- `docs/architecture/01-hybrid-architecture-v2.1.md` — **v2.1 分层融合架构主文档,必读**
- `docs/architecture/02-complex-topology-architecture.md` — 复杂拓扑架构
- `docs/architecture/08-metrics-collection-framework-v2.2.md` — 指标收集框架
- `docs/architecture/09-topology-visualization-pipeline.md` — 拓扑可视化流水线
- `docs/architecture/10-topology-generation-management-system.md` — TGMS
- `docs/architecture/13-dashboard-integration.md` — 仪表盘集成
- `docs/guide/GETTING_STARTED.md` / `DEVELOPER_GUIDE.md` / `TOPOLOGY_USER_GUIDE.md` — 用户与开发指南
- `docs/guide/PYTHON_TOOLING_GUIDE.md` — Python 工具链指南
- `docs/superpowers/plans/` — 活跃阶段实施计划(2026-06 系列: future-work-roadmap + phase7a-gpu-infra)
- `docs/superpowers/specs/2026-06-12-phase7-apu-fused-soc-design.md` — Phase 7 整体架构设计
- `docs-archived/superpowers/{plans,specs}/` — 已完成/已废弃的实施计划与设计文档(2026-05/06 早期系列)

### 2.3 C++ 核心层 (C++ Core,46 个文件)
- **基类**:`sim_object.hh` ⭐complex、`sim_module.hh`、`tlm_module.hh`、`chstream_module.hh`
- **端口系统**:`port_types.hh`(类型系统)、`master_port.hh` / `slave_port.hh` / `simple_port.hh`、`port_manager.hh` ⭐complex
- **拓扑与连接**:`topology_node.hh`、`topology_parser.hh`、`connection_resolver.hh` + `src/core/connection_resolver.cc`
- **工厂**:`module_factory.hh` ⭐complex + `src/core/module_factory.cc` ⭐complex(604 行)+ `src/core/module_factory_validate.cc` ⭐complex(396 行)
- **数据包**:`packet.hh` ⭐complex、`packet_pool.hh`、`ext/packet_pool.hh`（v2.1 已归档 `ext/packet_to_payload.hh` 和 `ext/payload_to_packet.hh` 到 `docs-archived/dead-code-headers-2026-q2/`）
- **其他核心**:`sim_core.hh`、`event_queue.hh`、`cmd.hh`、`coherence_domain.hh`、`virtual_channel.hh`、`load_policy.hh`、`param_parser.hh`、`param_rules.hh`、`param_errors.hh`、`error_category.hh`、`plugin_loader.hh`、`plugin_load_exception.hh`、`port_compatibility.hh`、`port_stats.hh`、`stream_adapter_base.hh`
- **注册宏**:`include/chstream_register.hh`(注册 ChStream 三件套)、`include/modules.hh`(注册 Object/Module)

### 2.4 C++ TLM 2.0 模块层 (C++ TLM Modules,15 个文件)
- **核心三件套**:`cache_tlm.hh`、`crossbar_tlm.hh`、`memory_tlm.hh` — 分层融合架构 v2.1 端到端实现
- **NoC 模块**:`router_tlm.hh` ⭐complex(283 行,24 函数)+ `src/tlm/router_tlm.cc` ⭐complex(464 行)、`nic_tlm.hh` + `src/tlm/nic_tlm.cc`、`link_tlm.hh` + `src/tlm/link_tlm.cc`
- **辅助**:`arbiter_tlm.hh`(仲裁器)、`cpu_tlm.hh`(CPU 模型)、`traffic_gen_tlm.hh`(流量生成器)、`stress_patterns.hh`(压力模式)
- **Stub**:`tlm_stub.hh` — SystemC 关闭时的 TLM 存根实现

### 2.5 C++ StreamAdapter 层 (Framework,7 个文件)
ChStream ↔ TLM 2.0 适配器桥接:
- `stream_adapter.hh` — 单端口适配器基础(194 行)
- `multi_port_stream_adapter.hh` — 多端口版本(82 行)
- `dual_port_stream_adapter.hh` — 双端口(155 行)
- `bidirectional_port_adapter.hh` ⭐complex(216 行)— 双向端口
- `chstream_adapter_factory.hh` — 适配器工厂(103 行)
- `debug_tracker.hh` ⭐complex(267 行)— 调试追踪
- `transaction_tracker.hh` ⭐complex(289 行)— 事务追踪

### 2.6 C++ Bundle / Extension / Utility / Metrics 层
- **Bundle 消息**(`include/bundles/`):`cache_bundles_tlm.hh`、`noc_bundles_tlm.hh`(121 行)、`bundle_serialization.hh`、`cpphdl_types.hh`
- **TLM 扩展**(`include/ext/`):`credit_stream.hh`(反压)、`error_context_ext.hh`(183 行)、`transaction_context_ext.hh`(100 行)、`mem_exts.hh`(117 行)
- **工具**(`include/utils/`):`config_utils.hh`、`dynamic_loader.hh`(插件加载)、`regex_matcher.hh`、`wildcard.hh`(通配符)、`json_includer.hh`、`module_group.hh`、`var_resolver.hh`、`force_directed_layout.hh` ⭐complex
- **指标**(`include/metrics/`):`stats_manager.hh`、`stats.hh` ⭐complex(421 行)、`histogram.hh`、`metrics_reporter.hh` ⭐complex(249 行)、`streaming_reporter.hh` ⭐complex(273 行)
- **Legacy**(`include/modules/legacy/`):`cpu_cluster.hh`、`cpu_sim.hh` — 已归档,仅修严重 bug（`modules_v2.hh` 已 v2.1 移除并归档至 `docs-archived/`）

### 2.7 C++ 入口与测试 (Entry Points & Tests)
- **入口**(`src/`):`main.cpp`(122 行,默认仿真,构建产物 `cpptlm_sim`)；v2.1 已移除 `cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp`
- **测试**(`test/`,共 77 个):`catch_amalgamated.cpp`(9820 行,326 函数)+ `catch_amalgamated.hpp` 单二进制;集成测试 `test_phase6_integration.cc` 验证 Cache→Crossbar→Memory 端到端;`test_e2e_simulation.cc`(491 行)、`test_stress_*` 压力测试、`test_tgms_v4_hierarchy_integration.cc` 层级拓扑

### 2.8 Python 工具链
- **`cpptlm_config/`**(Pydantic 驱动):`models.py`(类型)、`builder.py`(fluent API)、`validator.py` ⭐complex、`topology_adapter.py`(适配)、`types.py`
- **`cpptlm/`**(Python 端驱动):`cli.py`(入口)、`__main__.py`、`simulation/runner.py`、`simulation/result.py`、`topo/orchestrator.py` + `cli.py` + `patch.py` ⭐complex + `variant.py` + `layer.py`、`analysis/{metrics,comparator,detector,adapters}.py`、`config/{generator,topologies}.py`、`visualization/`(见下)
- **可视化**:`cpptlm/visualization/` — Dash/Plotly 仪表盘后端(`dashboard.py` / `dashboard_server.py` ⭐complex / `dashboard_ui.py` / `app.py` ⭐complex / `run_context.py` ⭐complex / `report.py` / `simulation_runner.py` / `template_loader.py` / `topology.py`)
- **Web 编辑器**:`cpptlm/visualization/editor/src/` — Svelte 前端(`App.svelte` / `Canvas.svelte` / `Palette.svelte` / `PropertiesPanel.svelte` / `stores/topology.js`)
- **Python 测试**(`test/python/`,15 个):`test_topology_generator.py`、`test_hierarchy_topology.py`、`test_credit_flow.py`、`test_port_types.py`、`test_topo_*.py` 等

---

## 3. 关键概念 (Key Concepts)

### 3.1 双注册表 (Dual Registry)
- `SimObject` 通过 `REGISTER_OBJECT(name, ctor)` 注册为**对象**,由 `ModuleFactory::registerObject` 管理
- `SimModule` 通过 `REGISTER_MODULE(name, ctor)` 注册为**模块**,由 `ModuleFactory::registerModule` 管理
- `REGISTER_CHSTREAM(name, ...)` 一键完成对象 + 单端口 StreamAdapter + 多端口 StreamAdapter 三重注册

### 3.2 端口索引语法
JSON 配置中 `"dst": "xbar.0"` 表示模块 `xbar` 的第 0 端口,由 `parsePortSpec(name)` 解析。Phase 3.2 已实施完整的端口类型系统。

### 3.3 ChStream 通信协议
```
Bundle (消息载荷) → StreamAdapter (适配) → ChStreamModuleBase → ChStreamPort (物理连接)
```
位于 `include/bundles/` 的 Bundle 定义消息格式;`framework/` 下是 ChStream↔TLM 适配层。

### 3.4 Phase 驱动开发
- Phase 0-6:核心功能分阶段交付
- Phase 3.x:子阶段(3.1 缺陷修复、3.2 端口管理、3.3 配置增强、3.4 验证工具链)
- Phase 7-8:事务生命周期、压力测试
- 历史 P0/P1 债务已完成修复

### 3.5 DPRINTF 调试宏
全局日志 `DPRINTF(MODULE, "fmt", args...)`,编译期 `-DDEBUG_PRINT` 启用,运行时按模块过滤。

### 3.6 ModuleFactory 装配流程
`instantiateAll` 完成 7 步装配,关键 Step 7 负责 StreamAdapter 注入。位于 `src/core/module_factory.cc` + `module_factory_validate.cc`。

---

## 4. 引导路线图 (Guided Tour,14 步)

| # | 主题 | 关键文件 |
|---|---|---|
| 1 | 项目概览 | `AGENTS.md`、`README.md` |
| 2 | C++ 核心:SimObject 与 ModuleFactory | `include/core/sim_object.hh`、`sim_module.hh`、`module_factory.hh` |
| 3 | 端口系统与类型抽象 | `include/core/port_types.hh`、`master_port.hh`、`slave_port.hh`、`port_manager.hh` |
| 4 | TLM 2.0 模块基类 | `include/core/tlm_module.hh`、`chstream_module.hh`、`chstream_port.hh`、`chstream_register.hh` |
| 5 | 周期精确 TLM 模块 | `include/tlm/cache_tlm.hh`、`crossbar_tlm.hh`、`memory_tlm.hh`、`router_tlm.hh`、`nic_tlm.hh`、`link_tlm.hh`、`tlm_stub.hh` |
| 6 | Bundle 消息格式 | `include/bundles/cache_bundles_tlm.hh`、`noc_bundles_tlm.hh`、`bundle_serialization.hh` |
| 7 | StreamAdapter 适配层 | `include/framework/stream_adapter.hh`、`multi_port_stream_adapter.hh`、`dual_port_stream_adapter.hh`、`bidirectional_port_adapter.hh`、`debug_tracker.hh`、`transaction_tracker.hh` |
| 8 | TLM 扩展插件 | `include/ext/credit_stream.hh`、`error_context_ext.hh`、`transaction_context_ext.hh`、`mem_exts.hh` |
| 9 | 工具与配置 | `include/utils/config_utils.hh`、`dynamic_loader.hh`、`wildcard.hh`、`regex_matcher.hh`、`metrics/stats_manager.hh` |
| 10 | 可执行入口 | `src/main.cpp` → `build/bin/cpptlm_sim`、`src/core/module_factory.cc:startAllTicks`<br>v2.1 已移除 `cpu_main.cpp` / `traffic_main.cpp` / `sc_main.cpp`（USE_SYSTEMC 选项删除，统一通过 `cpptlm_sim` + JSON config 入口） |
| 11 | Catch2 测试套件 | `test/test_phase6_integration.cc:registerChStreamModules`、`test/test_chstream_integration.cc:process_request_input` |
| 12 | Python 拓扑配置库 | `cpptlm_config/__init__.py`、`models.py:ConfigMetadata`、`builder.py:ConfigBuilder`、`validator.py:load_param_rules`、`topology_adapter.py:TopologyAdapter` |
| 13 | Python 仿真与分析 | `cpptlm/cli.py:main`、`simulation/runner.py:SimulationRunner`、`topo/orchestrator.py:TopoOrchestrator`、`analysis/metrics.py:MetricSummary` |
| 14 | 可视化:仪表盘与 Web 编辑器 | `cpptlm/visualization/` + `cpptlm/visualization/editor/src/` |

---

## 5. 文件地图 (File Map)

### 5.1 必读 (Top 10 文件,按重要性)
1. **`AGENTS.md`** — 项目入口,所有约定和命令
2. **`include/core/sim_object.hh`** ⭐complex(326 行)— 仿真对象基类
3. **`include/core/module_factory.hh`** ⭐complex(233 行)— JSON 驱动的模块工厂
4. **`src/core/module_factory.cc`** ⭐complex(604 行)— 工厂实现 + Step 7 StreamAdapter 注入
5. **`include/framework/stream_adapter.hh`**(194 行)— ChStream↔TLM 单端口适配
6. **`include/tlm/cache_tlm.hh`**(116 行)— Cache TLM 模块
7. **`include/tlm/crossbar_tlm.hh`**(115 行)— Crossbar TLM 模块
8. **`include/tlm/memory_tlm.hh`**(97 行)— Memory TLM 模块
9. **`include/utils/config_utils.hh`**(34 行)— JSON 配置加载
10. **`test/test_phase6_integration.cc`**(204 行)— 端到端集成测试

### 5.2 按层组织的关键文件

#### C++ 核心
| 文件 | 复杂度 | 职责 |
|---|---|---|
| `include/core/sim_object.hh` | complex | SimObject 基类、Tick 调度、统计注册 |
| `include/core/sim_module.hh` | moderate | SimModule 基类、参数解析 |
| `include/core/module_factory.hh` | complex | ModuleFactory 声明 |
| `src/core/module_factory.cc` | complex | 装配实现(5 函数,604 行) |
| `src/core/module_factory_validate.cc` | complex | 验证逻辑(8 函数,396 行) |
| `include/core/port_types.hh` | moderate | 端口类型系统(127 行) |
| `include/core/master_port.hh` | moderate | Master 端口(59 行) |
| `include/core/slave_port.hh` | moderate | Slave 端口(50 行) |
| `include/core/simple_port.hh` | simple | 简单端口(47 行) |
| `include/core/port_manager.hh` | complex | 端口管理(207 行) |
| `include/core/connection_resolver.hh` + `.cc` | moderate | 连接解析(109 行实现) |
| `include/core/packet.hh` | complex | 数据包(223 行) |
| `include/core/event_queue.hh` | moderate | 事件队列(50 行) |
| `include/core/topology_node.hh` / `topology_parser.hh` | simple | 拓扑节点与解析 |
| `include/chstream_register.hh` | — | REGISTER_CHSTREAM 宏入口 |

#### C++ TLM 2.0 模块
| 文件 | 复杂度 | 职责 |
|---|---|---|
| `include/tlm/cache_tlm.hh` | moderate | Cache 周期精确模型(116 行) |
| `include/tlm/crossbar_tlm.hh` | moderate | Crossbar 交换(115 行) |
| `include/tlm/memory_tlm.hh` | moderate | 内存模型(97 行) |
| `include/tlm/router_tlm.hh` | complex | NoC 路由器(283 行) |
| `src/tlm/router_tlm.cc` | complex | 路由器实现(464 行,24 函数) |
| `include/tlm/nic_tlm.hh` + `src/tlm/nic_tlm.cc` | moderate | NIC 网络接口 |
| `include/tlm/link_tlm.hh` + `src/tlm/link_tlm.cc` | moderate | 链路 |
| `include/tlm/arbiter_tlm.hh` | moderate | 仲裁器(87 行) |
| `include/tlm/cpu_tlm.hh` | moderate | CPU 模型(75 行) |
| `include/tlm/traffic_gen_tlm.hh` | complex | 流量生成器(203 行) |
| `include/tlm/stress_patterns.hh` | complex | 压力模式(206 行) |
| `include/tlm/tlm_stub.hh` | moderate | SystemC stub(83 行) |

#### C++ StreamAdapter
| 文件 | 复杂度 | 职责 |
|---|---|---|
| `include/framework/stream_adapter.hh` | moderate | 单端口适配(194 行) |
| `include/framework/multi_port_stream_adapter.hh` | moderate | 多端口(82 行) |
| `include/framework/dual_port_stream_adapter.hh` | moderate | 双端口(155 行) |
| `include/framework/bidirectional_port_adapter.hh` | complex | 双向端口(216 行) |
| `include/framework/chstream_adapter_factory.hh` | moderate | 工厂(103 行) |
| `include/framework/debug_tracker.hh` | complex | 调试追踪(267 行) |
| `include/framework/transaction_tracker.hh` | complex | 事务追踪(289 行) |

#### C++ 工具与指标
| 文件 | 复杂度 | 职责 |
|---|---|---|
| `include/utils/config_utils.hh` | simple | JSON 加载(34 行) |
| `include/utils/dynamic_loader.hh` + `src/utils/dynamic_loader.cc` | simple | 插件加载(16 行) |
| `include/utils/regex_matcher.hh` | moderate | 正则匹配(60 行) |
| `include/utils/wildcard.hh` | simple | 通配符(31 行) |
| `include/utils/json_includer.hh` | moderate | JSON 包含(75 行) |
| `include/utils/module_group.hh` | moderate | 模块组(119 行) |
| `include/utils/var_resolver.hh` | moderate | 变量解析(92 行) |
| `include/utils/force_directed_layout.hh` | complex | 力导向布局(218 行) |
| `include/metrics/stats_manager.hh` | moderate | 统计管理(144 行) |
| `include/metrics/stats.hh` | complex | 统计基础(421 行) |
| `include/metrics/histogram.hh` | moderate | 直方图(156 行) |
| `include/metrics/metrics_reporter.hh` | complex | 报告器(249 行) |
| `include/metrics/streaming_reporter.hh` | complex | 流式报告(273 行) |

#### C++ 入口
| 文件 | 复杂度 | 职责 |
|---|---|---|
| `src/main.cpp` | moderate | 主入口(122 行,构建产物 `cpptlm_sim`) |

> **v2.1 已移除** `src/cpu_main.cpp`、`src/traffic_main.cpp`、`src/sc_main.cpp`（USE_SYSTEMC 选项删除）。所有场景通过 `cpptlm_sim <config.json>` 统一入口。

#### Python 工具链
| 文件 | 复杂度 | 职责 |
|---|---|---|
| `cpptlm/cli.py` | moderate | CLI 入口 |
| `cpptlm/__main__.py` | simple | `python -m cpptlm` 入口 |
| `cpptlm/simulation/runner.py` | moderate | 仿真执行器 |
| `cpptlm/simulation/result.py` | simple | 仿真结果 |
| `cpptlm/topo/orchestrator.py` | moderate | 拓扑编排器 |
| `cpptlm/topo/cli.py` | moderate | 拓扑 CLI |
| `cpptlm/topo/patch.py` | complex | 拓扑补丁 |
| `cpptlm/topo/variant.py` | moderate | 拓扑变体 |
| `cpptlm/topo/layer.py` | moderate | 拓扑层 |
| `cpptlm/analysis/metrics.py` | moderate | 度量收集 |
| `cpptlm/analysis/comparator.py` | simple | 比较器 |
| `cpptlm/analysis/detector.py` | moderate | 检测器 |
| `cpptlm/config/generator.py` | simple | 配置生成 |
| `cpptlm/config/topologies.py` | moderate | 拓扑配置 |
| `cpptlm_config/models.py` | moderate | Pydantic 数据模型 |
| `cpptlm_config/builder.py` | moderate | Fluent Builder |
| `cpptlm_config/validator.py` | complex | 校验器 |
| `cpptlm_config/topology_adapter.py` | moderate | 拓扑适配 |
| `cpptlm_config/types.py` | — | 类型定义 |
| `cpptlm/visualization/dashboard.py` | moderate | 仪表盘 |
| `cpptlm/visualization/dashboard_server.py` | complex | 仪表盘服务器 |
| `cpptlm/visualization/dashboard_ui.py` | — | 仪表盘 UI |
| `cpptlm/visualization/app.py` | complex | 应用入口 |
| `cpptlm/visualization/run_context.py` | complex | 运行上下文 |
| `cpptlm/visualization/report.py` | — | 报告 |
| `cpptlm/visualization/simulation_runner.py` | — | 仿真执行 |
| `cpptlm/visualization/template_loader.py` | — | 模板加载 |
| `cpptlm/visualization/topology.py` | — | 拓扑视图 |

#### Web 编辑器前端 (Svelte)
| 文件 | 职责 |
|---|---|
| `cpptlm/visualization/editor/src/App.svelte` | 应用主组件 |
| `cpptlm/visualization/editor/src/components/Canvas.svelte` | 拓扑画布 |
| `cpptlm/visualization/editor/src/components/Palette.svelte` | 模块调色板 |
| `cpptlm/visualization/editor/src/components/PropertiesPanel.svelte` | 属性面板 |
| `cpptlm/visualization/editor/src/stores/topology.js` | 拓扑状态存储 |
| `cpptlm/visualization/editor/src/main.js` | 入口 |
| `cpptlm/visualization/editor/vite.config.js` | Vite 构建配置 |

### 5.3 关键测试文件
- `test/test_phase6_integration.cc` — Cache→Crossbar→Memory 端到端
- `test/test_chstream_integration.cc` — ChStream 集成
- `test/test_e2e_simulation.cc` — 完整端到端(491 行)
- `test/test_crossbar_tlm.cc` — Crossbar 模块
- `test/test_cache_tlm_*.cc` — Cache 模块综合/单元
- `test/test_tgms_v4_hierarchy_integration.cc` — TGMS v4 层级集成
- `test/test_phase7_transaction_lifecycle.cc` — 事务生命周期
- `test/test_phase8_performance_stress.cc` — Phase 8 压力
- `test/test_stress_*.cc` — 多种压力测试
- `test/test_topo_*.py` / `test_topology_generator.py` — Python 拓扑测试

### 5.4 关键配置示例
- `configs/base.json` — 基础拓扑
- `configs/mesh_2x2_tlm.json` / `mesh_4x4_tlm.json` — 网格拓扑
- `configs/hierarchical_2x2_tlm.json` — 层级拓扑
- `configs/ring_8_tlm.json` — 环形拓扑
- `configs/example_hierarchy/{chip,node,system}.json` — ~~层级化示例~~（已归档至 `docs-archived/dead-configs-2026-q2/example_hierarchy/`，原因：legacy 类型无注册；现代层级化见 `configs/hierarchical_2x2_tlm.json`）
- `configs/stress_*.json` — 压力测试场景
- `configs/param_rules/{nic_tlm,router_tlm}.json` — 参数规则

### 5.5 关键脚本（v2.1 起按用途分 5 个子目录）

| 类别 | 路径 | 说明 |
|------|------|------|
| 构建 | `scripts/build/build.sh` | 主构建脚本（ccache 自动检测） |
| 构建 | `scripts/build/format.sh` | clang-format 格式检查 |
| 测试 | `scripts/test/test.sh` | 单次测试入口 |
| 测试 | `scripts/test/run_all_tests.sh` | 全量测试（支持 `--quick`） |
| 测试 | `scripts/test/ci_e2e_test.sh` | CI 端到端验证 |
| 流水线 | `scripts/pipeline/run_full_pipeline.sh` | 仿真 → 统计 → 可视化 |
| 拓扑 | `scripts/topology/topology_validator.py` | 拓扑校验 |
| 拓扑 | `scripts/topology/topology_generator.py` | 拓扑生成 |
| 拓扑 | `scripts/topology/analyzer.py` / `path_tracer.py` / `credit_flow.py` | 分析与追踪 |
| 统计 | `scripts/stats/stats_annotator.py` / `stats_watcher.py` | 性能统计 |
| 统计 | `scripts/stats/layout_manager.py` / `derive_expr.py` | 布局与派生指标 |
- `scripts/topology/credit_flow.py` — 信用流分析
- `scripts/topology/path_tracer.py` — 路径追踪
- `scripts/topology/analyzer.py` — 通用分析
- `scripts/stats/stats_annotator.py` / `scripts/stats/stats_watcher.py` — 统计注解/监听
- `scripts/stats/derive_expr.py` / `scripts/stats/layout_manager.py` — 辅助工具

---

## 6. 复杂度热点 (Complexity Hotspots)

新成员应**谨慎接近**的 complex 文件(共 52 个,这里列出最关键的 20 个):

### 6.1 C++ 实现热点
1. `src/core/module_factory.cc` (604 行,5 函数) — 工厂主逻辑,所有模块的注入路径
2. `src/core/module_factory_validate.cc` (396 行,8 函数) — 验证逻辑,容易出现边界 case
3. `src/tlm/router_tlm.cc` (464 行,24 函数) — 路由器 24 个方法,路由算法 + 转发表
4. `include/core/sim_object.hh` (326 行) — 基础类,所有模块的根基
5. `include/core/module_factory.hh` (233 行) — 工厂声明
6. `include/core/port_manager.hh` (207 行) — 端口管理,涉及 Phase 3.2 端口类型系统
7. `include/core/packet.hh` (223 行) — 数据包结构

### 6.2 StreamAdapter 热点
8. `include/framework/transaction_tracker.hh` (289 行) — 事务追踪,调试基础设施
9. `include/framework/debug_tracker.hh` (267 行) — 调试追踪
10. `include/framework/bidirectional_port_adapter.hh` (216 行) — 双向端口适配

### 6.3 指标/统计热点
11. `include/metrics/stats.hh` (421 行) — 统计核心
12. `include/metrics/streaming_reporter.hh` (273 行) — 流式报告
13. `include/metrics/metrics_reporter.hh` (249 行) — 指标报告

### 6.4 TLM 模块热点
14. `include/tlm/router_tlm.hh` (283 行) — 路由器声明
15. `include/tlm/traffic_gen_tlm.hh` (203 行) — 流量生成器
16. `include/tlm/stress_patterns.hh` (206 行) — 压力模式

### 6.5 Python 热点
17. `cpptlm/topo/patch.py` — 拓扑补丁,层级编排核心
18. `cpptlm_config/validator.py` — 参数与连接校验
19. `cpptlm/visualization/dashboard_server.py` — Dash 服务器
20. `cpptlm/visualization/app.py` / `run_context.py` — 应用入口与运行上下文

### 6.6 测试热点
- `test/catch_amalgamated.cpp` (9820 行,326 函数) — Catch2 单二进制,**不要手改**
- `test/catch_amalgamated.hpp` (11009 行) — 同上
- `test/test_stress_framework.cc` (444 行,16 函数) — 压力框架测试
- `test/test_stats_core.cc` (507 行) — 统计核心测试
- `test/test_streaming_reporter.cc` (536 行) — 流式报告测试
- `test/test_e2e_simulation.cc` (491 行) — 端到端仿真
- `test/test_stress_scenarios.cc` (486 行) — 压力场景
- `test/test_metrics_reporter.cc` (432 行) — 指标报告
- `test/test_phase7_transaction_lifecycle.cc` (410 行) — 事务生命周期
- `test/test_extension_prototype.cc` (255 行) — 扩展原型

---

## 7. 推荐学习路径 (Recommended Learning Path)

### 第 1 天:环境与概览
1. 读 `AGENTS.md` — 了解项目结构、约定、命令
2. 读 `docs/architecture/01-hybrid-architecture-v2.1.md` — v2.1 架构主文档
3. 跑通构建:`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)`
4. 跑通测试:`cd build && ctest --output-on-failure`
5. 浏览 `configs/mesh_2x2_tlm.json` — 看一个简单拓扑

### 第 2 天:C++ 核心
1. 读 `include/core/sim_object.hh` — 仿真对象基类
2. 读 `include/core/port_types.hh` + `master_port.hh` + `slave_port.hh` — 端口系统
3. 读 `include/core/module_factory.hh` + `src/core/module_factory.cc` — 工厂装配
4. 跑 `./build/bin/cpptlm_tests "[phase6]"` — 看端到端测试

### 第 3 天:TLM 模块与 StreamAdapter
1. 读 `include/tlm/cache_tlm.hh` + `crossbar_tlm.hh` + `memory_tlm.hh` — 核心三件套
2. 读 `include/framework/stream_adapter.hh` — 单端口适配
3. 读 `include/bundles/noc_bundles_tlm.hh` — Bundle 消息格式

### 第 4 天:Python 工具链
1. 读 `cpptlm_config/builder.py` — Fluent Builder
2. 读 `cpptlm/simulation/runner.py` — 仿真执行器
3. 跑 `python -m cpptlm --help` — 看 CLI

### 第 5 天:扩展主题
- 指标系统:`include/metrics/`
- 可视化:`cpptlm/visualization/`
- ADR 文档:`docs/adr/`

---

## 8. 常用命令速查 (Quick Command Reference)

```bash
# 本地构建
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 运行测试
./build/bin/cpptlm_tests                    # 全部
./build/bin/cpptlm_tests "[chstream]"       # ChStream 集成
./build/bin/cpptlm_tests "[phase6]"         # 端到端集成
./build/bin/cpptlm_tests "[crossbar]"       # Crossbar 模块
ctest --test-dir build --output-on-failure  # ctest 入口

# 格式检查
./scripts/format.sh --check

# Python 工具
python -m cpptlm --help
python -m cpptlm_config.examples.mesh_2x2  # 运行示例

# GitHub CLI
export GITHUB_TOKEN="github_pat_xxx"
gh run list --limit 5
gh pr create --title "type: 描述" --body "..."
gh pr merge #<number> --squash
```

---

## 9. 反模式 (Anti-Patterns)

来自 `AGENTS.md` 的项目规约:
- **禁止 GLOB 源文件** — CMakeLists.txt 显式列举(test/ 除外)
- **禁止直接创建对象** — 必须通过 `ModuleFactory::registerObject/registerModule` 注册
- **禁止跳过 StreamAdapter** — ChStreamModuleBase 派生类必须通过 `set_stream_adapter()` 注入
- **Legacy 模块** — `include/modules/legacy/` 仅修严重 bug,新功能走 `include/tlm/`
- **测试禁止 `.disabled`** — 已知跳过状态,新代码禁止创建
- **禁止 TODO 残留** — Step 7 等未完成逻辑必须在 Phase 完成前清除或归档
- **禁止跳过本地 CI 验证** — 推送前必须本地通过构建和测试

---

## 10. 知识图谱元数据 (Graph Metadata)

- **图谱版本**:1.0.0
- **分析时间**:2026-06-04T08:57:47Z
- **Git Commit**:14acd9a1aa75887d74ae25e88ac75e87ce86568f
- **文件节点**:285(52 complex / 155 moderate / 78 simple)
- **执行流**:532
- **社区**:10(由 Leiden 算法识别)
- **FTS 索引**:4601 节点

> 本文档由 `/understand-onboard` 技能根据 `.understand-anything/knowledge-graph.json` 自动生成。
> 如需重新生成,请先运行 `/understand` 刷新知识图谱。
