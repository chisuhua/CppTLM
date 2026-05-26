# Phase 3.2: 端口管理系统实施计划

> **版本**: v1.2
> **编制日期**: 2026-05-07
> **基于文档**: ADR-X.9 v3.0 (端口类型系统), ADR-X.12 v2.0 (Python 配置生成器)
> **前置条件**: Phase 3.1 已完成 + 测试通过
> **预计工期**: 4 周 (Week 1-4)
> **目标**: 实现 nlohmann/json 驱动的端口类型系统，与 ADR-X.9 v3.0 完全对齐
> **状态**: ✅ C++ 端完成（所有测试通过）

---

## 一、阶段概述

### 1.1 阶段目标

Phase 3.2 聚焦于建立完整的端口类型注册和兼容性检查系统，核心目标包括:

1. **端口类型声明**: 使用 nlohmann/json 结构体序列化替代静态方法模式
2. **兼容性检查**: 实现三层检查矩阵 (方向 → Bundle → 宽度)
3. **端口别名**: 提供可读的端口命名系统
4. **Python 集成**: 端口索引枚举、版本管理、可视化元数据

### 1.2 共识事项覆盖

| 共识编号 | 内容 | 状态 |
|---------|------|------|
| **G3** | 端口索引与端口类型映射 | 本阶段实施 |
| **G4** | 可视化管线集成 | 本阶段实施 |
| **M2** | 拓扑版本管理决策 | 本阶段实施 |
| **M3** | Python 依赖版本统一 | 本阶段实施 |
| **M4** | DEF-04 与 ARCH-010 对齐 | 本阶段实施（DEF-04 WARNING 待实现） |
| **m2** | NICTLM 端口组说明 | 本阶段实施 |
| **T3.1-09** | NI PE-side 连接验证 | ✅ Phase 3.1 已实现 |

### 1.3 关键设计变更

| 变更项 | 旧设计 (v1.0) | 新设计 (v2.0) |
|--------|-------------|-------------|
| 端口规格声明 | 静态 `get_port_specs()` 方法 | nlohmann/json 结构体序列化 |
| PortSpec 字段 | 基础字段 | 增加 `layout_hint` (G4) |
| ModuleSpec 字段 | 基础字段 | 增加 `port_groups` (m2) |
| ConfigSchema | 简单 version 字符串 | ConfigMetadata (SemVer, M2) |

---

## 二、任务清单

### 2.1 C++ 端任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 | 状态 |
|---------|---------|:---:|:---:|---------|:----:|
| T3.2-01 | **创建 `port_types.hh`** — PortSpec, PortRole, BundleType + nlohmann/json 宏 | 1d | 无 | 枚举序列化/反序列化测试通过 | ✅ |
| T3.2-02 | **创建 `port_compatibility.hh`** — 三层兼容性检查矩阵 | 1d | T3.2-01 | 兼容性矩阵测试通过 | ✅ |
| T3.2-03~06 | **ModuleFactory 集成 L1/L2/L3 兼容性检查** | 2d | T3.2-01 | JSON 配置端口规格解析正确 | ✅ |
| T3.2-07 | **端口别名系统** — deprecated_names + resolve_port_alias() | 1.5d | T3.2-03 | `router.NORTH` → `router.0` | ✅ |
| T3.2-08 | **NICTLM 端口组支持** — port_groups JSON 解析 | 1.5d | T3.2-03 | DualPortStreamAdapter 端口组配置正确 | ✅ |
| T3.2-09 | **DEF-04 WARNING 日志** — 端口索引非法时打印警告 | 0.5d | 无 | `xbar.0abc` 产生 WARNING | ✅ |
| T3.2-14 | **端口类型单元测试** — C++ 端序列化/兼容性测试 | 2d | T3.2-01~06 | 25+ 测试用例通过 | ✅ |

**小计**: 12 天 (约 2.5 周)

### 2.2 Python 端任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.2-10 | **RouterPort/NICPort 枚举** — 端口索引枚举类 | 0.5d | 无 | `RouterPort.NORTH.value == 0` |
| T3.2-11 | **SemVer 版本管理** — ConfigMetadata, bump_version() | 0.5d | 无 | 版本号升级和 changelog 正确 |
| T3.2-12 | **pyproject.toml** — 依赖声明和构建配置 | 0.5d | 无 | `pip install -e .` 成功 |
| T3.2-13 | **layout_hint 支持** — PortSpec 增加可视化元数据 | 0.5d | 无 | 配置输出包含 visualization_metadata |
| T3.2-15 | **Python 端口类型测试** | 1d | T3.2-10~13 | 15+ 测试用例通过 |

**小计**: 3 天 (约 0.75 周)

### 2.3 集成与文档任务

| 任务 ID | 任务描述 | 工作量 | 依赖 | 验收标准 |
|---------|---------|:---:|:---:|---------|
| T3.2-16 | **端到端集成测试** — 完整配置加载 + 仿真验证 | 1d | 所有开发任务 | mesh_2x2/4x4 配置通过 | ✅ |
| T3.2-17 | **更新架构文档** — 记录端口管理系统设计 | 0.5d | T3.2-16 | C++ 端文档完成（Python 端待实施） | ⚠️ |
| T3.2-18 | **更新用户指南** — 端口配置指南 | 0.5d | T3.2-16 | C++ 端文档完成（Python 端待实施） | ⚠️ |

**小计**: 2 天 (约 0.5 周)

---

## 三、详细设计

### 3.1 port_types.hh

**文件路径**: `include/core/port_types.hh`

**核心内容**:

```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <vector>

namespace cpptlm {

// 端口角色枚举
enum class PortRole { INITIATOR, TARGET, BI_DIRECTIONAL, NETWORK, PE };

NLOHMANN_JSON_SERIALIZE_ENUM(PortRole, {
    {PortRole::INITIATOR, "initiator"},
    {PortRole::TARGET, "target"},
    {PortRole::BI_DIRECTIONAL, "bi_directional"},
    {PortRole::NETWORK, "network"},
    {PortRole::PE, "pe"},
})

// Bundle 类型枚举
enum class BundleType { CACHE_REQ, CACHE_RESP, NOC_FLIT, GENERIC };

NLOHMANN_JSON_SERIALIZE_ENUM(BundleType, {
    {BundleType::CACHE_REQ, "cache_req"},
    {BundleType::CACHE_RESP, "cache_resp"},
    {BundleType::NOC_FLIT, "noc_flit"},
    {BundleType::GENERIC, "generic"},
})

// 端口规格结构体
struct PortSpec {
    std::string name;
    PortRole role;
    BundleType bundle;
    unsigned width = 64;         // bits, 默认 64
    bool is_multi = false;
    unsigned port_count = 1;
    std::string layout_hint;     // "north", "east", "south", "west", "local" (G4)
};

// 非侵入式序列化
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PortSpec, name, role, bundle, width, is_multi, port_count, layout_hint)

// 端口别名配置
struct PortAliasConfig {
    std::string module_name;
    std::map<std::string, std::string> aliases;  // alias_name -> port_index
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PortAliasConfig, module_name, aliases)

// 全局端口别名
struct GlobalPortAliases {
    std::vector<PortAliasConfig> module_aliases;
    
    std::string resolve(const std::string& module_name, const std::string& alias) const;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GlobalPortAliases, module_aliases)

// 端口组 bundle 类型 (m2 共识)
enum class PortGroupBundleType {
    SINGLE,           // 独立端口，不分组
    BUNDLE_MASTER,    // 主端口组（控制侧）
    BUNDLE_SLAVE      // 从端口组（响应侧）
};

NLOHMANN_JSON_SERIALIZE_ENUM(PortGroupBundleType, {
    {PortGroupBundleType::SINGLE, "single"},
    {PortGroupBundleType::BUNDLE_MASTER, "bundle_master"},
    {PortGroupBundleType::BUNDLE_SLAVE, "bundle_slave"}
})

// 端口组成员（引用已有端口 by index）
struct PortGroupMember {
    unsigned index;                        // 端口索引
    PortRole role = PortRole::BI_DIRECTIONAL;  // 角色覆盖
    BundleType bundle = BundleType::GENERIC;   // bundle 类型覆盖
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PortGroupMember, index, role, bundle)

// 端口组 (m2 共识)
struct PortGroupSpec {
    std::string name;
    PortGroupBundleType bundle_type = PortGroupBundleType::SINGLE;
    std::vector<PortGroupMember> ports;  // 引用端口 by index
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    PortGroupSpec, name, bundle_type, ports)

} // namespace cpptlm
```

### 3.2 port_compatibility.hh

**文件路径**: `include/core/port_compatibility.hh`

**核心内容**:

```cpp
#pragma once
#include "port_types.hh"
#include <string>

namespace cpptlm {

class PortCompatibility {
public:
    // L1: 方向检查
    static bool is_role_compatible(PortRole src, PortRole dst);
    
    // L2: Bundle 匹配
    static bool is_bundle_compatible(BundleType src, BundleType dst);
    
    // L3: 宽度检查
    static bool is_width_compatible(unsigned src_width, unsigned dst_width);
    
    // 综合检查
    static bool is_compatible(const PortSpec& src, const PortSpec& dst);
    
    // 获取不兼容原因
    static std::string get_incompatibility_reason(const PortSpec& src, const PortSpec& dst);
    
private:
    // 兼容性矩阵 (L1)
    static bool check_role_matrix(PortRole src, PortRole dst);
    
    // Bundle 兼容矩阵 (L2)
    static bool check_bundle_matrix(BundleType src, BundleType dst);
};

} // namespace cpptlm
```

**兼容性矩阵**:

| Src Role | Dst Role | 允许? | 说明 |
|----------|----------|:---:|------|
| INITIATOR | TARGET | ✅ | 标准连接 |
| INITIATOR | BI_DIRECTIONAL | ✅ | 单向使用 |
| TARGET | INITIATOR | ❌ | 方向反了 |
| BI_DIRECTIONAL | BI_DIRECTIONAL | ✅ | NoC 互连 |
| NETWORK | NETWORK | ✅ | Router↔Router |
| PE | PE | ❌ | PE 不能直连 |
| PE | NETWORK | ⚠️ | 仅 NICTLM 允许 |

| Src Bundle | Dst Bundle | 兼容? |
|------------|------------|:---:|
| CACHE_REQ | CACHE_REQ | ✅ |
| CACHE_REQ | CACHE_RESP | ❌ |
| NOC_FLIT | NOC_FLIT | ✅ |
| CACHE_REQ | GENERIC | ⚠️ 需转换器 |

### 3.3 ModuleFactory 集成

**修改文件**: `src/core/module_factory.cc`

**关键变更**:

1. **Step 3 端口创建** — 添加 port_specs JSON 反序列化:

```cpp
// src/core/module_factory.cc - Step 3 修改

void ModuleFactory::createPortsFromSpec(BaseModule* instance, 
                                         const std::string& type,
                                         const json& module_config) {
    // 检查是否有 port_specs 配置
    if (module_config.contains("port_specs")) {
        for (const auto& spec_json : module_config["port_specs"]) {
            PortSpec spec = spec_json.get<PortSpec>();  // 自动反序列化
            
            // 根据 PortSpec 创建端口
            if (spec.is_multi) {
                for (unsigned i = 0; i < spec.port_count; ++i) {
                    instance->createPort(spec.name + "[" + std::to_string(i) + "]", 
                                         spec.role, spec.bundle, spec.width);
                }
            } else {
                instance->createPort(spec.name, spec.role, spec.bundle, spec.width);
            }
        }
    } else {
        // 回退到默认端口创建 (向后兼容)
        createDefaultPorts(instance, type);
    }
}
```

2. **bindPorts() 兼容性检查**:

```cpp
// src/core/module_factory.cc - bindPorts() 修改

bool ModuleFactory::bindPorts(const std::string& src, const std::string& dst) {
    // ... 解析端口规格 ...
    
    PortSpec src_spec = getPortSpec(src_module, src_port);
    PortSpec dst_spec = getPortSpec(dst_module, dst_port);
    
    // L1: 方向检查
    if (!PortCompatibility::is_role_compatible(src_spec.role, dst_spec.role)) {
        printf("[PORT ERROR] Incompatible port roles: %s (%s) -> %s (%s)\n",
               src.c_str(), port_role_name(src_spec.role).c_str(),
               dst.c_str(), port_role_name(dst_spec.role).c_str());
        return false;
    }
    
    // L2: Bundle 匹配检查
    if (!PortCompatibility::is_bundle_compatible(src_spec.bundle, dst_spec.bundle)) {
        printf("[PORT ERROR] Incompatible bundle types: %s (%s) -> %s (%s)\n",
               src.c_str(), bundle_type_name(src_spec.bundle).c_str(),
               dst.c_str(), bundle_type_name(dst_spec.bundle).c_str());
        return false;
    }
    
    // L3: 宽度检查
    if (!PortCompatibility::is_width_compatible(src_spec.width, dst_spec.width)) {
        printf("[PORT WARN] Width mismatch: %s (%u bits) -> %s (%u bits)\n",
               src.c_str(), src_spec.width, dst.c_str(), dst_spec.width);
        // WARNING 不阻止连接，但记录日志
    }
    
    // ... 执行绑定 ...
    return true;
}
```

3. **DEF-04 WARNING 日志** (含废弃命名格式处理):

```cpp
// src/core/module_factory.cc - parsePortSpec() 修改

std::pair<std::string, std::string> ModuleFactory::parsePortSpec(
    const std::string& full_name) const {
    
    size_t dot_pos = full_name.find('.');
    if (dot_pos == std::string::npos) {
        return {full_name, ""};  // 无端口索引，默认 0
    }
    
    std::string module_name = full_name.substr(0, dot_pos);
    std::string port_spec = full_name.substr(dot_pos + 1);
    
    // 检查是否为废弃的命名格式 (如 NORTH, EAST, E_out, N_in)
    static const std::set<std::string> deprecated_names = {
        "NORTH", "EAST", "SOUTH", "WEST", "LOCAL",
        "N_in", "N_out", "E_in", "E_out", 
        "S_in", "S_out", "W_in", "W_out"
    };
    if (deprecated_names.count(port_spec)) {
        printf("[PORT WARN] Deprecated port name '%s'. Use numeric index instead "
               "(e.g., '0' for NORTH, '1' for EAST). Resolving through aliases.\n",
               port_spec.c_str());
        // 继续通过别名系统解析
        return {module_name, port_spec};
    }
    
    // 检查端口索引是否为纯数字
    if (!port_spec.empty() && std::isdigit(port_spec[0])) {
        bool all_digits = std::all_of(port_spec.begin(), port_spec.end(), ::isdigit);
        if (!all_digits) {
            // DEF-04: 非法端口索引，打印 WARNING
            printf("[PORT WARN] Invalid port index '%s' (expected digits only), "
                   "defaulting to 0\n", port_spec.c_str());
            return {module_name, "0"};
        }
    }
    
    return {module_name, port_spec};
}
```

### 3.4 Python RouterPort/NICPort 枚举

**修改文件**: `cpptlm_config/types.py`

```python
from enum import IntEnum

class RouterPort(IntEnum):
    """RouterTLM 端口索引枚举 (G3 共识)"""
    NORTH = 0
    EAST = 1
    SOUTH = 2
    WEST = 3
    LOCAL = 4

class NICPort(IntEnum):
    """NICTLM 端口组索引枚举 (m2 共识)"""
    PE = 0       # PE 侧（连接 CPU/Cache）
    NETWORK = 1  # Network 侧（连接 Router）
```

### 3.5 Python SemVer 版本管理

**修改文件**: `cpptlm_config/models.py`

```python
from datetime import datetime
from typing import List, Optional, Dict, Any

class ConfigMetadata(BaseModel):
    """配置元数据 (M2 共识: SemVer 版本管理)"""
    version: str = Field(default="1.0.0", description="SemVer 版本号")
    created_at: datetime = Field(default_factory=datetime.now)
    modified_at: datetime = Field(default_factory=datetime.now)
    changelog: List[str] = Field(default_factory=list)
    author: Optional[str] = None
    visualization: Optional[Dict[str, Any]] = None  # G4: 可视化元数据

# 修改 ConfigSchema 使用 metadata
class ConfigSchema(BaseModel):
    name: str
    description: str = ""
    metadata: ConfigMetadata = Field(default_factory=ConfigMetadata)
    modules: List[ModuleSpec]
    connections: List[ConnectionSpec]
    groups: Optional[Dict[str, List[str]]] = None
    extends: Optional[str] = None
```

### 3.6 Python pyproject.toml

**新建文件**: `cpptlm_config/pyproject.toml`

```toml
[project]
name = "cpptlm-config"
version = "0.1.0"
description = "CppTLM configuration generator with type safety and validation"
requires-python = ">=3.10"
dependencies = [
    "pydantic>=2.0,<3.0",
]

[project.optional-dependencies]
visualization = ["matplotlib>=3.5"]
dev = ["pytest>=7.0", "mypy>=1.0", "ruff>=0.1"]

[build-system]
requires = ["setuptools>=68.0", "wheel"]
build-backend = "setuptools.backends._legacy:_Backend"

[tool.ruff]
target-version = "py310"
line-length = 100
```

---

## 四、时间线

### 4.1 周度计划

```
Week 1: 端口类型基础
  Day 1-2: port_types.hh (T3.2-01)
  Day 3-4: port_compatibility.hh (T3.2-02)
  Day 5:   ModuleFactory port_specs 集成 (T3.2-03) 开始

Week 2: ModuleFactory 集成与兼容性检查
  Day 1:   ModuleFactory port_specs 集成完成 (T3.2-03)
  Day 2:   端口方向检查 L1 (T3.2-04)
  Day 3:   Bundle 匹配检查 L2 (T3.2-05)
  Day 4:   数据宽度检查 L3 (T3.2-06)
  Day 5:   DEF-04 WARNING 日志 (T3.2-09)

Week 3: 端口别名、端口组与 Python 集成
  Day 1-2: 端口别名系统 (T3.2-07)
  Day 3-4: NICTLM 端口组支持 (T3.2-08)
  Day 5:   Python RouterPort/NICPort + SemVer + pyproject.toml (T3.2-10~12)

Week 4: 测试、集成与文档
  Day 1:   Python layout_hint 支持 (T3.2-13)
  Day 2-3: C++ 端口类型单元测试 (T3.2-14)
  Day 4:   Python 端口类型测试 (T3.2-15)
  Day 5:   端到端集成测试 (T3.2-16) + 文档更新 (T3.2-17~18)
```

### 4.2 里程碑

| 里程碑 | 日期 | 验收标准 |
|--------|------|---------|
| M1: 端口类型基础完成 | Week 1 | port_types.hh + port_compatibility.hh 可用 |
| M2: ModuleFactory 集成完成 | Week 2 | port_specs JSON 解析 + 三层检查矩阵工作正常 |
| M3: Python 集成完成 | Week 3 | RouterPort/NICPort/SemVer/pyproject.toml 完成 |
| M4: Phase 3.2 发布 | Week 4 | 所有测试通过 (50+)，文档完整，共识事项全部实施 |

---

## 五、测试策略

### 5.1 C++ 单元测试

**测试文件**: `test/test_port_types.cc`

| 测试类别 | 测试数量 | 覆盖内容 |
|---------|:---:|---------|
| PortSpec 序列化 | 5 | JSON ↔ PortSpec 双向转换、默认值处理 |
| 枚举转换 | 4 | PortRole/BundleType 字符串映射 |
| 兼容性矩阵 | 8 | L1/L2/L3 检查矩阵覆盖 |
| 端口别名 | 5 | 别名解析、冲突处理 |
| 端口组 | 3 | PortGroupSpec 序列化 |
| **总计** | **25** | |

**测试用例示例**:

```cpp
TEST_CASE("PortSpec: JSON serialization") {
    PortSpec spec{"NORTH", PortRole::BI_DIRECTIONAL, BundleType::NOC_FLIT, 64, true, 5};
    
    json j = spec;
    REQUIRE(j["name"] == "NORTH");
    REQUIRE(j["role"] == "bi_directional");
    REQUIRE(j["bundle"] == "noc_flit");
    REQUIRE(j["width"] == 64);
    REQUIRE(j["is_multi"] == true);
    REQUIRE(j["port_count"] == 5);
    
    PortSpec spec2 = j.get<PortSpec>();
    REQUIRE(spec.name == spec2.name);
    REQUIRE(spec.role == spec2.role);
}

TEST_CASE("PortCompatibility: role matrix") {
    REQUIRE(PortCompatibility::is_role_compatible(PortRole::INITIATOR, PortRole::TARGET));
    REQUIRE_FALSE(PortCompatibility::is_role_compatible(PortRole::TARGET, PortRole::INITIATOR));
    REQUIRE(PortCompatibility::is_role_compatible(PortRole::BI_DIRECTIONAL, PortRole::BI_DIRECTIONAL));
}
```

### 5.2 Python 单元测试

**测试文件**: `cpptlm_config/tests/test_port_types.py`

| 测试类别 | 测试数量 | 覆盖内容 |
|---------|:---:|---------|
| RouterPort 枚举 | 3 | 值正确性、类型安全 |
| NICPort 枚举 | 3 | 值正确性、类型安全 |
| SemVer 版本管理 | 4 | bump_version()、changelog |
| layout_hint | 3 | PortSpec layout_hint 序列化 |
| pyproject.toml | 2 | 依赖声明、安装验证 |
| **总计** | **15** | |

### 5.3 集成测试

**测试文件**: `test/test_port_integration.cc`

| 测试场景 | 验证内容 |
|---------|---------|
| mesh_2x2 配置加载 | port_specs JSON 解析正确 |
| 端口别名解析 | `router.NORTH` → `router.0` |
| 无效连接拒绝 | INITIATOR → INITIATOR 被拒绝 |
| DEF-04 WARNING | `xbar.0abc` 产生 WARNING |

### 5.4 回归测试保证

- **现有测试**: 434/434 必须全部通过
- **E2E 测试**: 32/32 必须全部通过
- **新增测试**: 40+ (C++ 25+, Python 15+)

---

## 六、风险与缓解

### 6.1 技术风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| nlohmann/json 嵌套结构体反序列化失败 | 中 | 高 | 使用 `WITH_DEFAULT` 宏，添加 JSON schema 验证层 |
| 枚举字符串映射拼写错误 | 低 | 中 | 单元测试覆盖所有枚举值的序列化/反序列化 |
| ModuleFactory 修改引入回归 | 中 | 高 | 每个修改伴随单元测试，434 tests 持续通过 |
| Python 包结构变更影响现有脚本 | 低 | 中 | 保持向后兼容，渐进迁移 |

### 6.2 进度风险

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|:---:|:---:|---------|
| 端口兼容性矩阵逻辑复杂 | 中 | 低 | 分层实现 (L1 → L2 → L3)，逐步测试 |
| 端口别名系统与现有配置冲突 | 低 | 中 | 向后兼容，别名可选 |

---

## 七、交付物清单

### 7.1 代码交付物

| 文件 | 类型 | 说明 |
|------|------|------|
| `include/core/port_types.hh` | 新建 | PortSpec, PortRole, BundleType + JSON 宏 |
| `include/core/port_compatibility.hh` | 新建 | 三层兼容性检查矩阵 |
| `src/core/module_factory.cc` | 修改 | port_specs 集成、兼容性检查、DEF-04 WARNING |
| `cpptlm_config/types.py` | 修改 | RouterPort/NICPort 枚举 |
| `cpptlm_config/models.py` | 修改 | ConfigMetadata, layout_hint |
| `cpptlm_config/pyproject.toml` | 新建 | 依赖声明和构建配置 |
| `test/test_port_types.cc` | 新建 | C++ 端口类型单元测试 |
| `cpptlm_config/tests/test_port_types.py` | 新建 | Python 端口类型测试 |

### 7.2 文档交付物

| 文件 | 说明 |
|------|------|
| `docs/architecture/13-port-management-system.md` | 端口管理系统架构设计 |
| `docs/guide/PORT_CONFIGURATION_GUIDE.md` | 端口配置指南 |

---

## 八、验收标准

| 验收项 | 标准 | 验证方式 |
|--------|------|---------|
| port_types.hh | 枚举序列化/反序列化正确 | 25+ 单元测试通过 |
| 端口兼容性 | 三层检查矩阵工作正常 | 无效连接被拒绝 |
| JSON port_specs | 配置中 port_specs 正确解析 | 测试配置加载 |
| 端口别名 | `router.NORTH` → `router.0` | 别名解析测试 |
| NICTLM 端口组 | DualPortStreamAdapter 端口组配置正确 | JSON 配置 + 仿真验证 |
| DEF-04 WARNING | 非法端口索引产生 WARNING | `xbar.0abc` 测试 |
| Python 枚举 | RouterPort/NICPort 值正确 | `RouterPort.NORTH.value == 0` |
| SemVer | 版本号升级正确 | bump_version() 测试 |
| pyproject.toml | `pip install -e .` 成功 | 安装测试 |
| 回归测试 | 434/434 通过 | `ctest --output-on-failure` |

---

## 九、与后续阶段的接口

Phase 3.2 完成后，为 Phase 3.3 和 Phase 3.4 提供以下接口:

| 后续阶段 | 依赖接口 | 说明 |
|---------|---------|------|
| Phase 3.3 | ParamRule 验证需要端口类型信息 | 端口兼容性影响参数验证 |
| Phase 3.4 | validator.py 使用端口类型检查 | VALID-01/VALID-02/PORT-01/PORT-03 依赖 |
| Phase 4+ | 子网络嵌套使用端口别名 | HIER-01/HIER-03 依赖 |

---

**文档结束**

| 版本 | 日期 | 变更 |
|------|------|------|
| v1.0 | 2026-05-05 | 初始版本，基于 ADR-X.9 v3.0 和 Phase 3+ 实施计划 v2.0 编制 |
| v1.1 | 2026-05-07 | 添加状态标注为草稿；更新共识事项（Phase 3.1 已实现 T3.1-09 NI连接验证）；移除不存在的 Python 包依赖引用 |
| v1.3 | 2026-05-09 | 修复枚举序列化大小写问题（PortRole/BundleType/PortGroupBundleType）；修复 ModulePortSpec::from_json 使 module_name 可选；标记 T3.2-08 为已完成 |

---

**文档状态**: ✅ C++ 端已完成（所有 505 测试通过，14,420 assertions）

