# gpu-soc 微架构文档

> **类别**: GPU > SoC · **状态**: ✅ Phase 8.A Task 6 + 📋 v1.0 dGPU SoC 战略补充(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D1 + L0 总架构蓝图 §1.3)
> **Header**: `include/tlm/cluster/gpu_soc_cluster.hh`
> **注册**: `REGISTER_MODULE` (`include/modules_cluster.hh`)
> **蓝图来源**: `docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md` §3 + dGPU SoC v1.0 总架构蓝图
> **OpenSpec**: `openspec/changes/2026-06-24-gpu-soc-phase8a-infra/`
> **首版 commit**: (Task 6) · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - **L0 总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) §1.3 多层 SimModule 拓扑
> - **关联 ADR**:
>   - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D1 — v1.0 双 vendor 蓝图
>   - [`ADR-SOC-10-module-factory-topology.md`](../../adr/ADR-SOC-10-module-factory-topology.md) D2 — SimModule 多层容器
> - Spec: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../../superpowers/specs/2026-06-24-gpu-soc-architecture.md) §3
> - ADR: [`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md`](../../adr/ADR-NV-01-gpu-soc-architecture-target.md)
> - 主 plan: [`docs/superpowers/plans/2026-06-24-gpu-soc-roadmap.md`](../../superpowers/plans/2026-06-24-gpu-soc-roadmap.md)

---

## 1. 设计目标

`GpuSocTLM` 是 CppTLM gpu_soc 路径的**顶层 SimModule 容器**，负责将 GPU 芯片内的 4 个核心模块（GpuCluster / GpuMeshNoC / MemoryClusterTLM / KernelLaunchTLM）组装为一个完整的单芯片仿真目标。通过 `GpuClusterSharedInterface` 抽象层与 apu_soc 共享 GpuCluster 等子模块。

**核心特性**:
- 继承 `SimModule`，支持 JSON 拓扑驱动实例化
- 包含 4 个子模块引用：GPU cluster、GPU NoC、内存控制器、kernel launcher
- 与 apu_soc 共享 `GpuCluster` 容器（通过 `GpuClusterSharedInterface`）
- tick() 按顺序推进所有子模块

## 2. 架构概览

### 2.1 4 级层次结构

```
GpuSocTLM (顶层, 1 chip)
├── GpuCluster (1 GPU die)
│   ├── GpcCluster × N (GPC)
│   │   └── TpcCluster × M (TPC)
│   │       └── ComputeCluster × K (1-2 CU)
│   ├── SharedMemoryTLM × S (per-SM)
│   └── L2PartitionTLM (Phase 8.B)
├── GpuMeshNoC (mesh interconnect)
├── MemoryClusterTLM (HBM/GDDR 多通道)
└── KernelLaunchTLM (AQL dispatcher)
```

### 2.2 内部数据流

```
KernelLaunchTLM ──(KernelDesc)──→ GpuComputeUnitTLM (SM)
                                        │
                                        ├── write/read → SharedMemoryTLM
                                        ├── L1 miss   → GpuMeshNoC
                                        │                   │
                                        │                   └→ MemoryClusterTLM
                                        └── completed  → requests_completed++
```

### 2.3 端口表

| 组件 | 类型 | 角色 |
|------|------|------|
| `gpu_cluster_` | `GpuCluster*` | GPU 核心容器（4 级层次） |
| `noc_` | `GpuMeshNoC*` | GPC 之间 mesh 路由 |
| `memory_cluster_` | `MemoryClusterTLM*` | 多通道内存控制器 |
| `kernel_launch_` | `KernelLaunchTLM*` | Kernel 调度发起器 |

## 3. 接口（Public API）

```cpp
class GpuSocTLM : public SimModule {
public:
    GpuSocTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "GpuSocTLM"; }

    // 子模块访问器
    GpuCluster* get_gpu_cluster();
    GpuMeshNoC* get_noc();
    MemoryClusterTLM* get_memory_cluster();
    KernelLaunchTLM* get_kernel_launch();

    void tick() override;
    void do_reset(const ResetConfig&) override;
};
```

## 4. 配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `num_gpc` | uint32 | 1 | GPC 数量 |
| `num_tpc_per_gpc` | uint32 | 1 | 每个 GPC 的 TPC 数 |
| `num_sm_per_tpc` | uint32 | 1 | 每个 TPC 的 CU/SM 数 |
| `mem_channels` | uint32 | 4 | 内存通道数 |
| `mem_capacity_gb` | uint32 | 8 | 显存容量 (GB) |
| `noc_dim` | uint32 | 2 | Mesh 维度 |
| `noc_hops_latency` | uint32 | 2 | 每跳延迟 (cycles) |

### JSON 配置示例 (`gpu_soc_gb203_v1.json`)

```json
{
  "name": "gpu_soc — GB203 minimal (Phase 8.A)",
  "modules": [
    { "name": "gpu_cluster", "type": "GpuCluster" },
    { "name": "compute_unit_0", "type": "GpuComputeUnitTLM" },
    { "name": "shared_memory_0", "type": "SharedMemoryTLM", "params": {"size_kb": 64, "banks": 32} },
    { "name": "noc", "type": "GpuMeshNoC", "params": {"dim": 2, "hops_latency": 2} },
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

## 5. 与 apu_soc 的共享机制

### 5.1 GpuClusterSharedInterface 抽象层

```cpp
struct GpuTopology {
    uint32_t num_gpc, num_tpc_per_gpc, num_sm_per_tpc;
    uint32_t num_subcore_per_sm = 4;
    uint32_t warp_size = 32;
};

class GpuClusterSharedInterface {
    virtual void set_gpu_topology(const GpuTopology&) = 0;
    virtual GpuTopology get_gpu_topology() const = 0;
    virtual void tick() = 0;
};
```

GpuSocTLM 和 ApuSoC 均引用同一 `GpuCluster` 实例（通过 `GpuClusterSharedInterface`），确保 GPU 子模块在两条路线间共享。

### 5.2 共享模块清单

| 模块 | apu_soc | gpu_soc | 共享方式 |
|------|:---:|:---:|------|
| GpuCluster | ✅ | ✅ | GpuClusterSharedInterface |
| GpcCluster | ✅ | ✅ | 同一实现 |
| TpcCluster | ✅ | ✅ | 同一实现 |
| ComputeCluster | ✅ | ✅ | 同一实现 |
| GpuComputeUnitTLM | F12a | F12a | 同一实现 |

## 6. 测试覆盖

| 测试文件 | 标签 | 测试数 | 覆盖内容 |
|------|------|:---:|------|
| `test/test_gpu_soc_phase8a.cc` | `[gpu][soc][phase8a]` | 1 | 端到端 kernel → mem 闭环 |
| `test/test_gpu_soc_phase8a_multism.cc` | `[gpu][soc][phase8a][multism]` | 1 | 4-SM 并行 contention |

**验收标准**:
- 1 SM × 1M cycles < 5s (M1)
- 4 SM × 100K cycles < 5s (G4+ multi-SM)
- `[gpu]` 66 cases + `[phase7]` + `[apu_soc]` 全绿

## 7. 风险

| 风险 | 缓解 |
|------|------|
| apu_soc 兼容性破坏 | GpuClusterSharedInterface 隔离两方变化 |
| multi-SM O(N²) contention | G4+ 4 SM × 100K < 5s 验收门 |

## 8. 参考文献

- gpgpu-sim SM_120 paper
- Jarmusch 2507.10789 Blackwell microbenchmarks
- Luo 2501.12084 Hopper microbenchmarks

---

*维护者: CppTLM Team · 最后更新: 2026-07-02*