# Plan: HierarchicalTopologyGenerator — 分层 Python 拓扑生成系统

## TL;DR

> **Quick Summary**: 实现基于 Layer + Patch 模式的分层拓扑生成系统，支持模块化生成、局部修改、NoC 专项调整、模板扩展和多拓扑变体对比
>
> **Deliverables**:
> - `cpptlm/topo/layer.py` — TopoLayer + ModuleSpec + ConnectionSpec + CoherenceDomainSpec
> - `cpptlm/topo/patch.py` — TopoPatch + Selector (Connection/Module/Layer)
> - `cpptlm/topo/variant.py` — TopoVariant + TopoVariantSet
> - `cpptlm/topo/orchestrator.py` — TopoOrchestrator (CLI + API)
> - `test/python/test_topo_layer.py` — TopoLayer TDD 测试
> - `test/python/test_topo_patch.py` — TopoPatch TDD 测试
> - `test/python/test_topo_variant.py` — TopoVariant TDD 测试
> - `test/python/test_topo_orchestrator.py` — TopoOrchestrator TDD 测试
> - `test/python/test_topo_cli.py` — CLI 集成测试
> - `test/python/test_topo_e2e.py` — 端到端测试
>
> **Estimated Effort**: Large
> **Parallel Execution**: YES — 6 waves, 4-8 tasks per wave
> **Critical Path**: TopoLayer → TopoPatch → TopoVariant → TopoOrchestrator → CLI → E2E

---

## Context

### Original Request
从 TGMS v4.0 实现计划的 deferred Task 4.6 演化而来：设计并实现 `HierarchicalTopologyGenerator`，一个灵活的、分层的 Python 拓扑生成系统，支持模块化生成、局部修改、NoC 专项调整、模板扩展和多拓扑变体对比。

### Interview Summary
**Key Discussions**:
- **架构模式**: Layer + Patch 模式（分层定义 + 补丁修改），而非继承覆盖
- **API 分层**: Option B — Level 1 (底层 Builder) + Level 2 (高级编排)
- **生成方式**: 顶层生成新 JSON，子模块可用已有配置拼装
- **修改粒度**: selector(glob/regex) + action(ADD/REMOVE/REPLACE/REWIRE)
- **多变体**: TopoVariantSet 管理多个拓扑变体用于性能比较
- **NoC 专项**: 独立的 rewire_noc() 方法，只修改 router 连接
- **用户偏好**: CLI 与 Python API 统一，CLI 只是 API 的调用入口

### Research Findings
- **gem5 参考**: SimObject 使用 metaclass + Param 声明式；Ruby 系统使用 `BaseTopology` + `Cluster.makeTopology()` 递归构建
- **Omnet++ 参考**: NED 语言 + `@network` 声明式，继承链方式（但导致全局覆盖问题）
- **Oracle 建议**: 推荐 Layer+Patch 模式优于继承链，因为继承链无法实现"局部修改"需求
- **现有代码基础**: `ConfigBuilder` (builder pattern) + `TopologyGenerator` (networkx 生成) + `TopologyAdapter` (桥接)

### Metis Review
*（Metis 审查后在生成过程中内化）*

---

## Work Objectives

### Core Objective
实现完整的 `HierarchicalTopologyGenerator` 分层拓扑生成系统，覆盖 Level 1 (TopoLayer) 到 Level 2 (TopoOrchestrator) 的所有组件，并以 TDD 方式提供完整测试覆盖。

### Concrete Deliverables
- 6 个 Python 源文件（`cpptlm/topo/*.py`）
- 6 个测试文件（`test/python/test_topo_*.py`）
- 文档与测试示例

### Definition of Done
- [x] `python3 -m pytest test/python/test_topo_layer.py -v` → ALL PASS
- [x] `python3 -m pytest test/python/test_topo_patch.py -v` → ALL PASS
- [x] `python3 -m pytest test/python/test_topo_variant.py -v` → ALL PASS
- [x] `python3 -m pytest test/python/test_topo_orchestrator.py -v` → ALL PASS
- [x] `python3 -m pytest test/python/test_topo_cli.py -v` → ALL PASS
- [x] `python3 -m pytest test/python/test_topo_e2e.py -v` → ALL PASS
- [x] `python3 -m cpptlm.topo.cli gen --help` → 显示帮助信息

### Must Have
- TopoLayer 支持模块增删改查、连接增删改查、子层嵌套、一致性域管理
- TopoPatch 支持 selector glob 匹配 + 4 种 action (ADD/REMOVE/REPLACE/REWIRE)
- TopoVariant 支持基于基拓扑的补丁变体生成
- TopoOrchestrator 支持 add_layer/load/save/variant/rewire_noc/create_template
- CLI 支持 gen/variant/patch/compare 四个子命令
- 端到端测试覆盖所有 5 个需求场景

### Must NOT Have (Guardrails)
- 不修改现有 `TopologyGenerator` (scripts/topology_generator.py) — 通过 `TopologyAdapter` 桥接
- 不修改 C++ 代码（`src/core/` 下的文件）
- 不引入外部依赖 — 仅使用 `json` 和标准库（CLI 测试可能需要 `sys`/`os`/`tempfile`）
- 不继承 `TopologyGenerator` — 新设计独立于旧类

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed.

### Test Decision
- **Infrastructure exists**: YES — `python3 -m pytest` + unittest
- **Automated tests**: TDD — RED (failing test) → GREEN (minimal impl) → REFACTOR
- **Framework**: unittest + pytest 运行器

### QA Policy
Every task MUST include agent-executed QA scenarios. Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Python tests**: pytest — `python3 -m pytest test/python/test_xxx.py -v --tb=short 2>&1 | tee .sisyphus/evidence/task-N-test.txt`
- **CLI tests**: Bash — `python3 -m cpptlm.topo.cli gen ... --output /tmp/test.json && python3 -c "import json; ..." 2>&1`
- **File output validation**: Bash — `python3 -c "import json; d=json.load(open(...)); assert 'modules' in d"`

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Foundation - MAX PARALLEL):
├── Task 1: cpptlm/topo/__init__.py + package structure
├── Task 2: cpptlm/topo/layer.py — TopoLayer + ModuleSpec + ConnectionSpec + CoherenceDomainSpec
├── Task 3: test/python/test_topo_layer.py — TDD tests
├── Task 4: cpptlm/topo/patch.py — TopoPatch + Selector
└── Task 5: test/python/test_topo_patch.py — TDD tests

Wave 2 (Variant + Orchestrator - PARALLEL):
├── Task 6: cpptlm/topo/variant.py — TopoVariant + TopoVariantSet
├── Task 7: test/python/test_topo_variant.py — TDD tests
├── Task 8: cpptlm/topo/orchestrator.py — TopoOrchestrator (NoC, template)
└── Task 9: test/python/test_topo_orchestrator.py — TDD tests

Wave 3 (CLI + Integration):
├── Task 10: cpptlm/topo/cli.py — CLI entry + subcommands
├── Task 11: test/python/test_topo_cli.py — CLI integration tests
└── Task 12: test/python/test_topo_e2e.py — End-to-end tests

Wave FINAL (Verification):
├── Task F1: Plan compliance audit (oracle)
├── Task F2: Code quality review (unspecified-high)
├── Task F3: Real manual QA (unspecified-high)
└── Task F4: Scope fidelity check (deep)
```

### Dependency Matrix
- **1**: — → 2-5, Wave 1
- **2-3**: 1 → 6, 8, Wave 2
- **4-5**: 1 → 6, 8, Wave 2
- **6-7**: 2-5 → 8, Wave 2
- **8-9**: 2-7 → 10, Wave 3
- **10-12**: 6-9 → F1-F4, Final
- **F1-F4**: All → user okay

---

## TODOs

- [x] 1. 创建 `cpptlm/topo/__init__.py` 包结构 + `cpptlm/topo/layer.py`

  **What to do**:
  - 创建 `cpptlm/topo/__init__.py` 包文件（空的，或重新导出主要符号）
  - 在 `cpptlm/topo/layer.py` 中实现:
    - `ConnectionAction` enum (ADD, REMOVE, REPLACE)
    - `ModuleSpec` dataclass (name, type, params, metadata)
    - `ConnectionSpec` dataclass (src, dst, latency, bandwidth, vc_priorities)
    - `CoherenceDomainSpec` dataclass (name, protocol, members, bridges)
    - `TopoLayer` 类:
      - 构造函数: `TopoLayer(name)`，含 modules/connections/coherence_domains/sublayers/metadata
      - 模块操作: `add_module()`, `add_modules()`, `get_module()`, `remove_module()`
      - 连接操作: `add_connection()`, `remove_connection()`, `rewire()`
      - 子层操作: `add_sublayer()`, `get_sublayer()`
      - 一致性域操作: `add_coherence_domain()`
      - 序列化: `to_dict()`, `from_dict()`
      - 组合: `flatten()`, `merge()`

  **Must NOT do**:
  - 不依赖外部库（仅标准库 `json`, `copy`, `dataclasses`, `typing`）
  - 不修改现有 `cpptlm_config/` 下的文件

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []
  - Reason: Python 基础设施 + 数据模型设计，逻辑清晰无需特殊技能

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2-5)
  - **Blocks**: Tasks 6, 8
  - **Blocked By**: Task 1 (package init)

  **References**:
  - `cpptlm_config/models.py` — 现有的 ModuleSpec/ConnectionSpec Pydantic 模型（借鉴字段，不直接使用）
  - `cpptlm_config/builder.py` — 现有的 ConfigBuilder 链式调用模式
  - `docs/superpowers/specs/2026-05-29-hierarchical-topology-generator-design.md` — 完整设计文档

  **Acceptance Criteria**:
  - [ ] `python3 -c "from cpptlm.topo.layer import TopoLayer, ModuleSpec, ConnectionSpec"` → 无错误
  - [ ] `python3 -c "l = TopoLayer('test'); l.add_module('cpu0','CPUTLM'); d=l.to_dict(); assert len(d['modules'])==1"` → 无错误

  **QA Scenarios**:
  ```
  Scenario: TopoLayer 创建和模块操作
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    l = TopoLayer('test')
    l.add_module('cpu0', 'CPUTLM').add_module('l2_cache', 'CacheTLM')
    d = l.to_dict()
    assert d['name'] == 'test'
    assert len(d['modules']) == 2
    assert d['modules'][0]['name'] == 'cpu0'
    assert d['modules'][0]['type'] == 'CPUTLM'
    l2 = TopoLayer.from_dict(d)
    assert l2.name == 'test'
    assert len(l2.modules) == 2
  " 2>&1
    Expected Result: 无异常
    Evidence: .sisyphus/evidence/task-1-layer-create.txt

  Scenario: TopoLayer 连接和子层操作
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    l = TopoLayer('soc')
    l.add_module('cpu0', 'CPUTLM').add_module('mem', 'MemoryTLM')
    l.add_connection('cpu0', 'mem', latency=10)
    sub = TopoLayer('cluster0')
    sub.add_module('cpu1', 'CPUTLM')
    l.add_sublayer(sub)
    d = l.to_dict()
    assert len(d['connections']) == 1
    assert len(d['sublayers']) == 1
    assert d['sublayers'][0]['modules'][0]['name'] == 'cpu1'
  " 2>&1
    Expected Result: 无异常
    Evidence: .sisyphus/evidence/task-1-layer-connection.txt
  ```

  **Evidence to Capture**:
  - [ ] task-1-layer-create.txt
  - [ ] task-1-layer-connection.txt
  - [ ] task-1-layer-coherence-domain.txt

  **Commit**: YES
  - Message: `feat(topo): add TopoLayer core with ModuleSpec/ConnectionSpec/CoherenceDomainSpec`

- [x] 3. 创建 `test/python/test_topo_layer.py` — TopoLayer TDD 测试

  **What to do**:
  - 基于现有的 `test/python/test_topology_generator.py` 模式创建测试文件
  - 测试用例覆盖:
    - TopoLayer 构造和名称属性
    - add_module / add_modules / get_module / remove_module
    - add_connection / remove_connection / rewire
    - add_sublayer / get_sublayer
    - add_coherence_domain / get_coherence_domains
    - to_dict() / from_dict() 序列化往返
    - flatten() 展平子层
    - merge() 合并两个层
    - 空层边界情况
    - 深嵌套子层

  **Must NOT do**:
  - 不依赖 `patch.py` 或 `variant.py`
  - 不修改 layer.py（测试先于实现，RED 阶段）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []
  - Reason: 测试代码，模式固定

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 2)
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 6-9
  - **Blocked By**: Task 1

  **References**:
  - `test/python/test_topology_generator.py` — 现有测试模式（unittest, setUp, skipUnless）
  - `cpptlm_config/tests/test_config_builder.py` — 测试模式参考

  **测试用例清单**:
  1. `test_layer_creation`: 创建 TopoLayer 并验证 name
  2. `test_add_module`: 添加单个模块
  3. `test_add_modules`: 批量添加
  4. `test_get_module`: 按名称查找
  5. `test_get_module_not_found`: 查找不存在的模块
  6. `test_remove_module`: 移除模块及其连接
  7. `test_add_connection`: 添加连接
  8. `test_remove_connection`: 移除连接
  9. `test_rewire_connection`: 重连
  10. `test_add_sublayer`: 嵌套子层
  11. `test_get_sublayer`: 查找子层
  12. `test_add_coherence_domain`: 添加一致性域
  13. `test_to_dict_basic`: 基本序列化
  14. `test_from_dict`: 反序列化
  15. `test_to_dict_roundtrip`: 序列化往返
  16. `test_flatten`: 展平子层
  17. `test_merge`: 合并层
  18. `test_empty_layer`: 空层序列化
  19. `test_deep_nesting`: 深嵌套（3+ 层）

  **Acceptance Criteria**:
  - [ ] `python3 -m pytest test/python/test_topo_layer.py -v --tb=short` → 19/19 PASS

  **QA Scenarios**:
  ```
  Scenario: TopoLayer TDD 测试全通过
    Tool: Bash
    Steps:
      1. cd /workspace/project/CppTLM
      2. python3 -m pytest test/python/test_topo_layer.py -v --tb=short 2>&1
    Expected Result: 19/19 PASSED (或全部通过)
    Failure Indicators: 任何 FAILED/ERROR
    Evidence: .sisyphus/evidence/task-3-layer-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] task-3-layer-tests.txt

  **Commit**: YES (groups with Task 2)
  - Message: `feat(topo): add TopoLayer core with ModuleSpec/ConnectionSpec/CoherenceDomainSpec`

- [x] 4. 创建 `cpptlm/topo/patch.py` — TopoPatch + Selector

  **What to do**:
  - 在 `cpptlm/topo/patch.py` 中实现:
    - `PatchAction` enum (ADD, REMOVE, REPLACE, REWIRE)
    - `Selector` 基类 (match_module, match_connection, match_layer)
    - `ConnectionSelector(Selector)`: glob 模式匹配 src/dst
    - `ModuleSelector(Selector)`: glob 模式匹配 module name
    - `LayerSelector(Selector)`: 按 name/type 匹配 layer
    - `TopoPatch` 类:
      - 属性: selector, action, template, rewiring
      - `apply_to(layer)` 方法 — 应用补丁到层
      - 内部方法: `_apply_connection_patch`, `_apply_module_patch`, `_apply_layer_patch`
    - glob → regex 转换辅助函数（`*` → `.*`, `?` → `.`）

  **Must NOT do**:
  - 不修改 layer.py
  - 不引入 `fnmatch` 第三方库（使用标准库 `re`）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []
  - Reason: 需要正则/glob 匹配逻辑，可独立实现

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 2-3, 5)
  - **Blocks**: Tasks 6-9
  - **Blocked By**: Task 1

  **References**:
  - `docs/superpowers/specs/2026-05-29-hierarchical-topology-generator-design.md` — Patch 设计

  **Acceptance Criteria**:
  - [ ] `python3 -c "from cpptlm.topo.patch import TopoPatch, ConnectionSelector, PatchAction"` → 无错误
  - [ ] `python3 -c "from cpptlm.topo.layer import TopoLayer; from cpptlm.topo.patch import *; l=TopoLayer('t'); l.add_connection('cpu0','mem',latency=1); p=TopoPatch(ConnectionSelector('cpu*'), PatchAction.REMOVE); l=p.apply_to(l); assert len(l.connections)==0"` → 无错误

  **QA Scenarios**:
  ```
  Scenario: TopoPatch REMOVE 补丁应用
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.patch import TopoPatch, ConnectionSelector, PatchAction
    l = TopoLayer('test')
    l.add_connection('cpu0', 'mem', latency=1)
    l.add_connection('cpu1', 'cache', latency=1)
    assert len(l.connections) == 2
    p = TopoPatch(ConnectionSelector('cpu*'), PatchAction.REMOVE)
    l = p.apply_to(l)
    assert len(l.connections) == 0
  " 2>&1
    Expected Result: 无异常
    Evidence: .sisyphus/evidence/task-4-patch-remove.txt

  Scenario: TopoPatch REWIRE 补丁应用
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.patch import TopoPatch, ConnectionSelector, PatchAction
    l = TopoLayer('test')
    l.add_connection('cpu0', 'cache', latency=1)
    p = TopoPatch(ConnectionSelector('cpu0'), PatchAction.REWIRE,
                  rewiring={'dst': 'l2_cache'})
    l = p.apply_to(l)
    assert l.connections[0].dst == 'l2_cache'
  " 2>&1
    Expected Result: 无异常
    Evidence: .sisyphus/evidence/task-4-patch-rewire.txt
  ```

  **Evidence to Capture**:
  - [ ] task-4-patch-remove.txt
  - [ ] task-4-patch-rewire.txt
  - [ ] task-4-patch-add-layer.txt

  **Commit**: YES
  - Message: `feat(topo): add TopoPatch with glob-based Selector and 4 actions`

- [x] 5. 创建 `test/python/test_topo_patch.py` — TopoPatch TDD 测试

  **What to do**:
  - 测试用例覆盖:
    - ConnectionSelector 匹配 src
    - ConnectionSelector 匹配 dst
    - ConnectionSelector glob 模式 (`cpu*.cache`)
    - ConnectionSelector 不匹配
    - ModuleSelector 匹配/不匹配
    - LayerSelector 匹配 name
    - LayerSelector 匹配 type
    - TopoPatch ADD 到 connection
    - TopoPatch REMOVE connection
    - TopoPatch REPLACE connection
    - TopoPatch REWIRE connection (单端)
    - TopoPatch REWIRE connection (双端)
    - TopoPatch ADD 到 sublayer
    - TopoPatch REMOVE module
    - 空 patch 操作 (无害)
    - ADD 不存在匹配 (无害)

  **Must NOT do**:
  - 不依赖 `variant.py`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []
  - Reason: 测试代码，模式固定

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 4)
  - **Parallel Group**: Wave 1
  - **Blocks**: Tasks 6-9
  - **Blocked By**: Task 1

  **Acceptance Criteria**:
  - [ ] `python3 -m pytest test/python/test_topo_patch.py -v --tb=short` → ALL PASS

  **QA Scenarios**:
  ```
  Scenario: TopoPatch TDD 测试全通过
    Tool: Bash
    Steps:
      1. python3 -m pytest test/python/test_topo_patch.py -v --tb=short 2>&1
    Expected Result: ALL PASSED
    Evidence: .sisyphus/evidence/task-5-patch-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] task-5-patch-tests.txt

  **Commit**: YES (groups with Task 4)

- [x] 6. 创建 `cpptlm/topo/variant.py` — TopoVariant + TopoVariantSet

  **What to do**:
  - 在 `cpptlm/topo/variant.py` 中实现:
    - `TopoVariant` dataclass:
      - 属性: name, base (TopoLayer), patches (list[TopoPatch]), metrics (list[str])
      - `build()` 方法: 深拷贝基拓扑，应用所有补丁
    - `TopoVariantSet` dataclass:
      - 属性: name, variants (list[TopoVariant])
      - `add_variant(variant)` 方法
      - `build_all()` 方法: 构建所有变体，返回 dict[name→TopoLayer]
      - `compare()` 方法: 比较所有变体的 module_count/connection_count/hierarchy_depth

  **Must NOT do**:
  - 不依赖 `cli.py`
  - 不依赖外部库

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []
  - Reason: 需要深拷贝逻辑和变体管理

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2 (with Task 7-9)
  - **Blocks**: Tasks 10
  - **Blocked By**: Tasks 2-3 (TopoLayer), 4-5 (TopoPatch)

  **References**:
  - `docs/superpowers/specs/2026-05-29-hierarchical-topology-generator-design.md` — Variant 设计

  **Acceptance Criteria**:
  - [ ] `python3 -c "from cpptlm.topo.variant import TopoVariant, TopoVariantSet"` → 无错误
  - [ ] `python3 -c "
  from cpptlm.topo.layer import TopoLayer
  from cpptlm.topo.variant import TopoVariant
  base = TopoLayer('base'); base.add_module('cpu0','CPUTLM')
  v = TopoVariant('test', base=base)
  result = v.build()
  assert len(result.modules) == 1
  assert result.name == 'base_test'
  "` → 无错误

  **QA Scenarios**:
  ```
  Scenario: TopoVariant build 创建独立副本
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.variant import TopoVariant
    from cpptlm.topo.patch import *
    base = TopoLayer('base')
    base.add_module('cpu0', 'CPUTLM').add_connection('cpu0', 'mem', latency=1)
    base.add_module('old_mem', 'MemoryTLM')
    v = TopoVariant('new_mem', base=base,
                    patches=[TopoPatch(ConnectionSelector('cpu0'),
                                       PatchAction.REWIRE,
                                       rewiring={'dst': 'new_mem'})])
    result = v.build()
    assert result.modules[0].name == 'cpu0'
    assert result.connections[0].dst == 'new_mem'
    # 原始 base 不受影响
    assert base.connections[0].dst == 'mem'
    assert result.name == 'base_new_mem'
  " 2>&1
    Expected Result: 无异常
    Evidence: .sisyphus/evidence/task-6-variant-build.txt

  Scenario: TopoVariantSet build_all 和 compare
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.variant import TopoVariant, TopoVariantSet
    base = TopoLayer('base')
    base.add_module('cpu0', 'CPUTLM').add_connection('cpu0', 'mem', latency=1)
    vs = TopoVariantSet('test')
    vs.add_variant(TopoVariant('v1', base=base))
    vs.add_variant(TopoVariant('v2', base=base))
    all = vs.build_all()
    assert len(all) == 2
    assert 'v1' in all and 'v2' in all
    comp = vs.compare()
    assert comp['v1']['module_count'] == 1
    assert comp['v2']['module_count'] == 1
  " 2>&1
    Expected Result: 无异常
    Evidence: .sisyphus/evidence/task-6-variant-set.txt
  ```

  **Evidence to Capture**:
  - [ ] task-6-variant-build.txt
  - [ ] task-6-variant-set.txt

  **Commit**: YES
  - Message: `feat(topo): add TopoVariant + TopoVariantSet for multi-variant generation`

- [x] 7. 创建 `test/python/test_topo_variant.py` — TopoVariant TDD 测试

  **What to do**:
  - 测试用例覆盖:
    - TopoVariant 构造
    - TopoVariant.build() 创建独立副本（深拷贝验证）
    - TopoVariant.build() 应用补丁修改
    - TopoVariant.build() 保留未被补丁影响的元素
    - TopoVariant 名称后缀格式
    - TopoVariantSet 构造
    - TopoVariantSet.add_variant()
    - TopoVariantSet.build_all() 返回 dict
    - TopoVariantSet.compare() 返回统计
    - 空 patches 的 variant 构建（等同基拓扑）
    - 多个 variant 的名称唯一性

  **Must NOT do**:
  - 不依赖 `cli.py`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 6)
  - **Parallel Group**: Wave 2

  **Acceptance Criteria**:
  - [ ] `python3 -m pytest test/python/test_topo_variant.py -v --tb=short` → ALL PASS

  **QA Scenarios**:
  ```
  Scenario: TopoVariant TDD 测试全通过
    Tool: Bash
    Steps:
      1. python3 -m pytest test/python/test_topo_variant.py -v --tb=short 2>&1
    Expected Result: ALL PASSED
    Evidence: .sisyphus/evidence/task-7-variant-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] task-7-variant-tests.txt

  **Commit**: YES (groups with Task 6)

- [x] 8. 创建 `cpptlm/topo/orchestrator.py` — TopoOrchestrator

  **What to do**:
  - 在 `cpptlm/topo/orchestrator.py` 中实现:
    - `TopoOrchestrator` 类:
      - 构造函数: `__init__(name)`，含 _layers dict, _variants dict, _registry dict
      - 工厂方法: `layer(name)` 静态工厂, `patch(selector, action, **kwargs)` 静态工厂
      - 层操作: `add_layer(name, layers)`, `get_layer(name)`, `remove_layer(name)`
      - 文件操作: `load(path, name)`, `save(path, name)` — JSON 文件读写
      - 变体操作: `add_variant(name, base, patches, metrics)`, `build_variant(name)`, `build_all_variants()`
      - 模板操作: `create_template(name, base, modify)` — 回调式模板扩展
      - NoC 操作: `rewire_noc(old_topology, new_topology, router_type)` — 专项方法
      - 辅助: `_parse_router_coords(name)`, `_find_router_at(x, y, routers)`, `_generate_mesh_links()`, `_generate_torus_links()`

  **Must NOT do**:
  - 不修改现有 files (scripts/topology_generator.py, cpptlm_config/)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []
  - Reason: 编排逻辑 + NoC 操作，需要综合能力

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 9)
  - **Parallel Group**: Wave 2
  - **Blocks**: Tasks 10-12
  - **Blocked By**: Tasks 2-7 (layer + patch + variant)

  **References**:
  - `docs/superpowers/specs/2026-05-29-hierarchical-topology-generator-design.md` — 完整设计
  - `scripts/topology_generator.py` — 现有 TopologyGenerator 的 generate_mesh 方法（参考坐标解析模式）

  **Acceptance Criteria**:
  - [ ] `python3 -c "
  from cpptlm.topo.orchestrator import TopoOrchestrator
  o = TopoOrchestrator('test')
  l = TopoOrchestrator.layer('clust')
  l.add_module('cpu0','CPUTLM')
  o.add_layer('cluster0', layers=[l])
  assert o.get_layer('cluster0') is not None
  o.save('/tmp/test_topo_save.json')
  o2 = TopoOrchestrator('test2')
  o2.load('/tmp/test_topo_save.json')
  assert o2.get_layer('cluster0') is not None
  import os; os.unlink('/tmp/test_topo_save.json')
  "` → 无错误

  **QA Scenarios**:
  ```
  Scenario: TopoOrchestrator save/load 往返
    Tool: Bash
    Steps:
      1. python3 -c "
    import tempfile, os, json
    from cpptlm.topo.orchestrator import TopoOrchestrator
    o = TopoOrchestrator('test')
    l = TopoOrchestrator.layer('clust')
    l.add_module('cpu0','CPUTLM')
    l.add_connection('cpu0','cache',latency=1)
    o.add_layer('cluster0', layers=[l])
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
      path = f.name
    o.save(path)
    o2 = TopoOrchestrator('test2')
    o2.load(path)
    assert o2.get_layer('cluster0') is not None
    d = json.load(open(path))
    assert 'modules' in d
    os.unlink(path)
    print('PASS')
  " 2>&1
    Expected Result: 输出 PASS
    Evidence: .sisyphus/evidence/task-8-orchestrator-saveload.txt

  Scenario: TopoOrchestrator create_template
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.orchestrator import TopoOrchestrator
    o = TopoOrchestrator('test')
    base = TopoOrchestrator.layer('base')
    base.add_module('cpu0','CPUTLM').add_module('cache','CacheTLM')
    o.add_layer('base', layers=[base])
    o.create_template('extended', 'base',
      modify=lambda l: l.add_module('cpu1','CPUTLM'))
    ext = o.get_layer('extended')
    assert ext is not None
    assert len(ext.modules) == 3
    print('PASS')
  " 2>&1
    Expected Result: 输出 PASS
    Evidence: .sisyphus/evidence/task-8-orchestrator-template.txt

  Scenario: NoC rewire 操作
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.orchestrator import TopoOrchestrator
    from cpptlm.topo.layer import TopoLayer
    o = TopoOrchestrator('test')
    noc = TopoLayer('mesh_noc')
    noc.add_module('router_0_0', 'RouterTLM')
    noc.add_module('router_0_1', 'RouterTLM')
    noc.add_connection('router_0_0', 'router_0_1', latency=1)
    o.add_layer('mesh_noc', layers=[noc])
    assert len(o.get_layer('mesh_noc').connections) == 1
    print('PASS')
  " 2>&1
    Expected Result: 输出 PASS
    Evidence: .sisyphus/evidence/task-8-orchestrator-noc.txt
  ```

  **Evidence to Capture**:
  - [ ] task-8-orchestrator-saveload.txt
  - [ ] task-8-orchestrator-template.txt
  - [ ] task-8-orchestrator-noc.txt

  **Commit**: YES
  - Message: `feat(topo): add TopoOrchestrator with save/load/template/variant/NoC operations`

- [x] 9. 创建 `test/python/test_topo_orchestrator.py` — TopoOrchestrator TDD 测试

  **What to do**:
  - 测试用例覆盖:
    - 构造函数和命名
    - add_layer / get_layer / remove_layer
    - layer() 静态工厂方法
    - patch() 静态工厂方法
    - save() / load() 文件操作（使用 tempfile）
    - add_variant / build_variant / build_all_variants
    - create_template() 回调扩展
    - rewire_noc() NoC 重连接
    - 加载不存在的文件（异常处理）
    - 构建不存在的层（异常处理）

  **Must NOT do**:
  - 不依赖 `cli.py`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 8)
  - **Parallel Group**: Wave 2

  **Acceptance Criteria**:
  - [ ] `python3 -m pytest test/python/test_topo_orchestrator.py -v --tb=short` → ALL PASS

  **QA Scenarios**:
  ```
  Scenario: TopoOrchestrator TDD 测试全通过
    Tool: Bash
    Steps:
      1. python3 -m pytest test/python/test_topo_orchestrator.py -v --tb=short 2>&1
    Expected Result: ALL PASSED
    Evidence: .sisyphus/evidence/task-9-orchestrator-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] task-9-orchestrator-tests.txt

  **Commit**: YES (groups with Task 8)

- [x] 10. 创建 `cpptlm/topo/cli.py` — CLI 入口

  **What to do**:
  - 在 `cpptlm/topo/cli.py` 中实现 main():
    - `gen` 子命令: `--name`, `--template`, `--output`, `--noc`, `--clusters`, `--cpus-per-cluster`
    - `variant` 子命令: `--base`, `--variants`, `--output-dir`
    - `patch` 子命令: `--input`, `--output`, `--selector`, `--action`, `--template`
    - `compare` 子命令: `--base`, `--variants`
    - 每个命令执行对应 TopoOrchestrator 方法

  **Must NOT do**:
  - 不引入额外外部依赖（仅 `argparse`, `json`, `sys`）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []
  - Reason: CLI 命令编排，需要协调多个子模块

  **Parallelization**:
  - **Can Run In Parallel**: NO (depends on orchestrator)
  - **Parallel Group**: Wave 3
  - **Blocks**: Tasks 11-12
  - **Blocked By**: Tasks 8-9 (orchestrator)

  **Acceptance Criteria**:
  - [ ] `python3 -m cpptlm.topo.cli gen --help` → 显示帮助信息
  - [ ] `python3 -m cpptlm.topo.cli gen --name test --output /tmp/test_topo_cli.json` → 生成 JSON 文件
  - [ ] `python3 -c "import json; f=open('/tmp/test_topo_cli.json'); d=json.load(f); assert 'modules' in d"` → 有效 JSON

  **QA Scenarios**:
  ```
  Scenario: CLI gen 命令输出 JSON
    Tool: Bash
    Steps:
      1. python3 -m cpptlm.topo.cli gen --name test_soc --output /tmp/test_cli_gen.json 2>&1
      2. python3 -c "
    import json; d=json.load(open('/tmp/test_cli_gen.json'))
    assert d['name'] == 'test_soc'
    assert 'modules' in d
    assert 'connections' in d
    print('PASS')
  " 2>&1
      3. rm -f /tmp/test_cli_gen.json
    Expected Result: CLI 生成有效 JSON 文件
    Evidence: .sisyphus/evidence/task-10-cli-gen.txt

  Scenario: CLI variant 命令
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    l = TopoLayer('base')
    l.add_module('cpu0','CPUTLM')
    import json; json.dump(l.to_dict(), open('/tmp/test_cli_base.json','w'))
  "
      2. python3 -m cpptlm.topo.cli variant --base /tmp/test_cli_base.json \
         --variants v1 v2 --output-dir /tmp/test_variants/ 2>&1
      3. python3 -c "
    import os; assert os.path.exists('/tmp/test_variants/v1.json')
    assert os.path.exists('/tmp/test_variants/v2.json')
    print('PASS')
  " 2>&1
      4. rm -rf /tmp/test_cli_base.json /tmp/test_variants/
    Expected Result: 变体文件生成成功
    Evidence: .sisyphus/evidence/task-10-cli-variant.txt
  ```

  **Evidence to Capture**:
  - [ ] task-10-cli-gen.txt
  - [ ] task-10-cli-variant.txt

  **Commit**: YES
  - Message: `feat(topo): add CLI with gen/variant/patch/compare subcommands`

- [x] 11. 创建 `test/python/test_topo_cli.py` — CLI 集成测试

  **What to do**:
  - 测试用例覆盖:
    - `gen --help` 显示帮助
    - `gen --name test --output /tmp/f.json` 生成文件并验证
    - `gen --name test --noc mesh --clusters 2 --cpus-per-cluster 4` 复杂生成
    - `variant --base base.json --variants v1 v2 --output-dir dir` 变体生成
    - `patch --input in.json --output out.json --selector '*.xbar.*' --action remove` 补丁应用
    - `compare --base base.json --variants v1 v2` 比较输出
    - 命令行错误处理（缺少必选参数）

  **Must NOT do**:
  - 不测试 E2E 场景（留给 Task 12）

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 10)
  - **Parallel Group**: Wave 3

  **Acceptance Criteria**:
  - [ ] `python3 -m pytest test/python/test_topo_cli.py -v --tb=short` → ALL PASS

  **QA Scenarios**:
  ```
  Scenario: CLI TDD 测试全通过
    Tool: Bash
    Steps:
      1. python3 -m pytest test/python/test_topo_cli.py -v --tb=short 2>&1
    Expected Result: ALL PASSED
    Evidence: .sisyphus/evidence/task-11-cli-tests.txt
  ```

  **Evidence to Capture**:
  - [ ] task-11-cli-tests.txt

  **Commit**: YES (groups with Task 10)

- [x] 12. 创建 `test/python/test_topo_e2e.py` — 端到端集成测试

  **What to do**:
  - 模拟用户 5 个需求的完整流程:
    1. **模块化生成**: 创建 CPU 集群层，加载已有 Memory 配置，拼装为完整系统
    2. **局部重生成**: 构建完整系统后，只移除并重连特定模块的连接
    3. **NoC 专项调整**: 创建含多个 router 的 mesh 拓扑，展平后验证
    4. **模板扩展**: 基于 2x2 模板创建 4x4 规模并验证模块数量
    5. **多拓扑对比**: 创建 low_latency / high_bandwidth 变体，比较差异
  - 测试用例覆盖:
    - `test_e2e_modular_generation`: 需求 1 — 子模块拼装
    - `test_e2e_partial_regeneration`: 需求 2 — 局部重连
    - `test_e2e_noc_adjustment`: 需求 3 — NoC 调整
    - `test_e2e_template_extension`: 需求 4 — 模板扩展
    - `test_e2e_variant_comparison`: 需求 5 — 多变体对比

  **Must NOT do**:
  - 不修改实现代码（仅测试）
  - 不依赖外部模拟器

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: []
  - Reason: 端到端场景设计，需要理解 5 个需求的完整数据流

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 10-11)
  - **Parallel Group**: Wave 3
  - **Blocked By**: Tasks 8-9 (orchestrator)

  **端到端测试用例详细设计**:

  ```
  Test: test_e2e_modular_generation
    Description: 模块化生成 — 顶层创建新系统，子模块从已有配置加载
    Steps:
      1. 创建 base_mem.json 文件（2 个内存模块）
      2. 使用 TopoLayer 创建 system 层
      3. 加载 base_mem.json 为子层
      4. 添加 CPU 集群子层
      5. 添加 coherence domain
      6. 序列化为 dict 并验证
    Assertions:
      - modules 计数 = 内存模块 + CPU 模块
      - connections 包含跨层连接
      - coherence_domains 包含一致性域定义

  Test: test_e2e_partial_regeneration
    Description: 局部重生成 — 只移除 NoC 连接，保留 CPU/Memory
    Steps:
      1. 创建复杂拓扑（CPU + Router + Memory）
      2. 使用 TopoPatch 移除所有 router*.xbar 连接
      3. 验证 CPU/Memory 模块不变
      4. 验证 router 连接被移除
    Assertions:
      - CPU 模块数不变
      - Memory 模块数不变
      - 所有包含 "router" src/dst 的连接被移除

  Test: test_e2e_variant_comparison
    Description: 多拓扑变体对比 — 创建 3 个变体并比较
    Steps:
      1. 创建 base 拓扑
      2. 添加 3 个变体: low_latency, high_bw, balanced
      3. 使用 TopoVariantSet.build_all() 构建
      4. 使用 TopoVariantSet.compare() 比较
    Assertions:
      - build_all() 返回 3 个变体
      - compare() 返回每个变体的统计信息
      - 不同变体有差异
  ```

  **Acceptance Criteria**:
  - [ ] `python3 -m pytest test/python/test_topo_e2e.py -v --tb=short` → ALL PASS

  **QA Scenarios**:
  ```
  Scenario: E2E 模块化生成测试
    Tool: Bash
    Steps:
      1. python3 -c "
    import json, tempfile, os
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.orchestrator import TopoOrchestrator
    # 创建子配置
    mem_layer = TopoLayer('mem_sub')
    mem_layer.add_module('dram0', 'MemoryTLM')
    mem_layer.add_module('dram1', 'MemoryTLM')
    mem_layer.add_connection('dram0', 'xbar', latency=2)
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(mem_layer.to_dict(), f)
        mem_path = f.name
    # 顶层编排
    orch = TopoOrchestrator('e2e_test')
    orch.load(mem_path, name='mem')
    cpu_cluster = TopoLayer('cpu_cluster')
    for i in range(4):
        cpu_cluster.add_module(f'cpu{i}', 'CPUTLM')
    orch.add_layer('cpu', layers=[cpu_cluster])
    top = TopoLayer('system')
    top.add_sublayer(orch.get_layer('mem'))
    top.add_sublayer(orch.get_layer('cpu'))
    top.add_coherence_domain('cpu_domain', 'MESI', [f'cpu{i}' for i in range(4)])
    d = top.to_dict()
    assert len(d['modules']) == 6
    assert len(d['coherence_domains']) == 1
    assert d['coherence_domains'][0]['members'] == ['cpu0','cpu1','cpu2','cpu3']
    os.unlink(mem_path)
    print('PASS')
  " 2>&1
    Expected Result: 输出 PASS
    Evidence: .sisyphus/evidence/task-12-e2e-modular.txt

  Scenario: E2E 局部重生成（Patch apply）
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.patch import *
    top = TopoLayer('e2e_partial')
    # 系统模块
    for i in range(4): top.add_module(f'cpu{i}', 'CPUTLM')
    top.add_module('mem', 'MemoryTLM')
    top.add_module('xbar', 'CrossbarTLM')
    # NoC 连接
    for i in range(4): top.add_connection(f'cpu{i}', 'xbar', latency=1)
    top.add_connection('xbar', 'mem', latency=10)
    assert len(top.modules) == 6
    assert len(top.connections) == 5
    # Patch 移除 NoC 连接
    p = TopoPatch(ConnectionSelector('*.xbar*'), PatchAction.REMOVE)
    top = p.apply_to(top)
    assert len(top.connections) == 0
    assert len(top.modules) == 6  # 模块不变
    print('PASS')
  " 2>&1
    Expected Result: 输出 PASS
    Evidence: .sisyphus/evidence/task-12-e2e-partial.txt

  Scenario: E2E 多拓扑变体对比
    Tool: Bash
    Steps:
      1. python3 -c "
    from cpptlm.topo.layer import TopoLayer
    from cpptlm.topo.variant import TopoVariant, TopoVariantSet
    base = TopoLayer('soc_base')
    for i in range(4): base.add_module(f'cpu{i}', 'CPUTLM')
    base.add_module('mem', 'MemoryTLM')
    vs = TopoVariantSet('soc_variants')
    vs.add_variant(TopoVariant('minimal', base=base))
    extra = TopoLayer('')
    extra.add_module('gpu0', 'GPUTLM')
    big = base.merge(extra)
    vs.add_variant(TopoVariant('with_gpu', base=big))
    all = vs.build_all()
    assert len(all) == 2
    comp = vs.compare()
    assert comp['minimal']['module_count'] == 5
    assert comp['with_gpu']['module_count'] == 6
    print('PASS')
  " 2>&1
    Expected Result: 输出 PASS
    Evidence: .sisyphus/evidence/task-12-e2e-variant.txt
  ```

  **Evidence to Capture**:
  - [ ] task-12-e2e-modular.txt
  - [ ] task-12-e2e-partial.txt
  - [ ] task-12-e2e-variant.txt

  **Commit**: YES
  - Message: `test(topo): add end-to-end tests for all 5 requirements`

---

## Final Verification Wave

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists. Check all 5 user requirements are met.
  - Evidence files exist in `.sisyphus/evidence/`
  - All TDD tests defined as tasks exist in test files

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run all Python tests: `python3 -m pytest test/python/test_topo_*.py -v --tb=short 2>&1`
  No code smells: no bare `except:`, no unused imports, no commented-out code

- [x] F3. **Real Manual QA** — `unspecified-high`
  Execute EVERY QA scenario from EVERY task. Test cross-task integration.

- [x] F4. **Scope Fidelity Check** — `deep`
  Verify: all 5 user requirements implemented, no scope creep beyond Must NOT Have guardrails

---

## Commit Strategy

- **1**: `chore(topo): create cpptlm/topo package structure`
- **2-3**: `feat(topo): add TopoLayer core with ModuleSpec/ConnectionSpec/CoherenceDomainSpec`
- **4-5**: `feat(topo): add TopoPatch with glob-based Selector and 4 actions`
- **6-7**: `feat(topo): add TopoVariant + TopoVariantSet for multi-variant generation`
- **8-9**: `feat(topo): add TopoOrchestrator with save/load/template/variant/NoC operations`
- **10-11**: `feat(topo): add CLI with gen/variant/patch/compare subcommands`
- **12**: `test(topo): add end-to-end tests for all 5 requirements`

---

## Success Criteria

### Verification Commands
```bash
# 单元测试
python3 -m pytest test/python/test_topo_layer.py -v --tb=short
python3 -m pytest test/python/test_topo_patch.py -v --tb=short
python3 -m pytest test/python/test_topo_variant.py -v --tb=short
python3 -m pytest test/python/test_topo_orchestrator.py -v --tb=short
python3 -m pytest test/python/test_topo_cli.py -v --tb=short
python3 -m pytest test/python/test_topo_e2e.py -v --tb=short

# 全部一次通过
python3 -m pytest test/python/test_topo_*.py -v --tb=short

# CLI 可用性
python3 -m cpptlm.topo.cli gen --help
```

### Final Checklist
- [ ] 所有 19 个 TopoLayer 测试通过
- [ ] 所有 16 个 TopoPatch 测试通过
- [ ] 所有 11 个 TopoVariant 测试通过
- [ ] 所有 12 个 TopoOrchestrator 测试通过
- [ ] 所有 6 个 CLI 测试通过
- [ ] 所有 5 个 E2E 测试通过
- [ ] CLI 命令显示帮助信息
- [ ] 5 个用户需求均有对应测试覆盖