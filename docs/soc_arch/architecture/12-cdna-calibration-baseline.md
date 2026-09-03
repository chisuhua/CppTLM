# 12. CDNA 真实 ISA 校准基线设计方案

> **版本**: v1.0-draft (2027-02-09)
> **状态**: 📋 Proposed — 阶段 D 启动前准备
> **作者**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../adr/ADR-SOC-15-cdna-real-isa-roadmap.md) §4.4 阶段 D（双轨运行时与生产校准）
> **关联设计**: [`architecture/11-cdna-real-isa-integration.md`](./11-cdna-real-isa-integration.md) §11.5 阶段 D 验收门禁
> **关联测试骨架**: `test/python/test_gpgpu_sim_comparison.py`（G-D5 已存在）
> **目的**: 定义 CppTLM 阶段 D（双轨校准）的校准基线方案：5 类 microbenchmark vs AMD 官方白皮书 / MGPUSim / gem5-GPU，建立可重复性能基准。
> **触发**: 阶段 C Gate 通过后启动阶段 D change `cpptlm-dgpu-d1-cdna-isa-phase-d`

---

## 12.1 校准目标

### 12.1.1 核心目标

建立 CppTLM 在 **CDNA3 (GFX942) ISA 路径下的周期精度校准**，对标 3 个权威基准：

| 基准 | 来源 | 价值 |
|------|------|------|
| **AMD 官方白皮书** | AMD Instinct MI300X Architecture Whitepaper | 工业界标准，IPC 与官方吞吐率 |
| **MGPUSim** (GCN3/CDNA 仿真) | `https://github.com/MGPUSim/MGPUSim` | 学术仿真器对照（同类型 ISA 模型） |
| **gem5-GPU** (GPGPU-Sim 分支) | `https://github.com/gem5-gpu/gem5` | 学术仿真器对照（独立实现） |

### 12.1.2 量化指标

| 指标 | 定义 | 容差 |
|------|------|------|
| **IPC 误差** | `\|IPC_cpp - IPC_ref\| / IPC_ref` | ≤ 15%（per ADR-SOC-15 §4.4 Gate D） |
| **MFMA 吞吐率** | `MFMA cycles_cpp / MFMA cycles_ref` | 0.85-1.15 |
| **Memory 延迟分布** | L1/L2/HBM 命中率与平均延迟 | 误差 ≤ 10% |
| **s_waitcnt 周期** | 真实硬件 trace vs CppTLM 周期数 | 误差 ≤ 15% |
| **Wave 调度顺序** | Wave 发射序列与硬件匹配度 | ≥ 90% 调度位置匹配 |

---

## 12.2 5 类 Microbenchmark 设计

**总原则**：每类 microbenchmark 聚焦 **单一微架构特性**，避免混合因素掩盖偏差。

### 12.2.1 类 M1：VALU 标量运算

**目的**：测试 VALU pipeline 的基础执行延迟与吞吐率

**Kernel 设计**：
```c
// GFX942 VALU 循环
__global__ void valu_scalar_kernel(float* a, float* b, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) return;
    float sum = 0.0f;
    #pragma unroll
    for (int i = 0; i < 64; i++) {
        sum += a[gid];        // v_add_f32
        sum *= 1.5f;          // v_mul_f32
        sum = fmaxf(sum, 0.0f); // v_max_f32
    }
    b[gid] = sum;
}
```

**测量指标**：
- 单 VALU 指令平均 IPC（理想 1.0 IPC）
- 64 次循环总周期数
- 与 AMD 白皮书 §3.4 "VALU Pipeline" 对比

**校准文件**：`examples/microbenchmarks/m1_valu_scalar.hip` + `test/python/test_gpgpu_sim_comparison.py::test_m1_valu`

### 12.2.2 类 M2：MFMA 矩阵运算

**目的**：测试 Matrix Core 的 MFMA 吞吐率

**Kernel 设计**：
```c
// GFX942 MFMA SGEMM-like
__global__ void mfma_sgemm_kernel(half* A, half* B, float* C, int M, int N, int K) {
    // 16x16x16 FP16 MFMA
    using float16_t = __half;
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    // ... (MFMA 内联汇编略)
}
```

**测量指标**：
- 单 MFMA 指令周期数（理想 32 cycles for `v_mfma_f32_16x16x16_fp16`）
- SGEMM 总周期数
- 与 AMD 白皮书 §6 "Matrix Core" 对比

**校准文件**：`examples/microbenchmarks/m2_mfma_sgemm.hip` + `test/python/test_gpgpu_sim_comparison.py::test_m2_mfma`

### 12.2.3 类 M3：Memory 访存模式

**目的**：测试 L1/L2/HBM 各级延迟与带宽

**Kernel 设计**（3 子测试）：
```c
// M3a: L1 命中（连续地址）
__global__ void l1_hit_kernel(int* data) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    int sum = 0;
    #pragma unroll
    for (int i = 0; i < 100; i++) {
        sum += data[gid];  // L1 命中
    }
}

// M3b: L2 命中（跨 block 共享）
__global__ void l2_hit_kernel(int* data, int stride) {
    // ... (跨 block 共享同一行)
}

// M3c: HBM 访存（cold miss）
__global__ void hbm_kernel(int* data, int large_stride) {
    // ... (随机访存，HBM 主导)
}
```

**测量指标**：
- L1 命中平均延迟（理想 30 cycles）
- L2 命中平均延迟（理想 200 cycles）
- HBM 平均延迟（理想 500-800 cycles）
- 各级命中率
- 与 AMD 白皮书 §4 "Memory Hierarchy" 对比

**校准文件**：`examples/microbenchmarks/m3_memory_*.hip` + `test/python/test_gpgpu_sim_comparison.py::test_m3_memory`

### 12.2.4 类 M4：s_waitcnt 显式同步

**目的**：测试 CDNA 显式计数器精度

**Kernel 设计**：
```c
// GFX942 vmcnt/lgkmcnt 显式计数器
__global__ void waitcnt_kernel(half* A, half* B, float* C) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    
    // 触发 4 个 global_load
    half a0 = A[gid * 4 + 0];
    half a1 = A[gid * 4 + 1];
    half a2 = A[gid * 4 + 2];
    half a3 = A[gid * 4 + 3];
    
    // 等待所有 global_load 完成 (vmcnt=0)
    asm volatile("s_waitcnt vmcnt(0)" ::: "memory");
    
    half sum = __hadd(a0, a1);
    sum = __hadd(sum, a2);
    sum = __hadd(sum, a3);
    
    C[gid] = __half2float(sum);
}
```

**测量指标**：
- `s_waitcnt` 触发到 wave 恢复执行的周期数
- 4 个 global_load 完成 vs vmcnt 计数到 0 的精确周期
- 与 AMD 白皮书 §2.5 "Wait Counters" 对比

**校准文件**：`examples/microbenchmarks/m4_waitcnt.hip` + `test/python/test_gpgpu_sim_comparison.py::test_m4_waitcnt`

### 12.2.5 类 M5：Wave 调度与 SIMT 分歧

**目的**：测试 Wave64 调度与 EXEC mask 控制流分歧

**Kernel 设计**：
```c
// GFX942 EXEC mask + 分歧
__global__ void wave_diverge_kernel(int* data, int n) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= n) return;
    
    // 模拟 SIMT 分歧
    if (gid % 2 == 0) {
        data[gid] = data[gid] * 2;
    } else {
        data[gid] = data[gid] + 1;
    }
}
```

**测量指标**：
- 分歧路径的 wave 切换周期数
- EXEC mask 切换的周期开销
- 与 AMD 白皮书 §5.2 "Wavefront Execution" 对比

**校准文件**：`examples/microbenchmarks/m5_wave_diverge.hip` + `test/python/test_gpgpu_sim_comparison.py::test_m5_wave`

---

## 12.3 校准测试基础设施

### 12.3.1 参考数据采集脚本

**Python 工具**（`tools/calibration/collect_reference.py`）：

```python
# 伪代码（不在本草案范围内实施，仅设计）
# 1. 调用 MGPUSim/gem5-GPU 运行同一 kernel，输出 cycle count
# 2. 调用 AMD rocprofiler 在 MI300X 上运行同一 kernel，输出 PMC trace
# 3. 输出 JSON: { kernel_name, cycles_mgpusim, cycles_gem5, cycles_amd, cycles_cpptlm }

# 输出格式（per microbenchmark）
{
    "M1_VALU": {
        "cycles_amd_published": 64,
        "cycles_mgpusim": 68,
        "cycles_gem5": 71,
        "cycles_cpptlm_stage_c": 70,
        "ipc_error_pct": 9.4,  # 70 vs 64 = 9.4% 误差
        "gate_status": "PASS"  # ≤15% 误差
    },
    ...
}
```

### 12.3.2 CppTLM 校准输出 API

**新增头文件**：`include/tlm/gpu/calibration_metrics.hh`

```cpp
// 提供 cycle count 输出钩子
class ICalibrationSink {
public:
    virtual ~ICalibrationSink() = default;
    virtual void record_kernel_cycles(const std::string& kernel_name, uint64_t cycles) = 0;
    virtual void record_mfma_throughput(uint32_t sm_id, double gflops) = 0;
    virtual void record_memory_latency(uint32_t sm_id, uint8_t level, double cycles) = 0;
};
```

**CppTLM 端实现**：`JsonCalibrationSink`（输出 JSON 给 `test_gpgpu_sim_comparison.py` 解析）

### 12.3.3 CI 集成

**CTest 注册**（`test/CMakeLists.txt`）：

```cmake
# 阶段 D 启动时新增
add_test(
    NAME calibration_m1_valu
    COMMAND python3 ${CMAKE_SOURCE_DIR}/test/python/run_calibration.py
            --kernel m1_valu
            --isa cdna3
            --tolerance 0.15
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
# 类似注册 M2-M5
```

**CI 工作流**：每日定时校准运行 + PR 阻断（误差超 15% 时 PR 红）

---

## 12.4 校准实施时间表

| 周次 | 任务 | 产出 |
|------|------|------|
| D.1 | M1/M2 microbenchmark 内核实现（CUDA/HIP/汇编混合） | `examples/microbenchmarks/*.hip` |
| D.2 | M3a/M3b/M3c 内存测试 + L1/L2/HBM 延迟打点 | 同上 |
| D.3 | M4 waitcnt 测试 + 精确 vmcnt/lgkmcnt 测量 | 同上 |
| D.4 | M5 wave 分歧测试 + EXEC mask 切换周期 | 同上 |
| D.5 | `ICalibrationSink` + `JsonCalibrationSink` + 测试集成 | `calibration_metrics.hh` |
| D.6 | `tools/calibration/collect_reference.py` + MGPUSim/gem5 数据采集 | 校准基线数据 |
| D.7 | 5 类 microbenchmark vs AMD/MGPUSim/gem5 误差分析 | 校准报告 |
| D.8 | 误差 > 15% 的 microbenchmark 调优 | 调优 PR |
| D.9 | CI 集成 + 自动化 PR 阻断 | CTest 注册 |
| D.10 | Gate D 评审 + 文档同步 | ADR-SOC-15 Gate D |

---

## 12.5 已知限制与未来扩展

### 12.5.1 阶段 D 范围内的限制

- **AMD 官方硬件 trace 数据依赖外部采集**：需要 MI300X 真实硬件访问权限；如无硬件，使用 MGPUSim + gem5-GPU 数据作为替代基准
- **Microbenchmark 内核简化**：每个 kernel 聚焦单一特性，不覆盖全部指令集
- **误差容差 15%**：是工业标准，精度受限于仿真器本身

### 12.5.2 阶段 D 后续扩展（不在本草案范围）

- 全应用级 benchmark（GEMM、Convolution、Attention）校准
- 多 SM 拓扑扩展（阶段 D 当前假设单 SM）
- 真实 GPU 驱动栈 ROCm/HSA 集成（per ADR-SOC-09 §2 D3 由 UsrLinuxEmu 协调）

---

## 12.6 验收门禁（与 ADR-SOC-15 §4.4 Gate D 对齐）

✅ **Gate D 必交付物**：
1. 5 类 microbenchmark 内核 + 测量脚本完整（10 个文件）
2. `ICalibrationSink` + `JsonCalibrationSink` + CTest 注册（3 个文件）
3. 校准基线报告（误差 ≤ 15% per microbenchmark）
4. CI 工作流每日定时校准 + PR 阻断
5. 切换 `"isa_type": "ptx"` 与 `"cdna3"` 两种模式各自完整回归

✅ **Gate D 评审**：
- Oracle 评审应用 `task(subagent_type="oracle", ...)` 验证误差分布
- 用户评审 5 类 microbenchmark 结果
- 校准报告归档到 `docs/validation/2027-02-XX-cdna-phase-d-calibration.md`

---

## 12.7 参考文献

### 12.7.1 AMD 官方资料

| 资料 | URL |
|------|-----|
| AMD Instinct MI300X Architecture Whitepaper | https://www.amd.com/system/files/documents/amd-instinct-mi300x-whitepaper.pdf |
| AMD ROCm Documentation | https://rocmdocs.amd.com/ |
| AMD GPU Open (CDNA ISA Reference) | https://gpuopen.com/documentation/ |

### 12.7.2 学术仿真器

| 仿真器 | URL | 用途 |
|--------|-----|------|
| MGPUSim | https://github.com/MGPUSim/MGPUSim | AMD GCN3/CDNA 仿真对照 |
| gem5-GPU | https://github.com/gem5-gpu/gem5 | 独立实现的 GPGPU 仿真 |
| GPGPU-Sim | https://github.com/gpgpu-sim/gpgpu-sim_distribution | NVIDIA PTX 仿真（PTX 阶段对照） |
| accel-sim | https://github.com/accel-sim/accel-sim-framework | NVIDIA SASS trace-driven 仿真 |

### 12.7.3 关联 OpenSpec

| Change | 计划启动 |
|--------|----------|
| `cpptlm-dgpu-d1-cdna-isa-phase-d` | 阶段 C Gate 通过后 |

### 12.7.4 关联 OpenSpec tests

| 标签 | 描述 |
|------|------|
| `[gpgpu-sim-comparison]` | G-D5 校准测试（Phase 1 阶段已建立） |
| `[calibration]` | 阶段 D 校准新增 |