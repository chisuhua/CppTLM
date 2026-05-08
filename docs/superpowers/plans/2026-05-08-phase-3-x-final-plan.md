# Phase 3.x Final Implementation Plan

> **版本**: 3.1 (整合版，含 docs/plan/ 文档同步)
> **日期**: 2026-05-08
> **状态**: 📋 待执行
> **前置条件**: 基于 Metis 六文档交叉分析 + 代码现场验证 + docs/plan/ 三阶段计划文档对齐
> **关联**: ADR-X.9, ADR-X.10, ADR-X.11, ADR-X.12, docs/plan/ROADMAP.md

---

## Executive Summary

本计划整合 Metis 对六份 Phase 3.x 计划文档的全面分析与代码现场验证结果，产出可立即执行的最终任务清单。

**已完成工作**: Phase 3.2 (默认 port specs, cpptlm_config 包)、Phase 3.3 (ParamRule JSON 序列化、ParamParser、param_errors)、Phase 3.4 (validator.py、topology_adapter、examples、wrapper) 的大部分代码已落地，但存在**三个 P1 级关键缺口**阻塞后续流程。

**核心修正** (vs 2026-05-07 原始计划):
- A1 根因已修正为"默认 port specs" (commit 4a87877)，非"port_specs 从未加载"
- Part B (C++ param 集成) 因构建系统降级 defer 至 Phase 3.5，但 B1/B2 因 `validate_module_params` 死代码问题提升为 P1
- Phase 3.4 Python 工具链合并为 Part C，但其中 ConfigBuilder 缺失导致 examples 全部无法运行

---

## Current State

### Completed (✅)

| 任务 | 文件 | Commit | 说明 |
|------|------|--------|------|
| A1 — 默认 port specs | `src/core/module_factory.cc:328-401` | 4a87877 | `get_default_port_specs()` 已实现并集成 |
| A1 — port_specs 加载修复 | `src/core/module_factory.cc` | ea2c7e7 | 从 JSON 加载 port_specs 并传入 check_port_compatibility |
| C1-C3 — cpptlm_config 包 | `cpptlm_config/` | 3603c5d | types.py, models.py, pyproject.toml, __init__.py |
| C1 — validator.py | `cpptlm_config/validator.py` | 4a64af1 | VALID-01/02, PORT-01/03 |
| C2 — topology_adapter.py | `cpptlm_config/topology_adapter.py` | 4a64af1 | from_mesh, from_ring |
| C3 — __init__.py 导出 | `cpptlm_config/__init__.py` | 4a64af1 | TopologyValidator, TopologyAdapter |
| C4 — 示例脚本 | `cpptlm_config/examples/*.py` | 4a64af1 | mesh_2x2, mesh_4x4_validated, hierarchical |
| C5 — topology_validator.py wrapper | `scripts/topology_validator.py` | 4a64af1 | 轻量包装 |
| T3.2-01~07 | `include/core/port_types.hh`, `port_compatibility.hh`, module_factory.cc | — | port_types.hh, 三层兼容性检查, 端口别名系统 |
| T3.2-09 | `src/core/module_factory.cc` | — | DEF-04 WARNING 日志 |
| T3.2-10~13 | `cpptlm_config/types.py`, `models.py`, `pyproject.toml` | 3603c5d | RouterPort/NICPort 枚举, SemVer, layout_hint |
| T3.2-14 | `test/` | — | C++ 端端口类型单元测试 (25+ cases) |
| T3.2-16 | `test/` | — | E2E 集成测试 |
| T3.3-01/02/03/06 | `include/core/param_rules.hh`, `param_parser.hh/cc`, `param_errors.hh`, `configs/param_rules/*.json` | 5707d31 | ParamRule JSON 序列化, ParamParser, 异常类, 全局规则文件 |
| T3.3-10 | `src/core/param_parser.cc` | 5707d31 | 参数推导表达式解析器 (evaluate_derive_expr) |
| T3.2-15 | `test/python/test_port_types.py` | 8559104 | 17 Python 端口类型测试 |
| T3.1-07 | `include/utils/var_resolver.hh` | — | `${path}` 变量解析器, 已集成到 instantiateAll |
| T3.1-09 | `test/test_nic_side_rules.cc` | — | NI PE-side 连接验证 |
| T3.1-11 | `test/test_validate_config.cc` | — | validateConfig() 覆盖测试 (4 用例) |
| T3.1-04b/03b | `test/test_module_factory_fixes.cc` | — | DEF-04/03 WARNING 日志测试 |
| T3.4-01~05 | `cpptlm_config/validator.py` | 4a64af1 | TopologyValidator + VALID-01/02 + PORT-01/03 |

### In Progress (⚠️)

| 任务 | 文件 | 状态 | 阻塞原因 |
|------|------|------|---------|
| T3.2-08 — NICTLM port_groups | `include/core/port_types.hh`, module_factory.cc | 🔲 有待确认 | 需检查是否在 Phase 3.2 C++ 端完成，原 plan v1.2 标记为未完成 |
| B1 — loadParamRulesForType() | `src/core/module_factory.cc` | 未实现 | C++ 构建系统降级 |
| B2 — Wire validate_module_params | `src/core/module_factory.cc:954` | 函数存在但**从未被调用** | C++ 构建系统降级 |
| T3.3-04/05 — ModuleFactory 集成 | `src/core/module_factory.cc` | 仅 hardcoded RouterTLM 检查 | C++ 构建系统降级 |
| T3.2-17/18 — 文档更新 | `docs/architecture/`, `docs/guide/` | ⚠️ Partial | C++ 端文档完成, Python 端待实施 |

### Remaining (❌)

| 优先级 | 缺口 | 来源 | 说明 |
|--------|------|------|------|
| 🔴 P1 | **ConfigBuilder 缺失 / T3.4-06** | `docs/plan/phase3.4, phase3.2` | `cpptlm_config/builder.py` 不存在，examples 全部无法运行；同时 T3.4-06 (build() 自动验证) 依赖于此 |
| 🔴 P1 | **validate_module_params 死代码 / T3.3-04** | `docs/plan/phase3.3, superpowers` | module_factory.cc:954 存在但 instantiateAll 中从未调用 |
| 🔴 P1 | **ParamType 枚举与 ADR-X.10 不匹配** | `docs/plan/phase3.3 §3.1` | 代码: INTEGER/FLOAT/BOOLEAN/ENUM → ADR: INT/UNSIGNED/ADDRESS/LATENCY/BOOL |
| 🟡 P2 | **B1-B5: param_rules JSON 未加载** | `docs/plan/phase3.3` | `configs/param_rules/*.json` 存在但 instantiateAll 不加载 |
| 🟡 P2 | **C6: Python PARAM-01/02 缺失** | `docs/superpowers/plans` | validator.py 无 required_param/range 验证 |
| 🟡 P2 | **test_param_parser.cc 缺失 / T3.3-11** | `docs/plan/phase3.3 §2.1` | ParamParser 无单元测试 (25+ cases 需覆盖) |
| 🟡 P2 | **T3.4-07~10: 高级验证工具** | `docs/plan/phase3.4 §2.1` | StaticLoadAnalyzer, PathTracer, ConfigLinter + 测试 (25+ cases) |
| 🟡 P2 | **T3.4-14~17: 验证性测试** | `docs/plan/phase3.4 §2.1` | pyproject.toml 验证, 端口组/可视化/ SemVer 验证 |
| 🟡 P2 | **T3.3-07~09: Credit Flow 自动计算** | `docs/plan/phase3.3 §2.2` | Credit Flow 公式 + JSON Schema + Python 端两阶段验证 |
| 🟡 P2 | **T3.3-12: Python credit flow 测试** | `docs/plan/phase3.3 §2.2` | 15+ Python 测试用例 |
| 🟢 P3 | **T3.1-08 默认值未完全集成** | `docs/plan/phase3.1` | RouterTLM::get_param_rules() 存在但仅硬编码使用 |
| 🟢 P3 | **derive_expr 未调用** | `docs/superpowers/plans` | evaluate_derive_expr() 实现但从未执行 |
| 🟢 P3 | **T3.2-08 NICTLM port_groups** | `docs/plan/phase3.2 §2.1` | 需验证当前是否已实现 (原 plan v1.2 标记 🔲) |
| 🟢 P3 | **T3.3-13~15: E2E + 文档** | `docs/plan/phase3.3 §2.3` | E2E 集成测试, 参数系统架构文档, 用户指南 |
| 🟢 P3 | **T3.4-11~13: E2E + 文档** | `docs/plan/phase3.4 §2.2` | E2E 验证测试, 验证工具链架构文档, 用户指南 |
| 🟢 P3 | **T3.2-17/18: 端口管理文档** | `docs/plan/phase3.2 §2.3` | Python 端端口配置指南和架构文档待完成 |

---

## Gap Analysis

### Priority 1 (Critical)

| Gap | Root Cause | Fix | Files | Task |
|-----|-----------|-----|-------|------|
| **ConfigBuilder 缺失** | `cpptlm_config/builder.py` 未创建，但 examples/ 和 topology_adapter.py 均引用它 | 创建 ConfigBuilder 类，含 build()/save()/add_module()/add_connection()/set_extends() | `cpptlm_config/builder.py` | **P1.1** |
| **ModuleSpec/ConnectionSpec 缺失** | topology_adapter.py 引用这两个类但 models.py 中不存在 | 在 models.py 或 builder.py 中定义 | `cpptlm_config/models.py` | **P1.2** |
| **ConfigSchema 无 build/save** | 现有 ConfigSchema Pydantic 模型缺少 builder 语义方法 | 添加 build() → 返回 dict, save(path) → 写 JSON | `cpptlm_config/models.py` | **P1.3** |
| **validate_module_params 死代码** | module_factory.cc:954 函数完整实现但 instantiateAll 中**零处调用** | 在 instantiateAll 的模块循环中替换硬编码 RouterTLM 检查为通用 dispatch | `src/core/module_factory.cc:425-439` | **P1.4** |
| **ParamType 枚举不匹配 ADR-X.10** | ADR 要求 INT/UNSIGNED/STRING/ADDRESS/LATENCY/BOOL，代码是 INTEGER/FLOAT/BOOLEAN/ENUM | 重命名枚举值 + 更新 NLOHMANN_JSON_SERIALIZE_ENUM + 更新 parse() switch + 更新 param_rules JSON | `include/core/param_rules.hh`, `src/core/param_parser.cc`, `configs/param_rules/*.json` | **P1.5** |

**P1 关键依赖链**:
```
P1.1 (ConfigBuilder) → P1.2 (ModuleSpec) → P1.3 (build/save)
     ↓
P1.4 (validate_module_params 接线) → P1.5 (ParamType 对齐)
```

### Priority 2 (High)

| Gap | Root Cause | Fix | Files | Task |
|-----|-----------|-----|-------|------|
| **B1: loadParamRulesForType() 未实现** | configs/param_rules/*.json 存在但 instantiateAll 不加载 | 添加静态函数：根据 module_type 加载对应 JSON 文件 → 反序列化为 ParamRules | `src/core/module_factory.cc` | **P2.1** |
| **B2: derive_expr 未评估** | evaluate_derive_expr() 实现但 instantiateAll 不调用 | 在参数验证循环后添加 derive_expr 评估逻辑 | `src/core/module_factory.cc` | **P2.2** |
| **B3: set_config() 无异常处理** | instantiateAll 中直接调用 obj->set_config()，无 try-catch | 添加 try-catch ParamValidationError/std::exception | `src/core/module_factory.cc` | **P2.3** |
| **C6: Python PARAM-01/02 / T3.4-06** | validator.py 只有 VALID/PORT 检查，无参数验证；build() 不自动调用 validator | 添加 validate_required_params() 和 validate_param_ranges()，从 configs/param_rules/*.json 加载规则；ConfigBuilder.build() 自动调用 validator | `cpptlm_config/validator.py`, `cpptlm_config/builder.py` | **P2.4** |
| **T3.3-11: test_param_parser.cc 缺失** | ParamParser 类已实现但无单元测试覆盖 | 创建测试文件覆盖 parse() 各类型、validate()、evaluate_derive_expr() | `test/test_param_parser.cc` | **P2.5** |
| **T3.4-07~10: 高级验证工具** | Phase 3.4 plan 定义但未实现 StaticLoadAnalyzer, PathTracer, ConfigLinter | 创建 `analyzer.py`, `path_tracer.py`, `linter.py`，每个包含相应验证逻辑和测试 | `cpptlm_config/analyzer.py`, `path_tracer.py`, `linter.py`, `tests/` | **P2.6** |
| **T3.4-14~17: 验证性测试** | pyproject.toml 验证, 端口组/可视化/SemVer 验证未实施 | 为现有 cpptlm_config 功能编写验证测试 | `cpptlm_config/tests/` | **P2.7** |
| **T3.3-07~09: Credit Flow 自动计算** | Phase 3.3 plan 要求 credit_capacity 公式 + 手动覆盖支持 | 在 builder.py 实现自动计算 `max(4, node_count * 2)`，支持手动覆盖；扩展 ConnectionSpec | `cpptlm_config/builder.py`, `models.py` | **P2.8** |
| **T3.3-12: Python credit flow 测试** | Credit Flow 逻辑无测试覆盖 | 编写 15+ Python 测试覆盖自动计算/手动覆盖/边界值 | `cpptlm_config/tests/test_credit_flow.py` | **P2.9** |

### Priority 3 (Medium)

| Gap | Root Cause | Fix | Files | Task |
|-----|-----------|-----|-------|------|
| **var_resolver.hh 已存在但测试覆盖不足** | Metis 误报为"缺失"，实际已创建并集成到 instantiateAll | 验证现有 test_var_resolver.cc 覆盖度，补充边界用例 | `test/test_var_resolver.cc` | **P3.1** |
| **test_validate_config.cc 已存在** | Metis 误报为"缺失"，实际已有 4 个用例 | 验证现有覆盖度，确认无需新增 | `test/test_validate_config.cc` | **P3.2** |
| **T3.1-08 默认值逻辑仅硬编码** | RouterTLM::get_param_rules() 存在但 instantiateAll 中仅对 RouterTLM 使用 | 改为通过 loadParamRulesForType 通用加载（依赖 P2.1） | `src/core/module_factory.cc` | **P3.3** |
| **param_rules JSON derive_expr 字段缺失** | router_tlm.json 无 derive_expr 字段，导致 P2.2 即使实现也无数据 | 在 router_tlm.json 中添加 vc_count 的 derive_expr | `configs/param_rules/router_tlm.json` | **P3.4** |
| **T3.2-08 NICTLM port_groups 状态待确认** | 原 plan v1.2 标记为 🔲，需确认当前是否已实现 | 检查 `include/core/port_types.hh` 和 module_factory.cc 中 port_groups 处理逻辑 | `include/core/port_types.hh`, `src/core/module_factory.cc` | **P3.5** |
| **T3.3-13~15: E2E + 文档** | Phase 3.3 plan 要求的集成测试和文档未完成 | 创建 E2E test, 参数系统架构文档, 用户配置指南 | `test/test_credit_flow_e2e.cc`, `docs/architecture/14-parameter-system.md`, `docs/guide/PARAMETER_CONFIGURATION_GUIDE.md` | **P3.6** |
| **T3.4-11~13: E2E + 文档** | Phase 3.4 plan 要求的集成测试和文档未完成 | 创建 E2E 验证测试, 验证工具链架构文档, 验证工具使用指南 | `cpptlm_config/tests/test_integration.py`, `docs/architecture/15-validation-toolchain.md`, `docs/guide/VALIDATION_GUIDE.md` | **P3.7** |
| **T3.2-17/18: 端口管理文档** | Phase 3.2 C++ 端文档完成, Python 端待实施 | 补全 Python 端口枚举和配置 API 文档 | `docs/architecture/`, `docs/guide/` | **P3.8** |

---

## Execution Phases

### Phase 3.5a (Immediate — Python-Only, No Build)

> **目标**: 修复 Python 侧关键阻塞，使 Phase 3.4 examples 可运行  
> **时间预估**: 3-5 小时  
> **构建依赖**: 无  

```
依赖图:
P1.1 (ConfigBuilder) ──→ P1.2 (ModuleSpec/ConnectionSpec) ──→ P1.3 (build/save)
       │                        │
       ↓                        ↓
P2.4 (Python PARAM-01/02)   P2.6 (Advanced validator tools)
                                    ↓
                              P2.8 (Credit Flow)
                                    ↓
                              P2.9 (Credit Flow tests)
```

- [ ] **Task P1.1**: 创建 `cpptlm_config/builder.py` — ConfigBuilder 类
  - 方法: `__init__(name, description, metadata)`, `add_module(ModuleSpec)`, `add_connection(ConnectionSpec)`, `set_extends(path)`, `build() → ConfigSchema`, `save(path) → None`
  - `build()` 时自动调用 `TopologyValidator` 进行 Python 侧验证 (ADR-X.12 决策 7, T3.4-06)
  - **文件**: `cpptlm_config/builder.py`

- [ ] **Task P1.2**: 在 `cpptlm_config/models.py` 添加 ModuleSpec / ConnectionSpec
  - `ModuleSpec`: name, type(ModuleType), params(dict), port_spec(Optional[ModulePortSpec])
  - `ConnectionSpec`: src, dst, latency, bandwidth(Optional[int])
  - **文件**: `cpptlm_config/models.py`

- [ ] **Task P1.3**: 为 ConfigSchema 添加 `save()` 和 `to_json_dict()`
  - `to_json_dict() → dict`: 序列化为 C++ ModuleFactory 可解析的 JSON 格式
  - `save(path)`: 调用 to_json_dict() 并写入文件
  - **文件**: `cpptlm_config/models.py`

- [ ] **Task P2.4**: 在 `validator.py` 添加 PARAM-01/02 验证 + build() 自动验证
  - `validate_required_params()`: 从 `configs/param_rules/*.json` 加载规则，检查 required=true 的参数是否存在
  - `validate_param_ranges()`: 检查 min_value/max_value 约束
  - 在 `validate()` 方法中调用这两项
  - ConfigBuilder.build() 自动调用 TopologyValidator (T3.4-06)
  - **文件**: `cpptlm_config/validator.py`, `cpptlm_config/builder.py`

- [ ] **Task P2.6**: 高级验证工具
  - `analyzer.py`: StaticLoadAnalyzer (VALID-05 热点链路识别)
  - `path_tracer.py`: PathTracer (TOOL-07 BFS 路径追踪)
  - `linter.py`: ConfigLinter (TOOL-08 最佳实践检查)
  - **文件**: `cpptlm_config/analyzer.py`, `path_tracer.py`, `linter.py`

- [ ] **Task P2.7**: 验证性测试
  - `pyproject.toml` 验证: `pip install -e .` 成功
  - 端口组/可视化/SemVer 验证测试
  - **文件**: `cpptlm_config/tests/test_validator.py`, `test_verification.py`

- [ ] **Task P2.8**: Credit Flow 自动计算 (T3.3-07~08)
  - 在 builder.py 实现 `credit_capacity = max(4, node_count * 2)` 公式
  - 支持手动覆盖 (`ConnectionSpec.credit_flow`)
  - 扩展 ConnectionSpec 支持 credit_flow 字段
  - **文件**: `cpptlm_config/builder.py`, `models.py`

- [ ] **Task P2.9**: Python credit flow 测试 (T3.3-12)
  - 15+ 测试覆盖自动计算/手动覆盖/边界值
  - **文件**: `cpptlm_config/tests/test_credit_flow.py`

- [ ] **Task P3.4**: 更新 `configs/param_rules/router_tlm.json` 添加 derive_expr
  - 为 `vc_count` 添加 `"derive_expr": "(mesh_x >= 4) ? 8 : 4"`
  - **文件**: `configs/param_rules/router_tlm.json`

- [ ] **Task P3.5**: 验证 T3.2-08 NICTLM port_groups 状态
  - 检查 `port_types.hh` 中 PortGroupMember/PortGroupBundleType 定义
  - 检查 module_factory.cc 中 port_groups 解析逻辑
  - 如未实现，创建测试并实现

- [ ] **验证**: 运行所有 Python 示例和测试
  ```bash
  python3 cpptlm_config/examples/mesh_2x2.py
  python3 cpptlm_config/examples/mesh_4x4_validated.py
  python3 cpptlm_config/examples/hierarchical.py
  python3 -m pytest test/python/ -v
  python3 -m pytest cpptlm_config/tests/ -v
  ```

### Phase 3.5b (C++ Build-Dependent)

> **目标**: 修复 C++ 侧 P1/P2 缺口  
> **前置条件**: 构建系统修复 (NAS I/O 问题解决或本地编译环境可用)  
> **时间预估**: 1-2 天  
> **构建依赖**: 是 (所有任务需编译验证)  

```
依赖图:
P1.5 (ParamType 对齐) ──→ P2.1 (loadParamRulesForType) ──→ P1.4 (wire validate_module_params)
       ↓                                                    ↓
P2.5 (test_param_parser)                                  P2.2 (derive_expr)
                                                              ↓
                                                            P2.3 (set_config try-catch)
```

- [ ] **Task P1.5**: 对齐 ParamType 枚举与 ADR-X.10
  - `include/core/param_rules.hh`: INTEGER→INT, FLOAT→删除, BOOLEAN→BOOL, ENUM→删除; 新增 UNSIGNED, ADDRESS, LATENCY
  - `src/core/param_parser.cc`: 更新 parse() switch，添加 parse_address()，更新 parse_latency()
  - `configs/param_rules/*.json`: "INTEGER" → "INT"
  - **风险**: 破坏性变更，需同步更新所有引用点
  - **文件**: `include/core/param_rules.hh`, `src/core/param_parser.cc`, `configs/param_rules/router_tlm.json`, `configs/param_rules/nic_tlm.json`

- [ ] **Task P2.1**: 实现 `loadParamRulesForType()`
  - 输入: module_type string
  - 行为: 尝试打开 `configs/param_rules/{module_type}.json`，反序列化为 `cpptlm::ParamRules`
  - 返回: ParamRules (空 map 表示无规则)
  - **文件**: `src/core/module_factory.cc` (模块工厂内静态函数)

- [ ] **Task P1.4**: 将 `validate_module_params` 接入 instantiateAll
  - 替换 module_factory.cc:425-439 的硬编码 RouterTLM 检查
  - 新逻辑: `for each module → loadParamRulesForType(type) → validate_module_params(type, params, rules)`
  - 失败时返回 false，打印 PARAM ERROR
  - **文件**: `src/core/module_factory.cc:425-439`

- [ ] **Task P2.2**: 添加 derive_expr 评估
  - 在参数验证通过后、模块创建前，对含 derive_expr 的规则进行评估
  - 调用 `cpptlm::ParamParser::evaluate_derive_expr(rule.derive_expr, params_map)`
  - 结果写回 JSON config
  - **文件**: `src/core/module_factory.cc`

- [ ] **Task P2.3**: 为 `set_config()` 添加异常处理
  - 在 instantiateAll 的 set_config() 调用处添加 try-catch
  - 捕获 `cpptlm::ParamValidationError` 和 `std::exception`
  - 打印模块名和错误信息，返回 false
  - **文件**: `src/core/module_factory.cc`

- [ ] **Task P2.5**: 创建 `test/test_param_parser.cc`
  - 测试 parse() 对 INT/UNSIGNED/STRING/ADDRESS/LATENCY/BOOL 的处理
  - 测试 parse_address() 的 hex 和 size 格式
  - 测试 validate() 的 min/max 约束
  - 测试 evaluate_derive_expr() 的 ternary 表达式
  - **文件**: `test/test_param_parser.cc`

- [ ] **验证**: 全量编译 + 测试
  ```bash
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUSE_SYSTEMC=OFF
  cmake --build build -j$(nproc)
  cd build && ctest --output-on-failure
  ./bin/cpptlm_tests "[param]"
  ./bin/cpptlm_tests "[phase3]"
  ```

### Phase 3.5c (Cleanup & Verification)

> **目标**: 确保所有 Metis 报告缺口关闭，文档同步  

- [ ] **Task P3.1**: 验证 test_var_resolver.cc 覆盖度
  - 确认现有 4 个用例是否足够
  - 如需补充，添加数组越界、循环引用等边界用例
  - **文件**: `test/test_var_resolver.cc`

- [ ] **Task P3.2**: 确认 test_validate_config.cc 状态
  - 已有 4 个用例 (T3.1-11a~d)，确认覆盖 ADR-X.11 决策 7 的 8 项检查
  - 如需补充，添加 missing connections/version 测试
  - **文件**: `test/test_validate_config.cc`

- [ ] **Task P3.3**: 将 RouterTLM::get_param_rules() 改为通用加载
  - 移除 instantiateAll 中对 `tlm::RouterTLM::get_param_rules()` 的硬编码调用
  - 统一使用 `loadParamRulesForType()` → 支持所有模块类型
  - **文件**: `src/core/module_factory.cc`

- [ ] **Task P3.6**: Phase 3.3 文档 (T3.3-13~15)
  - 创建 E2E credit flow 集成测试: `test/test_credit_flow_e2e.cc`
  - 参数系统架构文档: `docs/architecture/14-parameter-system.md`
  - 参数配置用户指南: `docs/guide/PARAMETER_CONFIGURATION_GUIDE.md`
  - **文件**: `test/test_credit_flow_e2e.cc`, `docs/architecture/14-parameter-system.md`, `docs/guide/PARAMETER_CONFIGURATION_GUIDE.md`

- [ ] **Task P3.7**: Phase 3.4 文档 (T3.4-11~13)
  - E2E 验证集成测试: `cpptlm_config/tests/test_integration.py`
  - 验证工具链架构文档: `docs/architecture/15-validation-toolchain.md`
  - 验证工具用户指南: `docs/guide/VALIDATION_GUIDE.md`
  - **文件**: `cpptlm_config/tests/test_integration.py`, `docs/architecture/15-validation-toolchain.md`, `docs/guide/VALIDATION_GUIDE.md`

- [ ] **Task P3.8**: Phase 3.2 文档补全 (T3.2-17/18)
  - Python 端口枚举和配置 API 文档
  - **文件**: `docs/architecture/`, `docs/guide/`

- [ ] **文档同步**: 更新所有 plan 文档状态
  - `docs/plan/phase3.2-port-management-plan.md` → 标记已完成项
  - `docs/plan/phase3.3-config-enhancement-plan.md` → 更新 T3.3-04/05/10 状态
  - `docs/plan/phase3.4-validation-toolchain-plan.md` → 更新 T3.4-01~06 状态
  - `docs/superpowers/plans/2026-05-07-phase-3-remaining-tasks.md` → 标记已完成项
  - `docs/superpowers/plans/2026-05-07-phase-3-3-config-enhancement.md` → 更新状态
  - `docs/superpowers/plans/2026-05-08-phase-3-4-validation-toolchain.md` → 标记完成

---

## Verification

### Phase 3.5a 验证清单

| 检查项 | 命令 | 通过标准 |
|--------|------|---------|
| ConfigBuilder 可导入 | `python3 -c "from cpptlm_config.builder import ConfigBuilder; print('OK')"` | 无 ImportError |
| mesh_2x2 示例运行 | `python3 cpptlm_config/examples/mesh_2x2.py` | 生成 `configs/mesh_2x2.json` |
| mesh_4x4_validated 运行 | `python3 cpptlm_config/examples/mesh_4x4_validated.py` | 生成 JSON + 验证通过 |
| PARAM-01 验证 | `python3 -c "from cpptlm_config.validator import TopologyValidator; v = TopologyValidator({'modules':[{'name':'r0','type':'RouterTLM','params':{'node_x':0}}]}); print(v.validate().is_valid)"` | `False` (缺少 node_y/mesh_x/mesh_y) |
| PARAM-02 验证 | `python3 -c "from cpptlm_config.validator import TopologyValidator; v = TopologyValidator({'modules':[{'name':'r0','type':'RouterTLM','params':{'node_x':0,'node_y':0,'mesh_x':2,'mesh_y':2,'vc_count':100}}]}); print(v.validate().is_valid)"` | `False` (vc_count > 8) |
| Python 单元测试 | `python3 -m pytest test/python/test_port_types.py -v` | 全部通过 |

### Phase 3.5b 验证清单

| 检查项 | 命令 | 通过标准 |
|--------|------|---------|
| 编译通过 | `cmake --build build` | 零 error |
| ParamType 测试 | `./build/bin/cpptlm_tests "[param]"` | 全部通过 |
| param_parser 测试 | `./build/bin/cpptlm_tests "[param_parser]"` | 全部通过 (新增) |
| validateConfig 回归 | `./build/bin/cpptlm_tests "[validate]"` | 4/4 通过 |
| var_resolver 回归 | `./build/bin/cpptlm_tests "[var_ref]"` | 4/4 通过 |
| 全量回归 | `cd build && ctest --output-on-failure` | 445+ 通过，零新增失败 |
| param_rules JSON 加载 | 构造含 RouterTLM 的 config，instantiateAll 返回 true | 模块成功实例化 |
| derive_expr 评估 | mesh_x=4 时 vc_count 自动设为 8 | 验证 JSON 中 vc_count=8 |

---

## Git Log Template

### Phase 3.5a Commit (Python)

```bash
# 单个 atomic commit 或按 task 拆分
git add cpptlm_config/builder.py cpptlm_config/models.py cpptlm_config/validator.py configs/param_rules/router_tlm.json
git commit -m "feat(phase3.4): add ConfigBuilder and Python param validation (PARAM-01/02)

- Create cpptlm_config/builder.py with ConfigBuilder class
- Add ModuleSpec/ConnectionSpec to models.py
- Add save()/to_json_dict() to ConfigSchema
- Add validate_required_params() and validate_param_ranges() to validator.py
- Add derive_expr to router_tlm.json

Fixes P1.1, P1.2, P1.3, P2.4, P3.4 from Phase 3.x final plan."
```

### Phase 3.5b Commit 1 (ParamType 对齐)

```bash
git add include/core/param_rules.hh src/core/param_parser.cc configs/param_rules/
git commit -m "fix(phase3.3): align ParamType enum with ADR-X.10 spec

Replace INTEGER/FLOAT/BOOLEAN/ENUM with INT/UNSIGNED/ADDRESS/LATENCY/BOOL.
Add parse_address() for '0x' hex and '256MB' size format.
Update param_rules JSON files to use new type strings.

Fixes P1.5 from Phase 3.x final plan."
```

### Phase 3.5b Commit 2 (ModuleFactory 集成)

```bash
git add src/core/module_factory.cc
git commit -m "feat(phase3.3): wire validate_module_params and derive_expr into instantiateAll

- Add loadParamRulesForType() to load rules from configs/param_rules/*.json
- Replace hardcoded RouterTLM-only param check with generic dispatch
- Add derive_expr evaluation in pre-creation loop
- Add validate_module_params call after set_config()
- Add try-catch for ParamValidationError around set_config()

Fixes P1.4, P2.1, P2.2, P2.3 from Phase 3.x final plan."
```

### Phase 3.5b Commit 3 (测试)

```bash
git add test/test_param_parser.cc
git commit -m "test(phase3.3): add test_param_parser.cc with parser unit tests

Tests for parse() INT/UNSIGNED/ADDRESS/LATENCY/BOOL types,
parse_address() hex and size format, validate() min/max,
evaluate_derive_expr() ternary with >= operator.

Fixes P2.5 from Phase 3.x final plan."
```

---

## Risk Register

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| ParamType 重命名破坏现有测试/配置 | 中 | 高 | 同步更新所有 JSON 和测试，编译前全局搜索 INTEGER/FLOAT/BOOLEAN/ENUM |
| ConfigBuilder API 与 examples 期望不一致 | 低 | 高 | 先运行 examples 确认接口契约，再实现 builder |
| 构建系统持续降级阻塞 3.5b | 高 | 高 | 3.5a 可独立交付；3.5b 标记为阻塞，等待构建环境修复 |
| derive_expr 语法与 evaluate_derive_expr() 实现不匹配 | 低 | 中 | 在 router_tlm.json 中使用与现有实现一致的语法 |
| topology_generator.py API 变更导致 TopologyAdapter 失效 | 低 | 中 | TopologyAdapter 已使用 try/import 兼容模式 |

---

## Appendix

### Metis Finding Corrections

本计划对 Metis 原始报告的两处误报进行修正：

| Metis 报告 | 实际状态 | 说明 |
|-----------|---------|------|
| "var_resolver.hh missing" | ✅ 已存在 | `include/utils/var_resolver.hh` 已创建，且在 `module_factory.cc:414` 已集成到 instantiateAll |
| "test_validate_config.cc missing" | ✅ 已存在 | `test/test_validate_config.cc` 已有 4 个用例 (T3.1-11a~d) |

### docs/plan/ 文档引用与状态

本计划整合了以下 `docs/plan/` 文档中的任务：

| 文档 | 版本 | 状态(in plan) | 已整合任务 |
|------|------|-------------|-----------|
| `docs/plan/ROADMAP.md` | v1.0 | 📋 路线图 | Phase 3.1-3.4 总体框架 |
| `docs/plan/phase3.2-port-management-plan.md` | v1.2 | 🚧 实施中 | T3.2-01~18, C++ 端✅, T3.2-08/T3.2-17/18 🔲 |
| `docs/plan/phase3.3-config-enhancement-plan.md` | v1.1 | 📋 路线图 | T3.3-01~06 C++, T3.3-07~09 Python, T3.3-10~15 测试/文档 |
| `docs/plan/phase3.4-validation-toolchain-plan.md` | v1.1 | 📋 路线图 | T3.4-01~05 ✅, T3.4-06~10 🔲, T3.4-11~17 🔲 |

---

**计划完成，保存至 `docs/superpowers/plans/2026-05-08-phase-3-x-final-plan.md`**

**建议执行顺序**:
1. 立即执行 Phase 3.5a (Python-only，无阻塞)
2. 并行准备 Phase 3.5b (分析 ParamType 变更影响范围)
3. 构建系统修复后执行 Phase 3.5b
4. 最后执行 Phase 3.5c (清理)
