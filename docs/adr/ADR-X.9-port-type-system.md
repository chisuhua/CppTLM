# ADR-X.9: Phase 3+ 端口类型系统

> **版本**: 3.0
> **日期**: 2026-05-05
> **状态**: 📋 待实施（Phase 3.2）
> **关联**: TGMS Phase 3.2, ARCH-012 Gap Analysis, ARCH-010 §7, proposal.md Decision 1 & 5
> **变更**: v2.0 → v3.0: 补充术语表、端口索引定义、端口组说明、已弃用命名格式说明

---

## 术语

本 ADR 使用以下术语体系：

| 术语 | 定义 | 示例 |
|------|------|------|
| **Port Type** | 端口类型的广义术语，包含 PortRole 和 BundleType | - |
| **PortRole** | 端口角色枚举，定义端口在连接中的行为 | `initiator`, `target`, `bi_directional`, `network`, `pe` |
| **BundleType** | 捆绑类型枚举，定义端口传输的数据类型 | `cache_req`, `cache_resp`, `noc_flit`, `generic` |
| **Port Index** | 端口类型的物理实现，端口在模块中的数字索引 | RouterTLM: 0=NORTH, 1=EAST, 2=SOUTH, 3=WEST, 4=LOCAL |
| **Port Group** | 逻辑端口组，将多个物理端口组织为逻辑单元 | NICTLM 的 `port_groups` 配置 |

**阶段编号说明**（与 ADR-X.11 一致）：
- **Phase 3+**：Phase 3.1-3.4 的统称
- **Phase 3.2**：端口管理阶段（本 ADR 的实施阶段）
- **Phase 3.3**：配置增强阶段
- **Phase 3.4**：拓扑验证阶段

---

## 背景

ARCH-012 差距分析识别出端口管理是实现覆盖率最低的领域之一（~40% vs gem5 的 ~85%）。
当前代码中端口连接仅通过 `MasterPort`/`SlavePort` 区分方向，缺少类型检查、Bundle 匹配、
数据宽度验证等能力，导致无效配置可以在实例化时"通过"但在仿真时产生难以调试的错误。

Phase 3.2 的目标是建立完整的端口类型注册和兼容性检查系统。

---

## 决策 1: 使用 nlohmann/json 结构体序列化

### 问题

如何设计端口类型声明系统，让用户通过 JSON 配置完全控制，无需修改 C++ 代码？

### 选项对比

| 选项 | 设计 | 优点 | 缺点 |
|------|------|------|------|
| **A) nlohmann/json 结构体序列化** ✅ | 定义 C++ struct + JSON 宏，自动反序列化配置 | 用户完全通过 JSON 控制，代码零修改，DRAMSys 验证模式 | 需要 C++ struct 与 JSON schema 同步 |
| B) 静态 get_port_specs() 方法 | 模块类实现静态方法返回规格 | 编译时验证 | 每次修改参数需要改 C++ 代码 |
| C) 运行时注册 | 构造函数中调用 registerPort() | 灵活 | 初始化顺序敏感，容易遗漏 |

### 决策

✅ **选项 A) nlohmann/json 结构体序列化**

CppTLM 已经内置 nlohmann/json（`external/json/nlohmann/json.hpp`）。我们使用其宏定义
实现 C++ 结构体与 JSON 的自动双向转换，参考 DRAMSys 的配置驱动仿真模式。

**核心模式**:
```cpp
// include/core/port_types.hh
#include <nlohmann/json.hpp>

enum class PortRole { INITIATOR, TARGET, BI_DIRECTIONAL, NETWORK, PE };
enum class BundleType { CACHE_REQ, CACHE_RESP, NOC_FLIT, GENERIC };

NLOHMANN_JSON_SERIALIZE_ENUM(PortRole, {
    {PortRole::INITIATOR, "initiator"},
    {PortRole::TARGET, "target"},
    {PortRole::BI_DIRECTIONAL, "bi_directional"},
    {PortRole::NETWORK, "network"},
    {PortRole::PE, "pe"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(BundleType, {
    {BundleType::CACHE_REQ, "cache_req"},
    {BundleType::CACHE_RESP, "cache_resp"},
    {BundleType::NOC_FLIT, "noc_flit"},
    {BundleType::GENERIC, "generic"},
})

struct PortSpec {
    std::string name;
    PortRole role;
    BundleType bundle;
    unsigned width = 64;         // bits, default 64
    bool is_multi = false;
    unsigned port_count = 1;
};

// 非侵入式：无需修改 PortSpec 结构体
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PortSpec, name, role, bundle, width, is_multi, port_count)
```

**用户使用方式（JSON 配置）**:
```json
{
    "modules": [
        {
            "name": "r0",
            "type": "RouterTLM",
            "port_specs": [
                {"name": "NORTH", "role": "bi_directional", "bundle": "noc_flit", "width": 64},
                {"name": "EAST", "role": "bi_directional", "bundle": "noc_flit", "width": 64},
                {"name": "SOUTH", "role": "bi_directional", "bundle": "noc_flit", "width": 64},
                {"name": "WEST", "role": "bi_directional", "bundle": "noc_flit", "width": 64},
                {"name": "LOCAL", "role": "bi_directional", "bundle": "noc_flit", "width": 64}
            ],
            "params": {"mesh_x": 2, "mesh_y": 2}
        }
    ]
}
```

**ModuleFactory 自动反序列化**:
```cpp
// src/core/module_factory.cc
for (const auto& spec_json : module_json["port_specs"]) {
    PortSpec spec = spec_json.get<PortSpec>();  // 一行完成反序列化
    // spec.name, spec.role, spec.bundle 全部自动填充
    // 缺失字段使用默认值（width=64, is_multi=false, port_count=1）
}
```

**理由**:
- 用户完全通过 JSON 控制端口规格，无需修改 C++ 代码
- `WITH_DEFAULT` 宏自动处理可选字段，缺失时使用结构体默认值
- DRAMSys 已在 SystemC/TLM 领域验证此模式
- 与 CppTLM 现有的 JSON 配置驱动架构一致

---

## 决策 1.5: 端口索引定义（G3 共识）

### 问题

端口索引（port index）与端口类型（port type）的关系是什么？是否应在本 ADR 中定义端口索引？

### 决策

✅ **端口索引是端口类型的物理实现**，应在本 ADR 中统一定义。

**端口索引规范**：

| 模块类型 | 端口索引 | 端口名称 | 说明 |
|---------|---------|---------|------|
| **RouterTLM** | 0 | NORTH | 北向端口 |
| | 1 | EAST | 东向端口 |
| | 2 | SOUTH | 南向端口 |
| | 3 | WEST | 西向端口 |
| | 4 | LOCAL | 本地端口（连接 NIC/PE） |
| **NICTLM** | 0 | NETWORK | 网络侧端口 |
| | 1 | PE | PE 侧端口 |

**连接格式**：
- 标准格式：`module_name.port_index`（如 `router_0.0`）
- 已弃用格式：`module_name.port_name`（如 `router.NORTH`、`r0.E_out`）

**已弃用命名格式说明**（G3 共识）：

以下命名端口格式标记为 **deprecated**，将在 Phase 4 移除：
- `router.NORTH`, `router.EAST`, `router.SOUTH`, `router.WEST`, `router.LOCAL`
- `r0.E_out`, `r0.N_in` 等旧格式

**弃用理由**：
- 命名格式与端口索引映射不一致（不同模块可能有不同的命名约定）
- 数字索引是 RouterTLM 的物理实现，更稳定可靠
- 与 ARCH-010 §7.4 端口索引规范一致

**迁移建议**：
- 新配置应使用数字索引：`router_0.0`（NORTH）、`router_0.4`（LOCAL）
- 如需可读性，应使用端口别名系统（决策 3）：`{"r0": {"NORTH": "0"}}`
- 现有使用命名格式的配置仍可工作，但会打印 WARNING 日志

**Python API 支持**（G3 共识）：

Python 配置生成器提供端口索引枚举，同时允许直接使用数字：

```python
from cpptlm_config import RouterPort, NICPort

# 方式 1：使用枚举（推荐，类型安全）
builder.add_connection(ConnectionSpec(
    src=f"router_{x}_{y}.{RouterPort.NORTH.value}",  # 0
    dst=f"router_{x+1}_{y}.{RouterPort.WEST.value}"  # 3
))

# 方式 2：直接使用数字（简洁）
builder.add_connection(ConnectionSpec(
    src=f"router_{x}_{y}.0",  # NORTH
    dst=f"router_{x+1}_{y}.3"  # WEST
))
```

---

## 决策 1.6: 端口组（Port Groups）说明（m2 共识）

### 问题

NICTLM 的端口组（`port_groups`）与端口类型的关系是什么？

### 决策

✅ **端口组是逻辑端口容器**，组内端口共享相同的 BundleType 和连接策略。

**端口组配置格式**：
```json
{
    "modules": [
        {
            "name": "nic_0",
            "type": "NICTLM",
            "port_groups": [
                {
                    "name": "eth0_group",
                    "ports": [
                        {"index": 0, "role": "bi_directional", "bundle": "noc_flit"},
                        {"index": 1, "role": "bi_directional", "bundle": "noc_flit"}
                    ],
                    "bundle_type": "bundle_master"
                }
            ]
        }
    ]
}
```

**端口组与端口类型关系**：
- 端口组内的每个端口独立定义 PortRole 和 BundleType
- 端口组级别的 `bundle_type` 定义组的捆绑模式（`single`、`bundle_master`、`bundle_slave`）
- `bundle_master` 端口负责管理整个组的连接
- 与 ADR-X.10 的 BundleType 设计直接相关

**Python API 支持**（m2 共识）：

```python
from cpptlm_config import PortGroup, BundleType

builder.add_port_group("eth0_group", [
    PortSpec(role=PortRole.BIDIRECTIONAL, index=0, bundle=BundleType.NOC_FLIT),
    PortSpec(role=PortRole.BIDIRECTIONAL, index=1, bundle=BundleType.NOC_FLIT),
], bundle_type=BundleType.BUNDLE_MASTER)
```

---

## 决策 2: 端口兼容性检查矩阵

### 问题

如何判断两个端口可以合法连接？

### 决策

采用三层检查矩阵，按严格程度递增：

| 检查层 | 规则 | 错误级别 |
|--------|------|---------|
| L1: 方向检查 | INITIATOR→TARGET 或 BI_DIRECTIONAL↔任意 | ERROR |
| L2: Bundle 匹配 | 两端 BundleType 必须兼容 | ERROR |
| L3: 宽度检查 | 位宽一致或声明了转换器 | WARNING |

**连接规则矩阵**:

| Src Role | Dst Role | 允许? | 说明 |
|----------|----------|:---:|------|
| INITIATOR | TARGET | ✅ | 标准连接 |
| INITIATOR | BI_DIRECTIONAL | ✅ | 单向使用 |
| TARGET | INITIATOR | ❌ | 方向反了 |
| BI_DIRECTIONAL | BI_DIRECTIONAL | ✅ | NoC 互连 |
| NETWORK | NETWORK | ✅ | Router↔Router |
| PE | PE | ❌ | PE 不能直连 |
| PE | NETWORK | ⚠️ | 仅 NICTLM 允许 |

**Bundle 兼容矩阵**:

| Src Bundle | Dst Bundle | 兼容? |
|------------|------------|:---:|
| CACHE_REQ | CACHE_REQ | ✅ |
| CACHE_REQ | CACHE_RESP | ❌ |
| NOC_FLIT | NOC_FLIT | ✅ |
| CACHE_REQ | GENERIC | ⚠️ 需转换器 |

**实施位置**: `include/core/port_compatibility.hh`

```cpp
struct PortCompatibility {
    static bool is_compatible(const PortSpec& src, const PortSpec& dst);
    static bool is_role_compatible(PortRole src, PortRole dst);
    static bool is_bundle_compatible(BundleType src, BundleType dst);
    static std::string get_incompatibility_reason(const PortSpec& src, const PortSpec& dst);
};

// 也可以用 JSON 配置兼容矩阵，实现运行时可修改
struct CompatibilityRule {
    PortRole src_role;
    PortRole dst_role;
    bool allowed;
    std::string description;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CompatibilityRule, src_role, dst_role, allowed, description)
```

---

## 决策 3: 端口别名系统

### 问题

端口别名（如 `router.NORTH` 代替 `router.0`）应该如何定义和解析？

### 决策

✅ **JSON 配置驱动的别名系统**

**别名配置格式**:
```json
{
    "port_aliases": {
        "r0": { "NORTH": "0", "EAST": "1", "SOUTH": "2", "WEST": "3", "LOCAL": "4" },
        "r1": { "N": "0", "E": "1", "S": "2", "W": "3", "L": "4" }
    }
}
```

**C++ 结构体**:
```cpp
struct PortAliasConfig {
    std::string module_name;
    std::map<std::string, std::string> aliases;  // alias_name -> port_index
};

struct GlobalPortAliases {
    std::vector<PortAliasConfig> module_aliases;
    
    std::string resolve(const std::string& module_name, const std::string& alias) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PortAliasConfig, module_name, aliases)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GlobalPortAliases, module_aliases)
```

**解析位置**: `parsePortSpec()` 入口处

```cpp
std::pair<std::string, std::string> ModuleFactory::parsePortSpec(
    const std::string& spec, const GlobalPortAliases& aliases) const {
    
    auto [module_name, port_name] = basicParsePortSpec(spec);
    
    // 尝试别名解析
    std::string resolved = aliases.resolve(module_name, port_name);
    if (!resolved.empty()) {
        return {module_name, resolved};
    }
    return {module_name, port_name};
}
```

---

## 风险与权衡

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| JSON 结构体字段变更导致旧配置不兼容 | 高 | 使用 `WITH_DEFAULT` 宏，新增字段必须提供默认值 |
| 枚举字符串映射拼写错误 | 中 | 单元测试覆盖所有枚举值的序列化/反序列化 |
| 嵌套结构体反序列化失败难调试 | 中 | 添加 JSON schema 验证层，提前报告错误位置 |
| Bundle 兼容性检查增加实例化开销 | 低 | 仅在 bind 阶段执行一次，非 per-packet |

---

## 迁移计划

1. **Step 1**: 创建 `port_types.hh` 定义 PortSpec、PortRole、BundleType 和 JSON 宏
2. **Step 2**: 创建 `port_compatibility.hh` 定义兼容性检查逻辑
3. **Step 3**: ModuleFactory::instantiateAll() 添加 port_specs JSON 反序列化
4. **Step 4**: bindPorts() 添加 L1/L2 兼容性检查
5. **Step 5**: 添加 `port_aliases` config 段解析
6. **Step 6**: RouterTLM/NICTLM 的 JSON 配置示例和测试

所有变更均为**增量添加**，不影响现有配置和模块。

---

## 开放问题

1. 是否支持自定义 Bundle 转换器声明？（如 `{"converter": "CacheToNoC", "src": "CACHE_REQ", "dst": "NOC_FLIT"}`）
2. 是否需要在 PortSpec 中添加时钟域信息？
3. 端口别名是否支持通配符模式？（如 `"r*": {"NORTH": "0"}` 应用于所有 r 开头的模块）

---

## 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-05 | 初始提案，设计端口类型系统 |
| 2.0 | 2026-05-05 | 更新端口别名系统，补充兼容性检查矩阵 |
| 3.0 | 2026-05-05 | 补充术语表、端口索引定义（G3 共识）<br>补充端口组说明（m2 共识）<br>标记已弃用命名格式（G3 共识）<br>补充阶段编号说明（m3 共识）<br>与 ARCH-010 §7 端口索引规范对齐 |

## 共识追踪

| 议题 | 状态 | 说明 |
|------|------|------|
| G3 | ✅ 已整合 | 端口索引是端口类型的物理实现，Python API 提供枚举但允许数字 |
| m2 | ✅ 已整合 | 端口组说明，ConfigBuilder 提供 add_port_group() |
| m3 | ✅ 已整合 | 术语统一、阶段编号统一 |

---

**下一步**: Phase 3.2 实施时遵循此 ADR
