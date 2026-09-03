# Tasks — D1-Full 精度对齐 Wave 2

> **配套**: [proposal.md](./proposal.md)

---

## 任务清单

### T1: PipelineTLM 精度对齐（4.6 + G-D5 子项）

- [ ] T1.1 校准 `get_fractional_cycles_by_type(statement_type, PipelineId)` — 公开 benchmark 数据（Rodinia / Parboil / Polybench）— **P1：依赖 gpgpu-sim 参考数据**
- [ ] T1.2 校准 `get_fractional_cycles(instruction, PipelineId)` — **P1**
- [ ] T1.3 `is_placeholder()` 改为返回 `false` — T1.1/T1.2 通过后切换
- [ ] T1.4 TC delegation 规则：`if (statement_type == TC) return 0.0;`（TC 指令 Pipeline 不计延迟，由 TensorCoreTLM 单独计算）

### T2: TensorCoreTLM 精度对齐（4.6 + G-D5 子项）

- [ ] T2.1 校准 6 精度 `get_latency(prec)`：FP4 / FP6 / FP8 / FP16 / BF16 / TF32 — **P1**
- [ ] T2.2 校准 6 精度 `get_throughput_cycles(prec)` — **P1**
- [ ] T2.3 `is_placeholder()` 改为返回 `false` — T2.1/T2.2 通过后切换
- [ ] T2.4 `get_latency_mnk()` 由 `get_latency(prec)` 退化的占位行为替换为基于 M/N/K 矩阵规模的真实查表（如果数据可得）

### T3: 双端链路测试（4.7 + G-D2 + G-D8）

- [ ] T3.1 新增 `test/test_kernel_launch_ptx_integration.cc`（条件编译 `CPPTLM_WITH_PTX_EMU=ON`）
- [ ] T3.2 实现 `MockPtxEmuDriver` 桩（`include/tlm/gpu/test_helpers/`）— 用于无 PTX-EMU 时跑 cycle 对齐回归
- [ ] T3.3 G-D2 验证：`set_blocked_cycles_for_active()` 对 warp 内活跃线程正确设置延迟
- [ ] T3.4 G-D8 验证：`exe_once` stall → re-schedule → release → re-issue 完整循环
- [ ] T3.5 测试注册到 `test/CMakeLists.txt`，条件 `if(CPPTLM_WITH_PTX_EMU)`

### T4: cycle 契约统一 + gpgpu-sim 对比测试（4.9 + G-D3 + G-D5）

- [ ] T4.1 在 `IPtxEmuDriver::advance()` 文档化契约："1 CppTLM tick = 1 `GPUContext::exe_once()`"
- [ ] T4.2 `kernel_launch_tlm.cc::tick()` 明确调用一次 `driver_->advance(MAX_PTX_STEPS_PER_TICK)` 并 switch `AdvanceResult`（G-D3 隐含支持）
- [ ] T4.3 新增 `test/python/test_gpgpu_sim_comparison.py`
  - 5 类 microbenchmark：FFMA / MUFU / LDG / STG / IMAD
  - 与 gpgpu-sim 实测对比，误差目标 ±15%
- [ ] T4.4 把 microbenchmark runner 接入 `test.sh --mode off` + `--mode ptx-emu` 双路径

### T5: 验收

- [ ] T5.1 `PipelineTLM::is_placeholder() == false` 且 `get_fractional_cycles_by_type()` 返回 gpgpu-sim 精确值（误差 ≤ 15%）
- [ ] T5.2 `TensorCoreTLM::is_placeholder() == false` 且 `get_latency()` 返回 gpgpu-sim 精确值（误差 ≤ 15%）
- [ ] T5.3 G-D2 / G-D3 / G-D5 / G-D8 全部 PASS
- [ ] T5.4 `openspec validate 2026-09-03-cpptlm-dgpu-gpgpu-precision-wave2 --strict` PASS
- [ ] T5.5 archive change + 同步 main specs

---

## 状态

**所有任务均处于 Proposed 阶段**，未启动实施。触发条件：
1. gpgpu-sim 参考数据可用
2. PTX-EMU `SMContext::exe_once()` 真实链路测试就绪
3. 跨仓 PTX-EMU 团队确认测试接口

**前置依赖**（已具备）：
- Phase 1-3 + Wave 0/1 已交付并归档（父 change `cpptlm-d1-p1-pipeline-scoreboard` 全部完成项）
- `IPtxEmuDriver` 接口已稳定（4.1-4.5）
- `MockPtxEmuDriver` 测试桩已用于 `test_kernel_launch_tlm_ext.cc`（可借鉴）

**后续**：T1-T4 全部勾选后 → T5 验收 → archive。
