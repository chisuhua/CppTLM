# Proposal: cpptlm-dgpu-gpgpu-precision-wave2 — D1-Full 精度对齐 Wave 2

> **状态**: Proposed — 2026-09-03
> **父 change**: `cpptlm-d1-p1-pipeline-scoreboard`（已归档 44/51，7 个 Deferred 任务移交本 change）
> **Cross-project**: `PTX-EMU/openspec/changes/cpptlm-d1-full/`（依赖 PTX-EMU 真实 `SMContext::exe_once()` 链路测试）
> **Wave**: 后续 Wave 2（依赖 gpgpu-sim 参考数据 + PTX-EMU 真实链路）

## Why

CppTLM P1 已完成 Phase 1-3 与 Phase 4 Wave 0/1：Vendor 接口、ScoreboardTLM、PipelineTLM、TensorCoreTLM、AsyncCompletionAdapter、PTX-EMU driver seam、每 tick 一次 advance 以及 per-SM scoreboard 注入均已交付。Phase 4 Wave 2 的 7 个未完成任务（4.6 / 4.7 / 4.9 / G-D2 / G-D3 / G-D5 / G-D8）依赖外部 gpgpu-sim 参考数据与 PTX-EMU 真实 `SMContext::exe_once()` 端到端链路，因此从父 change 正式移交至本 change。

## What Changes

| 编号 | 内容 |
|------|------|
| **4.6** | PipelineTLM + TensorCoreTLM 占位值替换为 gpgpu-sim 精确值；`is_placeholder()` 置 false；Pipeline 对 TC 指令返回 0 延迟 |
| **4.7** | `test/test_kernel_launch_ptx_integration.cc`，条件链接 PTX-EMU，覆盖 G-D2/G-D3/G-D8 |
| **4.9** | `test/python/test_gpgpu_sim_comparison.py`，5 类 microbenchmark 对比 gpgpu-sim，误差目标 ±15% |
| **G-D2** | `set_blocked_cycles_for_active()` 正确作用于 warp 内活跃线程 |
| **G-D3** | `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle |
| **G-D5** | 5 类 microbenchmark 与 gpgpu-sim 对比误差 ≤ ±15% |
| **G-D8** | `exe_once` stall → re-schedule → release → re-issue 完整循环 |

## Scope

**In Scope**:
- PipelineTLM 与 TensorCoreTLM 的 gpgpu-sim 精度校准和 placeholder 状态切换
- TC delegation 规则：Pipeline 对 TC 指令返回 0 延迟
- `test_kernel_launch_ptx_integration.cc` 条件编译双端链路测试
- `test_gpgpu_sim_comparison.py` 五类 microbenchmark
- 统一 cycle 契约：1 CppTLM tick = 1 `GPUContext::exe_once()`

**Out of Scope**:
- 已完成的 Phase 1-3 与 Wave 0/1 工作
- `AsyncCompletionAdapter` 真实回调实现（独立 Phase 9+ change）
- WarpScheduler Adapter（已 SKIP）

## Acceptance Gate

- PipelineTLM 与 TensorCoreTLM 的 `is_placeholder() == false`
- gpgpu-sim 对照值误差 ≤ ±15%
- `test_kernel_launch_ptx_integration.cc` 双端 PASS
- G-D2、G-D3、G-D5、G-D8 全部 PASS
- `openspec validate 2026-09-03-cpptlm-dgpu-gpgpu-precision-wave2 --strict` PASS

## Risks

| # | 风险 | 缓解 |
|---|------|------|
| R1 | gpgpu-sim 参考数据不可访问 | 优先使用公开 benchmark 数据；没有数据时保持 placeholder 并明确标注 |
| R2 | PTX-EMU 真实链路需跨仓同步 | 通过 cross-project protocol 协调测试接口和版本 |
| R3 | 统一 cycle 契约引入性能开销 | 增加性能测试，必要时支持批量 advance |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Proposed — 等待 gpgpu-sim 参考数据 + PTX-EMU 真实链路就绪
