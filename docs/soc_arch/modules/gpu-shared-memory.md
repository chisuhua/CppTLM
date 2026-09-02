# gpu-shared-memory 微架构文档

> **类别**: GPU > SM 内部 · **状态**: ✅ Phase 8.A Task 1 + 📋 **v1.0 SMEM/LDS + DSMEM/TMEM 扩展**(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D5 + L6 SM/CU 子系统架构)
> **Header**: `include/tlm/gpu/shared_memory_tlm.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`)
> **蓝图来源**: NVIDIA SM shared memory + L1 unified (32 bank 可配置) + AMD LDS (Local Data Share, 64KB per CU)
> **首版 commit**: `b8bd411` (2026-06-28) · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - Spec: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../../superpowers/specs/2026-06-24-gpu-soc-architecture.md) §3.1
> - GPU Compute Unit: [`gpu-compute_unit.md`](./gpu-compute_unit.md)
> - **L6 SM/CU 子系统架构**: [`docs/soc_arch/architecture/05-sm-compute-unit.md`](../architecture/05-sm-compute-unit.md) §5-§6 Warp 调度 + 寄存器
> - **L7 Memory 子系统架构**: [`docs/soc_arch/architecture/07-memory-system.md`](../architecture/07-memory-system.md) §5 L1 + SMEM
> - **关联 ADR**:
>   - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D5 — v1.0 SMEM/LDS 共享
> - GPU 通用: [`gpu.common.md`](./gpu.common.md)

---

## 1. 设计目标

`SharedMemoryTLM` 是 **SM 内部 shared memory + L1 unified 简化模型**，提供 bank conflict 周期惩罚计算。按 ADR-NV-01 D2 决策，采用黑盒化 sub-core 策略，仅建模 bank 数量对并发访问的延迟影响，不模拟真实 SM 内部 L1/shared memory 分区和调度。

**核心特性**:
- 继承 `ChStreamModuleBase`，单端口
- 32 bank 可配置（默认：NVIDIA SM 标准）
- 64 KB 可配置（默认：GB203 SM L1/shared memory）
- Bank conflict 模型：base 1 cycle + 每个 conflict way +1 cycle

## 2. 架构概览

### 2.1 内部结构

```
    SharedMemoryTLM (size_kb=64, banks=32)
         │
    ┌────┴────┐
    │  bank_conflict_cycles(num_threads, stride_bytes):
    │    计算每个 bank 的访问数
    │    max_count = max(bank_count per bank)
    │    return 1 + (max_count - 1)
    │
    │  tick():
    │    adapter_->tick()
    └─────────┘
         │
    adapter_ → 连接 CU → GpuMeshNoC
```

### 2.2 Bank conflict 示例

```
32 banks, 4 banks × 32-bit = 128B per bank group

线程 0: reg 0  → bank 0
线程 1: reg 4  → bank 4
线程 2: reg 8  → bank 8
线程 3: reg 12 → bank 12  → max_count = 1 → 1 cycle (无冲突)

线程 0: reg 0  → bank 0
线程 1: reg 4  → bank 0  (stride=16B, 落在同一 bank)
线程 2: reg 8  → bank 0
线程 3: reg 12 → bank 0  → max_count = 4 → 1 + (4-1) = 4 cycles
```

### 2.3 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|:---:|------|
| `adapter_` | `StreamAdapterBase*` | 1 | ChStream 桥接 |

## 3. 接口（Public API）

```cpp
class SharedMemoryTLM : public ChStreamModuleBase {
public:
    explicit SharedMemoryTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "SharedMemoryTLM"; }

    // Bank conflict 周期计算（测试核心 API）
    uint32_t bank_conflict_cycles(uint32_t num_threads,
                                  uint32_t stride_bytes) const;

    // Setter（JSON 解析后注入）
    void set_size_kb(uint32_t size_kb);
    void set_banks(uint32_t banks);

    uint32_t get_size_kb() const;
    uint32_t get_banks() const;

    void tick() override;
};
```

## 4. 配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `size_kb` | uint32 | 64 | Shared memory 大小 (KB)，GB203: 100 KB per SM |
| `banks` | uint32 | 32 | Bank 数量，NVIDIA 标准: 32 |

## 5. 测试覆盖

| 测试文件 | 标签 | 测试数 | 覆盖内容 |
|------|------|:---:|------|
| `test/test_shared_memory_tlm.cc` | `[gpu][smem][phase8a]` | 9 | bank conflict / size_kb/banks getter |

**关键测试用例**:
```cpp
TEST_CASE("SharedMemoryTLM: bank conflict 4-way adds +1 cycle", "[gpu][smem]") {
    SharedMemoryTLM smem("smem", nullptr, 64, 32);
    auto t1 = smem.bank_conflict_cycles(4, 128);  // 4 threads, stride 128B
    REQUIRE(t1 == 4);  // base 1 + 3 extra
}
```

## 6. 已识别限制 (Oracle 审查)

| 限制 | 影响 | 计划 |
|------|------|------|
| `stride_bytes` 在公式中实际未使用（仅用 bank 取模） | bank 访问模式不精确 | Phase 8.B 修复 |
| tick() 纯计数器（无实际内存访问逻辑） | 无法验证真实 SMEM 读写 | Phase 8.B 与 SubCoreTLM 集成时补充 |
| StreamAdapter 注册缺失（Oracle 指出） | 无法通过 JSON 连接 | Phase 8.B 补充 `REGISTER_CHSTREAM` |

## 7. 参考文献

- NVIDIA CUDA C Programming Guide §5.4: Shared Memory + Bank Conflicts
- GB203 SM: 100 KB unified L1/Shared Memory, 32 banks
- gpgpu-sim SM_120 paper: shared memory bank configuration

---

*维护者: CppTLM Team · 最后更新: 2026-07-02*