# ADR-LIB-01: cpptlm.library Python 高级复合 Cluster 工厂 API

> **版本**: 1.0
> **日期**: 2026-06-23 (追溯补 — 实施早于 ADR)
> **状态**: ✅ 已实施 (commit `140fffd`, F5 阶段)
> **影响**: Python 用户一键生成 2-level CPU / 多通道 Memory / 4-level GPU 拓扑 JSON, 配合 `CxxCompatibleEmitter` 直接喂给 C++ `ModuleFactory`
> **前置依赖**: P1.5 GPU cu_template 完整传播 (`e8c2a97`)

---

## 1. Context (背景)

### 1.1 阶段背景

Phase 7 阶段 (2026-06 起) `cpptlm` Python 库已建立 3 个层级:

| 层级 | 抽象 | 用户面对的复杂度 |
|------|------|:---:|
| `cpptlm.topo` | 通用 TopoLayer/TopoPatch/TopoVariant | 高 — 用户须手写 modules/connections/sublayers |
| `cpptlm.library.standard` (基础) | `cpu_l1_cluster` / `memory_cluster` / `crossbar_cluster` 3 个扁平工厂 | 中 — 1-level cluster |
| `cpptlm.library.standard` (高级, **本 ADR**) | `cpu_nested_cluster` / `memory_cluster_hierarchical` / `gpu_topology` 3 个复合工厂 | **低** — 1 行代码生成 2/4-level 嵌套 |

F5 之前, Python 用户生成 2-level CPU cluster 需 ~30 行 `add_module`/`add_connection`/`add_sublayer` 嵌套代码; GPU 4-level 需 ~80 行 (GpuCluster → GpcCluster → TpcCluster → ComputeCluster)。F5 之前没有"高级工厂", 仅有扁平 `cpu_l1_cluster` (1-level: N cores + 1 shared L1), 实际仿真需 2-level (L1xN + L2 shared)。

### 1.2 P1.5 GPU cu_template 透传 — 工厂的前置依赖

GPU 4-level 工厂 (`gpu_topology`) 依赖 P1.5 (commit `e8c2a97`) 的 cu_template 完整传播:
- P1.5 之前: `cu_template` 参数仅传到 ComputeCluster 层, GpuCluster/GpcCluster/TpcCluster 收到空值
- P1.5 之后: 4 级完整透传, `gpu_topology(cu_template=...)` 才能正确生成 GpuCluster → GpcCluster → TpcCluster → ComputeCluster 链

### 1.3 模块字段语义约束

`openspec/changes/field-name-unification` (2026-06-17) 规定:
- **`params`** — 模块参数 dict 的规范位置 (ModuleSpec.params → emitted JSON `"params": {...}`)
- **`config`** — **仅**用于外部配置文件路径 (string), 适用于 `CpuCluster` 等 SimModule 加载独立 JSON
- `params={...}` 放 `config={...}` 字段触发 C++ 端 `LINT005` 错误

F5 工厂全部遵循此约束 — 所有模块参数用 `params` 字段 (通过 `l.add_module(name, type, **kwargs)` → 内部映射到 `params`)。

### 1.4 deprecated 范围 (ADR 范围澄清)

F5 **不**涉及:
- ❌ `cpptlm_config.*` (旧 Pydantic 库, 每次 import 触发 `DeprecationWarning`) — 合并迁移在 `openspec/changes/unified-config-emitter/`
- ❌ `cpptlm.config.topologies` (MeshTopology/RingTopology/CrossbarTopology, 依赖已删除的 `scripts.topology_generator` backend) — 保留仅为向后兼容, 新代码用 `cpptlm.library.mesh_cluster`

---

## 2. Decision (决策)

### 2.1 决策表

| 决策 | 选择 | 备选 | 理由 |
|------|------|------|------|
| **API 形式** | **3 个独立工厂函数 (返回 `TopoLayer`)** | 类继承 / Fluent API / 类方法 | 与 `cpu_l1_cluster` 等扁平工厂一致, 学习成本 0; 退化为单行 `soc.add_cluster(gpu_topology(...))` 也能工作 |
| **嵌套实现** | **`add_sublayer` 递归构造** | 单层 emit + JSON patch | `TopoLayer` 设计原意是递归嵌套; patch 不解决"4-level 蓝图需 4 层 sublayer"问题 |
| **命名** | **`cpu_nested_cluster` / `memory_cluster_hierarchical` / `gpu_topology`** | `make_cpu_2level` / `make_memory_multi_channel` / `make_gpu_4level` | 后者太长, 不符合 Python PEP 8; 前者语义清晰 ("nested" = 嵌套, "hierarchical" = 多层, "topology" = 拓扑) |
| **默认参数** | **保守 (2 cores / 2 GPC / 2 TPC / 4 channels)** | 激进 (8 cores / 4 GPC / 4 TPC / 8 channels) | 与现有 `configs/templates/cpu_cluster_2level.json` / `compute_unit_v1.json` 对齐; 用户按需 expand |
| **cu_template** | **必填参数 + 默认 `configs/templates/compute_unit_v1.json`** | 隐式 (从 C++ 端 default) | Python 端显式传避免 C++ 默认路径漂移; 强制用户知道有蓝图文件 |
| **`_check_reserved` 防御** | **factory 内 `RESERVED` 集合校验** | 不校验 | 防止模块名与 reserved 字段 (groups/modules/connections/hierarchy/...) 冲突, 提前 fail-fast |

### 2.2 3 个工厂 API 详细

#### 2.2.1 `cpu_nested_cluster()`

```python
def cpu_nested_cluster(
    num_cores: int = 2,
    l1_count: int = 2,
    l1_size: str = "16KB",
    l2_size: str = "256KB",
    cluster_id: str = "cpu0",
    name: str = "cpu",
) -> TopoLayer:
    """2-level CPU cluster: CpuCluster + CacheCluster sublayer (L1xN + L2)."""
```

**生成结构**:
- 顶层: 1 个 `CpuCluster` 模块 (参数: `num_cpus`, `cluster_id`)
- inner sublayer:
  - N 个 `CPUTLM` (`{cluster_id}_cpu0` ... `{cluster_id}_cpu{N-1}`)
  - 1 个 `CacheCluster` sublayer (`{cluster_id}_l2cache`) 含 L1xN + 1 L2
- connections: `cpu{i} → l2cache.l1_{i % l1_count}` (round-robin L1 distribution)

**对齐**: `configs/templates/cpu_cluster_2level.json`

#### 2.2.2 `memory_cluster_hierarchical()`

```python
def memory_cluster_hierarchical(
    channels: int = 4,
    channel_size: str = "1GB",
    memory_type: str = "HBM",
    name: str = "mem",
) -> TopoLayer:
    """多通道 hierarchical Memory cluster: N MemoryTLM + Arbiter."""
```

**生成结构**:
- 顶层: 1 个 `MemoryCluster` 模块 (参数: `channel_count`, `channel_size`, `memory_type`)
- inner sublayer:
  - N 个 `MemoryTLM` (`{name}_channel0` ... `{name}_channel{N-1}`)
  - 1 个 `ArbiterTLM` (`{name}_arbiter`, `n_ports=channels`)

**对齐**: C++ `MemoryCluster` P5 模式 (P2 实施)

#### 2.2.3 `gpu_topology()`

```python
def gpu_topology(
    gpc_count: int = 2,
    tpc_per_gpc: int = 2,
    cu_per_tpc: int = 2,
    cu_template: str = "configs/templates/compute_unit_v1.json",
    name: str = "gpu",
) -> TopoLayer:
    """4-level GPU topology: GpuCluster → N×GpcCluster → M×TpcCluster → cu_count×ComputeCluster."""
```

**生成结构**:
- 顶层: 1 个 `GpuCluster` 模块 (参数: `gpc_count`, `tpc_per_gpc`, `cu_per_tpc`, `cu_template`)
- inner sublayer: `gpc_count` × GpcCluster (each: `tpc_per_gpc` × TpcCluster (each: `cu_per_tpc` × ComputeCluster))
- 所有 4 级均传 `cu_template` (P1.5 透传)

**对齐**: `configs/templates/gpu_2gpc_2tpc_2cu.json` (`cu_count = gpc_count × tpc_per_gpc × cu_per_tpc`)

### 2.3 实施概览

**新代码** (commit `140fffd`, 7 files, +694 LOC):

1. **`cpptlm/library/standard.py`** (+186 LOC):
   - `cpu_nested_cluster()` 2-level 工厂 (L72-127)
   - `_cache_cluster_subcluster()` 内部 helper (L130-139)
   - `memory_cluster_hierarchical()` 多通道工厂 (L142-189)
   - `gpu_topology()` 4-level 工厂 (L192-249)
   - `RESERVED` 字段集合 + `_check_reserved()` 防御

2. **`cpptlm/library/__init__.py`** (+6 LOC):
   - 导出 3 个新工厂到 `__all__`

3. **`test/python/test_library.py`** (+102 LOC):
   - 3 个 TestCase 类, 9 个 test method:
     - `TestCpuNestedCluster`: test_default_2core / test_4core_with_l1_l2 / test_inner_connections_route_to_l1
     - `TestMemoryClusterHierarchical`: test_4_channels / test_inner_channels_and_arbiter
     - `TestGpuTopology`: test_2gpc_2tpc_2cu_default / test_cu_template_passthrough / test_4level_cu_count_8

4. **`examples/generate_library_configs.py`** (+85 LOC):
   - 命令行工具: `--demo cpu|memory|gpu` 或 `--all`
   - 生成 `configs/library_{name}_demo.json` (3 个 JSON 配置文件, 315 行)

5. **`configs/library_cpu_demo.json`** (+80 LOC): 示例产物
6. **`configs/library_memory_demo.json`** (+69 LOC): 示例产物
7. **`configs/library_gpu_demo.json`** (+166 LOC): 示例产物

### 2.4 默认参数保守性 (与现有模板对齐)

| 工厂 | 默认 | 对应模板 |
|------|------|----------|
| `cpu_nested_cluster` | 2 cores / 2 L1 / 16KB L1 / 256KB L2 | `cpu_cluster_2level.json` |
| `memory_cluster_hierarchical` | 4 channels / 1GB / HBM | `MemoryCluster` P5 模式 |
| `gpu_topology` | 2 GPC / 2 TPC / 2 CU / `compute_unit_v1.json` | `gpu_2gpc_2tpc_2cu.json` |

---

## 3. Consequences (后果)

### 3.1 解锁的能力

- ✅ **Python 用户 API 收敛**: 1 行代码生成 2/4-level 嵌套拓扑, 避免 ~30-80 行 `add_module`/`add_connection`/`add_sublayer` 嵌套
- ✅ **cu_template 完整传播**: GPU 4-level 工厂自动传 `cu_template` 到所有 4 级 (P1.5 集成)
- ✅ **JSON 产物可直接喂 C++**: `CxxCompatibleEmitter.to_json()` 输出与 C++ `ModuleFactory` 完整兼容
- ✅ **`_check_reserved` 防御**: 工厂 fail-fast 避免模块名与 reserved 字段冲突
- ✅ **F1 P2 补测覆盖**: 9 个 test method 全 pass (pytest 230/230)

### 3.2 已知技术债

- ⚠️ **`cpu_nested_cluster` 默认 l1_count=2 与 num_cores=2 强耦合**: 若用户传 `num_cores=4, l1_count=2` 会 round-robin, 需文档明示 (`cpu{i} → l1_{i % l1_count}`)
- ⚠️ **`memory_cluster_hierarchical` 不支持 ECC / rank 配置**: 当前只支持 N channels + 1 arbiter, 实际 HBM 需 rank/bank 参数 (P2+ 扩展)
- ⚠️ **`gpu_topology` 不支持不同 GPC 配置**: 当前所有 GPC 同构 (相同 `tpc_per_gpc`/`cu_per_tpc`), 异构 GPU 需手动 patch
- ⚠️ **`RESERVED` 集合与 `cpptlm.topo` 内部字段可能漂移**: 若 `TopoLayer` 新增 reserved 字段, `_check_reserved` 需同步更新
- ⚠️ **`generate_library_configs.py` 生成的 JSON 需手动测试加载**: 当前未做 CI 集成 (F10/F15+ 可加 GitHub Action)

### 3.3 Phase 7+ 扩展点

- **F15 (Phase 7.F Full APU Demo)**: `gpu_topology()` + `cpu_nested_cluster()` + `memory_cluster_hierarchical()` 三者组合 → `apu_full_soc.json`
- **Phase 7.D (F13) TCC Bridge**: `memory_cluster_hierarchical` 可加 `tcc_bridge` 参数 → TCC-aware memory
- **Phase 7.E (F14) Multi-CU**: `gpu_topology` 可加 `multi_cu_per_tpc: bool` 切到 ComputeUnitTLM 数组模式
- **cpptlm 0.2+**: 工厂可加 `validate=True` 选项 → emit 后立即 `ModuleFactory.load()` round-trip 验证

---

## 4. References (参考)

### 4.1 设计来源

- **`docs/superpowers/plans/2026-06-20-future-work-roadmap.md` F5** — cpptlm.library Python 高级工厂函数 (本 ADR 实施目标)
- **`cpptlm/AGENTS.md`** — cpptlm 包结构 + 核心抽象 + 推荐用法
- **`openspec/changes/field-name-unification/`** (2026-06-17) — `params` vs `config` 字段语义约束

### 4.2 关联 ADR

- **`ADR-INC-01-incorporate-parent-late-binding.md`** — P1 阶段基础 (ApuSoC 真实 late-binding wiring, 工厂生成的 JSON 才能被 wiring)
- **`ADR-METRIC-01-cputlm-cache-memory-telemetry.md`** — F10 telemetry 集成 (工厂生成的 JSON 可立即被 telemetry 收集)

### 4.3 实施追溯

- **F5 commit `140fffd`** — cpptlm.library Python 高级工厂 (本 ADR 实施, 2026-06-23)
- **P1.5 commit `e8c2a97`** — GPU cu_template 完整传播 (`gpu_topology` 前置依赖)

### 4.4 关联任务

- `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` F5 (本 ADR) + F15 (Phase 7.F Demo 集成) + F1.P2.3 (l1_size/l2_size 透传验证)

---

## 5. Status Update

无（首次签发即已实施完成, 追溯补 ADR 仅为文档化, 设计决策与实施一致）。
