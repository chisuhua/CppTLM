# ADR-SOC-02: ComputeUnit 黑盒优先（CU Black-Box Granularity）

> **状态**: ✅ 已确认
> **日期**: 2026-06-14
> **影响**: `ComputeUnitTLM` 设计范围（Phase 7.B+）
> **类别**: SoC 架构 / GPU 建模粒度

---

## 1. 背景

Phase 7 引入 `ComputeUnitTLM`（CU）作为 GPU 计算单元。需要决定 CU 内部建模粒度：

| 选项 | 描述 | 工作量 |
|------|------|--------|
| **黑盒** | CU 就是一个 `tick()` 循环，按 `kernel_issue_interval_` 参数发请求 | Low |
| 5-stage pipeline | Fetch → Scoreboard → Schedule → Exec → Mem + 精确 wavefront 调度 | Very High |
| 完整 ISA | 模拟 SIMD lane、寄存器堆、shader 调度 | Prohibitive |

---

## 2. 决策

✅ **黑盒优先**。CU 在 Phase 7.B 就是一个 `tick()` 循环，行为简化为：

```cpp
void ComputeUnitTLM::tick() {
    if (cycles_until_next_kernel_issue_ == 0) {
        emit_kernel_launch_request();
        cycles_until_next_kernel_issue_ = kernel_issue_interval_;
    } else {
        --cycles_until_next_kernel_issue_;
    }
}
```

**不做**: 5-stage pipeline、SIMD lane、寄存器堆、wavefront 调度、ISA 解析、精确的 issue/execute 周期建模。

**只做**: 参数化 `kernel_issue_interval_`、`workgroup_progress_` map、`inflight_kernel_reqs_` 跟踪。

---

## 3. 实施

| 字段 | 默认值 | 说明 |
|------|--------|------|
| `kernel_issue_interval_` | 100 cycles | 两次 kernel launch 间隔 |
| `num_cus_` | 1 (Phase 7.B) / 4 (Phase 7.E) | CU 实例数量 |
| `workgroup_progress_` | `std::map<kernel_id, cycles_left>` | 内核进度跟踪 |
| `inflight_kernel_reqs_` | `std::map<kernel_id, ComputeReqBundle>` | 已发出未完成请求 |

---

## 4. 推迟 / 永不做

- ❌ 5-stage pipeline 精确建模（永久推迟）
- ❌ SIMD lane / 寄存器堆 / shader 调度（永久不做）
- ❌ Wavefront 内线程调度（参见 [ADR-SOC-03](./ADR-SOC-03-wavefront-coalescing-abstraction.md)）
- ❌ LDS（Local Data Share）建模（Phase 7.F+ 可选）

---

## 5. 参考文献

- 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §3 D2（L617-624）
- 复述: [`roadmap.md`](../../../roadmap.md) L64 D2 行
- 微架构: [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md), [`gpu-gputlm.md`](../modules/gpu-gputlm.md)
- SoC 集成: [`docs/soc_arch/specs/apu-soc-design.md`](../specs/apu-soc-design.md)

---

## Status Update

- **2027-02-09**: ⚠️ **Superseded by ADR-SOC-06 D2** (CU 黑盒路径 → CudaCoreAdapter SM 微架构探索器 + 深度集成 PTX-EMU internal + WarpState 镜像 PC/cycle)
  **理由**:Phase 7.B+ 黑盒 MVP 路径不足以观测 PC/cycle,与"接近真实 CudaCore"目标矛盾(per `docs/research/SM/overview.md` §3.2 + ADR-SOC-06 Phase F-H 架构重定义 D2 反转);PTX-EMU internal C++ 接口已成熟(per `include/cudart/` 4 个 vendored 接口 + `cpptlm_emu/ptxsim` 编译防火墙)。
  **保留原文作为 v0.5 s2 历史记录**;`ComputeUnitTLM::tick()` 黑盒循环在 v0.5 MVP 仍有 1 处使用(per `docs/soc_arch/specs/apu-soc-design.md` D2)。
- **2027-02-09**: v1.0 战略下 ComputeUnitTLM 升级为蓝图模式 + 双 vendor 路径切换(per [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](./ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D1)
- **2027-02-09**: 📋 本 ADR 决策**被反转**。新设计 [`architecture/15-sm-microarchitecture-design.md`](../architecture/15-sm-microarchitecture-design.md) §15.2-§15.6 实施完整 SM 微架构（5-stage pipeline + SIMT lane + wavefront 调度 + 精确 issue/execute 周期建模），取代本 ADR 的"CU 黑盒 + 永久推迟"决策。背书: [`ADR-SOC-16-sm-microarchitecture.md`](./ADR-SOC-16-sm-microarchitecture.md)。
