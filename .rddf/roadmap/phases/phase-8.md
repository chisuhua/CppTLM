---
id: phase-8
kind: phase
status: completed
phase_refs: []
主题: timing-mode SoC v0.5 (per ADR-DGPU-11)
---

## phase-8: timing-mode SoC v0.5 (per ADR-DGPU-11)

> **真源**: [`docs/designs/dgpu-soc/timing-mode.md`](../../docs/designs/dgpu-soc/timing-mode.md) (v0.2 终稿, Oracle 八轮复评 PASS, 21/21 硬伤落盘)
> **关键 ADR**: [`docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md`](../../docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md) (T0 已签发)
> **OpenSpec 已交付**: [dgpu-soc-timing](../../openspec/specs/dgpu-soc-timing/) (18 ADDED Requirements + 52 scenarios)
> **实施笔记**: [`docs/superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md`](../../docs/superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md)

### v0.5 timing-mode SoC MVP 实施完成

- **完成日期**: 2027-02-11 (37 cases + 0 regressions, per AGENTS.md §PHASE STATE)
- **关键交付** (Phase 4 `cpptlm-dgpu-soc-timing-mvp` T1-T8):
  - MemoryTLM cycle-approximate
  - GmmuTLM TLB (per `include/tlm/gpu/gmmu_tlm.hh:147-193` `translate_timing()`)
  - SDMA cycle accounting
  - **VramControllerTLM** (per `include/tlm/vram_controller_tlm.{hh,cc}`, Phase 4 T4 timing-mvp 已实现)
  - `DGpuBoard::init_timing_mode`
  - `configs/dgpu_soc_timing_v1.json` (timing-mode 配置)
  - 37 E2E cases (`[dgpu_soc_timing]` 标签)
- **关联 commit**: `b034ad23..`
- **8 轮 Oracle 评审**: PASS

### 配置 + 测试

- `configs/dgpu_soc_timing_v1.json` — timing-mode dGPU SoC 顶层配置
- 18 ADDED Requirements + 52 scenarios spec
- `[dgpu_soc_timing]` 标签测试 37 cases / 12024 assertions

### VramControllerTLM 1 GHz 操作锚点 (per `include/tlm/vram_controller_tlm.hh`)

- `kOperatingFrequencyHz = 1'000'000'000ULL` (1 GHz)
- `kSimCycleNs = 1` 常量 (仅文档化锚点, 不参与仿真逻辑)
- `bandwidth_gbps_` 字段语义实为 **bytes/cycle** (命名沿用 design.md §5.2 历史用法避免破坏 ABI)
- 真实带宽换算: `bandwidth_GBps = bandwidth_gbps_ × 1 GHz = bandwidth_gbps_ GB/s` (例: 默认 32 → 32 GB/s 上限, 注入 128 → 128 GB/s)

### 11-12d 工程量 (T0 已签发) → T1-T8 实施完成 2027-02-11