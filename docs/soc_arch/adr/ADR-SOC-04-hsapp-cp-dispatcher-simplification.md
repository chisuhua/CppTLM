# ADR-SOC-04: HSAPP/CP/Dispatcher 极简化（Host-Software Stack Simplification）

> **状态**: ✅ 已确认
> **日期**: 2026-06-14
> **影响**: `KernelLaunchTLM` 取代完整 HSA runtime
> **类别**: SoC 架构 / Host-GPU 接口

---

## 1. 背景

gem5 中完整的 CPU→GPU 软件栈由三个 gem5 特定模块组成（参见 `src/dev/amdgpu/`）：

| 模块 | 职责 | 估计行数 |
|------|------|----------|
| `HSAPPacketProcessor` | 解析 HSA 协议包 | ~800 |
| `GPUCommandProcessor` | 命令队列、doorbell、AQL 解析 | ~1200 |
| `GPUDispatcher` | wavefront 调度、分发到 CU | ~1000 |
| **合计** | | **~3000+ 行** |

CppTLM 作为 TLM 仿真框架，**不模拟 Host 软件栈**，只关心 GPU 端计算行为。

---

## 2. 决策

✅ **用一个 `KernelLaunchTLM` 模块（~150 行）代替完整 HSA 三件套**。

`KernelLaunchTLM` 在 `tick()` 中定期向 `ComputeUnitTLM` 发 kernel launch 命令。配置示例：

```json
{
  "name": "kernel_launcher",
  "type": "KernelLaunchTLM",
  "params": {
    "num_workgroups": 8,
    "workgroup_size": 64,
    "kernel_duration": 100
  }
}
```

**不做**:
- ❌ PIO（Port I/O）寄存器
- ❌ Doorbell 机制
- ❌ AQL (Architected Queuing Language) 包解析
- ❌ HSA packet 协议
- ❌ 真实 ROCm/HSA runtime 行为

---

## 3. 实施

| 阶段 | 任务 |
|------|------|
| 7.B | `KernelLaunchTLM` 基础版（~150 行），按 `num_workgroups × workgroup_size` 发出 `ComputeReqBundle` |
| 7.D | `TCC_TLM`（GPU last-level cache）+ DualPortStreamAdapter 接 KernelLaunchTLM |
| 7.E | 多个 KernelLaunchTLM 实例 + Multi-CU 调度 |

---

## 4. 推迟 / 永不做

- ❌ 真实 HSA runtime 行为模拟（永久不做）
- ❌ ROCm KFD 接口（Post MVP）
- ❌ AQL packet 流解析（永久不做）
- ❌ Doorbell 寄存器交互（永久不做）

---

## 5. 参考文献

- 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §3 D4（L635-655）
- 复述: [`roadmap.md`](../../../roadmap.md) L66 D4 行
- 微架构: [`docs/soc_arch/modules/gpu-kernel-launch.md`](../modules/gpu-kernel-launch.md)
- SoC 集成: [`docs/soc_arch/specs/apu-soc-design.md`](../specs/apu-soc-design.md)
- 蓝图参考（仅作了解，不复制）: gem5 `src/dev/amdgpu/amdgpu_device.py`

---

## Status Update

- **2027-02-09**: ⚠️ **Superseded partial**(Section 4 "永不做" 列表与 v1.0 双 vendor 战略部分冲突)
  **冲突项与处理**:
  - ❌ ROCm KFD 接口（Post MVP）→ **v1.0 AMD 路径必需**(per [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](./ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D1 + D3);需依赖 UsrLinuxEmu 跨仓承诺(UsrLinuxEmu ADR-088 当前 NVIDIA-only,需 owner 联签/分阶段实施)
  - ❌ AQL packet 流解析（永久不做）→ **Pm4Decoder-AMD TYPE3 opcode v1.0 MVP 实施**(per [`docs/soc_arch/architecture/02-command-processor.md`](../architecture/02-command-processor.md) §5);v1.1 完整版追加全 TYPE3 opcode
  - ❌ Doorbell 寄存器交互（永久不做）→ **AMD Ring Buffer / Doorbell v1.0 MVP 实施**(per [`02-command-processor.md`](../architecture/02-command-processor.md) §7.2)
  **保留项**:
  - ✅ 真实 HSA runtime 行为模拟（永久不做）—— 保留
  - ✅ KernelLaunchTLM ~150 行代替 HSA 三件套 —— **保留**(per ADR-SOC-06 D5 CP→TMU→SQ→Cuda Core 链路,KernelLaunchTLM 作为 CP 一部分)
  **决策性变更不写入本 ADR 正文**;需双 vendor 战略落地的具体决策决策请参考 [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](./ADR-SOC-09-v1-nvidia-amd-dual-vendor.md)
