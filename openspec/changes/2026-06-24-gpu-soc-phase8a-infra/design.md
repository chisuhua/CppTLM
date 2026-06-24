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
                      │                     │                     ├─ GpuComputeUnitTLM [F12 已建]
                      │                     │                     ├─ VectorRegFileTLM [F12 已建]
                      │                     │                     └─ WavefrontTLM [F12 已建]
                      │                     └─ (空)
                      └─ (空)

Phase 8.A 后 (目标)
  GpuSocTLM [NEW] ─ GpuCluster [完善] ─┬─ GpcCluster [完善] ─┬─ TpcCluster [完善] ─┬─ ComputeCluster [完善]
  ├─ GpuNoC [NEW]                      │                     │                     ├─ GpuComputeUnitTLM
  ├─ MemoryClusterTLM [NEW]            │                     │                     ├─ VectorRegFileTLM
  └─ KernelLaunchTLM [NEW]             │                     │                     ├─ WavefrontTLM
                                       │                     │                     └─ SharedMemoryTLM [NEW]
                                       │                     └─ (GpuNoC 内部 mesh router)
                                       └─ (GpuNoC 内部 mesh router)
```

---

## 2. GpuClusterSharedInterface 抽象层（关键设计）

### 2.1 设计动机

apu_soc 与 gpu_soc 需要共享 GpuCluster 容器，但两者顶层 API 不同：
- apu_soc 顶部：`ApuSoC → CpuCluster + CoherentXBarTLM + GpuCluster + MemoryCluster`
- gpu_soc 顶部：`GpuSocTLM → GpuCluster + GpuNoC + MemoryCluster + KernelLauncher`

**直接共享的障碍**：GpuCluster 当前是 `SimModule` 派生，apu_soc 的 `ApuSoC::incorporate_parent` 通过 `ModuleFactory::get(name)` 强类型获取，无法让 gpu_soc 也获取同一实例。

### 2.2 接口设计

```cpp
// include/tlm/gpu/gpu_cluster_shared_interface.hh
namespace tlm {

struct GpuTopology {
    uint32_t num_gpc = 1;
    uint32_t num_tpc_per_gpc = 1;
    uint32_t num_sm_per_tpc = 1;
    uint32_t num_subcore_per_sm = 4;  // 恒为 4（NVIDIA sub-core 数）
    uint32_t warp_size = 32;
};

class GpuClusterSharedInterface {
public:
    virtual ~GpuClusterSharedInterface() = default;
    virtual void set_gpu_topology(const GpuTopology& topo) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
    virtual void tick() = 0;
    virtual std::string get_module_type() const = 0;
};

}  // namespace tlm
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

### 3.3 GpuNoC

**职责**：GPC 之间 mesh interconnect（基于现有 RouterTLM + LinkTLM）

**接口**：
```cpp
class GpuNoC : public ChStreamModuleBase {
public:
    GpuNoC(const std::string& name, EventQueue* eq,
           uint32_t dim, uint32_t hops_latency);
    std::string get_module_type() const override { return "GpuNoC"; }
    uint32_t route_latency(std::pair<uint32_t,uint32_t> src,
                           std::pair<uint32_t,uint32_t> dst) const;
};
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
class GpuSocTLM : public SimModule {
public:
    GpuSocTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "GpuSocTLM"; }
    GpuCluster* get_gpu_cluster();
    GpuNoC* get_noc();
    MemoryClusterTLM* get_memory_cluster();
    KernelLaunchTLM* get_kernel_launch();
    void tick() override;
};
```

**注册**：`REGISTER_MODULE(GpuSocTLM)` 添加到 `include/modules_cluster.hh`

---

## 4. 注册宏扩展

### 4.1 `include/chstream_register.hh`

末尾追加：
```cpp
REGISTER_CHSTREAM(SharedMemoryTLM)
REGISTER_CHSTREAM(MemoryClusterTLM)
REGISTER_CHSTREAM(GpuNoC)
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
  → ComputeCluster → GpuComputeUnitTLM
    → SharedMemoryTLM (load/store)
    → CacheTLM (L1)
    → GpuNoC
    → MemoryClusterTLM
    → 响应回传
```

### 5.2 JSON 配置示例

`configs/templates/gpu_soc/gpu_soc_gb203_v1.json`:
```json
{
  "name": "gpu_soc — GB203 minimal (Phase 8.A)",
  "modules": [
    { "name": "gpu_soc", "type": "GpuSocTLM" },
    { "name": "kernel_launch", "type": "KernelLaunchTLM" },
    { "name": "compute_unit_0", "type": "GpuComputeUnitTLM" },
    { "name": "shared_memory_0", "type": "SharedMemoryTLM", "params": {"size_kb": 64, "banks": 32} },
    { "name": "noc", "type": "GpuNoC", "params": {"dim": 2, "hops_latency": 2} },
    { "name": "memory_cluster", "type": "MemoryClusterTLM", "params": {"channels": 4, "capacity_gb": 8} }
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
| 3 | GpuNoC（独立） | XY 路由 |
| 4 | KernelLaunchTLM（独立） | AQL 简化 |
| 5 | GpuClusterSharedInterface + 4 级 cluster 改造（**风险最高**） | apu_soc 兼容性 |
| 6 | GpuSocTLM 顶层（依赖 1-5） | 集成 |
| 7 | 集成测试 + JSON 配置 | 闭环 |
| 8 | 5 个微架构 doc + 性能验收 | 文档同步 |

并行机会：Task 1-4 可 4 人并行；Task 5-8 串行

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
- ❌ SubCore / WarpScheduler / Scoreboard / TensorCore / Pipeline（→ 8.B）
- ❌ L2Partition（→ 8.B）
- ❌ Tcc / Tma / Dsm / PowerModel（→ 8.C）
