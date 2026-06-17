# Unified Config Emitter — Design

## Context

CppTLM 项目当前有 3 套并行的 Python 配置生成路径，产生不同形态的 JSON：

| 路径 | 风格 | 输出字段 | C++ 是否消费 |
|---|---|---|---|
| `cpptlm_config/` (旧) | Pydantic | `module_groups` 数组 | ❌ 静默忽略 |
| `cpptlm/topo/` (新) | dataclass | `sublayers` 字段 | ❌ 不识别 |
| `examples/generate_*.py` | 直接 dict | `hierarchy` + `groups` + `coherence_domains` | ✅ 全部识别 |

C++ `ModuleFactory` 真正消费的 schema 字段（见 `src/core/module_factory.cc:125-744`）：
- 顶层：`modules`、`connections`、`groups`（**dict 形式**）、`hierarchy`（**顶层对象树**）、`coherence_domains`、`extends`、`include`、`plugin`、`metadata`
- 模块：`name`、`type`、`params`、`layout`、`port_spec`、`config`
- 连接：`src`、`dst`、`latency`、`exclude`、`buffer_sizes`、`vc_priorities`，加 `group:`/`regex:`/`wildcard:*` 前缀

`module_groups` 数组出现在 5 个 JSON 中（旧 demo），C++ 静默忽略——典型的"文档-实现-数据"三不一致。
3 处 `from scripts.topology_generator import` 引用了不存在的模块。

用户决策：JSON 保持最小化（不引入 `tag`/`role`/`metadata` 等新字段），所有复杂逻辑（cluster 复用、annotation、tag）在 Python 端处理，emit 时展开为 C++ 已识别的字段。

## Goals / Non-Goals

**Goals:**
- 提供 `CxxCompatibleEmitter` 把 Python `TopoLayer` 树展开为 C++ 可加载 JSON
- 提供 `cpptlm/library/` 工厂库消除手写重复
- 扩展 `TopoLayer` 增 `tag()` / `clone()` / `layout_grid()` 三个方法
- 清理 5 个 stale `module_groups` JSON → `groups` dict
- 修复 3 处 dead import
- `cpptlm_config` 标记为 Legacy（仍可用，DeprecationWarning）
- `coherence_domains` 标记为 stub（C++ 解析但不消费）
- **C++ 端零 schema 变更**

**Non-Goals:**
- 合并 `cpptlm_config/` + `cpptlm/topo/`（推迟到下一期）
- 实际实现 `coherence_domains` 运行时行为
- 收敛 `"params"` vs `"config"` 字段名分裂
- 命名漂移治理（`cpu_N` vs `proc_N` vs `cpus_N`）
- 删除 `cpptlm_config/` 包

## Decisions

### Decision 1: Emitter 输出严格白名单 JSON 字段

**Why**: 用户决策"JSON 保持简单必要字段"，不引入任何新字段。`CxxCompatibleEmitter` 的 `emit()` 方法只输出 C++ 端已识别的 schema 字段，禁止 `module_groups`、`sublayers` 等死字段。

**Alternative considered**:
- (A) 透传所有 Python 内部字段（灵活性高，但 C++ 忽略，噪声大）— **拒绝**
- (B) 严格白名单（仅 `modules`/`connections`/`groups`/`hierarchy`/`coherence_domains`）— **选择**

**白名单**：
```python
ALLOWED_KEYS = {
    "name", "description", "version", "metadata",
    "modules", "connections", "groups", "hierarchy",
    "coherence_domains", "extends", "include", "plugin",
}
```

### Decision 2: Python 端 tag 展平成 `groups` dict（不带独立 `tag` 概念）

**Why**: C++ 端只有 `groups` 字典支持批量连接展开（`group:` 前缀）。Python 端维护 tag 是为了复用便利，emit 时全部展平成 `groups[name] = [members]`，与 C++ 现有机制完全对齐。

**emit 规则**：
- `TopoLayer.tags` 集合（Python 内部）
- `emit()` 时为每个 tag 生成 group，name = tag，members = 该 tag 出现的所有 layer 的本地 module names 之并集（去重，保留插入顺序）
- 跨子层合并行为：当 root 和其子 layer 都打了同名 tag，子 layer 的本地 module names 会合并到 root 的 group 中
- 这一行为对 SoC 的 `add_cluster(...).tag("compute")` 用例至关重要 —— 多个 cluster 共享 tag 时，连接 `group:compute` 可一次性扇出到所有 cluster 的 modules

**Alternative considered**:
- (A) JSON 加 `tags` 数组字段（C++ 改）— **拒绝**（用户决策不改 C++）
- (B) JSON 用 `group` 数组而非 dict（C++ 改）— **拒绝**
- (C) Python tag 展平成 groups dict — **选择**

### Decision 3: Cluster 嵌套展平成 `hierarchy` 树

**Why**: `TopoLayer.sublayers` 是 Python 端递归嵌套抽象。emit 时递归构造 `hierarchy.children`，叶节点 `children: []`（与 `configs/hierarchy_tree_3level.json` 现有用法一致）。

**emit 规则**：
```python
def _emit_hierarchy(layer: TopoLayer) -> dict:
    return {
        "name": layer.name,
        "children": [_emit_hierarchy(sub) for sub in layer.sublayers]
    }
```

**Alternative considered**:
- (A) 展平时只在顶层 `hierarchy` 放叶节点（丢失结构）— **拒绝**
- (B) 嵌套递归（保留结构）— **选择**

### Decision 4: hierarchy 绑定校验在 Python 端，不改 C++

**Why**: C++ `topology_parser.cc:106-111` 的 `parse_hierarchy_tree_with_validation` 当前**完全忽略** `coherence_json` 参数。若 C++ 强校验 `children[].name ⊆ modules[].name`，现有 4 个 APU 配置 + `hierarchy_tree_3level.json` 会立即失败。Python 端 emit 时校验，错误在生成阶段捕获。

**Python 校验规则**：
```python
def _validate_hierarchy_binding(root: TopoLayer) -> None:
    module_names = {m.name for m in root._all_modules()}
    for h_node in _walk_hierarchy(root):
        if h_node["name"] not in module_names:
            raise TopoEmitError(
                f"hierarchy node '{h_node['name']}' has no matching module"
            )
```

**Alternative considered**:
- (A) C++ 强校验（破坏现有配置）— **拒绝**
- (B) Python 端校验 — **选择**
- (C) 不校验（hierarchy 独立命名空间）— **拒绝**（用户希望人阅读时一致）

### Decision 5: `cpptlm_config` 不删除，加 DeprecationWarning

**Why**: 用户决策 2B（合并推迟到下一期）。`cpptlm_config` 仍被 7 个旧 `examples/generate_*.py` 引用。直接删除会破坏现有 demo 流程（虽然 4 个 demo 用的是 `module_groups` 死字段）。最小破坏路径：
- `cpptlm_config/__init__.py` 顶部 import-time warning
- `cpptlm_config/builder.py` 写 `module_groups` 字段时 warn
- 行为不变，旧 demo 仍可运行（带 warning）

**下一期**：合并 `cpptlm_config` → `cpptlm/topo/`，删除旧包。

### Decision 6: `cpptlm/library/` 独立包（不放 `topo/` 子包）

**Why**: 用户决策 9B。"library" 在语义上与"topo"（核心抽象）平级，独立包给社区贡献留口子。

**包结构**：
```
cpptlm/library/
├── __init__.py          # 导出
├── standard.py          # CPU+L1+Memory 基础 cluster
├── interconnect.py      # Crossbar/Mesh/Ring cluster
└── soc.py               # SoC 顶层组合器
```

**Alternative considered**:
- (A) 放 `cpptlm/topo/library.py`（单文件）— **拒绝**（不利于扩展）
- (B) 独立 `cpptlm/library/` 包 — **选择**

### Decision 7: `coherence_domains` 标 stub，仅文档化

**Why**: C++ 端 `parse_hierarchy_tree_with_validation` 接收 `coherence_json` 但**完全不用**。`module_factory.cc` 流程没有 `CoherenceDomain` 实例化逻辑。完整实现需要 2-4 周（涉及 cache 协议 + snoop filter）。本期只加 stub 日志和文档标记，不实现功能。

**C++ 改动**（`src/core/topology_parser.cc:106-111`）：
```cpp
if (!coherence_json.is_null() && !coherence_json.empty()) {
    DPRINTF(PARSER, "[STUB] coherence_domains 字段当前为 stub, 解析但不消费\n");
}
```

**ADR**：`docs/adr/ADR-X.14-coherence-domains-stub.md` 记录此决策。

### Decision 8: `include_chain_demo.json` 归档

**Why**: 该文件用 `"$include": [{...}]`（数组内联语法），C++ 端只识别 `"include": "<path>"` 字符串语法。当前是 stale demo，**不能被 C++ 实际加载**。归档到 `docs-archived/dead-configs-2026-q2/`（与 `2026-05-26-cleanup-redundant-configs` 一致的归档目录）。

**验证**：归档前 grep 全项目 `include_chain_demo` 引用，除 `configs/AGENTS.md` 一处提及外无其他引用。

### Decision 9: `placement` 不写入 JSON（Python-only 概念）

**Why**: C++ `module_factory.cc:297-305` 只读取 `groups[name] = [members]` flat 形式，不解析 `placement` 子字段。`topology_dumper` 是**独立的可视化工具**，与 ModuleFactory 实例化路径无关。emit 时不写入 `placement`，避免给用户造成"C++ 会消费此字段"的误解。

**后续路径**：如果未来 `topology_dumper` 需要从 JSON 读取 placement，应在 `groups[name]` 内增加嵌套 dict 结构（C++ 端同步更新 schema）。当前阶段 `placement` 仅作为 Python 端 `.dot` 生成器的内部 metadata。

**可由用户覆盖**：未来通过 `SoC.add_cluster(...).set_placement("radial")` API 显式设置（待后续迭代）。

## Risks / Trade-offs

[Risk] `TopoLayer.clone()` 嵌套深时 deepcopy 性能 → Mitigation: 限制最大嵌套深度（8 层）超限报错

[Risk] 5 个 JSON 迁移后 `extends` 合并行为可能与原来不同 → Mitigation: Phase 4.7 强制 C++ smoke test，失败立即回滚

[Risk] `cpptlm_config` deprecation warning 刷屏 pytest 输出 → Mitigation: `conftest.py` 加 `filterwarnings("ignore::DeprecationWarning, module:cpptlm_config")`

[Risk] `include_chain_demo.json` 归档后若有测试引用 → Mitigation: Phase 4.6 前 grep 全项目 `include_chain_demo`

[Risk] `docs/user-guide/python-usage.md §3.5` 重写破坏读者 muscle memory → Mitigation: 顶部加 changelog note "Module_groups 已废弃，请用 groups"

[Risk] emitter 输出的 JSON 与 C++ `groups` 字段命名冲突（用户 module name = "groups"）→ Mitigation: 启动时校验 `__reserved_module_names__ = {"groups", "modules", "connections", ...}`

[Risk] 端到端 round-trip 测出未发现的 schema 漂移 → Mitigation: Phase 8 强制 C++ sim smoke test 通过

[Risk] 19 个 `layout.x/y` 自动计算的 cluster 在某些拓扑下布局难看 → Mitigation: 用户可用 `module.metadata["layout"] = {"x": ..., "y": ...}` 覆盖

## Migration Plan

### Rollout 顺序

1. **Phase 1+6+4.1-4.2 并行**（无依赖）：
   - 扩展 `TopoLayer` API
   - 加 stub log 到 C++
   - 修 3 处 dead import + 加 deprecation warning

2. **Phase 2+3**（依赖 Phase 1）：
   - 创建 `CxxCompatibleEmitter`
   - 创建 `cpptlm/library/` 工厂库

3. **Phase 4.3-4.7**（独立）：
   - 迁移 5 个 JSON
   - 归档 `include_chain_demo.json`
   - C++ smoke test

4. **Phase 7**（依赖 Phase 2+3 完成）：
   - 写端到端 demo
   - 更新所有 AGENTS.md
   - 重写 `docs/user-guide/python-usage.md §3.5`
   - 更新 `docs/ONBOARDING.md`

5. **Phase 8**：全量验证

### Rollback 策略

- `cpptlm_config` 仍工作（不删除）→ emitter 误改可禁用 `from cpptlm.topo.emitter import`
- 5 个 JSON 迁移后若有 `extends` 行为变化 → git revert 该 JSON 即可
- 归档 `include_chain_demo.json` → git checkout 可恢复
- `coherence_domains` stub log 可注释掉

### Backward Compatibility

- **C++ 端零变更** → 所有现有 `cpptlm_tests` 610/610 pass
- `cpptlm_config` 加 warning 但 API 不变 → 旧 demo 仍可运行
- 现有 5 个 `module_groups` JSON 迁移后行为不变（`groups` 是更精确表达）
- `TopoLayer` 新增 3 个方法，不修改任何现有方法 → 74 个旧测试全 pass

## Open Questions

无。所有关键决策已在用户审阅过程中拍板。
