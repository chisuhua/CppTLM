# Spec Delta: cpptlm-d1-p1-pipeline-scoreboard

> **Capability**: cpptlm-d1-p1-pipeline-scoreboard
> **Source change**: [`cpptlm-dgpu-d1-cdna-isa-phase-a`](../../)
> **Parent main spec**: [`openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md`](../../../../specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md)
> **Created**: 2027-02-09 (post-Oracle-revision-2, per N-P1-1 capability 错位修复)
> **Status**: Proposed（阶段 A 启动）

> **修订理由**：本 spec delta 与新增 capability `cdna-isa-abstraction` delta 必须分文件归档。MODIFIED `cpptlm-pipeline` / `cpptlm-scoreboard` 是**主 spec**（`cpptlm-d1-p1-pipeline-scoreboard`）已有 Requirement 的修订，归属本文件；ADDED `cpptlm-instruction-descriptor` / `cpptlm-cdna-pipeline` / `cpptlm-hazard-tracker-v2` / `cpptlm-cuda-core-adapter-v2-injection` 归属 `cdna-isa-abstraction/spec.md`。`openspec archive` 按文件夹归并 capability，避免主 spec 污染。

---

## MODIFIED Requirements

### Requirement: cpptlm-pipeline

The existing `PipelineTLM` class in `include/tlm/gpu/pipeline_tlm.hh` + `src/tlm/gpu/pipeline_tlm.cc` MUST remain **unchanged at the source level** during 阶段 A (no source modifications; 验证: `git diff main..cpptlm-dgpu-d1-cdna-isa-phase-a -- src/tlm/gpu/pipeline_tlm.cc` 必须为空). It serves as the PTX 阶段 A backward compatibility shim. New code MUST use `CdnaPipelineTLM` instead.

**MODIFIED 范围背景**：在阶段 A 期间**显式禁止**修改 `PipelineTLM` 源文件（`src/tlm/gpu/pipeline_tlm.cc` / `include/tlm/gpu/pipeline_tlm.hh`），新代码 MUST 使用 `CdnaPipelineTLM`（见 `cdna-isa-abstraction/spec.md` cpptlm-cdna-pipeline）。

> **Stage A 阶段异常声明**：ADR-SOC-15 §4.1 A.3 描述"PipelineTLM::get_fractional_cycles 重写为 get_latency(LatencyClass) 枚举查表"在阶段 A **不实施**——本 spec 显式偏离该字面计划。偏离理由（per Oracle P0-A1）：PipelineTLM 实际连续值（4.22/2.0/20.0/200.0）无法用 6 入口枚举表 {1,4,8,16,32} 表达，按 ADR 字面计划会破坏 PTX 模式 byte-equal 契约。实际方案是**新增 CdnaPipelineTLM + PipelineTLM 双轨**（PipelineTLM 100% 源文件不变）。**阶段 C 启动时需同步修订 ADR-SOC-15 §4.1 表 A.3 描述**（见 `cdna-isa-abstraction/tasks.md` group 7.5 显式追踪）。

#### Scenario: PipelineTLM legacy path unchanged
- **WHEN** existing tests use `PipelineTLM::get_fractional_cycles(instr, pipe)` directly
- **THEN** the function returns the same value as before 阶段 A (no behavior change)
- **AND** the function signature remains `double get_fractional_cycles(const std::string& instr, PipelineId pipe) const`
- **AND** `get_fractional_cycles_by_type(int, PipelineId)` 仍为 6 管线 switch 真值表 (per `pipeline_tlm.cc:120-137`)
- **AND** all 6 PTX 字符串模式 + 退化路径（空串/`bar.sync`）返回值与阶段 A 改造前 byte-equal

### Requirement: cpptlm-scoreboard

The existing `ScoreboardTLM` class in `include/tlm/gpu/scoreboard_tlm.hh` MUST remain **unchanged at the source level** during 阶段 A. `ScoreboardTLMv2::kVirtualReg` mode delegates to it via composition + 影子 per-warp outstanding 集合复用其语义.

**MODIFIED 范围背景**：在阶段 A 期间**显式禁止**修改 `ScoreboardTLM` 源文件（`src/tlm/gpu/scoreboard_tlm.cc` / `include/tlm/gpu/scoreboard_tlm.hh`），`ScoreboardTLMv2::kVirtualReg` 模式通过组合复用（见 `cdna-isa-abstraction/spec.md` cpptlm-hazard-tracker-v2）。

#### Scenario: ScoreboardTLM legacy path unchanged
- **WHEN** existing tests use `ScoreboardTLM::allocate(reg_id, warp_id)` directly
- **THEN** the function returns the same value as before 阶段 A
- **AND** the function signature remains `bool allocate(uint32_t reg_id, uint32_t warp_id)`
- **AND** duplicate-allocate returns false (rejects; per `ScoreboardTLM` line 35, PTX-EMU `sm_context.cpp:37-43` rollback 依赖)
- **AND** `release(reg_id, warp_id)` / `has_free_entry()` / `tick()` 行为不变
