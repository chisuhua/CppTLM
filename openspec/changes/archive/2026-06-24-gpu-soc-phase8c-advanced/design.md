# Design: gpu_soc Phase 8.C — 高级特性

> **Status**: 🔄 Draft
> **Parent**: `proposal.md` (2026-06-24-gpu-soc-phase8c-advanced)

---

## 1. 4 个高级模块设计

### 1.1 TccTLM (Translation Cache Coherent Bridge, GPU L2)

**职责**：CU 与 Memory 之间的桥接，写合并 + snoop fan-in

**接口**：
```cpp
class TccTLM : public ChStreamModuleBase {
public:
    TccTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "TccTLM"; }
    void submit_write(uint64_t addr, uint32_t size, const uint8_t* data);
    void flush();
    uint32_t transactions_issued() const;  // 测试：合并后的事务数
};
```

**简化模型**：
- 4 个连续 16B 写入合并为 1 个 64B MemoryTLM transaction
- 使用 `DualPortStreamAdapter`（已存在）

### 1.2 TmaTLM (Tensor Memory Accelerator)

**职责**：async copy 抽象（DRAM ↔ SMEM）+ mbarrier

**接口**：
```cpp
class TmaTLM : public ChStreamModuleBase {
public:
    TmaTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "TmaTLM"; }
    uint32_t load_latency(uint32_t size_b, bool swizzle) const;
    uint32_t store_latency(uint32_t size_b) const;
};
```

**简化模型**（按 SM_120 paper §9.1）：
- TMA Load 1D 1024B: 488 cyc
- TMA Load 2D 1024B: 620 cyc
- TMA Store 2D 1024B: 33 cyc (仅提交，实际写回 620 cyc)

### 1.3 DsmTLM (Distributed Shared Memory)

**职责**：inter-SM 共享内存（Hopper 独有）

**接口**：
```cpp
class DsmTLM : public ChStreamModuleBase {
public:
    DsmTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "DsmTLM"; }
    uint32_t remote_read_latency() const { return 230; }  // per SM_120 paper
    uint32_t remote_write_latency() const { return 36; }
};
```

### 1.4 PowerModelTLM (经验能耗模型)

**职责**：post-hoc 统计 SM 功耗

**接口**：
```cpp
class PowerModelTLM : public ChStreamModuleBase {
public:
    PowerModelTLM(const std::string& name, EventQueue* eq, uint32_t num_sm);
    std::string get_module_type() const override { return "PowerModelTLM"; }
    double compute_power(bool tc_saturated) const;  // 单位 W
};
```

**简化模型**（按 SM_120 paper §11.3）：`P = 80 + N × 1.0 (+ 30 if TC saturated)`

---

## 2. 3 个 Python 子包设计

### 2.1 `cpptlm.nvidia` — 拓扑生成

**关键 API**：
```python
@dataclass
class GpuTopology:
    sm_arch: str = "blackwell_sm_120"
    num_gpc: int = 9
    # ... 22 个参数
    def to_json(self, path): ...
    def to_dot(self, path): ...
    def validate(self): ...
    def with_sm_count(self, n): ...

def gpu_topology(**kwargs) -> GpuTopology: ...
def gb203_consumer() -> GpuTopology: ...
def gb200_datacenter() -> GpuTopology: ...
def gh100_hopper() -> GpuTopology: ...
def ga100_ampere() -> GpuTopology: ...
```

### 2.2 `cpptlm.gpu_workload` — 工作负载

**关键 API**：
```python
class WorkloadGenerator:
    def __init__(self, kernel_pattern, grid_size, block_size, data_type, ...): ...
class TraceReplay:
    def __init__(self, trace_path, cycle_granularity=1, format="ncu"): ...
def GEMM(m, n, k, dtype="FP16"): ...
def FlashAttention(batch, head, seq_len): ...
def VectorAdd(n): ...
def Stencil3D(n, points=7): ...
def SparseSpMV(rows, cols, density): ...
```

### 2.3 `cpptlm.gpu_soc` — 顶层仿真

**关键 API**：
```python
def simulate(topo, workload, duration_cycles, metrics) -> SimulationResult: ...
class SimulationResult:
    def report(self) -> dict: ...
    def to_markdown(self, baseline_csv=None) -> str: ...
```

**Python ↔ C++ 接口**：Pybind11 桥接（或 subprocess + JSON）

---

## 3. apu_soc 集成测试

`test/test_gpu_soc_phase8c.cc`:
```cpp
TEST_CASE("gpu_soc 与 apu_soc 共享 GpuCluster 集成", "[gpu][apu][phase8c]") {
    auto* factory = ModuleFactory::instance();
    // 加载 apu_soc Phase 7.F 配置 + gpu_soc Phase 8.C 配置
    factory->loadConfig("configs/apu_soc_phase7f.json");
    factory->loadConfig("configs/templates/gpu_soc/gpu_soc_phase8c.json");
    factory->instantiateAll();
    
    // 验证两者引用同一 GpuCluster
    auto* cluster_in_apu = factory->get<GpuCluster>("gpu_cluster");
    auto* cluster_in_gpu = factory->get<GpuCluster>("gpu_cluster");
    REQUIRE(cluster_in_apu == cluster_in_gpu);
    
    // 跑 100 cycles
    for (int i = 0; i < 100; ++i) factory->tick();
    REQUIRE(cluster_in_apu->getCycleCount() == 100);
}
```

---

## 4. 完整验证报告

`docs/validation/phase8c_verification_report.md` 模板：

| 场景 | 测量带宽 | gpgpu-sim baseline | 误差% | 是否通过(±15%) |
|------|---------|-------------------|:-----:|:---:|
| GEMM | TBD | 700 GB/s | TBD% | ✓/✗ |
| FlashAttn | TBD | 470 GB/s | TBD% | ✓/✗ |
| vector_add | TBD | 1176 GB/s | TBD% | ✓/✗ |
| stencil | TBD | 940 GB/s | TBD% | ✓/✗ |
| sparse SpMV | TBD | 230 GB/s | TBD% | ✓/✗ |

+ 延迟 p99 误差表（±20%）
+ Cache 命中率误差（±5pp）
+ TCC 合并比（趋势一致）
+ PowerModel 输出
