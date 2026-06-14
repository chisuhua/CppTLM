# Changelog

## [Unreleased]

### Added
- Phase 7.A GPU 基础设施落地（2026-06-11）
  - `include/bundles/compute_bundles_tlm.hh` — ComputeReqBundle / ComputeRespBundle 类型
  - `include/tlm/gpu/gpu_tlm.hh` — GPUTLM v0 黑盒发起器
  - `REGISTER_CHSTREAM` 宏注册 GPUTLM（同时绑定 StreamAdapter 与多端口适配器）
  - `configs/gpu_standalone.json` — 验证用最小拓扑配置
  - `test/test_gpu_standalone.cc` — 5 个 `[gpu]` 标签单元测试
  - AGENTS.md STRUCTURE 节新增 `include/tlm/gpu/` 子目录条目（Phase7.A+ GPU 模块）

### Notes
- Phase 7.A 状态: 🟡 Pending → ✅ Done
- 后续: Phase 7.B–7.F（ComputeUnitTLM / Coherence / TCC / Multi-CU / Full APU Demo）保持 🟡 Pending

## [v2.1.0] - 2026-06-08

### Added
- tlm_stub multi-extension support (SystemC TLM 2.0 API compatible)
  - tlm_extension_registry singleton with type_index → ID mapping
  - tlm_array<T> for O(1) indexed extension storage
  - new release_extension<T>() API (delete + nullify)
  - new resize_extensions() / free_all_extensions() / deep_copy_from() APIs
- test_tlm_multi_extension.cc with 12 test cases (auto-globbed by test/CMakeLists.txt)
- docs/adr/ADR-X.13-stub-multi-extension.md
- .github/workflows/ci.yml code-format job (aligns with AGENTS.md)
- BUILD_LEGACY_MODULES CMake option (default OFF, #ifdef guard for legacy CPUSim/CpuCluster)
- include/AGENTS.md: BUILD_LEGACY_MODULES documentation in registration macro table

### Changed
- **BREAKING**: tlm_extension<T>::ID migrated from per-TU function-local static
  to class static const (compile-time registration, TU-safe)
- TransactionContextExt explicitly released on MemoryV2 error path
  (modules_v2.hh:79 — semantic clarity, no longer relies on stream_id fallback)
- ~tlm_generic_payload() destructor and reset() now loop-delete all extensions
  (was: single delete of a single extension pointer)
- include/core/ext/cmd_exts.hh reduced to macros-only library
  (deleted duplicate ReadCmdExt/WriteCmdExt/etc. class definitions; only
  ReqIDExt is unique to cmd_exts.hh)
- CMakeLists.txt version synced from 2.0.3 to 2.1.0
- docs/architecture/README.md index updated to v2.1
- docs/architecture/01-hybrid-architecture-v2.1.md: §4.4 pseudocode corrected
- docs/architecture/01-hybrid-architecture-v2.1.md: Bundle description updated to lightweight
- docs/architecture/02-complex-topology-architecture.md: class names updated
  (MeshRouter→RouterTLM, Processor→CPUTLM, Bus→BusSim, Crossbar→CrossbarTLM)
- docs/architecture/02-transaction-architecture.md: version label v1.0→v2.1
- include/AGENTS.md: dual registry design intent documentation
- include/core/AGENTS.md: dual registry motivation paragraph
- include/modules.hh: design intent header comment added

### Removed
- **BREAKING**: USE_SYSTEMC build option (always use TLM stub)
- external/systemc/ directory (was placeholder README only, not a submodule)
- src/sc_main.cpp (10-line empty stub; only existed behind USE_SYSTEMC=ON guard)
- extern "C" int sc_main placeholder at src/main.cpp:21
- src/cpu_main.cpp and src/traffic_main.cpp (15 + 22 lines of placeholder
  main() with 2 TODOs in v2.1 upgrade backlog). Use cpptlm_sim with configs/ instead.
- include/core/ext/packet_to_payload.hh and payload_to_packet.hh
  (zero .cc users; archived to docs-archived/dead-code-headers-2026-q2/)
- mock_modules.hh duplicate #ifdef USE_SYSTEMC_STUB nesting
- test/*.cc, include/ext/*.hh, include/core/ext/*.hh:16 files'
  #ifdef USE_SYSTEMC_STUB blocks (USE_SYSTEMC option is gone; only stub path)
- include/modules/legacy/modules_v2.hh (208-line v2 module definitions, archived)
- 5 test files migrated from v2 to TLM modules
  (test_phase5_modules.cc, test_phase6_regression.cc, test_phase7_transaction_lifecycle.cc,
   test_phase8_performance_stress.cc, test_p3_2_tlm_integration.cc)

### Fixed
- MemoryV2 error path no longer silently destroys upstream
  TransactionContextExt (modules_v2.hh:79 + Phase 1c multi-extension
  array implementation co-fix)
- Type ID cross-TU consistency (no more per-TU local static counter)
