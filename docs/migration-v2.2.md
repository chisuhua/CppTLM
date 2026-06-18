# CppTLM v2.2 迁移指南 (Migration Guide)

> **适用对象**: CppTLM v2.1.x → v2.2.0 升级用户
> **变更类型**: **BREAKING** — 涉及模块类型退场、include 路径变更、CMake 选项移除
> **发布日期**: 2026-06-18
> **关联 OpenSpec change**: [`json-nested-simmodule`](../../openspec/changes/json-nested-simmodule/proposal.md)

---

## 1. 概述 (Overview)

v2.2 主要完成三件事：

1. **CPUSim 退场**: 由 `CPUTLM` 替代（`ChStreamModuleBase` 派生，走 `REGISTER_CHSTREAM`）
2. **`include/modules/legacy/` 目录删除**: 整个 legacy 模块子树已不存在
3. **`BUILD_LEGACY_MODULES` CMake 选项删除**: 不再有 legacy 编译守卫，`CpuCluster` 无条件注册

新增能力：

- `CpuCluster` 增强（`include/tlm/cluster/cpu_cluster.hh`）：override `tick`/`set_config`/`get_module_type`，支持 N 层 JSON 嵌套 + `MAX_DEPTH=8` 限深保护
- SimModule 暴露端口 API（`outputs`/`inputs` 字段）端到端可用
- 多层嵌套 JSON 配置示例（`configs/example_simmodule_nested_{2level,3level_static}.json`）

---

## 2. BREAKING 变更清单 (Breaking Changes)

### 2.1 移除 `BUILD_LEGACY_MODULES` CMake 选项

**v2.1.x**:

```bash
cmake -S . -B build -DBUILD_LEGACY_MODULES=ON   # 启用 legacy 模块
cmake -S . -B build -DBUILD_LEGACY_MODULES=OFF  # 默认, 禁用 legacy 模块
```

**v2.2.0**:

```bash
cmake -S . -B build   # 无 BUILD_LEGACY_MODULES 选项
```

**迁移步骤**:

- 删除 `CMakeLists.txt` / `src/CMakeLists.txt` 中所有 `BUILD_LEGACY_MODULES` 引用
- 无需修改 `include/modules.hh`（v2.2 已无条件注册 `CpuCluster`）

---

### 2.2 移除 `include/modules/legacy/` 目录

**v2.1.x 路径**:

```
include/
└── modules/
    └── legacy/
        ├── cpu_sim.hh        # CPUSim
        ├── cpu_cluster.hh    # CpuCluster
        └── README.md
```

**v2.2.0 路径**:

```
include/
└── tlm/
    └── cluster/
        ├── cpu_cluster.hh    # CpuCluster (从 legacy 移出, 已增强)
        └── cpu_cluster.cc    # 实现
```

**迁移映射**:

| v2.1.x 路径 | v2.2.0 路径 | 备注 |
|-------------|-------------|------|
| `include/modules/legacy/cpu_sim.hh` | **删除** | `CPUSim` 已由 `CPUTLM` 替代 |
| `include/modules/legacy/cpu_cluster.hh` | `include/tlm/cluster/cpu_cluster.hh` | 内容已重写, 增强 override |
| `include/modules/legacy/README.md` | **删除** | 目录整体退场 |

---

### 2.3 移除 `CPUSim` 类型

**v2.1.x**:

```cpp
#include "modules/legacy/cpu_sim.hh"
#include "modules/legacy/cpu_cluster.hh"

CPUSim* cpu = new CPUSim("cpu0", eq);
cpu->tick();
```

**v2.2.0**:

```cpp
#include "tlm/cpu_tlm.hh"
#include "tlm/cluster/cpu_cluster.hh"

// CPUTLM 替代 CPUSim
CPUTLM* cpu = new CPUTLM("cpu0", eq);
cpu->tick();

// CpuCluster 新位置
CpuCluster* cluster = new CpuCluster("cluster0", eq);
```

**功能对比**:

| 维度 | CPUSim (v2.1) | CPUTLM (v2.2) |
|------|---------------|---------------|
| 基类 | `SimObject` (PortPair 通信) | `ChStreamModuleBase` (Bundle+StreamAdapter 通信) |
| 注册 | `REGISTER_OBJECT` | `REGISTER_CHSTREAM` |
| 端到端响应观测 | 不支持 | `last_response_transaction_id()` |
| 端到端测试 | N/A | 641 C++ 用例 + 220 Python 用例 |
| 推荐度 | **DEPRECATED** | **推荐** |

**JSON 配置迁移**:

```diff
- { "name": "cpu0", "type": "CPUSim", "params": { ... } }
+ { "name": "cpu0", "type": "CPUTLM", "params": { ... } }
```

**所有引用点替换**:

- 测试代码 (`test/*.cc`)
- 示例代码 (`examples/*.cc`)
- JSON 配置 (`configs/*.json`、历史归档 `docs-archived/`)

---

### 2.4 路径替换速查表 (Path Replacement Cheatsheet)

| v2.1.x | v2.2.0 |
|--------|--------|
| `#include "modules/legacy/cpu_sim.hh"` | `#include "tlm/cpu_tlm.hh"` |
| `#include "modules/legacy/cpu_cluster.hh"` | `#include "tlm/cluster/cpu_cluster.hh"` |
| `type: "CPUSim"` (JSON) | `type: "CPUTLM"` (JSON) |
| `CPUSim` (C++ class) | `CPUTLM` |
| `CpuCluster` (旧 `cpu_cluster.hh` 实现) | `CpuCluster` (新 `tlm/cluster/cpu_cluster.hh` 实现, API 兼容) |
| `BUILD_LEGACY_MODULES=ON` | (删除, 默认行为) |
| `BUILD_LEGACY_MODULES=OFF` | (删除, 默认行为) |

---

## 3. 新增能力 (New Features)

### 3.1 CpuCluster 增强

**位置**: `include/tlm/cluster/cpu_cluster.hh` + `include/tlm/cluster/cpu_cluster.cc`

**派生关系**: `CpuCluster : public SimModule`

**Override 方法**:

- `std::string get_module_type() const override` — 返回 `"CpuCluster"`
- `void set_config(const nlohmann::json& params) override` — 解析 `num_cpus` / `cluster_id`
- `void tick() override` — 转发到 `internal_factory->getAllInstances()`

**新增访问器**:

- `int num_cpus() const` — 当前 `num_cpus_` 值
- `const std::string& cluster_id() const` — 当前 `cluster_id_` 值

**JSON 参数示例**:

```json
{
  "name": "cluster0",
  "type": "CpuCluster",
  "params": {
    "num_cpus": 8,                    // 可选, 默认 4
    "cluster_id": "outer_cluster"     // 可选, 默认 ""
  }
}
```

**注册**: `REGISTER_MODULE` 宏（`include/modules.hh`），无条件注册。

---

### 3.2 SimModule 嵌套 JSON 模式

`SimModule` 派生类支持 JSON 内联 `modules` 数组实现多层嵌套。

**2 层嵌套示例**（`configs/example_simmodule_nested_2level.json`）:

```json
{
  "modules": [
    {
      "name": "cluster0",
      "type": "CpuCluster",
      "params": { "num_cpus": 4, "cluster_id": "outer" },
      "modules": [
        { "name": "cpu0", "type": "CPUTLM", "params": { ... } },
        { "name": "cache", "type": "CacheTLM", "params": { ... } },
        { "name": "mem", "type": "MemoryTLM", "params": { ... } }
      ],
      "connections": [
        { "src": "cpu0", "dst": "cache", "latency": 1 },
        { "src": "cache", "dst": "mem", "latency": 10 }
      ]
    }
  ]
}
```

**3 层嵌套示例**: 见 `configs/example_simmodule_nested_3level_static.json`

**暴露端口模式** (`outputs` / `inputs`):

```json
{
  "name": "cluster0",
  "type": "CpuCluster",
  "modules": [
    { "name": "cpu0", "type": "CPUTLM", ... }
  ],
  "outputs": [
    { "internal": "cpu0.req_out", "external": "cpu0_to_bus" }
  ],
  "inputs": [
    { "internal": "cpu0.resp_in", "external": "bus_to_cpu0" }
  ]
}
```

**限深保护**:

- `SimModule::MAX_DEPTH = 8`（`include/core/sim_module.hh`）
- 超出抛 `std::runtime_error("SimModule::simulate_instantiate depth limit 8 exceeded at path ...")`
- `static thread_local int depth_` 保证多线程仿真独立

---

## 4. 快速迁移检查清单 (Migration Checklist)

```bash
# 1. 全局搜索 CPUSim 引用
git ls-files | xargs grep -l "CPUSim" 2>/dev/null

# 2. 全局搜索 BUILD_LEGACY_MODULES
git ls-files | xargs grep -l "BUILD_LEGACY_MODULES" 2>/dev/null

# 3. 全局搜索 include/modules/legacy
git ls-files | xargs grep -l "include/modules/legacy" 2>/dev/null
```

**预期结果**: 上述搜索仅命中以下 3 处:

1. `CHANGELOG.md`（历史变更说明）
2. `docs/migration-v2.2.md`（本文件）
3. `openspec/changes/json-nested-simmodule/`（change 自身）

其他目录命中 = 未完成迁移。

**编译验证**:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -j4   # 预期 641/641 pass
```

---

## 5. Known Documentation Drift

v2.2 移除 legacy 模块与 `BUILD_LEGACY_MODULES` 后,以下文档仍含过时引用,**未在本次 Stage 7 修改范围内**（计划在后续 change 中清理）:

| 文档 | 位置 | 过期内容 | 建议修复方式 |
|------|------|----------|--------------|
| `AGENTS.md` (根) | §STRUCTURE 注释 (`modules/legacy/` 引用) | 提及 `modules/legacy/CPUSim/CpuCluster` 与 `BUILD_LEGACY_MODULES=OFF` | 替换为 `include/tlm/cluster/` 描述, 移除 `BUILD_LEGACY_MODULES` 守卫说明 |
| `AGENTS.md` (根) | §ANTI-PATTERNS (line 75) | "添加 Legacy 模块 → `include/modules/legacy/`" | 替换为 "添加 SimModule 容器 → `include/tlm/cluster/`" |
| `AGENTS.md` (根) | §归档索引 (line 182) | "`cpu_cluster` DEPRECATED" 描述需更新 | 简化为 v2.2 已迁移到 `include/tlm/cluster/` |
| `docs/ONBOARDING.md` | §2.6 (line 81) | "**Legacy**(`include/modules/legacy/`):`cpu_cluster.hh`、`cpu_sim.hh`" | 删除此行, 指向 `include/tlm/cluster/cpu_cluster.hh` + `include/tlm/cpu_tlm.hh` |
| `docs/ONBOARDING.md` | §9 反模式 (line 436) | "**Legacy 模块** — `include/modules/legacy/` 仅修严重 bug" | 替换为 "**SimModule 容器** — `include/tlm/cluster/` 添加新容器" |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | §4 目录树 (line 449) | "[迁移] include/modules/legacy/" 节点 | 替换为 `include/tlm/cluster/` |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | §CPUSim 引用 (line 120) | "cpu_sim.hh # CPUSim" 节点 | 替换为 `include/tlm/cpu_tlm.hh # CPUTLM` |
| `docs/architecture/02-complex-topology-architecture.md` | §示例代码 (line 125) | `// include/modules/legacy/cpu_cluster.hh` 注释 | 更新为 `// include/tlm/cluster/cpu_cluster.hh` |
| `docs/soc_arch/modules/README.md` | §模块表 (line 38) | `CPUSim` → `include/modules/legacy/cpu_sim.hh` | 标记 DEPRECATED, 指向 `CPUTLM` (`include/tlm/cpu_tlm.hh`) |
| `docs/soc_arch/modules/cpu-cpusim_legacy.md` | 整个文件 | 整个文件以 `cpu_sim.hh` 为核心 | 在文件顶部加 DEPRECATED 横幅, 引导至 `CPUTLM` 文档 |
| `test/README.md` | 多个位置 | 提及 `CPUSim` 或 `BUILD_LEGACY_MODULES` | 后续 change 清理（test/ 不在 Stage 7 修改范围） |

**Stage 7 不修复原因**: Stage 7 范围限定为 `include/AGENTS.md`、`include/tlm/AGENTS.md`、`configs/AGENTS.md`、`CHANGELOG.md` 与新建 `docs/migration-v2.2.md`。其他文档（`docs/architecture/*`、`docs/ONBOARDING.md`、`AGENTS.md` 根、`test/README.md`、`docs/soc_arch/*`）的清理属于后续 change, 见 [`openspec/changes/json-nested-simmodule/`](../../openspec/changes/json-nested-simmodule/) 的"待归档后开新 change"段。

---

## 6. 常见问题 (FAQ)

**Q1: 我的项目 v2.1 用了 `CPUSim`, 升级 v2.2 一定要改吗?**

A: 是的。`CPUSim` 类型已删除, 任何 `CPUSim` 引用会导致编译失败。改用 `CPUTLM`（功能等价, API 略有差异）。详见 §2.3。

**Q2: `BUILD_LEGACY_MODULES=OFF` 在 v2.2 还能用吗?**

A: 不能。`BUILD_LEGACY_MODULES` CMake 选项已从 `CMakeLists.txt` / `src/CMakeLists.txt` 删除。直接删除 CI/CD 中的 `BUILD_LEGACY_MODULES` 标志即可。

**Q3: `CpuCluster` 升级后 API 兼容吗?**

A: 基本兼容。`CpuCluster` 类名不变, 仍是 `SimModule` 派生。但 `set_config` 行为增强（解析 `num_cpus`/`cluster_id`），`tick()` 显式转发到子模块（v2.1 是空函数）。JSON 配置格式需加 `params` 字段（v2.1 空 `params` 也可）。

**Q4: 嵌套 JSON 配置中 `outputs` / `inputs` 字段是必须的吗?**

A: 否。`outputs` / `inputs` 仅在需要把内部子模块端口暴露到外部命名空间时使用。最简配置（仅含 `modules` + `connections`）也合法。

**Q5: 嵌套深度限制 8 是否硬性?**

A: 是。`SimModule::MAX_DEPTH = 8` 是硬限制, 超出抛 `std::runtime_error`。设计上 8 层已覆盖大多数实际场景（CpuCluster→CpuCluster→...→CPUTLM/缓存/内存）。如需调整, 改 `include/core/sim_module.hh` 的 `MAX_DEPTH` 编译期常量即可。

**Q6: 性能有影响吗?**

A: 无。`SimModule::simulate_instantiate` 的 `static thread_local` 计数器是 O(1) 自增, 对仿真整体性能无影响。RTTI `dynamic_cast<SimModule*>` 开销 < 1ns/调用, 远低于 JSON 解析成本。

---

## 7. 相关链接 (References)

- **OpenSpec change**: [`openspec/changes/json-nested-simmodule/proposal.md`](../../openspec/changes/json-nested-simmodule/proposal.md)
- **设计文档**: [`openspec/changes/json-nested-simmodule/design.md`](../../openspec/changes/json-nested-simmodule/design.md)
- **CHANGELOG**: [`CHANGELOG.md`](../../CHANGELOG.md) v2.2 条目
- **示例配置**:
  - `configs/example_simmodule_nested_2level.json`
  - `configs/example_simmodule_nested_3level_static.json`
- **测试**:
  - C++: `test/test_simmodule_nested.cc` (7 用例, `[simmodule]` 标签)
  - Python: `test/python/test_simmodule_emitter.py` (6 用例)
- **源文件**:
  - `include/tlm/cluster/cpu_cluster.hh` / `cpu_cluster.cc`
  - `include/tlm/cpu_tlm.hh` (`CPUTLM` 替代 `CPUSim`)
  - `include/core/sim_module.hh` (限深保护 + `simulate_instantiate` 公共方法)
  - `include/modules.hh` (`REGISTER_OBJECT` no-op + `REGISTER_MODULE` 无条件注册)

---

**维护**: CppTLM 开发团队
**版本**: v2.2.0
**最后更新**: 2026-06-18
