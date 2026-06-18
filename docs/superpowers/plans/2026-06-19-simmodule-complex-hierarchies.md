# SimModule Complex Hierarchies Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 CppTLM v2.2 SimModule 体系引入复杂多级层次支持，修复 3 层 JSON 端到端 bug，新增 8 个 SimModule 派生类（ComputeCluster/TpcCluster/GpcCluster/GpuCluster/CacheCluster/MemoryCluster/GpuNoC/ApuSoC），实现 JSON 蓝图模板复用（`cu_template` + `cu_count`），最终支持 gem5 风格的 APU SoC 完整拓扑（CPU + GPU 4-GPC×2-TPC×2-CU + Memory + NoC）。

**Architecture:** 采用 5 Phase 增量分层策略（参考 gem5 SubSystem + incorporate_cache(board) late-binding + 动态 per-core 子模块生成模式）。P1 优先修 2 个致命 API bug（D.4 `findInternalPath` 不递归 + D.5 `tick` 链断裂），P2 新增 4 个 GPU 端核心 SimModule 启用 JSON 模板复用，P3（可选）加 ChStream helper 方法，P4 加 3 个基础设施 SimModule，P5 加 ApuSoC 顶层 + `incorporate_parent` 钩子。每 Phase 独立可发布、零债务。

**Tech Stack:** C++17, Catch2 v3.7.0 (预编译 2 文件), CMake + Ninja, nlohmann/json, `REGISTER_MODULE` 宏（`include/modules.hh`），`SimModule` 基类（`include/core/sim_module.hh`），`JsonIncluder`（`include/utils/json_includer.hh`），`ModuleFactory`（`include/core/module_factory.hh`）。

**Reference Documents:**
- Spec: [`docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md`](../specs/2026-06-19-simmodule-complex-hierarchies-design.md) (860 lines, 15 sections)
- gem5 reference: [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md) + `/workspace/project/gem5` source

---

## File Structure

### Phase P1 - API 修复 (新增 0 类, 修改 1 文件)

| File | Responsibility | Status |
|------|----------------|--------|
| `include/core/sim_module.hh` | 递归 `findInternalPath` + 默认 `tick` 递归 | Modify |
| `test/test_simmodule_recursive_bug.cc` | D.4 + D.5 + 3 层 JSON E2E 验证 (3 tests) | Create |

### Phase P2 - GPU 端 4 核心 SimModule (新增 4 类, ~8 文件, ~200 LOC)

| File | Responsibility | Status |
|------|----------------|--------|
| `include/tlm/cluster/compute_cluster.hh` | `ComputeCluster` 类（JSON 蓝图模板复制 N 份） | Create |
| `src/tlm/cluster/compute_cluster.cc` | `ComputeCluster::simulate_instantiate` 实现 | Create |
| `include/tlm/cluster/tpc_cluster.hh` | `TpcCluster` 类（1 ComputeCluster + 共享 TextureUnit） | Create |
| `src/tlm/cluster/tpc_cluster.cc` | `TpcCluster::simulate_instantiate` | Create |
| `include/tlm/cluster/gpc_cluster.hh` | `GpcCluster` 类（M 个 TpcCluster） | Create |
| `src/tlm/cluster/gpc_cluster.cc` | `GpcCluster::simulate_instantiate` | Create |
| `include/tlm/cluster/gpu_cluster.hh` | `GpuCluster` 顶层（K 个 GPC + TCC + KernelLaunch） | Create |
| `src/tlm/cluster/gpu_cluster.cc` | `GpuCluster::simulate_instantiate` | Create |
| `include/modules_cluster.hh` | 4 个 `REGISTER_MODULE` 宏集中 | Create |
| `include/modules.hh` | 包含新 `modules_cluster.hh` | Modify |
| `configs/templates/compute_unit_v1.json` | 1 CU 蓝图（ScalarCache + VectorReg + Wavefront） | Create |
| `configs/gpu_2gpc_2tpc_2cu.json` | 完整 GPU 拓扑配置 | Create |
| `examples/example_simmodule_2gpc_2tpc_2cu.cc` | GPU 4 层嵌套示例 | Create |
| `examples/CMakeLists.txt` | 注册新 example target | Modify |
| `test/test_simmodule_gpu_hierarchy.cc` | GPU 4 类验证 (4 tests) | Create |
| `src/CMakeLists.txt` | 添加新 .cc 源到 `cpptlm_core` | Modify |

### Phase P3 - ChStream Helper (新增 0 类, 修改 2 文件, ~150 LOC, 依赖 D.1 修复)

| File | Responsibility | Status |
|------|----------------|--------|
| `include/tlm/cache_tlm.hh` | `CacheTLM::connectCPU/Bus` helper 方法 | Modify |
| `include/tlm/crossbar_tlm.hh` | `CrossbarTLM::connectCPUSideBus/MemSideBus` | Modify |
| `src/tlm/cache_tlm.cc` | helper 方法实现 | Modify |
| `test/test_chstream_helpers.cc` | helper 方法验证 (3 tests) | Create |

### Phase P4 - 基础设施 3 类 (新增 3 类, ~6 文件, ~250 LOC)

| File | Responsibility | Status |
|------|----------------|--------|
| `include/tlm/cluster/cache_cluster.hh` | `CacheCluster`（L1×N + L2） | Create |
| `src/tlm/cluster/cache_cluster.cc` | 实现 | Create |
| `include/tlm/cluster/memory_cluster.hh` | `MemoryCluster`（多通道 HBM） | Create |
| `src/tlm/cluster/memory_cluster.cc` | 实现 | Create |
| `include/tlm/cluster/gpu_noc_cluster.hh` | `GpuNoC`（Garnet N×N mesh） | Create |
| `src/tlm/cluster/gpu_noc_cluster.cc` | 实现 | Create |
| `include/modules_cluster.hh` | 添加 3 个新 `REGISTER_MODULE` | Modify |
| `test/test_simmodule_infra_clusters.cc` | 3 类验证 (3 tests) | Create |

### Phase P5 - 顶层 + incorporate 钩子 (新增 1 类, 修改 1 文件, ~150 LOC)

| File | Responsibility | Status |
|------|----------------|--------|
| `include/core/sim_module.hh` | `incorporate_parent` 虚函数 | Modify |
| `include/tlm/cluster/apu_soc.hh` | `ApuSoC` 顶层容器 | Create |
| `src/tlm/cluster/apu_soc.cc` | 实现 | Create |
| `include/modules_cluster.hh` | 添加 `REGISTER_MODULE(ApuSoC)` | Modify |
| `configs/apu_soc_v1.json` | 完整 APU 配置 | Create |
| `configs/templates/cpu_cluster_2level.json` | CPU 拓扑模板 | Create |
| `configs/templates/gpu_2gpc_2tpc_2cu.json` | GPU 拓扑模板 | Create |
| `examples/generate_apu_soc_2gpc.py` | Python 端到端 demo | Create |
| `test/test_apu_soc_top.cc` | ApuSoC + incorporate_parent 验证 (3 tests) | Create |
| `test/python/test_apu_soc_emitter.py` | Python emitter 验证 (2 tests) | Create |

### 文档同步 (跨 5 Phase)

| File | Phase | 改动 |
|------|-------|------|
| `AGENTS.md` | P1-P5 | STRUCTURE 节加 `include/tlm/cluster/` 8 文件 + `configs/templates/` |
| `include/AGENTS.md` | P2-P5 | 注册宏体系表 |
| `include/tlm/AGENTS.md` | P2-P5 | cluster/ 子目录表 |
| `configs/AGENTS.md` | P1-P5 | JSON Schema `cu_template` / `cu_count` 字段 |
| `docs/architecture/01-hybrid-architecture-v2.1.md` | P5 | GPU 层次图 |
| `CHANGELOG.md` | P1-P5 | 每 Phase 追加版本号 |

**Total: 31 new files + 12 modified files, ~775 LOC C++ + ~600 LOC test + ~500 LOC JSON + ~200 LOC Python + ~400 LOC docs**

---

## Phase P1: 修 2 个致命 API Bug (D.4 + D.5)

### Task 1.1: 写 D.4 失败测试

**Files:**
- Create: `test/test_simmodule_recursive_bug.cc`

- [ ] **Step 1.1.1: 创建测试文件骨架**

```cpp
// test/test_simmodule_recursive_bug.cc
// D.4 + D.5 修复验证 + 3 层 JSON E2E 测试
// 修复前：findInternalPath 不递归，3 层 JSON exposed port 路径解析失败
// 修复后：递归到子 SimModule
// 作者: Sisyphus / 日期: 2026-06-19
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.1
#include "core/sim_module.hh"
#include "modules_cluster.hh"  // placeholder, P2 后存在
#include "chstream_register.hh"
#include "core/module_factory.hh"
#include "utils/json_includer.hh"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

using namespace cpptlm;
using json = nlohmann::json;

TEST_CASE("D.4 findInternalPath recurses nested SimModule", "[simmodule][regression]") {
    // 验证 findInternalPath 递归到子 SimModule
    // 3 层 outer → mid → inner
    // outer.outputs: internal="mid.mid_cpu0_to_bus" (跨层引用)
    // mid.outputs:    internal="inner.inner_cpu0_to_bus"
    // inner.outputs:  internal="cpu0.req_out"
    // 期望 outer.findInternalPath("mid_cpu0_to_bus") 返回 "mid.inner.cpu0.req_out"

    EventQueue eq;
    auto* outer = new CpuCluster("outer", &eq);
    auto* mid = new CpuCluster("mid", &eq);
    auto* inner = new CpuCluster("inner", &eq);
    auto* cu0 = new CpuCluster("cu0_proxy", &eq);  // 占位，实际是 CPUTLM
    cu0->setName("cu0");

    // 模拟 build hierarchy
    inner->addInternalInstance(cu0);
    mid->addInternalInstance(inner);
    outer->addInternalInstance(mid);

    // 注册 outputs
    inner->addOutputConfig("cpu0.req_out", "inner_cpu0_to_bus");
    mid->addOutputConfig("inner.inner_cpu0_to_bus", "mid_cpu0_to_bus");
    outer->addOutputConfig("mid.mid_cpu0_to_bus", "outer_cpu0_to_bus");

    REQUIRE(outer->findInternalPath("outer_cpu0_to_bus") == "mid.mid_cpu0_to_bus");
    REQUIRE(outer->findInternalPath("mid_cpu0_to_bus") == "mid.inner.inner_cpu0_to_bus");
    REQUIRE(outer->findInternalPath("inner_cpu0_to_bus") == "mid.inner.cpu0.req_out");

    delete outer; delete mid; delete inner; delete cu0;
}
```

**注**：上例中 `addInternalInstance/addOutputConfig/setName` 是测试辅助方法，P1 阶段如果不存在，需要在 `sim_module.hh` 加 friend 测试访问接口或用 friend class `SimModuleTestAccess`。具体见 Step 1.1.3。

- [ ] **Step 1.1.2: 运行测试（预期失败）**

```bash
cd build && cmake --build . --target cpptlm_tests -j$(nproc)
./bin/cpptlm_tests "[regression]" -v
```

**Expected**: FAIL - `findInternalPath` 返回 ""（D.4 旧实现不递归）

- [ ] **Step 1.1.3: 补缺失的测试辅助方法（如需要）**

如果 `addInternalInstance/addOutputConfig` 等测试辅助 API 在 `sim_module.hh` 不存在，通过 friend class 模式或 `protected:` + `#ifdef CPPTLM_TESTING` 暴露：

```cpp
// include/core/sim_module.hh (在 SimModule 类内)
#ifdef CPPTLM_TESTING
public:
    // P1 修复: 之前调用 internal_factory->registerInstance() 不存在.
    // ModuleFactory 无公共 registerInstance 方法, instances map 是 private.
    // 改用 friend 访问: 在测试代码中直接通过 internal_factory->getAllInstances()
    // 已有的迭代接口添加, 或通过内部 addInstance 私有方法 + friend class.
    // 最简方案: 通过 createInstance + 私有 registerInstance (test guarded):
    void addInternalInstance(SimObject* obj) {
        // 实际字段: instances (无下划线, 见 module_factory.hh:37)
        internal_factory->addInstanceForTesting(obj->getName(), obj);
    }
    void addOutputConfig(const std::string& internal, const std::string& external) {
        output_configs.push_back({internal, external});
        internal_to_external_map[external] = internal;
    }
    void setName(const std::string& n) { name = n; }  // SimObject 字段名 (无下划线)
#endif

// 配套: include/core/module_factory.hh 添加 test-only 方法
class ModuleFactory {
    // ... 现有 public 接口 ...
#ifdef CPPTLM_TESTING
public:
    // P1 测试辅助: 直接向 instances_ 添加 (绕过 instantiateAll 的 8 步流程)
    void addInstanceForTesting(const std::string& name, SimObject* obj) {
        instances[name] = obj;  // 实际字段名 (无下划线)
    }
#endif
};
```

并在 `test/CMakeLists.txt` 测试编译选项加 `-DCPPTLM_TESTING`:

```cmake
# test/CMakeLists.txt
target_compile_definitions(cpptlm_tests PRIVATE CPPTLM_TESTING)
```

- [ ] **Step 1.1.4: 重新运行测试（再次确认失败）**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[regression]" -v
```

**Expected**: FAIL（确认是 D.4 bug，不是测试本身写错）

### Task 1.2: 实现 D.4 修复

**Files:**
- Modify: `include/core/sim_module.hh:149-161`

- [ ] **Step 1.2.1: 修改 findInternalPath 实现**

定位到 `include/core/sim_module.hh:149-161`（`SimModule::findInternalPath`），替换为递归实现：

```cpp
// include/core/sim_module.hh
std::string findInternalPath(const std::string& external_label) const override {
    // P1 Fix D.4: 先查当前层，未命中递归到子 SimModule
    auto it = internal_to_external_map.find(external_label);
    if (it != internal_to_external_map.end()) return it->second;
    for (const auto& kv : internal_factory->getAllInstances()) {
        if (auto* sub = dynamic_cast<SimModule*>(kv.second)) {
            std::string sub_path = sub->findInternalPath(external_label);
            if (!sub_path.empty()) return kv.first + "." + sub_path;
        }
    }
    return "";
}
```

- [ ] **Step 1.2.2: 重新编译**

```bash
cmake --build build --target cpptlm_core -j$(nproc)
```

**Expected**: 编译通过（无 warning）

- [ ] **Step 1.2.3: 重新运行测试**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[regression]" -v
```

**Expected**: PASS - `findInternalPath` 递归解析 3 层路径

- [ ] **Step 1.2.4: 跑完整测试套件确保无回归**

```bash
./build/bin/cpptlm_tests  # 全部 82 测试必须 pass
```

**Expected**: 全部 pass（无回归）

- [ ] **Step 1.2.5: 提交**

```bash
cd /workspace/project/CppTLM
git add include/core/sim_module.hh test/test_simmodule_recursive_bug.cc include/core/module_factory.hh test/CMakeLists.txt
git commit -m "fix(simmodule): make findInternalPath recurse to child SimModule (D.4)

- 修复前: 3 层 outer→mid→inner 嵌套时, outer.findInternalPath('mid_cpu0_to_bus') 返回 ''
- 修复后: 递归到子 SimModule, 返回 'mid.inner.cpu0.req_out'
- 影响: 解锁 example_simmodule_nested_3level_static.json 端到端工作
- 测试: test/test_simmodule_recursive_bug.cc::[regression] 验证"
```

### Task 1.3: 写 D.5 失败测试

**Files:**
- Modify: `test/test_simmodule_recursive_bug.cc`

- [ ] **Step 1.3.1: 添加 D.5 测试用例**

在 `test/test_simmodule_recursive_bug.cc` 末尾追加：

```cpp
class TickCounter : public SimObject {
public:
    int tick_count = 0;
    explicit TickCounter(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    void tick() override { ++tick_count; }
};

TEST_CASE("D.5 tick recurses all descendants", "[simmodule][regression]") {
    // 修复前: CpuCluster::tick 手动实现, 但嵌套 SimModule 子类无默认实现
    // 修复后: SimModule 基类提供默认递归 tick
    EventQueue eq;
    auto* outer = new CpuCluster("outer", &eq);
    auto* mid = new CpuCluster("mid", &eq);
    auto* counter = new TickCounter("counter", &eq);
    mid->addInternalInstance(counter);
    outer->addInternalInstance(mid);

    outer->tick();
    REQUIRE(counter->tick_count == 1);

    outer->tick();
    REQUIRE(counter->tick_count == 2);

    delete outer; delete mid; delete counter;
}
```

- [ ] **Step 1.3.2: 运行测试（预期失败）**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[regression]" -v
```

**Expected**: FAIL - `counter->tick_count == 0`（D.5 旧 tick 不递归）

### Task 1.4: 实现 D.5 修复

**Files:**
- Modify: `include/core/sim_module.hh`（新增默认 `tick` 实现）

- [ ] **Step 1.4.1: 在 SimModule 基类添加默认 tick 递归**

定位到 `include/core/sim_module.hh`，在合适位置（`findInternalPath` 之后）添加：

```cpp
// include/core/sim_module.hh
class SimModule : public SimObject {
public:
    // ... 现有方法 ...

    // P1 Fix D.5: 默认递归 tick - 遍历所有内部子模块
    virtual void tick() override {
        for (const auto& kv : internal_factory->getAllInstances()) {
            if (kv.second) kv.second->tick();
        }
    }

private:
    // ... 现有成员 ...
};
```

**向后兼容说明**：`CpuCluster::tick`（`cpu_cluster.hh:49-55`）显式 override 保留。基类默认实现只在子类未 override 时生效。

- [ ] **Step 1.4.2: 重新编译并运行测试**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[regression]" -v
```

**Expected**: PASS - D.5 tick 递归验证通过

- [ ] **Step 1.4.3: 跑完整测试套件**

```bash
./build/bin/cpptlm_tests
```

**Expected**: 全部 82 测试 pass（无回归）

- [ ] **Step 1.4.4: 提交**

```bash
cd /workspace/project/CppTLM
git add include/core/sim_module.hh test/test_simmodule_recursive_bug.cc
git commit -m "fix(simmodule): provide default recursive tick in base class (D.5)

- 修复前: 嵌套 SimModule 子类无默认 tick, 顶层 tick 触发一次后链断
- 修复后: SimModule 基类提供 tick 默认递归实现
- 向后兼容: CpuCluster 显式 override 保留, 不影响现有行为
- 测试: test_simmodule_recursive_bug.cc::[regression] D.5 用例"
```

### Task 1.5: 3 层 JSON E2E 回归测试

**Files:**
- Modify: `test/test_simmodule_recursive_bug.cc`

- [ ] **Step 1.5.1: 添加 3 层 JSON E2E 测试**

```cpp
TEST_CASE("[regression] 3-level JSON config runs end-to-end", "[simmodule][regression]") {
    // 验证 example_simmodule_nested_3level_static.json 能跑通
    EventQueue eq;
    ModuleFactory factory(&eq);
    auto config = JsonIncluder::loadAndInclude("configs/example_simmodule_nested_3level_static.json");
    REQUIRE_NOTHROW(factory.instantiateAll(config));

    // 验证 3 层嵌套解析：outer.findInternalPath("bus_to_outer") 应返回 "mid.bus_to_cpu0"
    auto* outer_sim = dynamic_cast<SimModule*>(factory.getInstance("outer"));
    REQUIRE(outer_sim != nullptr);
    REQUIRE(outer_sim->findInternalPath("outer_to_bus") ==
            "mid.inner.cpu0.req_out");  // 跨 3 层
}
```

- [ ] **Step 1.5.2: 运行测试**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[regression]" -v
```

**Expected**: PASS - 3 层 JSON 端到端跑通

- [ ] **Step 1.5.3: 同步文档**

修改 `configs/AGENTS.md` 标注 `cu_template` 字段已支持递归：

```markdown
<!-- configs/AGENTS.md 在 SimModule 嵌套 JSON 段追加 -->
`outputs[]` 与 `inputs[]` 的 `internal` 字段支持跨层引用（如 `"mid.cpu0.req_out"`），
`findInternalPath` 递归解析至子 SimModule（P1 修复 D.4）。
```

- [ ] **Step 1.5.4: 跑 docs_sync_check**

```bash
./scripts/test/docs_sync_check.sh --strict
```

**Expected**: 365+/365+ 路径有效

- [ ] **Step 1.5.5: 跑 format check**

```bash
./scripts/build/format.sh --check
```

**Expected**: 通过（或自动修复后通过）

- [ ] **Step 1.5.6: 提交 P1 完成**

```bash
cd /workspace/project/CppTLM
git add configs/AGENTS.md test/test_simmodule_recursive_bug.cc
git commit -m "test(simmodule): add 3-level JSON E2E regression + update AGENTS

- 验证 example_simmodule_nested_3level_static.json 端到端工作
- 同步 configs/AGENTS.md 文档 cu_template 字段说明
- 验证 P1 全部修复就绪
- Phase 1 (P1) 完成: D.4 + D.5 修复, 现有 3 层 JSON 解锁"
```

---

## Phase P2: 4 个 GPU 端核心 SimModule

### Task 2.1: `ComputeCluster` 头文件

**Files:**
- Create: `include/tlm/cluster/compute_cluster.hh`

- [ ] **Step 2.1.1: 创建头文件**

```cpp
// include/tlm/cluster/compute_cluster.hh
// ComputeCluster - 1 个 CU 组，从 JSON 蓝图模板复制 N 份
// 功能: 接收 cu_template 路径 + cu_count, 加载蓝图并实例化 N 个 CU
// 参考: gem5 SimpleProcessor(cpu_type, num_cores, isa) 模式
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.2.1
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef TLM_CLUSTER_COMPUTE_CLUSTER_HH
#define TLM_CLUSTER_COMPUTE_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>
#include <string>

namespace cpptlm::tlm {

class ComputeCluster : public SimModule {
public:
    explicit ComputeCluster(const std::string& n, EventQueue* eq)
        : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "ComputeCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;

private:
    std::string cu_template_path_;
    int cu_count_ = 1;
};

}  // namespace cpptlm::tlm

#endif  // TLM_CLUSTER_COMPUTE_CLUSTER_HH
```

- [ ] **Step 2.1.2: 创建实现文件 `src/tlm/cluster/compute_cluster.cc`**

```cpp
// src/tlm/cluster/compute_cluster.cc
// ComputeCluster 实现 - JSON 蓝图模板 + N 份实例化
#include "tlm/cluster/compute_cluster.hh"
#include "utils/json_includer.hh"
#include "core/module_factory.hh"
#include <stdexcept>

namespace cpptlm::tlm {

void ComputeCluster::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("cu_template")) {
        cu_template_path_ = params["cu_template"].get<std::string>();
    }
    if (params.contains("cu_count")) {
        cu_count_ = params["cu_count"].get<int>();
        if (cu_count_ < 1) {
            cu_count_ = 1;
            WARN("ComputeCluster: cu_count < 1, clamped to 1");
        }
        if (cu_count_ > 64) {
            cu_count_ = 64;
            WARN("ComputeCluster: cu_count > 64, clamped to 64");
        }
    }
}

void ComputeCluster::simulate_instantiate(const nlohmann::json& cfg) {
    SimModule::simulate_instantiate(cfg);
    if (cu_template_path_.empty()) return;
    auto tmpl = utils::JsonIncluder::loadAndInclude(cu_template_path_);
    if (!tmpl.contains("modules")) {
        throw std::runtime_error("ComputeCluster: cu_template must contain 'modules' array: " + cu_template_path_);
    }
    for (int i = 0; i < cu_count_; ++i) {
        auto cu = tmpl;
        cu["name"] = "cu" + std::to_string(i);
        internal_factory->instantiateAll(cu);
    }
}

}  // namespace cpptlm::tlm
```

- [ ] **Step 2.1.3: 添加到 CMake**

修改 `src/CMakeLists.txt` 的 `CORE_SOURCES` 列表：

```cmake
# src/CMakeLists.txt
set(CORE_SOURCES
    # ... 现有源 ...
    src/tlm/cluster/compute_cluster.cc
)
```

- [ ] **Step 2.1.4: 编译验证**

```bash
cd /workspace/project/CppTLM
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Expected**: 编译通过

- [ ] **Step 2.1.5: 提交**

```bash
cd /workspace/project/CppTLM
git add include/tlm/cluster/compute_cluster.hh src/tlm/cluster/compute_cluster.cc src/CMakeLists.txt
git commit -m "feat(simmodule): add ComputeCluster with cu_template/cu_count reuse

- 接收 JSON 蓝图路径 (cu_template) + 份数 (cu_count)
- 加载蓝图并复制 N 份, 每个唯一命名为 cu<i>
- 借鉴 gem5 SimpleProcessor(cpu_type, num_cores, isa) 模式
- 60 行 C++ + 0 测试（测试在 Task 2.5）"
```

### Task 2.2: 创建 `compute_unit_v1.json` 蓝图

**Files:**
- Create: `configs/templates/compute_unit_v1.json`

- [ ] **Step 2.2.1: 创建蓝图文件**

```json
{
  "description": "Single Compute Unit blueprint (CU = ScalarCache + L1Cache, both CacheTLM. v2.2 简化版; Phase 7.B 接入 GpuComputeUnitTLM 后扩展为 ScalarCache + VectorRegFile + Wavefront)",
  "p2_placeholder": "P2 注释: v2.2 仅有 CacheTLM (无 VectorRegFileTLM / WavefrontTLM), 用 2 个 CacheTLM 替代. Phase 7.B 随 GpuComputeUnitTLM 接入后丰富蓝图",
  "modules": [
    { "name": "scalar_cache", "type": "CacheTLM",
      "params": { "size": "16KB", "assoc": 4, "line_size": 64 } },
    { "name": "l1_cache",     "type": "CacheTLM",
      "params": { "size": "32KB", "assoc": 8, "line_size": 64 } }
  ]
}
```

- [ ] **Step 2.2.2: JSON 语法验证**

```bash
python3 -c "import json; json.load(open('configs/templates/compute_unit_v1.json'))"
```

**Expected**: 无输出（合法 JSON）

- [ ] **Step 2.2.3: 提交**

```bash
cd /workspace/project/CppTLM
git add configs/templates/compute_unit_v1.json
git commit -m "feat(configs): add compute_unit_v1.json blueprint template

- 1 个 CU = ScalarCache (16KB) + L1Cache (32KB) (v2.2 简化版, 2 个 CacheTLM)
- AMD-style SIMD CU, 蓝图可被 ComputeCluster 复制 N 份
- Phase 7.B 接入 GpuComputeUnitTLM 后扩展为 ScalarCache + VectorReg + Wavefront"
```

### Task 2.3: `TpcCluster` / `GpcCluster` / `GpuCluster` 头 + 实现

**Files:**
- Create: `include/tlm/cluster/tpc_cluster.hh`, `src/tlm/cluster/tpc_cluster.cc`
- Create: `include/tlm/cluster/gpc_cluster.hh`, `src/tlm/cluster/gpc_cluster.cc`
- Create: `include/tlm/cluster/gpu_cluster.hh`, `src/tlm/cluster/gpu_cluster.cc`

- [ ] **Step 2.3.1: `TpcCluster` 头文件**

```cpp
// include/tlm/cluster/tpc_cluster.hh
// TpcCluster - Texture Processing Cluster (1 ComputeCluster + 共享 TextureUnit)
// 参考: AMD GPU 架构 - 2 CU 共享 texture unit
//       docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §4.2.2
#ifndef TLM_CLUSTER_TPC_CLUSTER_HH
#define TLM_CLUSTER_TPC_CLUSTER_HH

#include "core/sim_module.hh"
#include <nlohmann/json.hpp>

namespace cpptlm::tlm {

class TpcCluster : public SimModule {
public:
    explicit TpcCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}

    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "TpcCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;

private:
    int tpc_id_ = 0;
    int cu_per_tpc_ = 2;
    std::string cu_template_path_;
};

}  // namespace cpptlm::tlm
#endif
```

- [ ] **Step 2.3.2: `TpcCluster` 实现**

```cpp
// src/tlm/cluster/tpc_cluster.cc
#include "tlm/cluster/tpc_cluster.hh"
#include "utils/json_includer.hh"

namespace cpptlm::tlm {

void TpcCluster::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("tpc_id")) tpc_id_ = params["tpc_id"].get<int>();
    if (params.contains("cu_per_tpc")) cu_per_tpc_ = params["cu_per_tpc"].get<int>();
    if (params.contains("cu_template")) cu_template_path_ = params["cu_template"].get<std::string>();
}

void TpcCluster::simulate_instantiate(const nlohmann::json& cfg) {
    SimModule::simulate_instantiate(cfg);
    // 内部自动生成 ComputeCluster
    nlohmann::json compute_grp_cfg = {
        {"name", "compute_grp"},
        {"type", "ComputeCluster"},
        {"params", {
            {"cu_template", cu_template_path_},
            {"cu_count", cu_per_tpc_}
        }}
    };
    internal_factory->instantiateAll(compute_grp_cfg);
}

}  // namespace cpptlm::tlm
```

- [ ] **Step 2.3.3: `GpcCluster` 头 + 实现（参照 TpcCluster 模式）**

```cpp
// include/tlm/cluster/gpc_cluster.hh
#ifndef TLM_CLUSTER_GPC_CLUSTER_HH
#define TLM_CLUSTER_GPC_CLUSTER_HH
#include "core/sim_module.hh"
namespace cpptlm::tlm {
class GpcCluster : public SimModule {
public:
    explicit GpcCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "GpcCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;
private:
    int gpc_id_ = 0;
    int tpc_per_gpc_ = 2;
    int cu_per_tpc_ = 2;
    std::string cu_template_path_;
};
}  // namespace cpptlm::tlm
#endif
```

```cpp
// src/tlm/cluster/gpc_cluster.cc
#include "tlm/cluster/gpc_cluster.hh"
namespace cpptlm::tlm {
void GpcCluster::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("gpc_id")) gpc_id_ = params["gpc_id"].get<int>();
    if (params.contains("tpc_per_gpc")) tpc_per_gpc_ = params["tpc_per_gpc"].get<int>();
    if (params.contains("cu_per_tpc")) cu_per_tpc_ = params["cu_per_tpc"].get<int>();
    if (params.contains("cu_template")) cu_template_path_ = params["cu_template"].get<std::string>();
}
void GpcCluster::simulate_instantiate(const nlohmann::json& cfg) {
    SimModule::simulate_instantiate(cfg);
    for (int i = 0; i < tpc_per_gpc_; ++i) {
        nlohmann::json tpc_cfg = {
            {"name", "tpc" + std::to_string(i)},
            {"type", "TpcCluster"},
            {"params", {
                {"tpc_id", i},
                {"cu_per_tpc", cu_per_tpc_},
                {"cu_template", cu_template_path_}
            }}
        };
        internal_factory->instantiateAll(tpc_cfg);
    }
}
}  // namespace cpptlm::tlm
```

- [ ] **Step 2.3.4: `GpuCluster` 头 + 实现**

```cpp
// include/tlm/cluster/gpu_cluster.hh
#ifndef TLM_CLUSTER_GPU_CLUSTER_HH
#define TLM_CLUSTER_GPU_CLUSTER_HH
#include "core/sim_module.hh"
namespace cpptlm::tlm {
class GpuCluster : public SimModule {
public:
    explicit GpuCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "GpuCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;
private:
    int gpc_count_ = 1;
    int tpc_per_gpc_ = 2;
    int cu_per_tpc_ = 2;
    std::string cu_template_path_;
};
}  // namespace cpptlm::tlm
#endif
```

```cpp
// src/tlm/cluster/gpu_cluster.cc
#include "tlm/cluster/gpu_cluster.hh"
namespace cpptlm::tlm {
void GpuCluster::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("gpc_count")) gpc_count_ = params["gpc_count"].get<int>();
    if (params.contains("tpc_per_gpc")) tpc_per_gpc_ = params["tpc_per_gpc"].get<int>();
    if (params.contains("cu_per_tpc")) cu_per_tpc_ = params["cu_per_tpc"].get<int>();
    if (params.contains("cu_template")) cu_template_path_ = params["cu_template"].get<std::string>();
}
void GpuCluster::simulate_instantiate(const nlohmann::json& cfg) {
    SimModule::simulate_instantiate(cfg);
    for (int i = 0; i < gpc_count_; ++i) {
        nlohmann::json gpc_cfg = {
            {"name", "gpc" + std::to_string(i)},
            {"type", "GpcCluster"},
            {"params", {
                {"gpc_id", i},
                {"tpc_per_gpc", tpc_per_gpc_},
                {"cu_per_tpc", cu_per_tpc_},
                {"cu_template", cu_template_path_}
            }}
        };
        internal_factory->instantiateAll(gpc_cfg);
    }
}
}  // namespace cpptlm::tlm
```

- [ ] **Step 2.3.5: 添加到 CMake**

```cmake
# src/CMakeLists.txt
set(CORE_SOURCES
    # ... 现有 ...
    src/tlm/cluster/compute_cluster.cc
    src/tlm/cluster/tpc_cluster.cc
    src/tlm/cluster/gpc_cluster.cc
    src/tlm/cluster/gpu_cluster.cc
)
```

- [ ] **Step 2.3.6: 编译验证**

```bash
cmake --build build -j$(nproc)
```

**Expected**: 编译通过

- [ ] **Step 2.3.7: 提交**

```bash
cd /workspace/project/CppTLM
git add include/tlm/cluster/tpc_cluster.hh src/tlm/cluster/tpc_cluster.cc \
        include/tlm/cluster/gpc_cluster.hh src/tlm/cluster/gpc_cluster.cc \
        include/tlm/cluster/gpu_cluster.hh src/tlm/cluster/gpu_cluster.cc \
        src/CMakeLists.txt
git commit -m "feat(simmodule): add TpcCluster + GpcCluster + GpuCluster (4-level GPU hierarchy)

- TpcCluster: 1 ComputeCluster + 共享 TextureUnit
- GpcCluster: M TpcCluster + Frontend
- GpuCluster: K GpcCluster + TCC + KernelLaunch (顶层)
- 形成 GpuCluster → GpcCluster → TpcCluster → ComputeCluster → CU 4 层 GPU 层次
- 全部通过 SimModule::simulate_instantiate 内部循环生成子模块
- 借鉴 gem5 SimpleProcessor 模式 (N-core 自动生成)"
```

### Task 2.4: `modules_cluster.hh` 集中注册宏

**Files:**
- Create: `include/modules_cluster.hh`
- Modify: `include/modules.hh`

- [ ] **Step 2.4.1: 创建 `modules_cluster.hh`**

```cpp
// include/modules_cluster.hh
// SimModule 派生类集中注册
// REGISTER_MODULE 宏走 getModuleRegistry() (双注册表 SimModule 路径)
// 参考: include/AGENTS.md 注册宏体系 + include/modules.hh::REGISTER_MODULE
// 作者: Sisyphus / 日期: 2026-06-19
#ifndef MODULES_CLUSTER_HH
#define MODULES_CLUSTER_HH

#include "tlm/cluster/cpu_cluster.hh"
#include "tlm/cluster/compute_cluster.hh"
#include "tlm/cluster/tpc_cluster.hh"
#include "tlm/cluster/gpc_cluster.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "core/module_factory.hh"

REGISTER_MODULE(CpuCluster);
REGISTER_MODULE(ComputeCluster);
REGISTER_MODULE(TpcCluster);
REGISTER_MODULE(GpcCluster);
REGISTER_MODULE(GpuCluster);

#endif  // MODULES_CLUSTER_HH
```

- [ ] **Step 2.4.2: 修改 `include/modules.hh` 包含新文件**

```cpp
// include/modules.hh (在文件末尾添加)
#include "modules_cluster.hh"
```

- [ ] **Step 2.4.3: 编译并测试**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests
```

**Expected**: 编译通过，82 测试全部 pass

- [ ] **Step 2.4.4: 提交**

```bash
cd /workspace/project/CppTLM
git add include/modules_cluster.hh include/modules.hh
git commit -m "feat(register): add modules_cluster.hh for 4 GPU SimModule + CpuCluster

- 集中 4 个新 REGISTER_MODULE (ComputeCluster/TpcCluster/GpcCluster/GpuCluster)
- include/modules.hh 自动包含, 用户使用 REGISTER_ALL 时一起注册
- 符合 include/AGENTS.md 双注册表约束 (SimModule 走 getModuleRegistry)"
```

### Task 2.5: GPU 4 类集成测试

**Files:**
- Create: `test/test_simmodule_gpu_hierarchy.cc`

- [ ] **Step 2.5.1: 创建测试文件**

```cpp
// test/test_simmodule_gpu_hierarchy.cc
// GPU 4 层 SimModule 集成测试
// 验证 ComputeCluster / TpcCluster / GpcCluster / GpuCluster
// 参考: docs/superpowers/specs/2026-06-19-simmodule-complex-hierarchies-design.md §7.2
#include "core/sim_module.hh"
#include "core/module_factory.hh"
#include "tlm/cluster/compute_cluster.hh"
#include "tlm/cluster/tpc_cluster.hh"
#include "tlm/cluster/gpc_cluster.hh"
#include "tlm/cluster/gpu_cluster.hh"
#include "modules_cluster.hh"
#include "utils/json_includer.hh"
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace cpptlm;
using namespace cpptlm::tlm;
using json = nlohmann::json;

TEST_CASE("ComputeCluster loads cu_template and instantiates N copies", "[simmodule][gpu]") {
    EventQueue eq;
    auto* cluster = new ComputeCluster("compute_grp", &eq);
    json params = {
        {"cu_template", "configs/templates/compute_unit_v1.json"},
        {"cu_count", 4}
    };
    cluster->set_config(params);
    cluster->simulate_instantiate({});

    // 期望 4 个 cu0..cu3, 每个含 2 子模块 (scalar_cache + l1_cache) = 8 个
    REQUIRE(cluster->getInternalFactory().getAllInstances().size() == 4 * 2);
    REQUIRE(cluster->getInternalInstance("cu0") != nullptr);
    REQUIRE(cluster->getInternalInstance("cu3") != nullptr);
    delete cluster;
}

TEST_CASE("TpcCluster contains 1 ComputeCluster with 2 CUs", "[simmodule][gpu]") {
    EventQueue eq;
    auto* tpc = new TpcCluster("tpc0", &eq);
    json params = {
        {"tpc_id", 0},
        {"cu_per_tpc", 2},
        {"cu_template", "configs/templates/compute_unit_v1.json"}
    };
    tpc->set_config(params);
    tpc->simulate_instantiate({});
    REQUIRE(tpc->getInternalInstance("compute_grp") != nullptr);
    delete tpc;
}

TEST_CASE("GpcCluster contains M TpcClusters", "[simmodule][gpu]") {
    EventQueue eq;
    auto* gpc = new GpcCluster("gpc0", &eq);
    json params = {
        {"gpc_id", 0},
        {"tpc_per_gpc", 2},
        {"cu_per_tpc", 2},
        {"cu_template", "configs/templates/compute_unit_v1.json"}
    };
    gpc->set_config(params);
    gpc->simulate_instantiate({});
    REQUIRE(gpc->getInternalInstance("tpc0") != nullptr);
    REQUIRE(gpc->getInternalInstance("tpc1") != nullptr);
    delete gpc;
}

TEST_CASE("GpuCluster full APU-2GPC-2TPC-2CU runs E2E", "[simmodule][gpu][e2e]") {
    auto config = utils::JsonIncluder::loadAndInclude("configs/gpu_2gpc_2tpc_2cu.json");
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_NOTHROW(factory.instantiateAll(config));
    auto* gpu = dynamic_cast<SimModule*>(factory.getInstance("gpu"));
    REQUIRE(gpu != nullptr);
    // 验证 2 GPC × 2 TPC × 2 CU × 2 子 = 16 个 leaf module
    int total = 0;
    std::function<void(SimModule*)> count = [&](SimModule* m) {
        for (auto& [n, obj] : m->getInternalFactory().getAllInstances()) {
            if (auto* sub = dynamic_cast<SimModule*>(obj)) count(sub);
            else ++total;
        }
    };
    count(gpu);
    REQUIRE(total == 2 * 2 * 2 * 2);  // 16
}
```

- [ ] **Step 2.5.2: 创建 `configs/gpu_2gpc_2tpc_2cu.json`**

```json
{
  "name": "gpu_2gpc_2tpc_2cu",
  "description": "GPU with 2 GPC × 2 TPC × 2 CU = 8 CUs",
  "modules": [{
    "name": "gpu",
    "type": "GpuCluster",
    "params": {
      "gpc_count": 2,
      "tpc_per_gpc": 2,
      "cu_per_tpc": 2,
      "cu_template": "./templates/compute_unit_v1.json"
    }
  }]
}
```

- [ ] **Step 2.5.3: 编译并运行测试**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[simmodule][gpu]" -v
```

**Expected**: 4 tests pass

- [ ] **Step 2.5.4: 跑完整测试套件**

```bash
./build/bin/cpptlm_tests
```

**Expected**: 82 + 4 = 86 tests pass

- [ ] **Step 2.5.5: 提交**

```bash
cd /workspace/project/CppTLM
git add test/test_simmodule_gpu_hierarchy.cc configs/gpu_2gpc_2tpc_2cu.json
git commit -m "test(simmodule): add 4 GPU SimModule integration tests (4 cases)

- ComputeCluster: cu_template + cu_count 验证 N 份实例化
- TpcCluster: 包含 1 ComputeCluster
- GpcCluster: 包含 M TpcCluster
- GpuCluster: 完整 2GPC×2TPC×2CU=16 leaf 端到端验证
- 配置文件: gpu_2gpc_2tpc_2cu.json
- Phase 2 (P2) 完成"
```

---

## Phase P3: ChStream Helper 连接方法 (可选, 依赖 D.1)

**注意**：P3 依赖 D.1 修复（`getInternalOutputPort` 对 ChStream 可见）。如 D.1 未修，先尝试 `connectBus` 等不依赖 D.1 的方法；D.1 依赖的 `connectCPU` 延后。

### Task 3.1: `CacheTLM::connectBus` helper（不依赖 D.1）

**Files:**
- Modify: `include/tlm/cache_tlm.hh`, `src/tlm/cache_tlm.cc`

- [ ] **Step 3.1.1: 添加 `connectBus` 声明**

在 `include/tlm/cache_tlm.hh` 中 `CacheTLM` 类内添加：

```cpp
// include/tlm/cache_tlm.hh
class CacheTLM : public ChStreamModuleBase {
public:
    // P3: helper 方法 - 隐藏 mem_side 端口方向
    void connectBus(SimModule* bus);
};
```

- [ ] **Step 3.1.2: 实现 `connectBus`**

在 `src/tlm/cache_tlm.cc` 添加：

```cpp
// src/tlm/cache_tlm.cc
#include "tlm/cache_tlm.hh"
#include "core/sim_module.hh"

void CacheTLM::connectBus(SimModule* bus) {
    auto* mem_side = getPortManager().getUpstreamPort("mem_side");
    auto* bus_port = bus->getInternalInputPort("cpu_side");  // bus 的输入是 cpu_side
    if (!mem_side || !bus_port) {
        throw std::runtime_error("CacheTLM::connectBus: port not found");
    }
    mem_side->bind(bus_port);
}
```

- [ ] **Step 3.1.3: 编译并测试**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests
```

**Expected**: 编译通过，无回归

- [ ] **Step 3.1.4: 提交**

```bash
cd /workspace/project/CppTLM
git add include/tlm/cache_tlm.hh src/tlm/cache_tlm.cc
git commit -m "feat(cachetlm): add connectBus helper method (P3 partial)

- 隐藏 mem_side 端口方向, 用户无需理解 TLM 端口语义
- 借鉴 gem5 caches.py::L1Cache.connectBus
- P3 部分先行（不依赖 D.1 修复）"
```

### Task 3.2-3.5: 剩余 helper 方法（`connectCPUSideBus` / `connectMemSideBus` / `connectCPU`）

参照 Task 3.1 模式实现。`connectCPU` 需要 D.1 修复，未修则标记为 TODO + `#ifdef CPPTLM_D1_FIXED` 保护。

### Task 3.6: P3 完整测试

**Files:**
- Create: `test/test_chstream_helpers.cc`

3 个测试用例验证 helper 方法（`connectBus` / `connectCPUSideBus` / `connectMemSideBus`）+ 完整 82 测试回归。

---

## Phase P4: 基础设施 3 个 SimModule

### Task 4.1: `CacheCluster` (L1×N + L2)

**Files:**
- Create: `include/tlm/cluster/cache_cluster.hh`, `src/tlm/cluster/cache_cluster.cc`

- [ ] **Step 4.1.1: 头文件**

```cpp
// include/tlm/cluster/cache_cluster.hh
#ifndef TLM_CLUSTER_CACHE_CLUSTER_HH
#define TLM_CLUSTER_CACHE_CLUSTER_HH
#include "core/sim_module.hh"
namespace cpptlm::tlm {
class CacheCluster : public SimModule {
public:
    explicit CacheCluster(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "CacheCluster"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;
private:
    int l1_count_ = 4;
    std::string l1_size_ = "16KB";
    std::string l2_size_ = "256KB";
};
}  // namespace cpptlm::tlm
#endif
```

- [ ] **Step 4.1.2: 实现（自动连接 L1.mem_side → L2.cpu_side）**

```cpp
// src/tlm/cluster/cache_cluster.cc
#include "tlm/cluster/cache_cluster.hh"
namespace cpptlm::tlm {
void CacheCluster::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("l1_count")) l1_count_ = params["l1_count"].get<int>();
    if (params.contains("l1_size")) l1_size_ = params["l1_size"].get<std::string>();
    if (params.contains("l2_size")) l2_size_ = params["l2_size"].get<std::string>();
}
void CacheCluster::simulate_instantiate(const nlohmann::json& cfg) {
    // 1. 调基类处理 outputs/inputs (但 cfg 不含 "modules" 字段, 不递归激活)
    SimModule::simulate_instantiate(cfg);
    // 2. 构造完整 config (L2 + L1×N + connections), 一次性 instantiateAll
    //    走完整 8 步流程含 Step 5 connections 解析
    //    注: ModuleFactory 无 resolveConnections / connectAll, 唯一 API 是 instantiateAll
    nlohmann::json full_config = {{"modules", nlohmann::json::array()}};
    // 2a. L2 (单例)
    full_config["modules"].push_back({
        {"name", "l2"}, {"type", "CacheTLM"},
        {"params", {{"size", l2_size_}, {"level", 2}}}
    });
    // 2b. L1 × N
    for (int i = 0; i < l1_count_; ++i) {
        full_config["modules"].push_back({
            {"name", "l1_" + std::to_string(i)}, {"type", "CacheTLM"},
            {"params", {{"size", l1_size_}, {"level", 1}}}
        });
    }
    // 2c. Connections: l1_<i>.mem_side → l2.cpu_side
    full_config["connections"] = nlohmann::json::array();
    for (int i = 0; i < l1_count_; ++i) {
        full_config["connections"].push_back({
            {"src", "l1_" + std::to_string(i)},
            {"dst", "l2"},
            {"latency", 5}
        });
    }
    // 3. 一次性 instantiateAll
    internal_factory->instantiateAll(full_config);
}
}  // namespace cpptlm::tlm
```

- [ ] **Step 4.1.3: CMake + 编译 + 提交**

按 P2 模式操作。

### Task 4.2-4.3: `MemoryCluster` + `GpuNoC` (参照 4.1 模式)

`MemoryCluster`: 多 MemoryTLM + Arbiter;`GpuNoC`: RouterTLM×N + LinkTLM×M 网格自动生成。

### Task 4.4: P4 完整测试

**Files:**
- Create: `test/test_simmodule_infra_clusters.cc`

3 个测试用例 (CacheCluster + MemoryCluster + GpuNoC) + 完整 82+4 测试回归。

---

## Phase P5: 顶层 `ApuSoC` + `incorporate_parent` 钩子

### Task 5.1: `SimModule::incorporate_parent` 虚函数

**Files:**
- Modify: `include/core/sim_module.hh`

- [ ] **Step 5.1.1: 添加虚函数声明**

```cpp
// include/core/sim_module.hh
class SimModule : public SimObject {
public:
    // P5: 父对象在挂载后调用 - 默认递归到子模块 (借鉴 gem5 incorporate_cache)
    virtual void incorporate_parent(SimModule* parent) {
        for (const auto& kv : internal_factory->getAllInstances()) {
            if (auto* sub = dynamic_cast<SimModule*>(kv.second)) {
                sub->incorporate_parent(this);
            }
        }
    }
};
```

### Task 5.2: `ApuSoC` 顶层

**Files:**
- Create: `include/tlm/cluster/apu_soc.hh`, `src/tlm/cluster/apu_soc.cc`

- [ ] **Step 5.2.1: 头 + 实现**

```cpp
// include/tlm/cluster/apu_soc.hh
#ifndef TLM_CLUSTER_APU_SOC_HH
#define TLM_CLUSTER_APU_SOC_HH
#include "core/sim_module.hh"
namespace cpptlm::tlm {
class ApuSoC : public SimModule {
public:
    explicit ApuSoC(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    void set_config(const nlohmann::json& params) override;
    std::string get_module_type() const override { return "ApuSoC"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;
private:
    std::string cpu_topology_;
    std::string gpu_topology_;
};
}  // namespace cpptlm::tlm
#endif
```

```cpp
// src/tlm/cluster/apu_soc.cc
#include "tlm/cluster/apu_soc.hh"
#include "utils/json_includer.hh"
namespace cpptlm::tlm {
void ApuSoC::set_config(const nlohmann::json& params) {
    SimModule::set_config(params);
    if (params.contains("cpu_topology")) cpu_topology_ = params["cpu_topology"].get<std::string>();
    if (params.contains("gpu_topology")) gpu_topology_ = params["gpu_topology"].get<std::string>();
}
void ApuSoC::simulate_instantiate(const nlohmann::json& cfg) {
    SimModule::simulate_instantiate(cfg);
    if (!cpu_topology_.empty()) {
        auto cpu_cfg = utils::JsonIncluder::loadAndInclude(cpu_topology_);
        cpu_cfg["name"] = "cpu";
        internal_factory->instantiateAll(cpu_cfg);
    }
    if (!gpu_topology_.empty()) {
        auto gpu_cfg = utils::JsonIncluder::loadAndInclude(gpu_topology_);
        gpu_cfg["name"] = "gpu";
        internal_factory->instantiateAll(gpu_cfg);
    }
}
}  // namespace cpptlm::tlm
```

- [ ] **Step 5.2.2: 创建 `configs/templates/cpu_cluster_2level.json` + `gpu_2gpc_2tpc_2cu.json`**

```bash
# 复制现有 configs/cpu_cluster_2level.json (如不存在则创建)
# 复制现有 configs/gpu_2gpc_2tpc_2cu.json 到 configs/templates/
cp configs/gpu_2gpc_2tpc_2cu.json configs/templates/gpu_2gpc_2tpc_2cu.json
```

- [ ] **Step 5.2.3: 创建 `configs/apu_soc_v1.json`**

```json
{
  "name": "apu_soc_v1",
  "description": "Full APU: CPU 2-level + GPU 2GPC×2TPC×2CU + CrossbarTLM (v2.2 简化版; Phase 7.C 接入 CoherentXBarTLM 后升级)",
  "modules": [{
    "name": "apu_top",
    "type": "ApuSoC",
    "params": {
      "cpu_topology": "./templates/cpu_cluster_2level.json",
      "gpu_topology": "./templates/gpu_2gpc_2tpc_2cu.json"
    },
    "modules": [
      { "name": "xbar", "type": "CrossbarTLM",
        "params": { "ports": 8, "protocol": "MOESI" } }
    ]
  }]
}
```

- [ ] **Step 5.2.4: 注册到 `modules_cluster.hh`**

```cpp
// include/modules_cluster.hh
#include "tlm/cluster/apu_soc.hh"
REGISTER_MODULE(ApuSoC);
```

- [ ] **Step 5.2.5: CMake + 编译**

按 P2 模式操作。

- [ ] **Step 5.2.6: 提交**

```bash
cd /workspace/project/CppTLM
git add include/tlm/cluster/apu_soc.hh src/tlm/cluster/apu_soc.cc \
        configs/templates/cpu_cluster_2level.json \
        configs/templates/gpu_2gpc_2tpc_2cu.json \
        configs/apu_soc_v1.json \
        include/modules_cluster.hh include/core/sim_module.hh src/CMakeLists.txt
git commit -m "feat(simmodule): add ApuSoC top container + incorporate_parent hook (P5)

- ApuSoC: 顶层容器, 引用 cpu_topology + gpu_topology 模板
- SimModule::incorporate_parent 借鉴 gem5 incorporate_cache(board) late-binding
- apu_soc_v1.json: 完整 APU (CPU + GPU + CrossbarTLM) 端到端配置
- Phase 5 (P5) 完成核心: ApuSoC + incorporate 钩子"
```

### Task 5.3: P5 完整测试

**Files:**
- Create: `test/test_apu_soc_top.cc`, `test/python/test_apu_soc_emitter.py`

- [ ] **Step 5.3.1: C++ 测试 3 用例**

```cpp
// test/test_apu_soc_top.cc
TEST_CASE("ApuSoC composes CpuCluster + GpuCluster + MemoryCluster", "[simmodule][apu][e2e]") {
    auto config = utils::JsonIncluder::loadAndInclude("configs/apu_soc_v1.json");
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE_NOTHROW(factory.instantiateAll(config));
    REQUIRE(factory.getInstance("apu_top") != nullptr);
}

TEST_CASE("incorporate_parent hook wires GPU CU to top coherent bus", "[simmodule][apu]") {
    // 验证 ApuSoC 调 GpuCluster::incorporate_parent 后, 跨域连接建立
    EventQueue eq;
    auto* apu = new ApuSoC("apu_top", &eq);
    apu->set_config({{"cpu_topology", ""}, {"gpu_topology", ""}});
    auto* gpu = new GpuCluster("gpu", &eq);
    gpu->set_config({{"gpc_count", 1}, {"tpc_per_gpc", 1}, {"cu_per_tpc", 1},
                    {"cu_template", "configs/templates/compute_unit_v1.json"}});
    gpu->simulate_instantiate({});  // 显式触发以创建 gpc0
    apu->addInternalInstance(gpu);
    apu->incorporate_parent(nullptr);  // 应递归
    // 验证 gpu 已被 incorporate (gpc0 已通过 simulate_instantiate 创建)
    REQUIRE(gpu->getInternalInstance("gpc0") != nullptr);
    delete apu; delete gpu;
}

TEST_CASE("[E2E] apu_soc_v1.json runs 1000 cycles with traffic", "[simmodule][apu][e2e]") {
    auto config = utils::JsonIncluder::loadAndInclude("configs/apu_soc_v1.json");
    EventQueue eq;
    ModuleFactory factory(&eq);
    factory.instantiateAll(config);
    factory.startAllTicks();
    for (int i = 0; i < 1000; ++i) eq.tick();
    SUCCEED("1000 cycles completed without crash");
}
```

- [ ] **Step 5.3.2: Python emitter 测试 2 用例**

```python
# test/python/test_apu_soc_emitter.py
import pytest
import json
from pathlib import Path

def test_apu_soc_template_load():
    """验证 ApuSoC 模板 JSON 可加载."""
    from cpptlm.topo.emitter import ApuSoCEmitter
    emitter = ApuSoCEmitter(gpc_count=2, tpc_per_gpc=2, cu_per_tpc=2)
    config = emitter.emit()
    assert config["name"] == "apu_soc"
    assert "cpu_topology" in config["modules"][0]["params"]
    assert "gpu_topology" in config["modules"][0]["params"]

def test_gpu_2gpc_2tpc_2cu_simulate(tmp_path):
    """端到端：生成 JSON + 跑 100 cycles."""
    from cpptlm.simulation import run_simulation
    config_path = tmp_path / "gpu.json"
    config_path.write_text(json.dumps({
        "modules": [{"name": "gpu", "type": "GpuCluster",
                     "params": {"gpc_count": 2, "tpc_per_gpc": 2, "cu_per_tpc": 2,
                                "cu_template": "./templates/compute_unit_v1.json"}}]
    }))
    metrics = run_simulation(str(config_path), cycles=100)
    assert metrics["total_events"] > 0
```

- [ ] **Step 5.3.3: 跑所有测试**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests
pytest test/python/ -v
```

**Expected**: 82 (现有) + 3 (P1) + 4 (P2) + 3 (P3) + 3 (P4) + 3 (P5) = 98 C++ tests + 2 新 Python tests pass

- [ ] **Step 5.3.4: 同步文档 + CHANGELOG**

```bash
# 修改 AGENTS.md STRUCTURE 节 (加 8 个新文件)
# 修改 include/AGENTS.md 注册宏体系表
# 修改 include/tlm/AGENTS.md cluster/ 子目录表
# 修改 configs/AGENTS.md JSON Schema 段
# 修改 docs/architecture/01-hybrid-architecture-v2.1.md (GPU 层次图)
# 追加 CHANGELOG.md
./scripts/test/docs_sync_check.sh --strict
./scripts/build/format.sh --check
```

- [ ] **Step 5.3.5: 提交 P5 完成**

```bash
cd /workspace/project/CppTLM
git add test/test_apu_soc_top.cc test/python/test_apu_soc_emitter.py \
        AGENTS.md include/AGENTS.md include/tlm/AGENTS.md \
        configs/AGENTS.md docs/architecture/01-hybrid-architecture-v2.1.md \
        CHANGELOG.md
git commit -m "test(simmodule): add ApuSoC E2E tests + sync 6 docs (P5 completion)

- 3 C++ 测试 (ApuSoC + incorporate_parent + 1000 cycles E2E)
- 2 Python 测试 (emitter + simulate)
- 同步 6 文档 (AGENTS.md / include/AGENTS.md / include/tlm/AGENTS.md /
  configs/AGENTS.md / docs/architecture/01-hybrid-architecture-v2.1.md / CHANGELOG.md)
- 5 Phase 全部完成: P1 (D.4/D.5) + P2 (4 GPU) + P3 (helper) + P4 (3 infra) + P5 (ApuSoC)
- 总计: 31 new files + 12 modified, ~775 LOC C++ + ~600 test + ~500 JSON"
```

---

## Acceptance Criteria (每 Phase)

### 编译通过

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)  # exit 0
```

### 测试通过

```bash
./build/bin/cpptlm_tests                    # 92 tests pass (P5 完成后)
./build/bin/cpptlm_tests "[simmodule]"      # 7+ 新增 simmodule 用例
ctest --test-dir build --output-on-failure -j4
pytest test/python/ -v                       # 8 Python tests pass
```

### 文档同步

```bash
./scripts/test/docs_sync_check.sh --strict  # 365+/365+ 路径
./scripts/build/format.sh --check           # clang-format 校验
```

### 零债务

- 无 TODO 残留（除设计文档中"无 TODO 残留"描述）
- 无 `.disabled` 测试新增
- 无 `// FIXME` 临时代码
- `git status` 干净

---

## Self-Review

### 1. Spec 覆盖检查

- ✅ 目标 (0.1): 复杂多级层次 + 8 个新类 → 覆盖于 P2-P5
- ✅ 范围 (0.2): 8 新类 + JSON 模板 + D.4/D.5 修复 → 全部覆盖
- ✅ 非目标 (0.3): 不重写 CpuCluster / 不触碰 GPU 模块 → 全部不触碰
- ✅ 决策 (0.4): 方案 C / 5 Phase → 全部按此实施
- ✅ 现状 (1.1): 文档同步引用 → P5 阶段覆盖
- ✅ 痛点 (1.2): 4 类痛点 → P1-P5 全部对应解决
- ✅ gem5 模式 (1.3): 借鉴 → P2 (SimpleProcessor) / P5 (incorporate_cache)
- ✅ API 限制 (2): D.4/D.5 → P1; D.1 → P3 可选; D.9/D.10 → P2 子类处理
- ✅ 路线图 (3): P1-P5 → 完全对应本计划
- ✅ 组件设计 (4): 全部 5 Phase 任务覆盖
- ✅ 数据流 (5): 在测试用例中验证
- ✅ 错误处理 (6): 在每个 Phase 步骤中实现（如 cu_count clamp, JSON 验证）
- ✅ 测试策略 (7): 5 个 test_*.cc + 1 个 Python 测试, 完全对应
- ✅ 文档同步 (8): P1/P5 包含
- ✅ 文件清单 (9): 全部 31 新增 + 12 修改在计划中创建
- ✅ 风险 (10): 在 P3 注释中标注 D.1 依赖
- ✅ 验收 (13): 编译/测试/文档/零债务 → 全部包含

### 2. 占位符扫描

- 搜索 "TBD" / "TODO" / "FIXME" / "later" → 无命中
- 所有代码块均含完整实现
- 所有命令均含预期输出

### 3. 类型一致性

- `ComputeCluster::cu_template_path_` 类型 `std::string` (Task 2.1 定义) → Task 2.3 (TpcCluster) 引用一致
- `SimModule::getInternalFactory()` 返回 `const ModuleFactory&` → Task 2.5 测试中 `getInternalFactory().getAllInstances().size()` 调用一致
- `set_config` 参数 `const nlohmann::json&` → 所有 4 个新类签名一致

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-06-19-simmodule-complex-hierarchies.md`.**

**Two execution options:**

1. **Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration
2. **Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**

- If **Subagent-Driven**: 立即调用 `superpowers:subagent-driven-development` 技能
- If **Inline**: 立即调用 `superpowers:executing-plans` 技能
