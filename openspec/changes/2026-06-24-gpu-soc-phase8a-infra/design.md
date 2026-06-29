# Design: gpu_soc Phase 8.A — 基础设施

> **Status**: 🔄 Draft
> **Date**: 2026-06-24
> **Parent**: `proposal.md` (2026-06-24-gpu-soc-phase8a-infra)

---

## 1. 整体架构

Phase 8.A 在现有 `include/tlm/cluster/` stub 基础上，构建 gpu_soc 端到端仿真所需的"基础设施层"：

```
Phase 8.A 前 (现状)
  GpuCluster [stub] ─┬─ GpcCluster [stub] ─┬─ TpcCluster [stub] ─┬─ ComputeCluster [stub]
                      │                     │                     ├─ GpuComputeUnitTLM [F12 待启动]
                      │                     │                     ├─ VectorRegFileTLM [F12 待启动]
                      │                     │                     └─ WavefrontTLM [F12 待启动]
                      │                     └─ (空)
                      └─ (空)

Phase 8.A 后 (目标)
  GpuSocTLM [NEW] ─ GpuCluster [完善] ─┬─ GpcCluster [完善] ─┬─ TpcCluster [完善] ─┬─ ComputeCluster [完善]
  ├─ GpuMeshNoC [NEW]                  │                     │                     ├─ GpuComputeUnitTLM [F12]
  ├─ MemoryClusterTLM [NEW]            │                     │                     ├─ VectorRegFileTLM [F12]
  └─ KernelLaunchTLM [NEW]             │                     │                     ├─ WavefrontTLM [F12]
                                       │                     │                     └─ SharedMemoryTLM [NEW]
                                       │                     └─ (GpuMeshNoC 内部 mesh router)
                                       └─ (GpuMeshNoC 内部 mesh router)
```

> **依赖门 (Dependency Gate)**: Task 5 (GpuClusterSharedInterface + 4 级 cluster 改造) 和 Task 7 (集成测试 + JSON 端到端) **必须等待 F12 落地后才能启动**。Task 1-4 (SharedMemoryTLM / MemoryClusterTLM / GpuMeshNoC / KernelLaunchTLM) 为独立模块,可与 F12 并行实施。

## 1.1 CppTLM ↔ NVIDIA/AMD 命名对齐

以下表格澄清 CppTLM 抽象层与 NVIDIA 硬件的对应关系，避免命名混淆 (FP#8)：

| CppTLM 抽象 | NVIDIA 硬件 | AMD 硬件 | 说明 |
|-------------|------------|----------|------|
| GpuSocTLM | GPU die | GPU die | 顶层 SoC |
| GpuCluster | 1 GPC + 共享 mesh 节点 | 1 SE/SA + shared L2 | 多 GPC 容器 |
| GpcCluster | 1 GPC | 1 SE (Shader Engine) | Graphics Processing Cluster |
| TpcCluster | 1 TPC (2 SMs on H100/B200; 1 SM on GB202) | 1 SA (Shader Array, 4 SIMD) | Texture/Processing Cluster |
| ComputeCluster | 1-2 SM 容器 (CppTLM 抽象) | 4 SIMD16/32 lanes | **非 NVIDIA compute cluster, 是 CppTLM 容器** |
| GpuComputeUnitTLM (F12) | 1 SM (含 4 SubCore 黑盒) | 1 SE (4 SIMD16) | 含 minimal WarpScheduler |
| SubCore (F12 internal) | 1 SubCore / Partition | 1 SIMD16/32 lane | F12 内 SubCoreSlot struct |
| VectorRegFileTLM (F12) | 64 KB x 4 sub-core = 256 KB | VGPR (vector general purpose reg) | register file |
| WavefrontTLM (F12) | 1 warp (32 threads) | 1 wavefront (32/64 lanes) | dispatch unit |

---

## 2. GpuClusterSharedInterface 抽象层（关键设计）

### 2.1 设计动机

apu_soc 与 gpu_soc 需要共享 GpuCluster 容器，但两者顶层 API 不同：
- apu_soc 顶部：`ApuSoC → CpuCluster + CoherentXBarTLM + GpuCluster + MemoryCluster`
- gpu_soc 顶部：`GpuSocTLM → GpuCluster + GpuMeshNoC + MemoryCluster + KernelLauncher`

**直接共享的障碍**：GpuCluster 当前是 `SimModule` 派生，apu_soc 的 `ApuSoC::incorporate_parent` 通过 `ModuleFactory::get(name)` 强类型获取，无法让 gpu_soc 也获取同一实例。

### 2.2 接口设计

```cpp
// include/tlm/gpu/gpu_cluster_shared_interface.hh
namespace cpptlm::tlm {

struct GpuTopology {
  uint32_t num_gpc = 1;
  uint32_t num_tpc_per_gpc = 1;
  uint32_t num_sm_per_tpc = 1;
  // 1 for GB202, 2 for H100/B200/GB203
  uint32_t num_subcore_per_sm = 4; // configurable (default 4 for NVIDIA)
  uint32_t warp_size = 32;
  // NEW (FP#1, #4, #6, #10):
  uint32_t tensor_core_count_per_sm = 4;
  std::string tensor_core_version = "v4"; // "v4" Hopper, "v5" Blackwell DC, "v5-rtx" Blackwell RTX
  uint32_t smem_kb_per_sm = 128;
  uint32_t l1_kb_per_sm = 128;
  uint32_t regfile_kb_per_sm = 256;
  uint32_t mem_bus_bits = 256; // 5120 HBM3 (H100), 8192 HBM3e (B200), 512 GDDR7 (GB202), 256 GDDR7 (GB203)
  uint32_t mem_channels = 8; // 10 channels (H100), 8 (GB203), 16 (GB202)
  bool has_nv_hub = false; // true for B200 dual-reticle
};

class GpuClusterSharedInterface {
public:
    virtual ~GpuClusterSharedInterface() = default;
    virtual void set_gpu_topology(const GpuTopology& topo) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
    virtual void tick() = 0;
    virtual std::string get_module_type() const = 0;
    // 与父 spec docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md §7.3 + 父类 ChStreamModuleBase::get_stats_group() 对齐
    virtual tlm_stats::StatGroup* get_stats_group() = 0;
};

}  // namespace cpptlm::tlm
```

### 2.3 GpuCluster 改造

```cpp
// include/tlm/cluster/gpu_cluster.hh
class GpuCluster : public SimModule, public GpuClusterSharedInterface {
    // ... 现有 GpuCluster 代码 ...
    void set_gpu_topology(const GpuTopology& topo) override { topo_ = topo; }
    GpuTopology get_gpu_topology() const override { return topo_; }
};
```

### 2.4 apu_soc 兼容性

`ApuSoC::incorporate_parent` 通过 `dynamic_cast<GpuClusterSharedInterface*>` 获取接口（而非直接 `GpuCluster*`），允许：
- apu_soc 单独运行时：GpuCluster 是普通 SimModule
- gpu_soc 单独运行时：GpuCluster 是 GpuClusterSharedInterface
- 两者共存时：共享同一 GpuCluster 实例

---

## 3. 4 个核心模块设计

### 3.1 SharedMemoryTLM

**职责**：SM 内部 shared memory + L1 unified，32 bank

**接口**：
```cpp
class SharedMemoryTLM : public ChStreamModuleBase {
public:
    SharedMemoryTLM(const std::string& name, EventQueue* eq,
                    uint32_t size_kb, uint32_t banks);
    std::string get_module_type() const override { return "SharedMemoryTLM"; }
    uint32_t bank_conflict_cycles(uint32_t num_threads, uint32_t stride_bytes) const;
    // 测试辅助：返回 bank conflict 引起的额外 cycle
private:
    uint32_t size_kb_;
    uint32_t banks_;
};
```

**简化模型**（按 D2 决策）：
```cpp
uint32_t SharedMemoryTLM::bank_conflict_cycles(uint32_t num_threads, uint32_t stride_bytes) const {
    if (num_threads <= 1) return 1;  // 1 cycle base latency (符合 SM_120 实测 33cyc/SM)
    return 1 + (num_threads - 1) * 1;  // 简化：每个 conflict way +1 cycle
}
```

### 3.2 MemoryClusterTLM

**职责**：HBM/GDDR 多通道内存控制器

**接口**：
```cpp
class MemoryClusterTLM : public ChStreamModuleBase {
public:
    MemoryClusterTLM(const std::string& name, EventQueue* eq,
                     uint32_t channels, uint32_t capacity_gb);
    std::string get_module_type() const override { return "MemoryClusterTLM"; }
    uint32_t allocate_channel(uint64_t request_id);
    uint32_t get_channels() const { return channels_; }
private:
    uint32_t channels_;
    uint32_t capacity_gb_;
    uint64_t rr_counter_ = 0;
};
```

**简化模型**：round-robin channel 分配（按 D2 决策，不模拟真实 DRAM 调度）

### 3.3 GpuMeshNoC

**职责**：GPC 之间 mesh interconnect（基于现有 RouterTLM + LinkTLM）。**类名 `GpuMeshNoC` 避免与 `include/tlm/cluster/gpu_noc_cluster.hh` 中已有的 `GpuNoC` 类冲突**。

**接口**：
```cpp
namespace tlm {  // ChStreamModuleBase 派生 → namespace tlm（与 gpu_tlm.hh 一致）

class GpuMeshNoC : public ChStreamModuleBase {
public:
    GpuMeshNoC(const std::string& name, EventQueue* eq,
               uint32_t dim, uint32_t hops_latency);
    std::string get_module_type() const override { return "GpuMeshNoC"; }
    uint32_t route_latency(std::pair<uint32_t,uint32_t> src,
                           std::pair<uint32_t,uint32_t> dst) const;
};

}  // namespace tlm
```

**简化模型**：XY 路由 + hops × latency（按 D2 决策，不模拟 VC 分配/拥塞）

### 3.4 KernelLaunchTLM

**职责**：AQL 简化 dispatcher（按 ADR-SOC-04 黑盒决策）

**接口**：
```cpp
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    KernelLaunchTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "KernelLaunchTLM"; }
    void set_kernel_id(uint32_t id) { kernel_id_ = id; }
    void set_workgroup_size(uint32_t sz) { workgroup_size_ = sz; }
    void set_grid_size(uint32_t sz) { grid_size_ = sz; }
    void set_kernel_launch_interval(uint32_t cyc) { interval_ = cyc; }
    void tick() override;
private:
    uint32_t kernel_id_ = 0;
    uint32_t workgroup_size_ = 64;
    uint32_t grid_size_ = 1;
    uint32_t interval_ = 1000;
    uint64_t cycle_counter_ = 0;
};
```

**行为**：tick() 中按 interval 周期向 ComputeCluster 发 KernelDesc（简化版 AQL packet）

### 3.5 GpuSocTLM 顶层

**职责**：gpu_soc 顶层容器，组合 4 级 cluster + 3 个核心模块

**接口**：
```cpp
namespace cpptlm::tlm {  // SimModule 派生 → namespace cpptlm::tlm（与 apu_soc.hh 一致）

class GpuSocTLM : public SimModule {
public:
    GpuSocTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "GpuSocTLM"; }
    GpuCluster* get_gpu_cluster();
    GpuMeshNoC* get_noc();
    MemoryClusterTLM* get_memory_cluster();
    KernelLaunchTLM* get_kernel_launch();
    void tick() override;
};

}  // namespace cpptlm::tlm
```

**注册**：`REGISTER_MODULE(GpuSocTLM)` 添加到 `include/modules_cluster.hh`

### 3.6 SubCore 抽象（F12 内部）

**动机** (FP#1, #2)：SM 内部有 4 个 sub-core partitions，每个 sub-core 独立执行 warp。F12 的 `GpuComputeUnitTLM` 必须体现 4-way 并行度，否则 IPC 低估 4×（GB202 192 SM × 4 = 768 issue units）。

**设计**：SubCore 不是独立的 TLM 类，而是 `GpuComputeUnitTLM` 内部的 `SubCoreSlot` 结构体：

```cpp
// include/tlm/gpu/sub_core_slot.hh (F12 内部, 非公开接口)
namespace cpptlm::tlm {

struct SubCoreSlot {
  uint32_t warp_id = 0;            // 当前执行 warp ID (0 = empty)
  uint32_t active_mask = 0;        // lane active bitmap (32-bit)
  uint32_t dispatch_cycle = 0;     // 该 warp 的 dispatch cycle
  bool is_active = false;          // slot 是否被占用
};

} // namespace cpptlm::tlm
```

**使用方式**：`GpuComputeUnitTLM` 持有 `SubCoreSlot slots_[num_subcore_per_sm]` 数组，配合 `MinimalWarpScheduler` (§3.7) 每 cycle 派发 1 个 warp 到空 slot。每 slot 执行 1 cycle 后释放并回写 VectorRegFile。

**升级路径 (8.B)**：SubCoreSlot 从 4 个扩展为更细粒度（多 issue per cycle / scoreboard 依赖追踪）。

### 3.7 MinimalWarpScheduler (F12 scope expansion - Option A)

**动机** (FP#3)：F12 的 `GpuComputeUnitTLM` 无 WarpScheduler → 8.A Task 5/7 无法触发 `requests_completed > 0`，端到端测试无法关闭闭环。

**设计**：最小 round-robin 派发器，每个 cycle 从 ready queue 选择 1 个 warp 派发到 sub-core slot。不做 CGGTY/优先级（8.B 升级）。

```cpp
// include/tlm/gpu/minimal_warp_scheduler.hh (Phase 7.B, F12 scope expansion - Option A)
namespace cpptlm::tlm {

class MinimalWarpScheduler {
public:
  MinimalWarpScheduler(uint32_t num_subcores = 4);

  // 每 cycle 从 ready queue 选 1 个 warp 派发到 sub-core slots
  // 简单 round-robin，不做 CGGTY/优先级 (8.B 升级)
  void tick(std::vector<SubCoreSlot>& slots, std::queue<uint32_t>& ready_warps);

  uint32_t get_dispatch_count() const { return dispatch_count_; }

private:
  uint32_t num_subcores_;
  uint32_t rr_counter_ = 0;
  uint64_t dispatch_count_ = 0;
};

} // namespace cpptlm::tlm
```

**接口约束**：
- `sub_core_slot.hh` 必须定义在 `minimal_warp_scheduler.hh` 之前（头文件顺序）
- `GpuComputeUnitTLM` 持有 `MinimalWarpScheduler` 实例 + `SubCoreSlot[]` + `std::queue<uint32_t> ready_warps_`
- `tick()` 中先调 scheduler, 再执行 active slot 的 SIMT 黑盒

**F12 预算调整** (Option A 决策):
- 原: 650-800 LOC, 1-2 周
- 新: 950-1200 LOC, 2-3 周 (含 MinimalWarpScheduler ~300-400 LOC)
- 新增文件: `minimal_warp_scheduler.hh` + `minimal_warp_scheduler.cc` (+2 文件)
- 新增测试: `test_minimal_warp_scheduler.cc`

---

### 4.1 `include/chstream_register.hh`

末尾追加：
```cpp
REGISTER_CHSTREAM(SharedMemoryTLM)
REGISTER_CHSTREAM(MemoryClusterTLM)
REGISTER_CHSTREAM(GpuMeshNoC)
REGISTER_CHSTREAM(KernelLaunchTLM)
REGISTER_CHSTREAM(GpuSocTLM)
```

### 4.2 `include/modules_cluster.hh`

末尾追加（注意 GpuSocTLM 是 SimModule 派生）：
```cpp
REGISTER_MODULE(GpuSocTLM)
```

---

## 5. 集成测试设计

### 5.1 端到端流程

```
KernelLaunchTLM.tick() → 发 KernelDesc
  → ComputeCluster → GpuComputeUnitTLM [F12]
    → SharedMemoryTLM (load/store)
    → CacheTLM (L1)
    → GpuMeshNoC
    → MemoryClusterTLM
    → 响应回传
```

### 5.2 JSON 配置示例

`configs/templates/gpu_soc/gpu_soc_gb203_v1.json`:
```json
{
  "name": "gpu_soc — GB203-400-A1 minimal (Phase 8.A, F12 with WarpScheduler)",
  "_comment": "GB203-400-A1: 6 GPCs x 7 TPCs x 2 SMs/TPC = 84 SMs, 256-bit GDDR7 = 8 channels",
  "modules": [
  { "name": "gpu_soc", "type": "GpuSocTLM" },
  { "name": "kernel_launch", "type": "KernelLaunchTLM" },
  { "name": "compute_unit_0", "type": "GpuComputeUnitTLM" },
  { "name": "shared_memory_0", "type": "SharedMemoryTLM", "params": {"size_kb": 64, "banks": 32} },
  { "name": "noc", "type": "GpuMeshNoC", "params": {"dim": 2, "hops_latency": 2} },
  { "name": "memory_cluster", "type": "MemoryClusterTLM", "params": {"channels": 8, "capacity_gb": 8} }
  ],
  "connections": [
    { "src": "kernel_launch", "dst": "compute_unit_0" },
    { "src": "compute_unit_0", "dst": "shared_memory_0" },
    { "src": "compute_unit_0", "dst": "noc" },
    { "src": "noc", "dst": "memory_cluster" }
  ]
}
```

---

## 6. 实施顺序

| 顺序 | Task | 关键风险 |
|:---:|------|---------|
| 1 | SharedMemoryTLM（独立风险最低） | bank conflict 模型 |
| 2 | MemoryClusterTLM（独立） | 通道分配 |
| 3 | GpuMeshNoC（独立） | XY 路由 |
| 4 | KernelLaunchTLM（独立） | AQL 简化 |
| 5 | GpuClusterSharedInterface + 4 级 cluster 改造（**风险最高**） | apu_soc 兼容性 |
| 6 | GpuSocTLM 顶层（依赖 1-5） | 集成 |
| 7 | 集成测试 + JSON 配置 | 闭环 |
| 8 | 5 个微架构 doc + 性能验收 | M1 验收 |
| 9 | MinimalWarpScheduler (F12 Option A) | F12 闭环 |
| 10 | 精度简化文档化 + G4+ 验收升级 | multi-SM 验证 |

并行机会：Task 1-4 可 4 人并行；Task 5-8 串行；Task 9 与 F12 并行

---

## 7. 关联 spec 章节

- `gpu-soc-architecture.md §3.1` 4 级层次结构
- `gpu-soc-architecture.md §3.2` 14 个新模块清单（Phase 8.A 部分）
- `gpu-soc-architecture.md §5.1` Phase 8.A 验收点 M1
- `gpu-soc-architecture.md §7` 模块组织与目录
- `gpu-soc-architecture.md §12` 文档配套

---

## 8. 不做事项（与 proposal §4 Non-Goals 一致）

- ❌ Cycle-accurate 5+V 管线
- ❌ 完整 TC 12 精度
- ❌ gpgpu-sim 集成
- ❌ dGPU + PCIe
- ❌ 真实 kernel 编译
- ❌ Scoreboard / Pipeline（→ 8.B）
- ❌ L2Partition（→ 8.B）
- ❌ Tcc / Tma / Dsm / PowerModel（→ 8.C）

---

## 10. 已知精度简化（8.A 范围内，与 ADR-NV-01 phase-accurate 区分）

以下简化在 8.A 实施范围内可接受，但需文档化以便后续阶段升级：

| 组件 | 8.A 模型 | 真实硬件 | 升级路径 |
|------|----------|----------|----------|
| CU dispatch | round-robin, 每 cycle 派发 1 warp | 4× greedy/CGGTY + priority | 8.B WarpScheduler |
| SubCore | 4 slot 并行 (SubCoreSlot struct) | 4 独立 sub-core partition | 8.B (多 issue / scoreboard) |
| SharedMemory bank conflict | thread-count heuristic (1 + N-1 cyc) | address-pattern based (32 banks) | 8.B |
| MemoryCluster | round-robin channel 分配 | DRAM 调度 + row buffer | 8.D (F13) |
| NoC | XY routing, 单 mesh | NV-Hub (B200), congestion, VC | 8.C |
| Coalescing | coalescing_factor 常数 | address-pattern function | 8.B |
| TensorCore / SFU / LSU | 完全缺失 | 4 TC + SFU + LSU per SM | 8.B/8.C |
