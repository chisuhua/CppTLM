# Phase 3.x Revised Implementation Plan

> **版本**: 2.0 (修订版)
> **日期**: 2026-05-08
> **状态**: 📋 待执行
> **前置条件**: 基于 2026-05-07-phase-3-remaining-tasks.md 和 2026-05-08-phase-3-4-validation-toolchain.md 重新评估
> **关联**: ADR-X.9 (端口类型系统), ADR-X.10 (参数框架), ADR-X.12 (Python 配置生成器)

---

## 执行摘要

本计划修正了 2026-05-07 版本中对 **Task A1** 的根本性误判，并针对**构建系统降级**重新安排了 Part B 任务的执行策略。

### 关键修正

| 项目 | 旧计划 (2026-05-07) | 本修订计划 |
|------|-------------------|-----------|
| **A1 根因** | "port_specs 从未加载" ❌ | "模块缺少 port_spec 时 check_port_compatibility 静默通过" ✅ |
| **A1 修复** | 添加 port_specs 加载代码 | 为已知模块类型生成默认 port specs |
| **Part B 策略** | 继续在 C++ 中实施 (B1-B5) | ** defer 至 Phase 3.5**，当前用 Python validator 替代 |
| **Phase 3.4** | 独立计划，待 cpptlm_config 完成后启动 | **合并为 Part C**，立即执行（Python-only，无构建依赖）|
| **构建系统** | 假设可编译 | 明确标记为阻塞项，限制 C++ 任务范围 |

---

## 1. 任务 A1 深度分析：空 port_specs 的真正问题

### 1.1 数据流追踪

**代码路径**（基于 `src/core/module_factory.cc`）：

```
line 371-384: port_specs 从 JSON 加载
    ↓
line 836: check_port_compatibility(src_name, dst_name, src_idx, dst_idx, port_specs)
    ↓
line 282-325: check_port_compatibility() 实现
```

**具体代码分析**：

```cpp
// module_factory.cc:371-384 — port_specs 加载（已存在，工作正常）
std::map<std::string, cpptlm::ModulePortSpec> port_specs;
for (const auto& mod : final_config["modules"]) {
    if (!mod.contains("name") || !mod.contains("type")) continue;
    std::string name = mod["name"];
    if (mod.contains("port_spec")) {  // ← 注意：是 "port_spec" 单数
        try {
            auto spec = mod["port_spec"].get<cpptlm::ModulePortSpec>();
            port_specs[name] = spec;
        } catch (...) { ... }
    }
}
```

```cpp
// module_factory.cc:282-325 — check_port_compatibility() 实现
static bool check_port_compatibility(...) {
    auto src_it = port_specs.find(src_name);
    auto dst_it = port_specs.find(dst_name);

    if (src_it == port_specs.end() || dst_it == port_specs.end()) {
        return true;  // ← BUG: 找不到规格时静默通过！
    }
    // ... 实际检查代码 ...
}
```

### 1.2 根因结论

**旧计划错误**: 声称 "port_specs 从未加载" —— 这是**错误的**。

**实际情况**:
1. `port_specs` **确实被加载**（lines 371-384）
2. 问题出在 `check_port_compatibility()` 的**回退逻辑**（line 288-289）
3. 当模块没有在 JSON 中显式定义 `"port_spec"` 时，函数返回 `true`（通过）
4. 由于**绝大多数配置文件不在模块级别定义 `port_spec`**，兼容性检查实际上从未执行

### 1.3 修复策略

**方案选择**（基于 ADR-X.9 决策 1.5 端口索引定义）：

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| A) 配置文件强制要求 port_spec | 让 validateConfig() 报错 | 简单 | 破坏向后兼容，所有现有配置需修改 |
| B) **生成默认 port specs** ✅ | 为已知模块类型提供内置默认规格 | 向后兼容，符合 ADR-X.9 端口索引定义 | 需要维护模块类型列表 |
| C) 模块自声明 | 每个 C++ 模块类实现 get_port_specs() | 灵活 | 需要修改每个模块类，工作量大 |

**选择方案 B**：
- 与 ADR-X.9 已定义的端口索引规范一致
- 向后兼容：现有配置无需修改
- 增量增强：未来新模块只需在默认规格映射中添加条目

**默认规格定义**（基于 ADR-X.9 端口索引规范）：

```cpp
// RouterTLM: 5 端口 (NORTH=0, EAST=1, SOUTH=2, WEST=3, LOCAL=4)
cpptlm::ModulePortSpec get_default_router_spec() {
    cpptlm::ModulePortSpec spec;
    spec.module_name = "RouterTLM";
    spec.ports = {
        {"NORTH", cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
        {"EAST",  cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
        {"SOUTH", cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
        {"WEST",  cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
        {"LOCAL", cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64}
    };
    return spec;
}

// NICTLM: 2 端口 (NETWORK=0, PE=1)
cpptlm::ModulePortSpec get_default_nic_spec() {
    cpptlm::ModulePortSpec spec;
    spec.module_name = "NICTLM";
    spec.ports = {
        {"NETWORK", cpptlm::PortRole::NETWORK, cpptlm::BundleType::NOC_FLIT, 64},
        {"PE",      cpptlm::PortRole::PE,      cpptlm::BundleType::CACHE_REQ, 64}
    };
    return spec;
}

// CacheTLM: 2 端口 (req_out=INITIATOR, req_in=TARGET)
cpptlm::ModulePortSpec get_default_cache_spec() {
    cpptlm::ModulePortSpec spec;
    spec.module_name = "CacheTLM";
    spec.ports = {
        {"req_out", cpptlm::PortRole::INITIATOR, cpptlm::BundleType::CACHE_REQ, 64},
        {"req_in",  cpptlm::PortRole::TARGET,    cpptlm::BundleType::CACHE_RESP, 64}
    };
    return spec;
}
```

---

## 2. Part B 重新评估：构建系统阻塞下的策略

### 2.1 构建系统现状

| 指标 | 状态 |
|------|------|
| 已有二进制 | ✅ 存在 (`build/bin/cpptlm_tests` 等) |
| 测试运行 | ✅ 可通过 (`./bin/cpptlm_tests "[phase3.2]"` 通过 14 用例) |
| 增量编译 | ❌ 超时 (>90s/文件，总构建 >5min 超时) |
| 完整重建 | ❌ 不可行 (300s 超时仍不足) |
| ccache | ✅ 可用，但首次编译仍极慢 |

**根因分析**:
- NAS 挂载的文件系统 (`*.cn-shanghai.nas.aliyuncs.com`) 作为编译目录
- NAS I/O 延迟导致编译极慢（尤其是头文件解析阶段）
- 内存压力（7.1GB 总内存，仅 2.4GB available）可能导致交换

### 2.2 Part B 任务重新分类

| 任务 | 原策略 | 修订策略 | 理由 |
|------|--------|---------|------|
| **B1**: 加载 param_rules JSON | C++ ModuleFactory 中实现 | **defer 至 Phase 3.5** | 需要 C++ 编译验证 |
| **B2**: Wire validate_module_params | C++ 通用分发 | **defer 至 Phase 3.5** | 需要 C++ 编译验证 |
| **B3**: set_config() 异常处理 | C++ try-catch | **defer 至 Phase 3.5** | 需要 C++ 编译验证 |
| **B4**: test_param_parser.cc | C++ Catch2 测试 | **defer 至 Phase 3.5** | 需要 C++ 编译验证 |
| **B5**: test_param_integration.cc | C++ Catch2 集成测试 | **defer 至 Phase 3.5** | 需要 C++ 编译验证 |
| **B6**: Python param validator | (原无此任务) | **Part C 新增** | Python 无需编译，立即执行 |

### 2.3 替代方案：Python 端参数验证

根据 ADR-X.12 决策 7（两阶段验证）和 ADR-X.10 决策 4.5（双重验证）：

```
Python ConfigBuilder.build()
    ↓ Pydantic 验证（生成时验证）← 当前可实现
    ↓ 捕获结构错误
    ↓ 生成 JSON 配置
    ↓
C++ ModuleFactory::instantiateAll()
    ↓ ModuleFactory.validate_params()（运行时验证）← defer 至 3.5
```

**本阶段实现 Python 侧验证**（Phase 3.4 Part C 包含）：
- `cpptlm_config.validator.TopologyValidator` 已规划 VALID-01/02, PORT-01/03
- 扩展 validator 增加参数验证规则（PARAM-01: required params, PARAM-02: range check）
- 这样 param 验证功能在 Python 侧立即可用，C++ 侧 deferred

---

## 3. Phase 3.4 整合：合并而非独立

### 3.1 整合理由

原 2026-05-08 Phase 3.4 计划是独立文档，存在以下问题：
1. **依赖关系不清晰**：Phase 3.4 与 Part A/B 的依赖未明确
2. **重复上下文**：两个计划都引用 ADR-X.9/10/12，信息重复
3. **执行顺序模糊**：应该先执行 3.4 Python 任务还是等 C++ 任务完成？

### 3.2 整合方式

将 Phase 3.4 作为 **Part C** 合并到本计划中：
- **Part A**: C++ 关键修复（A1 默认 port specs）
- **Part B**: defer 至 Phase 3.5（C++ param 集成）
- **Part C**: Phase 3.4 Python 验证工具链（立即执行）

**Part C 与原 Phase 3.4 计划的差异**：
- 明确前置条件：A1 完成（确保 C++ 端口检查可用）
- 明确 Part B 替代：Python validator 增加 param 验证规则
- 增加 pydantic 安装步骤（原 plan 假设已安装，实际未安装）

---

## 4. 任务执行顺序与依赖图

### 4.1 依赖关系

```
Part A: A1 Fix default port specs
    │
    ├─→ 需要: C++ 编译（受限，最小化变更）
    │
    └─→ 阻塞: 无（可独立执行）

Part C: Phase 3.4 Python Validation Toolchain
    │
    ├─→ 需要: pydantic 安装（C0）
    ├─→ 需要: cpptlm_config 包可用（已完成）
    ├─→ 需要: A1 完成（确保端口检查逻辑正确）
    │
    ├─→ C1: validator.py（VALID-01/02, PORT-01/03）
    ├─→ C2: topology_adapter.py
    ├─→ C3: __init__.py 导出更新
    ├─→ C4: 3 个示例脚本
    ├─→ C5: scripts/topology_validator.py 重构
    ├─→ C6: Python param 验证规则（替代 Part B）
    └─→ C7: 测试验证

Part B: C++ Param Integration (DEFERRED)
    │
    ├─→ 前置: 构建系统修复
    ├─→ 前置: A1 完成
    └─→ 包含: B1-B5（原任务不变，移至 Phase 3.5）
```

### 4.2 执行顺序

```
Phase 3.x (当前):
├── Step 1: A1 — Fix default port specs (C++, 最小化变更)
├── Step 2: C0 — Install pydantic
├── Step 3: C1 — Create validator.py
├── Step 4: C2 — Create topology_adapter.py
├── Step 5: C3 — Update __init__.py exports
├── Step 6: C4 — Create example scripts (3)
├── Step 7: C5 — Refactor topology_validator.py wrapper
├── Step 8: C6 — Add Python param validation rules
└── Step 9: C7 — Run all Python tests and validation

Phase 3.5 (未来，构建系统修复后):
├── Step 10: B1 — Load param_rules JSON in ModuleFactory
├── Step 11: B2 — Wire validate_module_params (generic dispatch)
├── Step 12: B3 — Add set_config() exception handling
├── Step 13: B4 — Create test_param_parser.cc
└── Step 14: B5 — Create test_param_integration.cc
```

---

## 5. 详细任务定义

### Task A1: Fix Default Port Specs — Enable Real Port Validation

**文件**:
- 修改: `src/core/module_factory.cc:282-325` (check_port_compatibility 逻辑)
- 修改: `src/core/module_factory.cc:368-384` (port_specs 加载后合并默认规格)
- 测试: `test/test_phase3_2_port_management.cc:196-272` (已有测试应继续通过)

**变更详情**:

Step 1: 在 module_factory.cc 添加默认规格生成函数（line 280 之前）:

```cpp
// Phase 3.2: Default port specs for known module types (T3.2-04 revised)
// When a module doesn't define "port_spec" in JSON config, use these defaults
// based on ADR-X.9 port index specification.
static std::map<std::string, cpptlm::ModulePortSpec> generateDefaultPortSpecs() {
    std::map<std::string, cpptlm::ModulePortSpec> defaults;

    // RouterTLM: 5 directional ports (NORTH=0, EAST=1, SOUTH=2, WEST=3, LOCAL=4)
    {
        cpptlm::ModulePortSpec spec;
        spec.module_name = "RouterTLM";
        spec.ports = {
            {"NORTH", cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
            {"EAST",  cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
            {"SOUTH", cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
            {"WEST",  cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64},
            {"LOCAL", cpptlm::PortRole::BI_DIRECTIONAL, cpptlm::BundleType::NOC_FLIT, 64}
        };
        defaults["RouterTLM"] = spec;
    }

    // NICTLM: 2 ports (NETWORK=0, PE=1)
    {
        cpptlm::ModulePortSpec spec;
        spec.module_name = "NICTLM";
        spec.ports = {
            {"NETWORK", cpptlm::PortRole::NETWORK, cpptlm::BundleType::NOC_FLIT, 64},
            {"PE",      cpptlm::PortRole::PE,      cpptlm::BundleType::CACHE_REQ, 64}
        };
        defaults["NICTLM"] = spec;
    }

    // CacheTLM: 2 ports (req_out=INITIATOR, req_in=TARGET)
    {
        cpptlm::ModulePortSpec spec;
        spec.module_name = "CacheTLM";
        spec.ports = {
            {"req_out", cpptlm::PortRole::INITIATOR, cpptlm::BundleType::CACHE_REQ, 64},
            {"req_in",  cpptlm::PortRole::TARGET,    cpptlm::BundleType::CACHE_RESP, 64}
        };
        defaults["CacheTLM"] = spec;
    }

    // CrossbarTLM: 2 ports (req_in=TARGET, req_out=INITIATOR)
    {
        cpptlm::ModulePortSpec spec;
        spec.module_name = "CrossbarTLM";
        spec.ports = {
            {"req_in",  cpptlm::PortRole::TARGET,    cpptlm::BundleType::CACHE_REQ, 64},
            {"req_out", cpptlm::PortRole::INITIATOR, cpptlm::BundleType::CACHE_REQ, 64}
        };
        defaults["CrossbarTLM"] = spec;
    }

    // MemoryTLM: 1 port (req_in=TARGET)
    {
        cpptlm::ModulePortSpec spec;
        spec.module_name = "MemoryTLM";
        spec.ports = {
            {"req_in", cpptlm::PortRole::TARGET, cpptlm::BundleType::CACHE_REQ, 64}
        };
        defaults["MemoryTLM"] = spec;
    }

    return defaults;
}
```

Step 2: 修改 port_specs 加载逻辑（line 368-384 之后），合并默认规格:

```cpp
// After loading user-defined port_specs (line 384):
// Merge with default specs for known module types
static auto default_specs = generateDefaultPortSpecs();
for (const auto& mod : final_config["modules"]) {
    if (!mod.contains("name") || !mod.contains("type")) continue;
    std::string name = mod["name"];
    std::string type = mod["type"];

    // If module doesn't have explicit port_spec, use default for its type
    if (port_specs.find(name) == port_specs.end()) {
        auto default_it = default_specs.find(type);
        if (default_it != default_specs.end()) {
            port_specs[name] = default_it->second;
            DPRINTF(MODULE, "[PORT] Using default port spec for %s (type: %s)\n",
                    name.c_str(), type.c_str());
        }
    }
}
```

Step 3: 修改 check_port_compatibility 回退逻辑（line 288-289）:

```cpp
// BEFORE (silent pass):
if (src_it == port_specs.end() || dst_it == port_specs.end()) {
    return true;  // Silently pass — BUG
}

// AFTER (warn but still pass to maintain backward compat):
if (src_it == port_specs.end() || dst_it == port_specs.end()) {
    DPRINTF(CONN, "[PORT WARN] Missing port spec for %s or %s, "
            "compatibility check skipped\n", src_name.c_str(), dst_name.c_str());
    return true;  // Still pass, but now logged
}
```

**成功标准**:
- `test_phase3_2_port_management.cc` 中 `[phase3.2][port_compat]` 测试通过
- 新增测试：使用无 `port_spec` 的 RouterTLM 配置，验证默认规格生效
- 编译通过（即使构建慢，也要确保变更正确）

---

### Task C0: Install pydantic

**文件**: 系统级 Python 包

```bash
pip install pydantic>=2.0
```

**验证**:
```bash
python3 -c "import pydantic; print(pydantic.__version__)"
# Expected: 2.x.x
```

---

### Task C1: Create cpptlm_config/validator.py

**文件**: 新建 `cpptlm_config/validator.py`

**内容**: 与原 Phase 3.4 Step 1 基本一致，但增加 param 验证规则（替代 Part B）:

```python
# 在 TopologyValidator 类中添加：

# === PARAM-01: Required parameter check ===
def validate_required_params(self) -> "TopologyValidator":
    """验证模块必需参数是否存在"""
    # 从 configs/param_rules/*.json 加载规则
    # 或使用内置规则
    required_rules = {
        "RouterTLM": ["node_x", "node_y", "mesh_x", "mesh_y"],
        "NICTLM": ["node_id"],
    }
    # ... 实现省略，与原 plan 一致 ...
    return self

# === PARAM-02: Parameter range check ===
def validate_param_ranges(self) -> "TopologyValidator":
    """验证参数值在允许范围内"""
    # 基于 param_rules JSON 或内置规则
    return self
```

**依赖**: C0 (pydantic)

---

### Task C2-C5: Phase 3.4 剩余步骤

与原 2026-05-08-phase-3-4-validation-toolchain.md 的 Step 2-5 一致：
- C2: `cpptlm_config/topology_adapter.py`
- C3: 更新 `cpptlm_config/__init__.py` 导出
- C4a/4b/4c: 3 个示例脚本
- C5: 重构 `scripts/topology_validator.py` 为轻量包装

**变更**: 在 `TopologyValidator.validate()` 中增加 param 验证调用:
```python
def validate(self) -> ValidationResult:
    self.validate_connectivity()      # VALID-01
    self.validate_reachability()      # VALID-02
    self.validate_port_directions()   # PORT-01
    self.validate_bundle_types()      # PORT-03
    self.validate_required_params()   # PARAM-01 (新增，替代 Part B)
    self.validate_param_ranges()      # PARAM-02 (新增，替代 Part B)
    return self.result
```

---

### Task C6: Python Param Validation Rules (替代 Part B)

**文件**: 修改 `cpptlm_config/validator.py`

**规则实现**:

```python
# 加载 configs/param_rules/*.json
def _load_param_rules(self) -> Dict[str, Dict]:
    """从 JSON 文件加载参数规则"""
    rules = {}
    rules_dir = Path("configs/param_rules")
    if not rules_dir.exists():
        return rules
    for json_file in rules_dir.glob("*.json"):
        with open(json_file) as f:
            data = json.load(f)
            if "module_type" in data and "rules" in data:
                rules[data["module_type"]] = data["rules"]
    return rules

# PARAM-01: 必需参数检查
def validate_required_params(self) -> "TopologyValidator":
    rules = self._load_param_rules()
    for module in self.config.get("modules", []):
        mod_type = module.get("type", "")
        mod_name = module.get("name", "")
        params = module.get("params", {})
        if mod_type in rules:
            for param_name, rule in rules[mod_type].items():
                if rule.get("required", False) and param_name not in params:
                    self.result.add_error(
                        code="PARAM-01",
                        message=f"Module '{mod_name}' ({mod_type}) missing required parameter '{param_name}'",
                        suggestion=f"Add '{param_name}' to params in module '{mod_name}'"
                    )
    return self

# PARAM-02: 参数范围检查
def validate_param_ranges(self) -> "TopologyValidator":
    rules = self._load_param_rules()
    for module in self.config.get("modules", []):
        mod_type = module.get("type", "")
        mod_name = module.get("name", "")
        params = module.get("params", {})
        if mod_type not in rules:
            continue
        for param_name, value in params.items():
            if param_name not in rules[mod_type]:
                continue
            rule = rules[mod_type][param_name]
            # 检查 min_value
            if "min_value" in rule and value < rule["min_value"]:
                self.result.add_error(
                    code="PARAM-02",
                    message=f"Parameter '{param_name}'={value} < min={rule['min_value']}",
                    suggestion=f"Set {param_name} >= {rule['min_value']}"
                )
            # 检查 max_value
            if "max_value" in rule and value > rule["max_value"]:
                self.result.add_error(
                    code="PARAM-02",
                    message=f"Parameter '{param_name}'={value} > max={rule['max_value']}",
                    suggestion=f"Set {param_name} <= {rule['max_value']}"
                )
    return self
```

---

### Task C7: 测试验证

**命令**:
```bash
# 1. Python 单元测试
cd /workspace/project/CppTLM
python3 -m pytest test/python/test_port_types.py -v

# 2. 运行 validator 示例
python3 cpptlm_config/examples/mesh_2x2.py
python3 cpptlm_config/examples/mesh_4x4_validated.py

# 3. 运行 topology_validator
python3 scripts/topology_validator.py configs/mesh_2x2.json -v

# 4. 测试 param 验证
python3 -c "
from cpptlm_config.validator import TopologyValidator
config = {
    'modules': [
        {'name': 'r0', 'type': 'RouterTLM', 'params': {'node_x': 0}}
        # missing node_y, mesh_x, mesh_y
    ],
    'connections': []
}
v = TopologyValidator(config)
result = v.validate()
print('Valid:', result.is_valid)
for e in result.errors:
    print(f'  ERROR [{e.code}]: {e.message}')
"
# Expected: PARAM-01 errors for missing node_y, mesh_x, mesh_y
```

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| A1 C++ 编译超时 | 无法验证 A1 修复 | 最小化变更（<30 行），使用现有测试验证逻辑 |
| pydantic 安装失败 | Part C 全部阻塞 | 使用 `pip install --user` 或虚拟环境 |
| 默认 port specs 与模块实际不符 | 假阳性兼容错误 | 基于 ADR-X.9 规范，与模块实现一致 |
| Phase 3.5 被遗忘 | Part B 永不被实施 | 本计划明确标记 defer，创建 Phase 3.5 计划时引用 |

---

## 7. 成功标准

### Phase 3.x 完成标准

- [ ] A1: `check_port_compatibility` 对已知模块类型使用默认规格（非静默跳过）
- [ ] C0: pydantic 2.x 安装成功
- [ ] C1: `cpptlm_config/validator.py` 包含 VALID-01/02, PORT-01/03, PARAM-01/02
- [ ] C2: `cpptlm_config/topology_adapter.py` 可从 mesh 生成配置
- [ ] C3: `cpptlm_config/__init__.py` 导出 validator 和 TopologyAdapter
- [ ] C4: 3 个示例脚本可执行并生成有效 JSON
- [ ] C5: `scripts/topology_validator.py` 重构为轻量包装
- [ ] C7: Python 测试全部通过，示例脚本运行成功

### Phase 3.5 准入标准（未来）

- [ ] 构建系统修复（NAS I/O 问题解决或本地构建环境可用）
- [ ] 本计划 A1 已完成
- [ ] 创建 Phase 3.5 计划文档，包含 B1-B5 任务

---

## 8. 附录：修订记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-05-07 | 初始版本（含 A1/A2/B1-B5 误判） |
| 2.0 | 2026-05-08 | 修正 A1 根因分析，defer Part B 至 3.5，合并 Phase 3.4 为 Part C，增加 Python param 验证替代方案 |

---

**计划完成，保存至 `docs/superpowers/plans/2026-05-08-phase-3-x-revised-plan.md`**

**执行选项**:
1. **按顺序执行**: A1 → C0 → C1 → C2 → C3 → C4 → C5 → C6 → C7
2. **并行执行**: C0 (Python 安装) 可与 A1 (C++ 编辑) 并行
