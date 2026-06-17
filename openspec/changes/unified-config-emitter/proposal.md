# Unified Config Emitter

## Why

CppTLM 当前存在 **3 套并行的 Python 配置生成路径**，产生不同形态的 JSON，且其中两套的输出字段 C++ 端完全不识别：

1. `cpptlm_config/`（旧 Pydantic）：输出 `module_groups` 数组 — C++ 静默忽略
2. `cpptlm/topo/`（新 dataclass）：输出 `sublayers` 字段 — C++ 不识别
3. `examples/generate_*.py`（直接 dict）：输出 `hierarchy` + `groups` + `coherence_domains` — C++ 全部识别，但缺乏抽象和复用

用户想要"可读 JSON + 可复用 cluster + Python 灵活生成 SoC"，但缺少统一路径。

本变更在**不改 C++ schema**的前提下，新增 `CxxCompatibleEmitter` 把 Python `TopoLayer` 树展开为 C++ 现有可识别的 JSON（`modules` + `connections` + `groups` + `hierarchy` + `coherence_domains`），并配套 `cpptlm/library/` 工厂库消除手写重复。同时清理 `module_groups` 死字段、修复 3 处 dead import。

## What Changes

### 新增能力

- **CxxCompatibleEmitter**（`cpptlm/topo/emitter.py`）：将 `TopoLayer` 树展开为 C++ ModuleFactory 可加载的 JSON dict。核心规则：
  - Python 端 `tag` 全部展平成顶层 `groups` 字典（与 C++ 现有 `group:` 前缀配合）
  - Cluster 嵌套展平成顶层 `hierarchy` 树
  - `layout` 坐标直接写入 `modules[].layout`（已有字段）
  - `params` 注入走 `modules[].params`（已有字段）
  - `placement: "grid"` 默认值写入 `groups[name].placement`（C++ `topology_dumper` 已支持）
- **cphtlm/library 工厂库**（`cpptlm/library/`）：预置 cluster 模板，消除手写重复
  - `standard.py`: `cpu_l1_cluster()`, `memory_cluster()`, `crossbar_cluster()`
  - `interconnect.py`: `mesh_cluster()`, `ring_cluster()`
  - `soc.py`: `SoC` 顶层组合器（含 `connect_group()` / `save()`）

### TopoLayer 扩展

`cpptlm/topo/layer.py` 增加 3 个方法（**不破坏现有 74 个测试**）：
- `tag(*tags) -> TopoLayer`：链式添加 Python 端 tag（不进入 JSON）
- `clone(prefix=None) -> TopoLayer`：深拷贝，可选 name 前缀重命名
- `layout_grid(dx, dy, x_offset=0, y_offset=0) -> TopoLayer`：自动算 `layout.x/y`

### 死字段清理（合并到本期）

- **5 个 stale JSON** 的 `module_groups` 数组迁移为 `groups` dict：
  - `configs/soc_cluster_a.json`
  - `configs/soc_cluster_b.json`（验证 `extends` 合并正确）
  - `examples/demo_configs/soc_cluster.json`
  - `examples/demo_configs/dual_cluster_soc.json`（验证 `extends`）
  - `configs/include_chain_demo.json` 同步检查
- **`include_chain_demo.json` 归档**：C++ 不识别 `$include` 数组语法，移动到 `docs-archived/dead-configs-2026-q2/`
- **3 个 Python 文件的 dead import** 修复：
  - `cpptlm_config/topology_adapter.py`
  - `cpptlm/config/topologies.py`
  - `cpptlm/config/generator.py`
- **`cpptlm_config` 标记为 Legacy**：
  - `cpptlm_config/__init__.py` 顶部加 `DeprecationWarning`
  - `cpptlm_config/builder.py` 写 `module_groups` 字段时 warn（仍写，向后兼容）
  - `cpptlm_config/models.py` 字段加 deprecation 注释
  - `cpptlm_config/AGENTS.md` 状态更新

### coherence_domains 标记为 stub

C++ `topology_parser.cc:106-111` 解析 `coherence_domains` 但 `parse_hierarchy_tree_with_validation` 丢弃该参数。加 stub 日志。详见 ADR-X.14。

### 文档同步

- `cpptlm/AGENTS.md` 新建（当前缺失）
- `cpptlm_config/AGENTS.md` 状态更新
- `configs/AGENTS.md` 核心 schema 例子更新 + 归档说明
- `docs/user-guide/python-usage.md §3.5` 重写（`module_groups` → `groups`）
- `docs/ONBOARDING.md §5` 新增 emitter 入门段
- `docs/adr/ADR-X.14-coherence-domains-stub.md` 新建

## Capabilities

### New Capabilities

- `config-emitter`: Python `TopoLayer` 树 → C++ ModuleFactory 兼容 JSON 的展开规则集
- `cluster-factory`: 预置 SoC cluster 模板库（CPU+L1、Crossbar、Mesh、Memory 等）

### Modified Capabilities

无（现有 specs 均为 C++ 端实现细节，本变更不修改 C++ 端行为或 schema）。

## Impact

### 新增文件

- `cpptlm/topo/emitter.py`（~150 行）
- `cpptlm/library/__init__.py`（~20 行）
- `cpptlm/library/standard.py`（~80 行）
- `cpptlm/library/interconnect.py`（~60 行）
- `cpptlm/library/soc.py`（~80 行）
- `cpptlm/AGENTS.md`（~30 行，新建）
- `examples/generate_via_emitter.py`（~80 行）
- `configs/example_emitter_soc.json`（~50 行，生成产物）
- `test/python/test_topo_emitter.py`（~10 用例）
- `test/python/test_library.py`（~6 用例）
- `docs/adr/ADR-X.14-coherence-domains-stub.md`（~50 行）

### 修改文件

- `cpptlm/topo/layer.py`：+3 方法（~30 行）
- `cpptlm/topo/__init__.py`：导出新类
- `cpptlm_config/topology_adapter.py`：try/except 包 import + warning
- `cpptlm_config/__init__.py`：顶部 DeprecationWarning
- `cpptlm_config/builder.py`：写 `module_groups` 时 warn
- `cpptlm_config/models.py`：`module_groups` 字段 deprecation 注释
- `cpptlm_config/AGENTS.md`：状态段更新
- `cpptlm/config/topologies.py`：try/except 包 import
- `cpptlm/config/generator.py`：try/except 包 import
- `configs/soc_cluster_a.json`：`module_groups` → `groups`
- `configs/soc_cluster_b.json`：验证 `extends` 合并
- `examples/demo_configs/soc_cluster.json`：`module_groups` → `groups`
- `examples/demo_configs/dual_cluster_soc.json`：验证
- `src/core/topology_parser.cc:106-111`：加 stub 日志（3 行）
- `configs/AGENTS.md`：schema 例子 + 归档说明
- `docs/user-guide/python-usage.md`：§3.5 重写
- `docs/ONBOARDING.md`：§5 新增 emitter 段
- `test/python/test_topo_layer.py`：+5 用例

### 归档文件

- `configs/include_chain_demo.json` → `docs-archived/dead-configs-2026-q2/include_chain_demo.json`

### 不修改

- 现有 31 个 `configs/*.json` 中 26 个（除上述 4 个迁移 + 1 个归档外）
- 现有 7 个 `examples/generate_*.py`
- 现有 610 个 C++ 测试
- 现有 207 个 Python 测试（除 3 个新增/扩展外）
- C++ ModuleFactory schema（**零字段变更**）

### 依赖

- 仅 Python 3.10+ 和已有依赖（无新 PyPI 依赖）
- 不引入新的 C++ 依赖
