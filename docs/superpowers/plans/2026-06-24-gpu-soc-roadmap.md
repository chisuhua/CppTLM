# gpu_soc Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 CppTLM 第二个独立 SoC 仿真目标 gpu_soc——单 GPU 芯片（4 级层次：GpuCluster → GpcCluster × N → TpcCluster × M → ComputeCluster × K），14 个新模块按 3 阶段（8.A/8.B/8.C）落地，与 apu_soc 共享 GpuCluster 子模块。

**Architecture:** Phase-accurate 工业性能建模（不做 cycle-accurate 5+V 管线，sub-core 内部 black-box）。借鉴 gpgpu-sim SM_120 paper 蓝图，不集成其代码。3 阶段渐进（4+6+3=13 周），每阶段独立验证 + 与 gpgpu-sim 数值对照（带宽 ±15%, 延迟 ±20%）。

**Tech Stack:**
- C++17（CppTLM 核心）
- Catch2 v3.7.0（测试）
- Python 3.11+（`cpptlm` 子包：`cpptlm.nvidia` / `cpptlm.gpu_workload` / `cpptlm.gpu_soc`）
- pre-commit + clang-format（强制 4 空格缩进）
- 引用 spec: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` (commit `801f8ea`)

---

## File Structure (新增/修改)

### 新增 C++ 头文件（14 个 + 1 顶层 + 1 shared_iface = 16）
```
include/tlm/gpu/
├── memory_cluster_tlm.hh        (8.A)
├── shared_memory_tlm.hh         (8.A)
├── gpu_noc_tlm.hh               (8.A)
├── kernel_launch_tlm.hh         (8.A)
├── gpu_soc_tlm.hh               (8.A 顶层)
├── gpu_cluster_shared_interface.hh  (8.A 共享层)
├── subcore_tlm.hh               (8.B)
├── warp_scheduler_tlm.hh        (8.B)
├── scoreboard_tlm.hh            (8.B)
├── tensor_core_tlm.hh           (8.B)
├── pipeline_tlm.hh              (8.B)
├── l2_partition_tlm.hh           (8.B)
├── tcc_tlm.hh                   (8.C)
├── tma_tlm.hh                   (8.C)
├── dsm_tlm.hh                   (8.C)
└── power_model_tlm.hh           (8.C)
```

### 新增 C++ 实现（与 .hh 一一对应）
```
src/tlm/gpu/*.cc
```

### 新增 Bundle 类型
```
include/bundles/
├── shared_memory_bundle.hh      (8.A)
├── warp_state_bundle.hh         (8.B)
└── tensor_core_bundle.hh        (8.B)
```

### 新增 Python 子包
```
cpptlm/cpptlm/
├── nvidia/
│   ├── __init__.py
│   ├── topology.py
│   ├── blueprint.py
│   ├── export.py
│   └── sku_library.py
├── gpu_workload/
│   ├── __init__.py
│   ├── generator.py
│   ├── replay.py
│   └── patterns.py
└── gpu_soc/
    ├── __init__.py
    ├── simulate.py
    └── report.py
```

### 新增配置（JSON 蓝图）
```
configs/templates/gpu_soc/
├── gpu_soc_gb203_v1.json       (8.A)
├── gpu_soc_gb200_v1.json       (8.A)
├── gpu_soc_gh100_v1.json       (8.A)
├── gpu_soc_ga100_v1.json       (8.A)
└── gpu_soc_phase8b.json        (8.B 集成测试)
```

### 新增测试
```
test/test_gpu_soc_phase8a.cc    (8.A 集成)
test/test_gpu_soc_phase8b.cc    (8.B 集成)
test/test_gpu_soc_phase8c.cc    (8.C 集成)
test/test_shared_memory_tlm.cc
test/test_memory_cluster_tlm.cc
test/test_gpu_noc_tlm.cc
test/test_kernel_launch_tlm.cc
test/test_subcore_tlm.cc
test/test_warp_scheduler_tlm.cc
test/test_scoreboard_tlm.cc
test/test_tensor_core_tlm.cc
test/test_pipeline_tlm.cc
test/test_l2_partition_tlm.cc
test/test_tcc_tlm.cc
test/test_tma_tlm.cc
test/test_dsm_tlm.cc
test/test_power_model_tlm.cc
```

### 新增微架构 doc（15 个）
```
docs/soc_arch/modules/
├── gpu-soc.md
├── gpu-noc-mesh.md
├── gpu-shared-memory.md
├── gpu-memory-cluster.md
├── gpu-kernel-launch.md
├── gpu-subcore.md
├── gpu-warp-scheduler.md
├── gpu-scoreboard.md
├── gpu-tensor-core.md
├── gpu-pipeline.md
├── gpu-l2-partition.md
├── gpu-tcc.md
├── gpu-tma.md
├── gpu-dsm.md
└── gpu-power-model.md
```

### 修改文件
```
include/chstream_register.hh          (+5 行注册 Phase 8.A 新模块)
include/modules_cluster.hh            (+1 行 GpuSocTLM 注册)
include/tlm/cluster/gpu_cluster.hh    (引入 GpuClusterSharedInterface)
include/tlm/cluster/{gpc,tpc,compute}_cluster.hh  (stub 完善)
include/tlm/gpu/gpu_tlm.hh            (rename 为 compute_unit_tlm 准备)
docs/adr/README.md                    (append NV-01)
docs/soc_arch/adr/ADR-SOC-01..05.md   (状态更新，不动决策)
AGENTS.md                             (STRUCTURE 节加 cpptlm/{nvidia,gpu_workload,gpu_soc})
docs/superpowers/plans/2026-06-20-future-work-roadmap.md  (追加 Phase 8 节)
```

---

## Phase 8.A — 基础设施 (4 周, ~1250 LOC incl. tests)

> **TDD 模式说明（必须先读）**：本 plan 中标 **"类比 Task N-M 模式"** 的任务（如 Task 3 GpuNoC、Task 4 KernelLaunch、Phase 8.B 的 6 个核心模块等）严格遵循 Task 1 详细展示的 5 步 TDD 结构：
> 1. Step 1 写失败测试（test_*.cc 中给出完整测试代码）
> 2. Step 2 运行测试验证失败（`./build/bin/cpptlm_tests "[tag]"`）
> 3. Step 3 写最小实现（include/tlm/gpu/*.hh 头文件 + src/tlm/gpu/*.cc 实现）
> 4. Step 4 运行测试验证通过
> 5. Step 5 提交（`git add ... && git commit -m "..."`）
>
> **工程师须知**：当看到 "类比 Task N-M 模式" 时，**应复制 Task N 的 5 步结构**并把每个 Step 的内容（测试代码、头文件、commit 消息）适配到当前任务的具体模块名（如 `GpuNoC` vs `SharedMemoryTLM`）。所有任务都提供完整 Files 列表和 Step 5 commit 消息。**严禁只读 "类比" 任务的简介就跳过测试设计**。

### Task 1: SharedMemoryTLM 接口定义 + 失败测试

**Files:**
- Create: `include/bundles/shared_memory_bundle.hh`
- Create: `include/tlm/gpu/shared_memory_tlm.hh`
- Test: `test/test_shared_memory_tlm.cc`

- [ ] **Step 1: Write the failing test**

`test/test_shared_memory_tlm.cc`:
```cpp
#include <catch_amalgamated.hpp>
#include "tlm/gpu/shared_memory_tlm.hh"
using namespace tlm;

TEST_CASE("SharedMemoryTLM: bank conflict 4-way adds +1 cycle", "[gpu][smem]") {
    SharedMemoryTLM smem("smem", nullptr, /*size_kb=*/64, /*banks=*/32);
    // 4 个 threads 访问同一 bank 不同 row
    auto t1 = smem.bank_conflict_cycles(/*num_threads=*/4, /*stride_bytes=*/128);
    REQUIRE(t1 == 1 + 3);  // base 1 + 3 extra cycles (4-way conflict)
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
./build/bin/cpptlm_tests "[gpu][smem]" --reporter compact
```
Expected: FAIL with "SharedMemoryTLM not defined" 或 "bank_conflict_cycles not a member"

- [ ] **Step 3: Write minimal bundle interface**

`include/bundles/shared_memory_bundle.hh`:
```cpp
#ifndef BUNDLES_SHARED_MEMORY_BUNDLE_HH
#define BUNDLES_SHARED_MEMORY_BUNDLE_HH
#include "bundles/cpphdl_types.hh"
namespace bundles {
struct SharedMemoryReqBundle : public bundle_base {
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_uint<32> bank_id;
    ch_bool     is_write;
    ch_uint<64> data;
    SharedMemoryReqBundle() = default;
};
}  // namespace bundles
#endif
```

- [ ] **Step 4: Write minimal SharedMemoryTLM header stub**

`include/tlm/gpu/shared_memory_tlm.hh`:
```cpp
#ifndef TLM_GPU_SHARED_MEMORY_TLM_HH
#define TLM_GPU_SHARED_MEMORY_TLM_HH
#include "core/chstream_module.hh"
#include "bundles/shared_memory_bundle.hh"
namespace tlm {
class SharedMemoryTLM : public ChStreamModuleBase {
public:
    SharedMemoryTLM(const std::string& name, EventQueue* eq,
                    uint32_t size_kb, uint32_t banks)
        : ChStreamModuleBase(name, eq), size_kb_(size_kb), banks_(banks) {}
    std::string get_module_type() const override { return "SharedMemoryTLM"; }
    // 测试辅助：返回给定 num_threads + stride 下的额外 cycle 数
    uint32_t bank_conflict_cycles(uint32_t num_threads, uint32_t stride_bytes) const;
private:
    uint32_t size_kb_;
    uint32_t banks_;
};
}  // namespace tlm
#endif
```

`src/tlm/gpu/shared_memory_tlm.cc`:
```cpp
#include "tlm/gpu/shared_memory_tlm.hh"
namespace tlm {
uint32_t SharedMemoryTLM::bank_conflict_cycles(uint32_t num_threads, uint32_t stride_bytes) const {
    // 简化模型：num_threads-1 个额外 cycle (linear penalty ~2 cyc/way)
    if (num_threads <= 1) return 1;
    return 1 + (num_threads - 1) * 1;  // 简化：1 + 冲突 way 数
}
}  // namespace tlm
```

- [ ] **Step 5: 编译 + 跑测试 + commit**

```bash
cmake --build build -j$(nproc) --target cpptlm_tests
./build/bin/cpptlm_tests "[gpu][smem]" --reporter compact
# Expected: PASS
git add include/bundles/shared_memory_bundle.hh include/tlm/gpu/shared_memory_tlm.hh src/tlm/gpu/shared_memory_tlm.cc test/test_shared_memory_tlm.cc CMakeLists.txt
git commit -m "feat(tlm/gpu): SharedMemoryTLM skeleton + bank conflict model (Phase 8.A Task 1)"
```

---

### Task 2: MemoryClusterTLM 多通道模型

**Files:**
- Create: `include/tlm/gpu/memory_cluster_tlm.hh`
- Create: `src/tlm/gpu/memory_cluster_tlm.cc`
- Test: `test/test_memory_cluster_tlm.cc`
- Modify: `include/chstream_register.hh` (+1 行)

- [ ] **Step 1: Write the failing test**

`test/test_memory_cluster_tlm.cc`:
```cpp
#include <catch_amalgamated.hpp>
#include "tlm/gpu/memory_cluster_tlm.hh"
using namespace tlm;

TEST_CASE("MemoryClusterTLM: 4-channel channel allocation round-robin", "[gpu][memcluster]") {
    MemoryClusterTLM mc("mc", nullptr, /*channels=*/4, /*capacity_gb=*/8);
    REQUIRE(mc.allocate_channel(0) == 0);
    REQUIRE(mc.allocate_channel(1) == 1);
    REQUIRE(mc.allocate_channel(2) == 2);
    REQUIRE(mc.allocate_channel(3) == 3);
    REQUIRE(mc.allocate_channel(4) == 0);  // round-robin 回到 0
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
./build/bin/cpptlm_tests "[gpu][memcluster]" --reporter compact
```
Expected: FAIL

- [ ] **Step 3: Write minimal header + impl**

`include/tlm/gpu/memory_cluster_tlm.hh`:
```cpp
#ifndef TLM_GPU_MEMORY_CLUSTER_TLM_HH
#define TLM_GPU_MEMORY_CLUSTER_TLM_HH
#include "core/chstream_module.hh"
namespace tlm {
class MemoryClusterTLM : public ChStreamModuleBase {
public:
    MemoryClusterTLM(const std::string& name, EventQueue* eq,
                     uint32_t channels, uint32_t capacity_gb)
        : ChStreamModuleBase(name, eq), channels_(channels), capacity_gb_(capacity_gb) {}
    std::string get_module_type() const override { return "MemoryClusterTLM"; }
    uint32_t allocate_channel(uint64_t request_id);
    uint32_t get_channels() const { return channels_; }
private:
    uint32_t channels_;
    uint32_t capacity_gb_;
    uint64_t rr_counter_ = 0;
};
}  // namespace tlm
#endif
```

`src/tlm/gpu/memory_cluster_tlm.cc`:
```cpp
#include "tlm/gpu/memory_cluster_tlm.hh"
namespace tlm {
uint32_t MemoryClusterTLM::allocate_channel(uint64_t request_id) {
    (void)request_id;
    uint32_t ch = rr_counter_ % channels_;
    rr_counter_++;
    return ch;
}
}  // namespace tlm
```

- [ ] **Step 4: 注册到 chstream_register.hh**

```cpp
// include/chstream_register.hh 末尾追加
REGISTER_CHSTREAM(SharedMemoryTLM)
REGISTER_CHSTREAM(MemoryClusterTLM)
```

- [ ] **Step 5: 编译 + 跑测试 + commit**

```bash
cmake --build build -j$(nproc) --target cpptlm_tests
./build/bin/cpptlm_tests "[gpu][memcluster]" --reporter compact
git add include/tlm/gpu/memory_cluster_tlm.hh src/tlm/gpu/memory_cluster_tlm.cc test/test_memory_cluster_tlm.cc include/chstream_register.hh CMakeLists.txt
git commit -m "feat(tlm/gpu): MemoryClusterTLM multi-channel round-robin (Phase 8.A Task 2)"
```

---

### Task 3: GpuNoC mesh 拓扑

**Files:**
- Create: `include/tlm/gpu/gpu_noc_tlm.hh`
- Create: `src/tlm/gpu/gpu_noc_tlm.cc`
- Test: `test/test_gpu_noc_tlm.cc`

- [ ] **Step 1: Write failing test**

`test/test_gpu_noc_tlm.cc`:
```cpp
#include <catch_amalgamated.hpp>
#include "tlm/gpu/gpu_noc_tlm.hh"
using namespace tlm;

TEST_CASE("GpuNoC: 2x2 mesh XY routing", "[gpu][noc]") {
    GpuNoC noc("noc", nullptr, /*dim=*/2, /*hops_latency=*/2);
    // router (0,0) → router (1,1) 应该是 (1+1)*hops_latency = 4 cycles
    REQUIRE(noc.route_latency(/*src=*/{0,0}, /*dst=*/{1,1}) == 4);
}
```

- [ ] **Step 2-5: 实现 + 注册 + 测试 + commit**

(完整 TDD 模式，参考 Task 1-2 的 5 步结构)

Header 关键:
```cpp
class GpuNoC : public ChStreamModuleBase {
public:
    GpuNoC(const std::string& name, EventQueue* eq, uint32_t dim, uint32_t hops_latency);
    uint32_t route_latency(std::pair<uint32_t,uint32_t> src,
                           std::pair<uint32_t,uint32_t> dst) const;
};
```

Commit message: `feat(tlm/gpu): GpuNoC mesh XY routing (Phase 8.A Task 3)`

---

### Task 4: KernelLaunchTLM AQL 简化

**Files:** 类比 Task 1-3 模式

- 简化版 AQL dispatch (tick() 中按 `kernel_launch_interval_` 周期发 KernelDesc)
- 4 个 setter: `kernel_id` / `workgroup_size` / `grid_size` / `kernel_launch_interval`

Commit: `feat(tlm/gpu): KernelLaunchTLM AQL simplified (Phase 8.A Task 4)`

---

### Task 5: GpuClusterSharedInterface 抽象层

**Files:**
- Create: `include/tlm/gpu/gpu_cluster_shared_interface.hh`
- Modify: `include/tlm/cluster/gpu_cluster.hh` (实现接口)
- Modify: `include/tlm/cluster/{gpc,tpc,compute}_cluster.hh` (stub 完善)

- [ ] **Step 1: Write the failing test**

`test/test_gpu_cluster_shared.cc`:
```cpp
#include <catch_amalgamated.hpp>
#include "tlm/cluster/gpu_cluster.hh"
#include "tlm/gpu/gpu_cluster_shared_interface.hh"
using namespace tlm;

TEST_CASE("GpuCluster implements GpuClusterSharedInterface", "[gpu][cluster]") {
    GpuCluster cluster("cluster", nullptr);
    GpuClusterSharedInterface* iface = &cluster;
    REQUIRE(iface != nullptr);
    REQUIRE(iface->get_gpu_topology().num_gpc == 1);  // default
}
```

- [ ] **Step 2-5: 定义 interface + GpuCluster 实现 + apu_soc 兼容性测试 + commit**

Header:
```cpp
struct GpuTopology { uint32_t num_gpc, num_tpc_per_gpc, num_sm_per_tpc; };
class GpuClusterSharedInterface {
public:
    virtual ~GpuClusterSharedInterface() = default;
    virtual void set_gpu_topology(const GpuTopology& topo) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
    virtual void tick() = 0;
};
```

GpuCluster 改造 (class GpuCluster : public SimModule, public GpuClusterSharedInterface)

apu_soc 兼容性测试: 重跑 `[gpu][phase7]` 测试套件确保 apu_soc 端不破坏

Commit: `feat(cluster): GpuClusterSharedInterface for apu_soc/gpu_soc sharing (Phase 8.A Task 5)`

---

### Task 6: GpuSocTLM 顶层

**Files:**
- Create: `include/tlm/gpu/gpu_soc_tlm.hh`
- Create: `src/tlm/gpu/gpu_soc_tlm.cc`
- Modify: `include/modules_cluster.hh` (+1 行注册)
- Test: `test/test_gpu_soc_tlm.cc`

- [ ] **Step 1: Write failing test**

`test/test_gpu_soc_tlm.cc`:
```cpp
TEST_CASE("GpuSocTLM: top-level contains 1 GpuCluster + 1 GpuNoC + 1 MemoryCluster", "[gpu][soc]") {
    GpuSocTLM soc("soc", nullptr);
    REQUIRE(soc.get_gpu_cluster() != nullptr);
    REQUIRE(soc.get_noc() != nullptr);
    REQUIRE(soc.get_memory_cluster() != nullptr);
}
```

- [ ] **Step 2-5: 实现 + 注册 + 测试 + commit**

Header:
```cpp
class GpuSocTLM : public SimModule {
public:
    GpuSocTLM(const std::string& name, EventQueue* eq);
    GpuCluster* get_gpu_cluster();
    GpuNoC* get_noc();
    MemoryClusterTLM* get_memory_cluster();
    std::string get_module_type() const override { return "GpuSocTLM"; }
};
```

注册: `REGISTER_MODULE(GpuSocTLM)` 加到 modules_cluster.hh 末尾

Commit: `feat(tlm/gpu): GpuSocTLM top-level (Phase 8.A Task 6)`

---

### Task 7: 集成测试 + JSON 配置

**Files:**
- Create: `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`
- Create: `test/test_gpu_soc_phase8a.cc`

- [ ] **Step 1: Write JSON config**

`configs/templates/gpu_soc/gpu_soc_gb203_v1.json` (参考 apu_soc_phase7b.json 风格):
```json
{
  "name": "gpu_soc — GB203 minimal (Phase 8.A)",
  "modules": [
    { "name": "gpu_cluster", "type": "GpuCluster" },
    { "name": "compute_unit_0", "type": "GpuComputeUnitTLM" },
    { "name": "shared_memory_0", "type": "SharedMemoryTLM", "params": {"size_kb": 64, "banks": 32} },
    { "name": "noc", "type": "GpuNoC", "params": {"dim": 2, "hops_latency": 2} },
    { "name": "memory_cluster", "type": "MemoryClusterTLM", "params": {"channels": 4, "capacity_gb": 8} },
    { "name": "kernel_launch", "type": "KernelLaunchTLM" }
  ],
  "connections": [
    { "src": "kernel_launch", "dst": "compute_unit_0" },
    { "src": "compute_unit_0", "dst": "shared_memory_0" },
    { "src": "compute_unit_0", "dst": "noc" },
    { "src": "noc", "dst": "memory_cluster" }
  ]
}
```

- [ ] **Step 2: Write integration test**

`test/test_gpu_soc_phase8a.cc`:
```cpp
TEST_CASE("gpu_soc_phase8a: end-to-end kernel → mem", "[gpu][soc][phase8a]") {
    auto* factory = ModuleFactory::instance();
    factory->loadConfig("configs/templates/gpu_soc/gpu_soc_gb203_v1.json");
    factory->instantiateAll();
    for (int i = 0; i < 1000; ++i) factory->tick();
    REQUIRE(factory->getStats("memory_cluster.requests_completed") > 0);
}
```

- [ ] **Step 3-5: 编译 + 跑测试 + commit**

Commit: `feat(gpu_soc): Phase 8.A end-to-end test + GB203 minimal config (Task 7)`

---

### Task 8: Phase 8.A 微架构 doc + docs_sync + 验收点 M1

**Files:**
- Create: 5 个微架构 doc (gpu-soc.md, gpu-noc-mesh.md, gpu-shared-memory.md, gpu-memory-cluster.md, gpu-kernel-launch.md)
- Modify: `docs/adr/README.md` (append NV-01 — 推迟到 Task 8 后统一签发)
- Modify: `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` (追加 Phase 8 节)

- [ ] **Step 1: 写 5 个微架构 doc**

每个 doc ~150-300 行，参考 `docs/soc_arch/modules/cache-l1.md` 模板：
- 概览 / 架构图 / 数据流 / 配置参数 / 测试覆盖 / 风险 / 参考文献

- [ ] **Step 2: 跑 docs_sync_check + format check**

```bash
./scripts/test/docs_sync_check.sh --strict
./scripts/build/format.sh --check
```

- [ ] **Step 3: 性能验收 1 SM × 1M cycles < 5s**

```bash
time ./build/bin/cpptlm_tests "[gpu][soc][phase8a]" --reporter compact
```

- [ ] **Step 4: 验证 M1 全部达标**

检查清单:
- [ ] `gpu_soc_phase8a.json` 端到端跑通
- [ ] SharedMemory bank conflict 测试 pass
- [ ] MemoryCluster 通道分配测试 pass
- [ ] CU → SharedMem → L1 → NoC → Mem 闭环
- [ ] 性能 1 SM × 1M cycles < 5s
- [ ] docs_sync_check 0 missing
- [ ] format check clean
- [ ] 现有 `[gpu]` (75 cases, Phase 8.A 完成后) + `[phase7]` (1 case) + `[apu_soc]` 全绿（不破坏 apu_soc）

- [ ] **Step 5: Commit**

```bash
git add docs/soc_arch/modules/gpu-soc.md docs/soc_arch/modules/gpu-noc-mesh.md docs/soc_arch/modules/gpu-shared-memory.md docs/soc_arch/modules/gpu-memory-cluster.md docs/soc_arch/modules/gpu-kernel-launch.md docs/superpowers/plans/2026-06-20-future-work-roadmap.md
git commit -m "docs(gpu_soc): Phase 8.A microarchitecture docs + roadmap update + M1 verification (Task 8)"
```

---

## Phase 8.B — 核心仿真 (6 周, ~2150 LOC incl. tests)

### Task 9: ScoreboardTLM ≥12 entries RAW hazard 检测

**Files:** 类比 Task 1-2 模式
- `include/tlm/gpu/scoreboard_tlm.hh` + `.cc`
- `test/test_scoreboard_tlm.cc`

关键测试:
```cpp
TEST_CASE("ScoreboardTLM: 12 outstanding loads no stall", "[gpu][sb]") {
    ScoreboardTLM sb("sb", nullptr, /*entries=*/12);
    for (int i = 0; i < 12; ++i) sb.allocate(/*sb_id=*/i);
    REQUIRE(sb.has_free_entry());
    sb.allocate(13);  // 13th 失败 (capacity 12)
    REQUIRE_FALSE(sb.has_free_entry());
}
```

Commit: `feat(tlm/gpu): ScoreboardTLM ≥12 entries (Phase 8.B Task 9)`

---

### Task 10: WarpSchedulerTLM round-robin + 5-warp CGGTY 阈值

**Files:** 类比
- 测试关键: 5 warps 时 latency jump (268→45 cyc/iter 简化模型)

```cpp
TEST_CASE("WarpScheduler: 4→5 warp 6x speedup", "[gpu][sched]") {
    WarpSchedulerTLM sched("sched", nullptr, /*max_warps=*/12);
    auto t4 = sched.scheduling_latency_cycles(/*active_warps=*/4, /*dep_chain_cyc=*/268);
    auto t5 = sched.scheduling_latency_cycles(/*active_warps=*/5, /*dep_chain_cyc=*/45);
    REQUIRE(t4 == 268);
    REQUIRE(t5 == 45);
}
```

Commit: `feat(tlm/gpu): WarpSchedulerTLM CGGTY 5-warp threshold (Phase 8.B Task 10)`

---

### Task 11: PipelineTLM 5+V 抽象 + 分数 cycle 输出

**Files:** 500 LOC
- 6 管线类 (P0_INT_FP32 / V_SIMD / P1_FP64 / P2_SFU / P3_LSU / P4_TC)
- 每管线一个 `execute(instruction) → cycles_used` 函数
- 简化模型: 查表 (pipe, instruction_type) → cycles

```cpp
TEST_CASE("PipelineTLM: FFMA 4.22 cyc P0", "[gpu][pipe]") {
    PipelineTLM pipe("pipe", nullptr);
    auto cyc = pipe.execute(/*instr=*/"FFMA", /*pipe_id=*/P0);
    REQUIRE(cyc == Approx(4.22).margin(0.01));
}
```

Commit: `feat(tlm/gpu): PipelineTLM 5+V fractional cycle (Phase 8.B Task 11)`

---

### Task 12: TensorCoreTLM 6 精度参数化

**Files:** 300 LOC
- 精度枚举: FP4/FP6/FP8/FP16/BF16/TF32
- 每精度 latency/throughput 查表 (来自 SM_120 paper 29/23 cyc 统一模型)
- 简化: 6 精度全用 29 cyc 延迟, 23 cyc 吞吐 (按 §D2 简化)

```cpp
TEST_CASE("TensorCoreTLM: all 6 precisions 29/23 cyc", "[gpu][tc]") {
    TensorCoreTLM tc("tc", nullptr);
    for (auto prec : {FP4, FP6, FP8, FP16, BF16, TF32}) {
        REQUIRE(tc.latency(prec) == 29);
        REQUIRE(tc.throughput_cyc(prec) == 23);
    }
}
```

Commit: `feat(tlm/gpu): TensorCoreTLM 6-precision unified (Phase 8.B Task 12)`

---

### Task 13: L2PartitionTLM multi-slice 近/远分区

**Files:** 200 LOC
- 测试: 同一 GPC 内访问 L2 slice 是 "near" (79 cyc)，跨 GPC 是 "far" (180 cyc)

```cpp
TEST_CASE("L2PartitionTLM: same-GPC near 79cyc, cross-GPC far 180cyc", "[gpu][l2]") {
    L2PartitionTLM l2("l2", nullptr, /*slices=*/9, /*capacity_mb=*/96);
    REQUIRE(l2.access_latency(/*gpc_id=*/0, /*slice_id=*/0) == 79);
    REQUIRE(l2.access_latency(/*gpc_id=*/0, /*slice_id=*/8) == 180);
}
```

Commit: `feat(tlm/gpu): L2PartitionTLM multi-slice (Phase 8.B Task 13)`

---

### Task 14: SubCoreTLM black-box pipe 封装

**Files:** 400 LOC
- 内部: 4×WarpSchedulerTLM + ScoreboardTLM + PipelineTLM + TensorCoreTLM
- 对外: tick() 推进, 输出 `cycles_used` 分数 cycle

```cpp
TEST_CASE("SubCoreTLM: 32 warps tick advances 1 cycle", "[gpu][subcore]") {
    SubCoreTLM sc("sc", nullptr, /*num_warps=*/32);
    sc.tick();
    REQUIRE(sc.get_current_cycle() == 1);
}
```

Commit: `feat(tlm/gpu): SubCoreTLM black-box pipe wrapper (Phase 8.B Task 14)`

---

### Task 15: Phase 8.B 集成测试 + 5 类 microbenchmark

**Files:**
- Create: `configs/templates/gpu_soc/gpu_soc_phase8b.json`
- Create: `test/test_gpu_soc_phase8b.cc`
- Create: `test/python/test_gpgpu_sim_comparison.py` (gpgpu-sim 区间对照)

- [ ] **Step 1: 5 类 microbenchmark JSON 模板**

5 个 JSON config:
- `gpu_soc_microbench_gemm.json` (M=N=K=4096, FP16)
- `gpu_soc_microbench_flashattn.json` (b=8, h=16, seq=512)
- `gpu_soc_microbench_vector_add.json` (n=1024²)
- `gpu_soc_microbench_stencil.json` (3D 7-point, N=512³)
- `gpu_soc_microbench_sparse_spmv.json` (10k×10k, 0.01 density)

- [ ] **Step 2: 跑 5 类 + 收集 metrics**

```python
# test/python/test_gpgpu_sim_comparison.py
import pytest
import cpptlm.gpu_soc as gs
import cpptlm.nvidia as nv
import cpptlm.gpu_workload as gw

@pytest.mark.parametrize("pattern,workload_factory,expected_bandwidth_gbs,baseline", [
    ("GEMM", lambda: gw.GEMM(m=4096, n=4096, k=4096, dtype="FP16"), 600, 700),  # gpgpu-sim reference
    ("FlashAttn", lambda: gw.FlashAttention(batch=8, head=16, seq_len=512), 400, 470),
    ("vector_add", lambda: gw.VectorAdd(n=1024*1024), 1100, 1176),
    ("stencil", lambda: gw.Stencil3D(n=512, points=7), 800, 940),
    ("sparse_spmv", lambda: gw.SparseSpMV(rows=10000, cols=10000, density=0.01), 200, 230),
])
def test_bandwidth_within_15pct_of_gpgpu_sim(pattern, workload_factory, expected_bandwidth_gbs, baseline):
    sim = gs.simulate(
        topo=nv.gb203_consumer(),
        workload=workload_factory(),
        duration_cycles=1_000_000,
        metrics=["bandwidth"],
    )
    measured = sim.report()["bandwidth"]
    error_pct = abs(measured - baseline) / baseline * 100
    assert error_pct <= 15, f"{pattern}: {measured} GB/s vs baseline {baseline} (error {error_pct:.1f}%)"
```

- [ ] **Step 3-5: 编译 + 跑测试 + commit + 验证 M2**

Commit: `feat(gpu_soc): Phase 8.B 5 microbenchmarks + gpgpu-sim ±15% bandwidth (Task 15)`

---

### Task 16: Phase 8.B 微架构 doc (6 个) + M2 验收

**Files:**
- Create: `docs/soc_arch/modules/gpu-subcore.md`
- Create: `docs/soc_arch/modules/gpu-warp-scheduler.md`
- Create: `docs/soc_arch/modules/gpu-scoreboard.md`
- Create: `docs/soc_arch/modules/gpu-tensor-core.md`
- Create: `docs/soc_arch/modules/gpu-pipeline.md`
- Create: `docs/soc_arch/modules/gpu-l2-partition.md`

- [ ] **Step 1-4: 写 6 个 doc + 跑 docs_sync + 性能验收 + commit**

M2 验收:
- 5 类场景 microbenchmark 全 pass
- 带宽 ±15% 区间对照全通过
- 1 GB203 (110 SM) × 1M cycles < 60s
- docs_sync 0 missing

Commit: `docs(gpu_soc): Phase 8.B microarchitecture docs + M2 verification (Task 16)`

---

## Phase 8.C — 高级特性 (3 周, ~900 LOC incl. tests)

### Task 17: TccTLM write coalescing (DualPortStreamAdapter)

**Files:** 200 LOC
- 测试: 4 个写请求合并为 1 个 MemoryTLM transaction

```cpp
TEST_CASE("TccTLM: 4 same-cache-line writes coalesce to 1", "[gpu][tcc]") {
    TccTLM tcc("tcc", nullptr);
    uint32_t trans_before = 0, trans_after = 0;
    for (int i = 0; i < 4; ++i) {
        tcc.submit_write(/*addr=*/0x1000 + i*16, /*size=*/16);
    }
    tcc.flush();
    REQUIRE(tcc.transactions_issued() == 1);
}
```

Commit: `feat(tlm/gpu): TccTLM write coalescing (Phase 8.C Task 17)`

---

### Task 18: TmaTLM async copy + mbarrier

**Files:** 200 LOC
- 测试: TMA Load 488cyc, TMA Store 33cyc (per SM_120 paper)

```cpp
TEST_CASE("TmaTLM: load 488cyc store 33cyc", "[gpu][tma]") {
    TmaTLM tma("tma", nullptr);
    REQUIRE(tma.load_latency(/*size_b=*/1024, /*swizzle=*/false) == 488);
    REQUIRE(tma.store_latency(/*size_b=*/1024) == 33);
}
```

Commit: `feat(tlm/gpu): TmaTLM async copy (Phase 8.C Task 18)`

---

### Task 19: DsmTLM inter-SM shmem

**Files:** 200 LOC
- 测试: 230cyc DSMEM read, 36cyc DSMEM write

```cpp
TEST_CASE("DsmTLM: cluster remote shmem 230cyc read", "[gpu][dsm]") {
    DsmTLM dsm("dsm", nullptr);
    REQUIRE(dsm.remote_read_latency() == 230);
    REQUIRE(dsm.remote_write_latency() == 36);
}
```

Commit: `feat(tlm/gpu): DsmTLM inter-SM shmem (Phase 8.C Task 19)`

---

### Task 20: PowerModelTLM 80W + 1W/SM 经验模型

**Files:** 150 LOC
- 测试: P = 80 + N×1 公式

```cpp
TEST_CASE("PowerModelTLM: P = 80 + 1*N watts", "[gpu][power]") {
    PowerModelTLM pm("pm", nullptr, /*num_sm=*/110);
    REQUIRE(pm.compute_power(/*tc_saturated=*/false) == Approx(80 + 110*1.0).margin(0.5));
    REQUIRE(pm.compute_power(/*tc_saturated=*/true) == Approx(80 + 110*1.0 + 30).margin(0.5));
}
```

Commit: `feat(tlm/gpu): PowerModelTLM 80W+1W/SM (Phase 8.C Task 20)`

---

### Task 21: cpptlm.nvidia Python 子包

**Files:**
- Create: `cpptlm/cpptlm/nvidia/__init__.py`
- Create: `cpptlm/cpptlm/nvidia/topology.py`
- Create: `cpptlm/cpptlm/nvidia/blueprint.py`
- Create: `cpptlm/cpptlm/nvidia/export.py`
- Create: `cpptlm/cpptlm/nvidia/sku_library.py`
- Test: `test/python/test_cpptlm_nvidia.py`

- [ ] **Step 1: Write failing test**

`test/python/test_cpptlm_nvidia.py`:
```python
import pytest
import cpptlm.nvidia as nv

def test_gb203_consumer_topology():
    topo = nv.gb203_consumer()
    assert topo.sm_arch == "blackwell_sm_120"
    assert topo.num_gpc == 9
    assert topo.num_tpc_per_gpc == 6
    assert topo.num_sm_per_tpc == 2
    assert topo.mem_type == "GDDR7"
    assert topo.mem_bandwidth_gbs == 1176

def test_gpu_topology_param_overrides():
    topo = nv.gpu_topology(sm_arch="blackwell_sm_120", num_gpc=4, mem_capacity_gb=16)
    assert topo.num_gpc == 4
    assert topo.mem_capacity_gb == 16
```

- [ ] **Step 2-5: 实现 topology.py + blueprint.py + export.py + sku_library.py + commit**

关键骨架 (`cpptlm/cpptlm/nvidia/topology.py`):
```python
from dataclasses import dataclass, field
from typing import List, Optional

@dataclass
class GpuTopology:
    sm_arch: str = "blackwell_sm_120"
    num_gpc: int = 9
    num_tpc_per_gpc: int = 6
    num_sm_per_tpc: int = 2
    num_subcore_per_sm: int = 4
    tensor_core_precisions: List[str] = field(default_factory=lambda: ["FP4","FP6","FP8","FP16","BF16","TF32"])
    mem_type: str = "GDDR7"
    mem_capacity_gb: int = 72
    mem_bandwidth_gbs: int = 1176
    mem_channels: int = 12
    l2_partitioned: bool = True
    l2_capacity_mb: int = 96
    shared_mem_kb_per_sm: int = 100
    coherence_protocol: str = "write_through"
    tcc_enabled: bool = True
    tma_enabled: bool = False
    dsm_enabled: bool = False

    def to_json(self, path): ...
    def to_dot(self, path): ...
    def validate(self): ...
    def with_sm_count(self, n): ...

def gpu_topology(**kwargs) -> GpuTopology:
    return GpuTopology(**kwargs)

def gb203_consumer() -> GpuTopology:
    return GpuTopology()  # 默认就是 GB203

def gb200_datacenter() -> GpuTopology:
    return GpuTopology(
        num_gpc=8, num_tpc_per_gpc=8, num_sm_per_tpc=2,
        mem_type="HBM3e", mem_capacity_gb=192, mem_bandwidth_gbs=8000,
        tma_enabled=True, dsm_enabled=True,
    )

def gh100_hopper() -> GpuTopology:
    return GpuTopology(
        sm_arch="hopper_sm_90", num_gpc=8, num_tpc_per_gpc=9, num_sm_per_tpc=2,
        mem_type="HBM3", mem_capacity_gb=80, mem_bandwidth_gbs=3000,
        tma_enabled=True, dsm_enabled=True,
    )

def ga100_ampere() -> GpuTopology:
    return GpuTopology(
        sm_arch="ampere_sm_80", num_gpc=8, num_tpc_per_gpc=8, num_sm_per_tpc=2,
        mem_type="HBM2e", mem_capacity_gb=80, mem_bandwidth_gbs=2000,
    )
```

Commit: `feat(cpptlm): nvidia topology subpackage + 4 SKU presets (Phase 8.C Task 21)`

---

### Task 22: cpptlm.gpu_workload Python 子包

**Files:**
- Create: `cpptlm/cpptlm/gpu_workload/__init__.py`
- Create: `cpptlm/cpptlm/gpu_workload/generator.py`
- Create: `cpptlm/cpptlm/gpu_workload/replay.py`
- Create: `cpptlm/cpptlm/gpu_workload/patterns.py`
- Test: `test/python/test_cpptlm_gpu_workload.py`

5 类 pattern 快捷构造器:
```python
def GEMM(m, n, k, dtype="FP16"): ...
def FlashAttention(batch, head, seq_len): ...
def VectorAdd(n): ...
def Stencil3D(n, points=7): ...
def SparseSpMV(rows, cols, density): ...
```

Commit: `feat(cpptlm): gpu_workload subpackage + 5 patterns (Phase 8.C Task 22)`

---

### Task 23: cpptlm.gpu_soc 顶层 + 报告生成

**Files:**
- Create: `cpptlm/cpptlm/gpu_soc/__init__.py`
- Create: `cpptlm/cpptlm/gpu_soc/simulate.py`
- Create: `cpptlm/cpptlm/gpu_soc/report.py`
- Test: `test/python/test_cpptlm_gpu_soc.py`

```python
# cpptlm/cpptlm/gpu_soc/simulate.py
def simulate(topo, workload, duration_cycles, metrics):
    # 调用 C++ Simulator (Pybind11 或 subprocess + JSON)
    ...
    return SimulationResult(...)

# cpptlm/cpptlm/gpu_soc/report.py
def render_report(result, baseline_csv="docs/validation/gpgpu_sim_baseline.csv") -> str:
    """生成与 gpgpu-sim 对照的 Markdown 报告"""
    ...
```

Commit: `feat(cpptlm): gpu_soc top-level + report generator (Phase 8.C Task 23)`

---

### Task 24: Phase 8.C 完整验证报告 + 与 apu_soc 集成

**Files:**
- Create: `test/test_gpu_soc_phase8c.cc` (apu_soc 集成测试)
- Create: `docs/validation/phase8c_verification_report.md`
- Create: `docs/validation/gpgpu_sim_baseline.csv`

- [ ] **Step 1: 5 类场景完整验证报告**

5 个场景的 TFLOPS / 带宽 / 延迟 / 命中率 / 合并比, 与 gpgpu-sim 数值对照

- [ ] **Step 2: apu_soc 集成测试**

`test/test_gpu_soc_phase8c.cc`:
```cpp
TEST_CASE("gpu_soc 与 apu_soc 共享 GpuCluster 集成", "[gpu][apu][phase8c]") {
    // 同时加载 apu_soc_phase7f.json + gpu_soc_phase8c.json
    // 验证两者引用同一个 GpuCluster 实例
    auto* cluster = ModuleFactory::get("gpu_cluster");
    REQUIRE(cluster != nullptr);
    ApuSoC apu("apu", nullptr);
    apu.set_gpu_cluster(cluster);  // 共享
    GpuSocTLM gpu_soc("gpu_soc", nullptr);
    gpu_soc.set_gpu_cluster(cluster);  // 共享
    // 跑 100 cycles
    apu.tick(); gpu_soc.tick();
    REQUIRE(cluster->getCycleCount() == 100);
}
```

- [ ] **Step 3-5: docs_sync + 性能验收 + commit**

M3 验收:
- 5 类场景完整报告生成
- gpgpu-sim 数值对照 ±15% 带宽, ±20% 延迟
- apu_soc 集成测试 pass
- PowerModel 输出

Commit: `feat(gpu_soc): Phase 8.C 完整验证报告 + apu_soc 集成 (Task 24)`

---

### Task 25: Phase 8.C 微架构 doc (4 个) + ADR-NV-01 签发 + roadmap 更新

**Files:**
- Create: `docs/soc_arch/modules/gpu-tcc.md`
- Create: `docs/soc_arch/modules/gpu-tma.md`
- Create: `docs/soc_arch/modules/gpu-dsm.md`
- Create: `docs/soc_arch/modules/gpu-power-model.md`
- Create: `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` (新命名空间)
- Modify: `docs/adr/README.md` (append NV-01)
- Modify: `AGENTS.md` (STRUCTURE 节加 cpptlm/{nvidia,gpu_workload,gpu_soc})

- [ ] **Step 1: 写 4 个微架构 doc**

每个 ~150-300 行

- [ ] **Step 2: 签发 ADR-NV-01**

`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` 模板:
```markdown
# ADR-NV-01: gpu_soc 独立 SoC 仿真目标

> **状态**: ✅ 已确认
> **日期**: 2026-06-24
> **影响**: Phase 8 完整路径（13 周 / 5100 LOC）
> **类别**: SoC 架构目标定义

## 1. 背景
（参考 docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md §1）

## 2. 决策
✅ **定义 gpu_soc 为 CppTLM 第二个独立 SoC 仿真目标**——与 apu_soc 并行，共享 GPU 子模块。

## 3. 实施
（参考 spec §3 14 个新模块 + §5 3 阶段）

## 4. 风险与缓解
（参考 spec §9 R1-R5）

## 5. 参考文献
- spec: docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md
- gpgpu-sim SM_120 paper
- Jarmusch 2507.10789 Blackwell microbenchmarks
- Luo 2501.12084 Hopper microbenchmarks
- 本地 notes 04_Knowledge/D01-gpu-architecture
```

- [ ] **Step 3: 跑 docs_sync + format check**

```bash
./scripts/test/docs_sync_check.sh --strict
./scripts/build/format.sh --check
```

- [ ] **Step 4: 跑全量测试套件**

```bash
./build/bin/cpptlm_tests --reporter compact
# 期望: 703 + 新增 ~30 测试 = 730+ pass
ctest --test-dir build --output-on-failure -j4
python -m pytest test/python/ -v
```

- [ ] **Step 5: Commit + push**

```bash
git add docs/soc_arch/modules/gpu-tcc.md docs/soc_arch/modules/gpu-tma.md docs/soc_arch/modules/gpu-dsm.md docs/soc_arch/modules/gpu-power-model.md docs/adr/ADR-NV-01-gpu-soc-architecture-target.md docs/adr/README.md AGENTS.md
git commit -m "docs(gpu_soc): Phase 8.C microarch docs + ADR-NV-01 + roadmap finalization (Task 25)"
git push origin main
```

---

## 总结

| Phase | 任务数 | LOC (impl+tests) | 周数 | 验收点 |
|:---:|:---:|:---:|:---:|------|
| 8.A | 8 | ~1250 | 4 | M1: 端到端 GPU 仿真跑通 |
| 8.B | 8 | ~2150 | 6 | M2: 5 类 microbenchmark + gpgpu-sim ±15% 带宽 |
| 8.C | 9 | ~900 + 800 Python | 3 | M3: 完整验证报告 + apu_soc 集成 + ADR-NV-01 |
| **总** | **25** | **~5100** | **13 周** | 3 个里程碑 |

**关键 commit pattern**：
- 8.A: `feat(tlm/gpu): ...` × 6 + `feat(gpu_soc): Phase 8.A ...` × 1 + `docs(gpu_soc): ...` × 1
- 8.B: `feat(tlm/gpu): ...` × 6 + `feat(gpu_soc): Phase 8.B ...` × 1 + `docs(gpu_soc): ...` × 1
- 8.C: `feat(tlm/gpu): ...` × 4 + `feat(cpptlm): ...` × 3 + `feat(gpu_soc): Phase 8.C ...` × 1 + `docs(gpu_soc): ...` × 1

**关键验收**：
- 所有 Task 的 Step 5 commit 通过
- 每个 Phase 结束后跑 `./build/bin/cpptlm_tests` 全绿
- 每个 Phase 结束后跑 `docs_sync_check.sh --strict` 0 missing
- 最终测试基线：703/703 + 新增 ~30 测试 = **730+/730+ pass**
- Python: 222/222 + 新增 ~15 测试 = **237+/237+ pass**
