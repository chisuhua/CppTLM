# ADR-SOC-05: GPU 目录结构（GPU Subdirectory Layout）

> **状态**: ✅ 已确认
> **日期**: 2026-06-14
> **影响**: `include/tlm/gpu/` 子目录结构
> **类别**: SoC 架构 / 代码组织

---

## 1. 背景

Phase 7 引入 4–5 个 GPU 特定 TLM 模块。需要决定目录布局：

| 选项 | 描述 | 评价 |
|------|------|------|
| **新建 `include/tlm/gpu/` 子目录** | GPU 特定代码独立子目录 | ✅ 推荐 |
| 扩展现有 `include/tlm/` | 与 CPU/IO 模块混放 | ⚠️ 污染通用 TLM 目录 |

---

## 2. 决策

✅ **新建 `include/tlm/gpu/` 子目录**，清晰隔离 GPU 特定代码，避免污染通用 TLM 目录。

采用结构：

```
include/tlm/
├── cache_tlm.hh              (扩展现有 — 支持 coherence protocol，参见 ADR-SOC-01)
├── gpu/
│   ├── compute_unit_tlm.hh   (ComputeUnitTLM — Phase 7.B+)
│   ├── tcc_tlm.hh            (TCC_TLM — GPU last-level cache，Phase 7.D)
│   ├── kernel_launch_tlm.hh  (KernelLaunchTLM — 参见 ADR-SOC-04)
│   └── pcie_bridge_tlm.hh    (Phase 2 备选 dGPU，defer)
├── bundles/
│   └── compute_bundles_tlm.hh (ComputeReqBundle/ComputeRespBundle — Phase 7.A 已落地)
```

---

## 3. 实施

| 阶段 | 任务 | 文件 |
|------|------|------|
| 7.A | GPU 基础设施 | `include/bundles/compute_bundles_tlm.hh` ✅、`include/tlm/gpu/gpu_tlm.hh` ✅ |
| 7.B | `ComputeUnitTLM` 黑盒（参见 [ADR-SOC-02](./ADR-SOC-02-cu-granularity.md)） | `include/tlm/gpu/compute_unit_tlm.hh` |
| 7.D | `TCC_TLM` (DualPortStreamAdapter, write coalescing) | `include/tlm/gpu/tcc_tlm.hh` |
| 7.D | `KernelLaunchTLM` 强化（参见 [ADR-SOC-04](./ADR-SOC-04-hsapp-cp-dispatcher-simplification.md)） | `include/tlm/gpu/kernel_launch_tlm.hh` |
| Phase 2 备选 | `PCIBridge_TLM` (dGPU) | `include/tlm/gpu/pcie_bridge_tlm.hh` |

---

## 4. 注册与发现

GPU 模块通过 `REGISTER_CHSTREAM` 宏注册，与 CPU/IO 模块走同一注册表：

```cpp
REGISTER_CHSTREAM(GPUTLM, ComputeReqBundle, ComputeRespBundle)        // 7.A ✅
REGISTER_CHSTREAM(ComputeUnitTLM, ComputeReqBundle, ComputeRespBundle) // 7.B
REGISTER_CHSTREAM(KernelLaunchTLM, ComputeReqBundle, ComputeRespBundle) // 7.D
REGISTER_CHSTREAM(TCC_TLM, ComputeReqBundle, ComputeRespBundle)         // 7.D
```

---

## 5. 参考文献

- 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §3 D5（L657-677）
- 复述: [`roadmap.md`](../../../roadmap.md) L67 D5 行
- 落地: [`AGENTS.md`](../../../AGENTS.md) L19 STRUCTURE 节（已含 `include/tlm/gpu/`）
- SoC 集成: [`docs/soc_arch/specs/apu-soc-design.md`](../specs/apu-soc-design.md)
- 关联: [ADR-SOC-01 一致性策略](./ADR-SOC-01-coherence-protocol-strategy.md)、[ADR-SOC-02 CU 粒度](./ADR-SOC-02-cu-granularity.md)、[ADR-SOC-04 HSAPP 简化](./ADR-SOC-04-hsapp-cp-dispatcher-simplification.md)
