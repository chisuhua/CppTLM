# gpu-kernel-launch 微架构文档

> **类别**: GPU > Dispatcher · **状态**: ✅ Phase 8.A Task 4 + 📋 **v1.0 双 vendor 蓝图共享**(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D2)
> **Header**: `include/tlm/gpu/kernel_launch_tlm.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`)
> **蓝图来源**: HSA AQL (Architected Queuing Language) 简化 + NVIDIA CUDA kernel dispatch + AMD PM4 dispatch
> **首版 commit**: `b8bd411` (2026-06-28) · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - Spec: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../../superpowers/specs/2026-06-24-gpu-soc-architecture.md) §3.4
> - ADR:
>   - [`docs/soc_arch/adr/ADR-SOC-04-hsapp-simplification.md`](../../soc_arch/adr/ADR-SOC-04-hsapp-simplification.md) — HSAPP/CP/Dispatcher 极简化 (KernelLaunchTLM ~150 行)
>   - [`docs/soc_arch/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../soc_arch/adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D2 — v1.0 双 vendor 蓝图共享
> - GPU Kernellaunch: [`gpu-kernel-launch.md`](./gpu-kernel-launch.md)（旧版 pending doc 已删除，2026-07-14）
> - GPU 通用: [`gpu.common.md`](./gpu.common.md)

---

## 1. 设计目标

`KernelLaunchTLM` 是 **AQL 简化 dispatcher**，按周期间隔向 ComputeCluster 发送 KernelDesc（简化版 AQL packet）。按 ADR-SOC-04 黑盒决策：不模拟真实 HSA Queue、AQL packet 格式、Command Processor。

**核心特性**:
- 继承 `ChStreamModuleBase`，单端口
- tick() 按 `interval` 周期发射 kernel launch
- 4 个可配置属性：kernel_id / workgroup_size / grid_size / launch_interval
- 统计：已 launch kernel 数（`kernels_launched`）

## 2. 架构概览

### 2.1 内部状态机

```
    KernelLaunchTLM (interval=1000)
         │
    ┌────┴────┐
    │  tick():
    │    cycle_counter_++
    │    if (cycle_counter_ % interval_ == 0):
    │        emit KernelDesc {
    │            kernel_id_,
    │            workgroup_size_,
    │            grid_size_
    │        }
    │        kernels_launched_++
    │    adapter_->tick()
    └─────────┘
         │
    adapter_ → GpuComputeUnitTLM
```

### 2.2 发射时序示例

```
interval = 1000

cycle  0:   launch kernel#0  (kernels_launched=1)
cycle  1000: launch kernel#1  (kernels_launched=2)
cycle  2000: launch kernel#2  (kernels_launched=3)
...
```

### 2.3 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|:---:|------|
| `adapter_` | `StreamAdapterBase*` | 1 | ChStream 桥接 |

## 3. 接口（Public API）

```cpp
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    KernelLaunchTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "KernelLaunchTLM"; }

    // Setter（JSON 解析在 Phase 7.B+ 完整实现）
    void set_kernel_id(uint32_t id);
    void set_workgroup_size(uint32_t sz);
    void set_grid_size(uint32_t sz);
    void set_kernel_launch_interval(uint32_t cyc);

    uint32_t get_kernel_id() const;
    uint32_t get_workgroup_size() const;
    uint32_t get_grid_size() const;
    uint32_t get_kernel_launch_interval() const;
    uint64_t kernels_launched() const;

    void tick() override;
};
```

## 4. 配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `kernel_id` | uint32 | 0 | Kernel 标识符 |
| `workgroup_size` | uint32 | 64 | Workgroup 大小 (threads/block) |
| `grid_size` | uint32 | 1 | Grid 大小 (blocks/kernel) |
| `kernel_launch_interval` | uint32 | 1000 | 两次 launch 间隔 (cycles) |

## 5. 测试覆盖

| 测试文件 | 标签 | 测试数 | 覆盖内容 |
|------|------|:---:|------|
| `test/test_kernel_launch_tlm.cc` | `[gpu][kernel_launch][phase8a]` | 7 | interval dispatch / setter-getter / kernels_launched 计数 |

**关键测试用例**:
```cpp
TEST_CASE("KernelLaunchTLM: dispatch at interval", "[gpu][kernel_launch]") {
    KernelLaunchTLM kl("kl", nullptr);
    kl.set_kernel_launch_interval(10);
    for (int i = 0; i < 50; ++i) kl.tick();
    REQUIRE(kl.kernels_launched() == 5);  // 5 launches in 50 cycles
}
```

## 6. 演进路径：F12b-LD PTX-EMU 集成

当前 `KernelLaunchTLM` 使用内置的周期计数器发射。F12b-LD 完成后，它将直接接收 PTX-EMU 的 `KernelLaunchRequest`：

```
F12a (当前):
  KernelLaunchTLM.tick() → cycle_counter_%interval_==0 → 发射

F12b-LD (PTX-EMU 集成):
  cudaLaunchKernel() intercepted by libcpptlm_cudart.so
    → PTX-EMU parses PTX → builds KernelLaunchRequest
    → KernelLaunchTLM::submit(request)
    → CppTLM EventQueue drives GPUContext::exe_once()
```

## 7. 已识别限制 (Oracle 审查)

| 限制 | 影响 | 计划 |
|------|------|------|
| tick() 纯计数器（无实际 dispatch） | 无法驱动 GpuComputeUnitTLM | Phase 8.A Task 7 集成测试时解决 |
| StreamAdapter 注册缺失（Oracle 指出） | 无法通过 JSON 连接 | Phase 8.B 补充 |
| 无 KernelDesc Bundle 类型 | 无法传递结构化 kernel 参数 | F12b-LD 时定义 |

## 8. 参考文献

- HSA Platform System Architecture Specification §2.5: AQL
- AMD ROCm: HSAPacketProcessor + GPUDispatcher
- NVIDIA CUDA: `cudaLaunchKernel` dispatch flow
- F12b-LD 设计: [`docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`](../../superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md)

---

*维护者: CppTLM Team · 最后更新: 2026-07-02*