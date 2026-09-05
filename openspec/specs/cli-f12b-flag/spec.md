# cli-f12b-flag Specification

## Purpose
Contract for the `cpptlm_sim` CLI `--f12b-ld` flag following HSK-8 Phase 2 Step 4 (commit `738b412c`). The flag is permanently disabled because the underlying `MemoryBridge` + `g_ptx_emu_driver` components have been physically removed (HSK-6 ack commit `369cf71`). The flag is retained for parser compatibility but emits an observability log on default invocation and errors when explicitly passed.

## Requirements
### Requirement: f12b-flag-hsk8-permanent-disable

The `cpptlm_sim` CLI binary's `--f12b-ld` command-line flag SHALL follow the HSK-8 Phase 2 Step 4 contract (commit `738b412c`): the flag is **permanently disabled** because the underlying `MemoryBridge` + `g_ptx_emu_driver` components have been physically removed (HSK-6 ack commit `369cf71`). The flag SHALL be retained for parser compatibility but MUST NOT activate any wiring.

The CLI MUST emit distinct, predictable output for both code paths so the `python-f12b-smoke` test suite (see `python-f12b-smoke/spec.md`) can assert observability invariants.

#### Scenario: Default (flag absent) — zero regression observability

- **WHEN** the user invokes `cpptlm_sim <config.json> --cycles <N>` without the `--f12b-ld` flag
- **THEN** the simulation SHALL run normally with returncode = 0
- **AND** stdout SHALL contain the literal substring `[INFO] --f12b-ld: MemoryBridge disabled (zero regression)` (observability log emitted by `src/main.cpp` else-branch)
- **AND** stderr SHALL NOT contain `[ERROR] --f12b-ld disabled`

#### Scenario: Flag present — HSK-8 permanent disable

- **WHEN** the user invokes `cpptlm_sim <config.json> --cycles <N> --f12b-ld`
- **THEN** the binary SHALL return immediately with returncode = 1 (no simulation loop entered)
- **AND** stderr SHALL contain the literal substring `[ERROR] --f12b-ld disabled (HSK-8 Phase 2 Step 4 removed MemoryBridge + g_ptx_emu_driver; use PtxEmuSubmoduleMVP facade->attach_timing instead)`
- **AND** stdout SHALL NOT contain `[INFO] --f12b-ld: MemoryBridge enabled` (HSK-8 permanently disabled — no legal "enabled" state exists)
- **AND** the simulation MUST NOT execute any cycles (returncode=1 emitted before `eq.run(sim_cycles)`)

#### Scenario: Flag present with minimal config — single returncode=1 path

- **WHEN** the user invokes `cpptlm_sim <config_without_kernel_launch.json> --cycles <N> --f12b-ld`
- **THEN** the behavior SHALL be identical to "Flag present — HSK-8 permanent disable" (returncode=1, stderr disabled-error)
- **AND** the binary SHALL NOT emit the legacy "requires JSON config with 'kernel_launch'" diagnostic (the entire wiring block is unreachable post-HSK-8)
## Status Update
- **2027-02-09**: **Superseded** by cpptlm-dgpu-d1-cdna-isa-sm-rewrite. 旧 PipelineTLM/ScoreboardTLM/TensorCoreTLM 路径已物理删除 (Task 13); gpgpu-sim comparison 路径由 SM 重构直接承接.
