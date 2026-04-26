# CppTLM 拓扑系统差距分析 — 审查修正版

**文档编号**: ARCH-012  
**版本**: v1.1 (审查修正版)  
**日期**: 2026-04-27  
**状态**: 已批准  
**依赖**: ARCH-010 (TGMS v2.0), IMPL-010 (实施计划), SPEC-010 (配置规范), ARCH-011 (一致性域准备状态)  
**作者**: CppTLM Architecture Team

---

> **本文档说明**: 本文档是对原始 ARCH-012 差距分析的审查修正版。修正基于对 CppTLM 实际源代码的逐行验证，以及对 ARCH-010 v2.0、IMPL-010 v2.0、SPEC-010 v2.0 的交叉引用。

---

## 目录

1. [执行摘要](#1-执行摘要)
2. [版本与基线说明](#2-版本与基线说明)
3. [已验证差距清单](#3-已验证差距清单)
4. [v4.0 新增差距项](#4-v40-新增差距项)
5. [IMPL-010 实施路线图遗漏项](#5-impl-010-实施路线图遗漏项)
6. [ModuleFactory 代码缺陷](#6-modulefactory-代码缺陷)
7. [修正后优先级与实施建议](#7-修正后优先级与实施建议)
8. [变更日志](#8-变更日志)

---

## 1. 执行摘要

本文档对 CppTLM 拓扑生成与维护系统的差距进行了全面审查。审查覆盖：

- **G1-G7**: ARCH-010 v1.2 识别的原始差距项（逐行代码验证）
- **G8-G10**: IMPL-010 v2.0 新增的 v4.0 差距项（层次化拓扑、一致性域、协议桥）
- **P0 缺失映射**: ARCH-012 原始版识别的 9 项 P0 差距在 IMPL-010 中的覆盖情况
- **ModuleFactory 代码缺陷**: 4 个未被任何实施计划覆盖的代码级缺陷（原始 5 个中 DEF-02 经审查为误报）

**核心结论**:

| 发现 | 详情 |
|------|------|
| 原始差距项验证 | G1/G2/G4/G5/G6 确认存在，G3 已修正为非阻塞，G7 确认部分存在 |
| v4.0 差距 | G8/G9/G10 在 IMPL-010 中已有设计，但代码实现为零 |
| 实施路线图遗漏 | IMPL-010 Phase 1-3 完全覆盖 G1-G7，但遗漏了 CFG/PARAM/PORT/VALID/SIM 类别的 9 项关键验证功能 |
| ModuleFactory 缺陷 | 4 个代码级缺陷未被任何实施计划覆盖（原始 5 个，经审查 DEF-02 为误报） |

---

## 2. 版本与基线说明

### 2.1 分析基准

| 文档 | 版本 | 日期 | 用途 |
|------|------|------|------|
| ARCH-010 | v2.0 | 2026-04-26 | 系统架构设计（已拆分为实施计划+规范） |
| IMPL-010 | v2.0 | 2026-04-26 | 实施路线图（含 v3.0 Phase 1-3 和 v4.0 Phase 4-6） |
| SPEC-010 | v2.0 | 2026-04-26 | JSON Schema + 端口索引规范 |
| ARCH-011 | v1.0 | 2026-04-26 | 一致性域准备状态分析 |
| CppTLM 源码 | HEAD | 2026-04-26 | 实际代码验证 |

### 2.2 源代码验证范围

| 文件 | 验证项 | 结论 |
|------|--------|------|
| `scripts/topology_generator.py` (716 行) | 端口索引生成、params 导出、验证器 | 无端口索引、无 params、无验证器 |
| `src/core/module_factory.cc` (412 行) | Step 2.5 参数传递、端口索引解析 | 无 Step 2.5，端口索引解析已实现 |
| `include/core/sim_object.hh` (303 行) | set_config() 虚方法 | 不存在 |
| `configs/mesh_2x2.json` | 配置格式一致性 | node_id 在顶层，无 params，无端口索引 |

---

## 3. 已验证差距清单

### 3.1 逐项验证结果

| 编号 | 差距描述 | 优先级 | 源代码验证结果 | 原始判定 | 审查修正 |
|------|---------|--------|---------------|---------|----------|
| **G1** | RouterTLM 拓扑参数未传递（无 set_config，所有实例默认 (0,0,2,2)） | P0 | `sim_object.hh` 无 `set_config()`；`module_factory.cc` 无 Step 2.5；RouterTLM 构造函数默认参数 `node_x=0, node_y=0, mesh_x=2, mesh_y=2` | 未实现 | **确认，无需修正** |
| **G2** | 连接缺少端口索引（Python 不生成，JSON 不包含） | P0 | `topology_generator.py` generate_mesh() 行 130-155：连接仅为 `router_r_c -> router_r_c+1`，无 `.1`/`.3` 后缀；export_json_config() 行 386-392：无端口索引 | 未实现 | **确认，无需修正** |
| **G3** | BidirectionalPortAdapter 端口索引 | P0→已修正 | `registerBidirectionalPortAdapter` 正确设置 `port_count_[type] = N`；`isMultiPort()` 正确返回 | 原判定错误 | **修正为：非阻塞，但需测试验证** |
| **G4** | noc_builder.py 端口格式不兼容 | — | **脚本不存在于 `scripts/` 目录** | 未实现 | **修正为：差距已消除（脚本已移除）** |
| **G5** | noc_mesh.py 不可运行 | — | **脚本不存在于 `scripts/` 目录** | 未处理 | **修正为：差距已消除（脚本已移除）** |
| **G6** | ModuleFactory 缺少 Step 2.5 参数传递 | P0 | `module_factory.cc` Step 2 后直接进入 Step 3（groups），无 params 传递代码 | 未实现 | **确认，与 G1 本质相同** |
| **G7** | 现有 JSON 配置格式不一致 | P1 | `configs/mesh_2x2.json`：`node_id` 在模块顶层而非 `params` 中；RouterTLM 完全缺少拓扑参数 | 部分 | **确认，无需修正** |

### 3.2 差距汇总

| 编号 | 差距 | 优先级 | 状态 | 实施计划覆盖 |
|------|------|--------|------|-------------|
| G1 | 拓扑参数未传递 | P0 | 未实现 | IMPL-010 Phase 1.1-1.3 |
| G2 | 连接缺少端口索引 | P0 | 未实现 | IMPL-010 Phase 2.1 |
| G3 | BidirectionalPortAdapter 索引 | — | 已修正 | 无需实施 |
| G4 | noc_builder.py 端口格式 | — | **已消除** | N/A（脚本已移除） |
| G5 | noc_mesh.py 不可运行 | — | **已消除** | N/A（脚本已移除） |
| G6 | ModuleFactory 缺少 Step 2.5 | P0 | 未实现 | IMPL-010 Phase 1.4（与 G1 合并） |
| G7 | JSON 配置格式不一致 | P1 | 部分 | IMPL-010 Phase 1.5 |

---

## 4. v4.0 新增差距项

以下差距项来自 IMPL-010 v2.0 Phase 4-6 的设计，但在当前代码中**完全未实现**。

### 4.1 G8: 层次化拓扑树解析器

| 属性 | 值 |
|------|-----|
| **编号** | G8 / HIER-01 |
| **优先级** | P1（v4.0 前置条件） |
| **描述** | ModuleFactory 无 `parse_hierarchy_tree()` 方法，无法解析 JSON 中的 `hierarchy` 字段 |
| **源代码验证** | `module_factory.cc` 无 `hierarchy` 字段处理；无 `parse_hierarchy_tree()` 函数 |
| **依赖** | Phase 1 完成（set_config 机制） |
| **实施计划覆盖** | IMPL-010 Phase 4.1（3 days），Phase 4.6（Python 生成器，5 days） |
| **SPEC-010 对应** | §1.2 v4.0 Schema Extensions — `hierarchy` 字段 |

### 4.2 G9: CoherenceDomain C++ 模块

| 属性 | 值 |
|------|-----|
| **编号** | G9 / COH-01 |
| **优先级** | P1（v4.0 核心功能） |
| **描述** | CoherenceDomain 类不存在。ARCH-011 确认代码准备度约 15%（仅有基础设施） |
| **源代码验证** | `include/core/coherence_domain.hh` 不存在；无任何 CoherenceDomain 相关文件 |
| **现有基础设施** | CoherenceState 枚举（已实现）、7 个一致性错误码（已实现）、DebugTracker（已实现） |
| **缺失核心** | CoherenceDomain 类、CoherenceDomainManager 类、Snooping 机制、Directory-based 协议、CacheCoherenceV2 模块、domain_id 字段 |
| **依赖** | G8（层次化拓扑）、Phase 1（set_config 机制） |
| **实施计划覆盖** | IMPL-010 Phase 4.2-4.5（合计 15 days） |
| **SPEC-010 对应** | §1.2 `coherence_domains` 字段、§3.3 CoherenceDomain Parameters |
| **参考** | ARCH-011 §3-4 详细准备状态分析 |

### 4.3 G10: ProtocolBridge C++ 模块

| 属性 | 值 |
|------|-----|
| **编号** | G10 / BRIDGE-01 |
| **优先级** | P2（v4.0 跨域通信） |
| **描述** | ProtocolBridge 类不存在，无地址翻译引擎，无协议转换逻辑 |
| **源代码验证** | `include/core/protocol_bridge.hh` 不存在；无任何 ProtocolBridge 相关文件 |
| **依赖** | G9（CoherenceDomain）、Phase 4 |
| **实施计划覆盖** | IMPL-010 Phase 5.1-5.5（合计 19 days） |
| **SPEC-010 对应** | §1.2 `bridges` 字段、§3.4 ProtocolBridge Parameters |

### 4.4 v4.0 差距汇总

| 编号 | 差距 | 优先级 | 状态 | 实施计划覆盖 |
|------|------|--------|------|-------------|
| G8 | 层次化拓扑树解析器 | P1 | 未实现 | IMPL-010 Phase 4.1, 4.6 |
| G9 | CoherenceDomain C++ 模块 | P1 | 未实现（准备度 15%） | IMPL-010 Phase 4.2-4.5 |
| G10 | ProtocolBridge C++ 模块 | P2 | 未实现 | IMPL-010 Phase 5.1-5.5 |

---

## 5. IMPL-010 实施路线图遗漏项

### 5.1 概述

ARCH-010 v1.2 原始差距分析（§3.2）识别了以下**9 项关键缺失功能**，这些功能在 **IMPL-010 的 6 个 Phase 中完全没有对应任务**：

| 遗漏类别 | 遗漏项数 | 影响 |
|----------|---------|------|
| Schema 验证 | 1 | 无法验证 JSON 配置合法性 |
| 参数类型声明/必填检查 | 2 | 无法在加载时捕获类型错误 |
| 端口方向/类型/Bundle 检查 | 3 | 无法检测端口连接错误 |
| 连接完整性/BFS 可达性验证 | 2 | 无法验证拓扑连通性 |
| 路由表自动生成 | 1 | 需手动配置路由表 |

### 5.2 遗漏项详细列表

#### CFG-08: JSON Schema 验证（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无 JSON Schema 验证器。加载配置时无法检测格式错误、缺少必填字段、类型不匹配等 |
| **现状** | `module_factory.cc` 直接使用 `nlohmann::json` 解析，无 Schema 验证 |
| **SPEC-010 对应** | §1.1 v3.0 Schema（Draft-07），已定义但未实现验证器 |
| **建议实施** | 使用 `nlohmann/json-schema` 库，在 `instantiateAll()` 入口添加 Schema 验证步骤 |

#### PARAM-01: 参数类型声明（P1）

| 属性 | 值 |
|------|-----|
| **描述** | `params` 字段无类型声明机制。`set_config()` 接收任意 JSON，无类型检查 |
| **现状** | IMPL-010 §3.1-3.3 的 `set_config()` 实现直接 `params["node_x"].get<int>()`，无类型检查，类型不匹配时抛出未捕获异常 |
| **建议实施** | 在 `set_config()` 中添加 `contains()` + `is_number()` 检查，或在基类添加类型注册机制 |

#### PARAM-05: 必填参数检查（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无必填参数检查。RouterTLM 缺少 `node_x/y` 时使用默认值（错误的 0,0），不报错 |
| **现状** | IMPL-010 §3.2 RouterTLM `set_config()` 使用 `if (params.contains("node_x"))` 静默跳过，不报错 |
| **建议实施** | 添加必填参数列表，缺失时抛出异常或警告 |

#### PORT-01: 端口方向检查（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无法检测端口方向错误（如将 req_out 连接到 req_out） |
| **现状** | `module_factory.cc` Step 6 仅创建 PortPair，不检查方向兼容性 |
| **建议实施** | 在 ConnectionResolver 中添加方向检查逻辑 |

#### PORT-02: 端口类型兼容性检查（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无法检测 Bundle 类型不匹配（如将 CacheReqBundle 连接到 CacheRespBundle） |
| **现状** | `PortPair` 构造函数接受任意 MasterPort/SlavePort 组合，无类型检查 |
| **建议实施** | 在 PortPair 构造时检查 Bundle 类型兼容性 |

#### PORT-03: Bundle 类型验证（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无 Bundle 类型注册表，无法验证连接两端的 Bundle 类型是否匹配 |
| **现状** | `ChStreamAdapterFactory` 知道适配器类型，但无全局 Bundle 类型注册表 |
| **建议实施** | 扩展 ChStreamAdapterFactory 维护 Bundle 类型注册表 |

#### VALID-01: 连接完整性验证（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无连接完整性验证。多端口模块的所有端口是否已连接无法自动检测 |
| **现状** | `topology_generator.py` 无 TopologyValidator 类；ARCH-010 §5.4 定义了验证器设计但未实现 |
| **ARCH-010 对应** | §5.4 TopologyValidator — 连接完整性检查 |
| **建议实施** | 实现 Python TopologyValidator 类，包含 `_check_port_connections()` 方法 |

#### VALID-02: BFS 可达性验证（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无路由可达性验证。无法检测网络分区或不可达节点 |
| **现状** | 同 VALID-01，Python 和 C++ 均无实现 |
| **ARCH-010 对应** | §5.4 TopologyValidator — 路由可达性检查（BFS） |
| **建议实施** | 实现 `_check_reachability()` 方法，支持 strict（双向）和 weak（无向）两种模式 |

#### SIM-03: 路由表自动生成（P1）

| 属性 | 值 |
|------|-----|
| **描述** | 无路由表自动生成。RouterTLM 使用硬编码 XY 路由，不支持自定义路由策略 |
| **现状** | RouterTLM 使用 `compute_xy_routing()` 硬编码，无路由表配置机制 |
| **建议实施** | 添加路由表生成器，支持从拓扑图自动计算路由表 |

### 5.3 遗漏项汇总表

| 编号 | 差距 | 优先级 | IMPL-010 覆盖 | 建议 Phase |
|------|------|--------|--------------|-----------|
| CFG-08 | JSON Schema 验证 | P1 | **未覆盖** | Phase 1 |
| PARAM-01 | 参数类型声明 | P1 | **未覆盖** | Phase 1 |
| PARAM-05 | 必填参数检查 | P1 | **未覆盖** | Phase 1 |
| PORT-01 | 端口方向检查 | P1 | **未覆盖** | Phase 2 |
| PORT-02 | 端口类型兼容性 | P1 | **未覆盖** | Phase 2 |
| PORT-03 | Bundle 类型验证 | P1 | **未覆盖** | Phase 2 |
| VALID-01 | 连接完整性验证 | P1 | **未覆盖** | Phase 3 |
| VALID-02 | BFS 可达性验证 | P1 | **未覆盖** | Phase 3 |
| SIM-03 | 路由表自动生成 | P1 | **未覆盖** | Phase 3 |

---

## 6. ModuleFactory 代码缺陷

以下 4 个代码级缺陷在 ARCH-010 v1.2 §7.1 中被识别，在 **IMPL-010 中完全未提及**。

> **审查说明**: 原始分析包含 5 个缺陷（DEF-01 到 DEF-05）。经源代码审查，DEF-02 为**误报**（Step 6 处理常规端口，Step 7b 处理 ChStream 端口，同一连接不会被两者重复处理），已从本列表移除。

### 6.1 DEF-01: ModuleGroup 通配符不展开

| 属性 | 值 |
|------|-----|
| **位置** | `module_factory.cc` Step 6，行 175-196 |
| **问题** | `ModuleGroup::resolve()` 仅返回预定义的组成员列表。对于通配符引用（如 `@nics`），如果组未预先定义，返回空列表。`Wildcard::match("*", src_spec)` 总是返回 true（匹配任何字符串），但通配符展开逻辑（行 177-183, 189-195）在所有实例上遍历，效率为 O(n^2) |
| **影响** | 大拓扑中性能退化；未定义组引用静默失败 |
| **建议修复** | 添加组定义存在性检查；优化通配符匹配为前缀/后缀匹配 |

### 6.2 DEF-03: BidirectionalPortAdapter 处理路径问题

| 属性 | 值 |
|------|-----|
| **位置** | `module_factory.cc` Step 7a，行 340-350 |
| **问题** | `DualPortStreamAdapter` 的 PE 侧和 Network 侧绑定是硬编码的（`req_out_vec[0]` = PE, `req_out_vec[1]` = Net）。如果 JSON 配置中的端口索引与硬编码顺序不一致，绑定错误 |
| **影响** | 配置中端口索引顺序必须与代码中硬编码顺序一致，缺乏灵活性 |
| **建议修复** | 添加端口映射配置，允许 JSON 定义 PE/Net 端口顺序 |

### 6.3 DEF-04: 端口索引解析不够严格

| 属性 | 值 |
|------|-----|
| **位置** | `module_factory.cc` parsePortSpec() + Step 7b，行 22-28, 374-380 |
| **问题** | `std::isdigit(src_spec[0])` 只检查第一个字符。对于 `router_0_0.1a`，`src_spec = "1a"`，`std::isdigit('1')` 返回 true，`std::stoul("1a")` 返回 1（静默忽略 "a"）。无效索引不报错 |
| **影响** | 配置错误（如 `router_0_0.a`）被静默忽略，可能导致连接绑定到错误端口 |
| **建议修复** | 使用 `std::stoul` + 位置参数检查整个字符串是否为有效数字 |

### 6.4 DEF-05: Python 类型映射不一致

| 属性 | 值 |
|------|-----|
| **位置** | `topology_generator.py` 行 52-69 (CPPTLM_TYPE_MAP) |
| **问题** | `CPPTLM_TYPE_MAP` 中的类型名（如 `'NetworkInterface': 'NICTLM'`）与 C++ `REGISTER_CHSTREAM` 宏注册的类型名可能不同步。无自动化机制确保 Python 和 C++ 类型注册表一致 |
| **影响** | Python 生成的配置中类型名与 C++ 注册表不匹配时，ModuleFactory 报错 "Unknown type" |
| **建议修复** | 从 C++ 注册表宏自动生成 `type_registry.json`，Python 读取该文件而非硬编码映射 |

### 6.5 缺陷汇总表

| 编号 | 缺陷 | 严重性 | IMPL-010 覆盖 | 建议 Phase |
|------|------|--------|--------------|-----------|
| DEF-01 | ModuleGroup 通配符不展开 | P2 | **未覆盖** | Phase 1 |
| DEF-03 | BidirectionalPortAdapter 硬编码绑定 | P2 | **未覆盖** | Phase 1 |
| DEF-04 | 端口索引解析不严格 | P1 | **未覆盖** | Phase 1 |
| DEF-05 | Python-C++ 类型映射不同步 | P2 | **未覆盖** | Phase 1 |

---

## 7. 修正后优先级与实施建议

### 7.1 完整差距分类

| 类别 | 差距项 | 优先级 | 状态 |
|------|--------|--------|------|
| **参数传递** | G1, G6 | P0 | 未实现 |
| **端口索引** | G2 | P0 | 未实现 |
| **配置格式** | G7 | P1 | 部分 |
| **脚本清理** | G4, G5 | — | **已消除** |
| **层次化拓扑** | G8 | P1 | 未实现 |
| **一致性域** | G9 | P1 | 未实现（15%） |
| **协议桥** | G10 | P2 | 未实现 |
| **Schema 验证** | CFG-08 | P1 | 未实现 |
| **参数验证** | PARAM-01, PARAM-05 | P1 | 未实现 |
| **端口验证** | PORT-01, PORT-02, PORT-03 | P1 | 未实现 |
| **拓扑验证** | VALID-01, VALID-02 | P1 | 未实现 |
| **路由表** | SIM-03 | P1 | 未实现 |
| **代码缺陷** | DEF-01, DEF-03, DEF-04, DEF-05 | P1/P2 | 未修复 |

### 7.2 建议实施路线图（修正版）

在 IMPL-010 现有 6 个 Phase 基础上，建议增加以下任务：

#### Phase 1 补充任务

| 任务 | 对应差距 | 工作量 |
|------|---------|--------|
| 添加 JSON Schema 验证器 | CFG-08 | 2 days |
| 增强 set_config() 类型检查 | PARAM-01, PARAM-05 | 1 day |
| 修复端口索引解析严格性 | DEF-04 | 0.5 day |
| 修复 ModuleGroup 通配符 | DEF-01 | 1 day |
| 修复 BidirectionalPortAdapter 绑定 | DEF-03 | 1 day |
| 生成 type_registry.json | DEF-05 | 1 day |

#### Phase 2 补充任务

| 任务 | 对应差距 | 工作量 |
|------|---------|--------|
| 添加端口方向检查 | PORT-01 | 1 day |
| 添加端口类型兼容性检查 | PORT-02 | 1 day |
| 添加 Bundle 类型验证 | PORT-03 | 1 day |

#### Phase 3 补充任务

| 任务 | 对应差距 | 工作量 |
|------|---------|--------|
| 实现 Python TopologyValidator | VALID-01, VALID-02 | 3 days |
| 实现路由表自动生成器 | SIM-03 | 3 days |
| C++ 验证器集成 | VALID-01, VALID-02 | 2 days |

### 7.3 实施顺序依赖图（修正版）

```
Phase 1 (Core + Validation Foundation)
├── SimObject.set_config()          [IMPL-010 1.1]
├── RouterTLM.set_config()          [IMPL-010 1.2]
├── NICTLM.set_config()             [IMPL-010 1.3]
├── ModuleFactory Step 2.5          [IMPL-010 1.4]
├── Config format v3.0              [IMPL-010 1.5]
├── JSON Schema 验证器              [新增 CFG-08]
├── set_config() 类型检查           [新增 PARAM-01, PARAM-05]
├── 端口索引解析严格性              [新增 DEF-04]
├── ModuleGroup 通配符              [新增 DEF-01]
├── BidirectionalPortAdapter 绑定   [新增 DEF-03]
└── type_registry.json              [新增 DEF-05]
        │
Phase 2 (Python Toolchain + Port Validation)
├── Port index generation           [IMPL-010 2.1]
├── NoCBuilder migration            [IMPL-010 2.2]
├── NoCMesh cleanup                 [IMPL-010 2.3]
├── 端口方向检查                    [新增 PORT-01]
├── 端口类型兼容性                  [新增 PORT-02]
├── Bundle 类型验证                 [新增 PORT-03]
└── Integration tests               [IMPL-010 2.4]
        │
Phase 3 (Validation + Routing)
├── 2x2 Mesh validation             [IMPL-010 3.1]
├── 4x4 Mesh validation             [IMPL-010 3.2]
├── Example configs                 [IMPL-010 3.3]
├── Python TopologyValidator        [新增 VALID-01, VALID-02]
├── 路由表自动生成                  [新增 SIM-03]
└── C++ 验证器集成                  [新增 VALID-01, VALID-02]
        │
Phase 4 (Hierarchy Core - v4.0)     [IMPL-010 4.1-4.6]
        │
Phase 5 (Protocol Bridge - v4.0)    [IMPL-010 5.1-5.5]
        │
Phase 6 (Multi-Cluster SoC - v4.0)  [IMPL-010 6.1-6.4]
```

---

## 8. 变更日志

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-04-26 | 初始差距分析（ARCH-010 v1.2 附带） |
| v1.1 | 2026-04-27 | **审查修正版**：逐项验证源代码，修正 G3/G4/G5 状态，删除误报 DEF-02，调整 9 项遗漏优先级（P0→P1），新增 v4.0 差距（G8-G10），新增 4 个 ModuleFactory 代码缺陷，修正实施路线图 |

---

**文档结束**
