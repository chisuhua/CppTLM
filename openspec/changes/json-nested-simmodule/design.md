# JSON-Nested SimModule — Design

## Context

### 当前状态

`include/core/sim_module.hh` 提供完整的 SimModule 嵌套基础设施：

```cpp
class SimModule : public SimObject {
    std::unique_ptr<ModuleFactory> internal_factory;  // 内部子模块工厂
    json config;                                       // JSON 配置
    void instantiate(const json& cfg);                 // 解析子模块 + 暴露端口
    SimObject* getInternalInstance(const std::string& name);
    SlavePort* getInternalInputPort(const std::string& path);
    MasterPort* getInternalOutputPort(const std::string& path);
    std::string findInternalPath(const std::string& external_label) const;
    bool isExposedPort(const std::string& external_label) const;
};
```

**问题 1**：`configs/*.json` 36 个 active 文件中**零个**用 `"outputs"`/`"inputs"` 暴露端口字段（`grep '"outputs"\s*:\s*\[' configs/*.json` = 0 命中）；`examples/*.cc` 也**零个**用 `getInternalInstance`/`getInternalOutputPort`。整套 API 处于"设计就绪、零用户"状态。

**问题 2**：唯一 `SimModule` 派生类 `CpuCluster`（`include/modules/legacy/cpu_cluster.hh:9-16`）是**空心类**：
```cpp
class CpuCluster : public SimModule {
public:
    explicit CpuCluster(const std::string& name, EventQueue* eq) : SimModule(name, eq) {}
    void tick() {}
private:  // ← 空
};
```

`include/modules.hh:25-38` 通过 `BUILD_LEGACY_MODULES` 宏守门，**默认 OFF → no-op**。`CMakeLists.txt:44` `option(BUILD_LEGACY_MODULES ... OFF)` 是默认配置。

**问题 3**：`include/modules/legacy/` 目录还剩 2 个文件（`cpu_cluster.hh` + `cpu_sim.hh`），但 v2.1 已有完整 TLM 模块替代（`include/tlm/cpu_tlm.hh` 的 `CPUTLM` 取代 `CPUSim`）。`docs-archived/samples-orphaned/README.md` 明确推荐"新代码用 `CPUTLM`"。

### 约束

- **保持 610 C++ 测试 + 207 Python 测试不破坏**（本 change 净增测试，不删测试）
- **C++ schema 零字段变更**（outputs/inputs 字段已存在，本 change 仅填充使用示例）
- **不引入新 C++/Python 依赖**

## Goals / Non-Goals

**Goals:**
- 把 SimModule 嵌套从"死代码"变成"活的、可演示、可测试"的能力
- `CpuCluster` 从 `modules/legacy/` 移出并增强，真正用 JSON 配置驱动子模块实例化
- 支持 N 层运行时嵌套（CpuCluster→CpuCluster→...），通过 `MAX_DEPTH=8` 限深保护防止 JSON 错误触发真递归
- 删除 `BUILD_LEGACY_MODULES` 开关、整个 `modules/legacy/` 目录、`CPUSim` 类型
- 新增 C++ Catch2 + Python pytest 双覆盖测试 + C++/Python 示例 + JSON 配置
- 文档同步（CHANGELOG、AGENTS.md 三处更新）

**Non-Goals:**
- 不修改 `outputs/inputs` JSON 字段的解析逻辑（`SimModule::parsePortConfigs` 现有代码已正确）
- 不修改现有 15 个 active `openspec/specs/` 中的任何一个
- 不重命名 `SimModule`/`CpuCluster`（保留概念边界）
- 不引入运行时性能优化（仅 O(1) 计数器）
- 不支持 `outputs/inputs` 之外的暴露机制（端口别名、动态端口重命名等后续 change）

## Decisions

### Decision 1: 运行时嵌套深度 = N（用户最终确认）

**Why**: 用户原话"可以多层嵌套"明确要求 N 层运行时展开（不是 1 层）。

**实现**:
- `SimModule::instantiate(cfg)` **委托**给新增的 `simulate_instantiate(cfg)`（保持向后兼容，外部 `instantiate` 调用代码不变）。
- 新增公共方法 `void SimModule::simulate_instantiate(const json& cfg)`：入口处做幂等守卫（`internal_factory->getAllInstances()` 非空则早退），然后 `internal_factory->instantiateAll(cfg)` + `parsePortConfigs(cfg)` + **RAII `DepthGuard` 包夹**的 depth_ 检查，最后遍历 `cfg["modules"]`，对子 SimModule 派生类用 `dynamic_cast` 识别后**递归触发** `simulate_instantiate`。
- 顶层 `ModuleFactory::instantiateAll(json)` 构造完成后，对每个顶层 SimObject 用 `dynamic_cast<SimModule*>` 识别，是 SimModule 则调 `simulate_instantiate(top_cfg)`。这一步**仅在顶层做 1 次 RTTI**（N 个顶层模块 = N 次 dynamic_cast），触发后内部递归由 SimModule 基类自管理。

**Alternative considered**:
- (A) `ModuleFactory::instantiateAll(json, bool allow_modules)` 改签名，内部按 `allow_modules` 决定是否触发 `simulate_instantiate`——**拒绝**：破坏现有调用签名，6 个测试改写，且无新功能收益。
- (B) `SimModule` 在自己 `instantiate` 内部递归调用——**拒绝**：`instantiate` 语义上是"构造期初始化"，与"模拟激活"混在一起；引入 `simulate_instantiate` 命名可显式区分两个生命周期阶段。
- (C) 用类型注册表 + 显式 `activate()` 钩子替代 RTTI——**拒绝**：每个 `SimModule` 派生类需手动调 `activate(cfg)`，违反"零配置"目标（用户 JSON 加 `type: "CpuCluster"` 即可用，不应要求 C++ 端额外注册钩子）。
- **最终采纳**: (D) `ModuleFactory` 顶层 RTTI 识别（1 次/顶层模块）+ `SimModule::simulate_instantiate` 内部 RTTI 递归（1 次/子模块）。RTTI 开销 < 1ns/调用，远低于 JSON 解析成本；架构自包含、零配置、签名零变更。

### Decision 2: 限深保护 = `static thread_local int depth_` + `MAX_DEPTH=8`

**Why**: N 层嵌套有栈溢出风险（默认线程栈 8MB，每层 instantiate 大约 1KB frame，理论可支撑 ~8000 层，但实际还有 ModuleFactory 构造、JSON 解析等开销），需要硬限制。`thread_local` 避免多线程仿真间计数器污染。

**实现** (使用 RAII guard 保证异常安全,throw 时所有层 depth_ 自动恢复):
```cpp
class SimModule : public SimObject {
private:
    static constexpr int MAX_DEPTH = 8;
    static thread_local int depth_;

    // RAII guard:作用域结束时 --depth_,包括 throw 路径
    struct DepthGuard {
        ~DepthGuard() noexcept { --depth_; }
    };
public:
    void simulate_instantiate(const json& cfg) {
        // 幂等守卫:已 populate 则不重复构造(只触发子 SimModule 递归激活)
        if (internal_factory && !internal_factory->getAllInstances().empty()) {
            return;
        }
        ++depth_;
        DepthGuard guard{};  // 析构时 --depth_,即使 throw 也执行
        if (depth_ > MAX_DEPTH) {
            throw std::runtime_error(
                "SimModule::simulate_instantiate depth limit "
                + std::to_string(MAX_DEPTH) + " exceeded at path "
                + buildDepthPath());
        }
        config = cfg;
        if (!internal_factory->instantiateAll(config)) { /* error log */ }
        parsePortConfigs(cfg);
        // 递归激活内部 SimModule 子模块
        for (auto& child_cfg : cfg["modules"]) {
            auto* child = internal_factory->getInstance(child_cfg["name"]);
            if (auto* sub = dynamic_cast<SimModule*>(child)) {
                sub->simulate_instantiate(child_cfg);
            }
        }
    }
};
```

**异常安全保证**: 由于 DepthGuard 在 simulate_instantiate 入口构造(在 ++depth_ 之后),作用域结束时(包括 throw 路径)自动 --depth_。**每一层都有自己的 guard**,所以第 9 层 throw 时,9 个 guard 全部析构,depth_ 恢复为 0。无 try/catch 污染调用链。

**Alternative considered**:
- (A) 在 `ModuleFactory::instantiateAll` 加深度检查——**拒绝**：工厂不知道调用链上下文（顶层 vs 嵌套），无法区分。
- (B) `static int depth_`（非 thread_local）——**拒绝**：多线程仿真会互相干扰。
- (C) 无限制——**拒绝**：JSON 配置错误（手滑写 1000 层嵌套）会让进程栈溢出崩溃。

### Decision 3: CpuCluster 移至 `include/tlm/cluster/`

**Why**: 与现有 `include/tlm/cache_tlm.hh`、`include/tlm/crossbar_tlm.hh` 等并列；语义上 CpuCluster 是 TLM 时代的"集群容器"（与 `CpuTlm` 平级），不是 v2.0 时代的 legacy 端口模块。`cluster/` 子目录为未来更多集群类模块（如 `MemoryCluster`、`CacheCluster`）预留空间。

**新位置**: `include/tlm/cluster/cpu_cluster.hh` + `include/tlm/cluster/cpu_cluster.cc`

**实现要点**:
```cpp
// cpu_cluster.hh
class CpuCluster : public SimModule {
private:
    int num_cpus_ = 4;        // 默认 4 个 CPU
    std::string cluster_id_;  // 可选集群 ID（用于 hierarchical debug）
public:
    explicit CpuCluster(const std::string& name, EventQueue* eq) 
        : SimModule(name, eq) {}
    std::string get_module_type() const override { return "CpuCluster"; }
    void set_config(const json& params) override;  // 解析 num_cpus / cluster_id
    void tick() override;  // 转发到 internal 子模块
};

// cpu_cluster.cc
void CpuCluster::set_config(const json& params) {
    if (params.contains("num_cpus")) num_cpus_ = params["num_cpus"].get<int>();
    if (params.contains("cluster_id")) cluster_id_ = params["cluster_id"].get<std::string>();
}
void CpuCluster::tick() {
    for (auto& [name, obj] : internal_factory->getAllInstances()) {
        obj->tick();
    }
}
```

**注册**: `REGISTER_MODULE ModuleFactory::registerModule<CpuCluster>("CpuCluster");`（无 `#ifdef`）

### Decision 4: CPUSim 退场策略

**Why**: `include/tlm/cpu_tlm.hh` 的 `CPUTLM`（`ChStreamModuleBase` 派生）已完全替代 `CPUSim`（`SimObject` 派生，PortPair 通信）。`docs-archived/samples-orphaned/README.md` 明确推荐迁移。

**实施**:
1. 删除 `include/modules/legacy/cpu_sim.hh`
2. `include/modules.hh` 的 `REGISTER_OBJECT` 宏简化为 **空**（不注册任何 Legacy 对象）：
   ```cpp
   #define REGISTER_OBJECT  /* no-op: legacy CPUSim removed in v2.2 */
   ```
3. 全局搜索 `CPUSim` 引用点：测试代码 (`test/*.cc`)、示例代码 (`examples/*.cc`)、JSON 配置 (`configs/*.json`、历史归档) → 替换为 `CPUTLM`。
4. `include/tlm/cpu_tlm.hh` 验证 `CPUTLM` 已注册（`REGISTER_CHSTREAM` 宏）——已确认 (`include/chstream_register.hh`)。

**Alternative considered**:
- (A) 保留 `cpu_sim.hh` 但 `#ifdef 0` 包裹——**拒绝**：违反"零债务原则"（`AGENTS.md` 明确禁 TODO 残留）。
- (B) 转 `CPUSim` 为 `ChStreamModuleBase` 子类（变成 `CPUSimTLM`）——**拒绝**：与 `CPUTLM` 功能重复，无新增价值。
- (C) 保留 `CPUSim` 但移到 `include/tlm/legacy/cpu_sim_tlm.hh`——**拒绝**：legacy 目录整个删除，不留特例。

### Decision 5: `BUILD_LEGACY_MODULES` 完全删除

**Why**: 开关已无任何应用场景（`CPUSim` 退场，`CpuCluster` 移出后改用无条件注册）。保留开关会污染 CMake 表面、增加新人理解成本。

**实施**:
- `CMakeLists.txt:44` 删除 `option(BUILD_LEGACY_MODULES ...)` 行
- `src/CMakeLists.txt:40-44` 删除 `if(BUILD_LEGACY_MODULES) ... target_compile_definitions(...)` 块
- `include/modules.hh` 删除 `#ifdef BUILD_LEGACY_MODULES ... #else ... #endif` 三段结构，改为无条件：
  ```cpp
  #include "tlm/cluster/cpu_cluster.hh"
  
  #define REGISTER_OBJECT  /* no-op: CPUSim removed in v2.2; use REGISTER_CHSTREAM for CPUTLM */
  #define REGISTER_MODULE  ModuleFactory::registerModule<CpuCluster>("CpuCluster");
  ```
- 全局 `grep -rn BUILD_LEGACY_MODULES` 验证无残留（含 `.github/workflows/`、`.gitignore`、`docs/`）

**Alternative considered**:
- (A) 保留开关但 `OFF` 为默认且 `ON` 不做任何事——**拒绝**：开关变 dead code，违反零债务。
- (B) 改名为 `BUILD_CLUSTER_MODULES` 保留 `CpuCluster` 守卫——**拒绝**：`CpuCluster` 已在 `include/tlm/`，无 legacy 性质。

## Risks / Trade-offs

| Risk | Mitigation |
|------|-----------|
| BREAKING：v2.1.x 引用 `BUILD_LEGACY_MODULES=ON` 启用 legacy 模块的项目无法编译 | CHANGELOG v2.2 条目明确说明；提供迁移指南（CPUSim → CPUTLM）；本 change 在 `docs/migration-v2.2.md` 新文件记录路径变更 |
| BREAKING：`#include "modules/legacy/cpu_sim.hh"` 路径不再存在 | CHANGELOG + `docs/migration-v2.2.md` 明确新路径 `#include "tlm/cpu_tlm.hh"` |
| `static thread_local` 在动态库（`.so`）中可能有多副本 | 当前项目静态构建（`build/lib/cptlm_core.a` 静态库），不构成问题；未来若改动态库需重新评估 |
| `dynamic_cast<SimModule*>` 在 `simulate_instantiate` 中每层调用 | RTTI 开销 < 1ns/调用，远低于 JSON 解析成本；可接受 |
| CpuCluster 增强后 `tick()` 转发到子模块可能与子模块自身 `tick()` 重复触发 | 顶层 `ModuleFactory::startAllTicks()` 仅在顶层 SimObject 列表上 `tick()`，内部子模块通过 `CpuCluster::tick()` 显式转发，**不重复**。需在测试中验证（用例 5 E2E） |
| 新增 6+6 用例增加 CI 时间 | 当前 CI ~3 分钟，新增 ~30 秒用例，可接受 |
| `examples/generate_nested_soc.py` 依赖 `cpptlm/topo/` 与 `cpptlm/library/` | 两者均已在 v2.1 落地（`openspec/specs/cluster-factory` 与 `openspec/specs/config-emitter`），无新依赖 |
| `openspec/specs/` 增加 4 个新 spec | 现有 15 个，+4 = 19 个；不构成膨胀（`openspec/specs/AGENTS.md` 建议 < 30 个） |

## Migration Plan

### 阶段 1：基础设施（CpuCluster + SimModule 限深）
1. 新建 `include/tlm/cluster/cpu_cluster.hh` + `.cc`（override tick/set_config/get_module_type）
2. 改造 `include/core/sim_module.hh`（加 depth_ + MAX_DEPTH + simulate_instantiate）
3. `include/modules.hh` 重构（移除 BUILD_LEGACY_MODULES 守卫）
4. 编译验证：`cmake --build build -j$(nproc)`

### 阶段 2：legacy 移除（CPUSim 退场 + 目录删除）
1. 全局搜索 `CPUSim` 引用点（`grep -rn "CPUSim" --include="*.cc" --include="*.hh" --include="*.json" --include="*.py"`），逐个替换为 `CPUTLM`
2. 删除 `include/modules/legacy/cpu_sim.hh`
3. 删除 `include/modules/legacy/cpu_cluster.hh`（**注意**：内容已移至新位置，删除的是旧文件）
4. 删除 `include/modules/legacy/README.md`
5. `rmdir include/modules/legacy/`
6. 验证目录为空：`ls include/modules/legacy/ 2>&1` 应为 "No such file or directory"
7. 编译验证 + 610 测试全通过

### 阶段 3：测试与示例
1. 新建 `test/test_simmodule_nested.cc`（6 用例）
2. 在 `test/CMakeLists.txt` 注册新测试文件
3. 新建 `examples/example_simmodule_nested.cc` + 在 `examples/CMakeLists.txt` 注册
4. 新建 `examples/generate_nested_soc.py`（用 cpptlm/topo/ + cpptlm/library/）
5. 新建 `configs/example_simmodule_nested_2level.json` + `3level_static.json`
6. 新建 `test/python/test_simmodule_emitter.py`（6 用例）+ 在 `pyproject.toml` 注册
7. 运行 C++ + Python 全套测试

### 阶段 4：文档同步
1. 更新 `include/AGENTS.md`（注册宏体系段移除 BUILD_LEGACY_MODULES 守卫说明）
2. 更新 `include/tlm/AGENTS.md`（加 CpuCluster 行）
3. 更新 `configs/AGENTS.md`（移除"已归档"段 BUILD_LEGACY_MODULES 描述）
4. 新建 `docs/migration-v2.2.md`（迁移指南）
5. 更新 `CHANGELOG.md`（v2.2 条目）
6. 运行 `scripts/test/docs_sync_check.sh --strict` 验证

### 阶段 5：CI 验证
1. `cmake --build build -j$(nproc)` 通过
2. `ctest --test-dir build --output-on-failure -j4` 全绿
3. `pytest test/python/ -v` 全绿
4. `./scripts/test/run_all_tests.sh --quick` 通过

### 回滚策略

阶段 2 是 BREAKING，若发现关键用户依赖未覆盖：
1. 立即 `git revert` 本 change 的所有 commits
2. 重新加 `BUILD_LEGACY_MODULES=ON` 路径
3. 在 CHANGELOG v2.2 加 DEPRECATED 标记（不删除）

阶段 1 / 3 / 4 / 5 非 BREAKING，可安全回滚。

## Open Questions

无。所有关键决策已通过用户问答闭环：
- CpuCluster 保留并增强 → 决策 3
- CPUSim 删除由 CPUTLM 替代 → 决策 4
- 嵌套层数 N 层 + 限深 8 → 决策 1 + 2
- BUILD_LEGACY_MODULES 移除 → 决策 5
- 测试 + 示例 C++ + Python 双覆盖 → 已确认

后续可在 v2.3 探索：动态端口重命名、`outputs/inputs` 之外的暴露机制（如 `expose_chstream_port`）、`SimModule` 参数模板（`params: { template: "cpu_l1_cluster" }` 引用 Python 端 `cpptlm/library/`）。
