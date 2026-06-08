# Changelog

## [Unreleased]

### Added
- tlm_stub multi-extension support (SystemC TLM 2.0 API compatible)
  - tlm_extension_registry singleton with type_index → ID mapping
  - tlm_array<T> for O(1) indexed extension storage
  - new release_extension<T>() API (delete + nullify)
  - new resize_extensions() / free_all_extensions() / deep_copy_from() APIs
- test_tlm_multi_extension.cc with 12 test cases (auto-globbed by test/CMakeLists.txt)
- docs/adr/ADR-X.13-stub-multi-extension.md
- .github/workflows/ci.yml code-format job (aligns with AGENTS.md)

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

### Fixed
- MemoryV2 error path no longer silently destroys upstream
  TransactionContextExt (modules_v2.hh:79 + Phase 1c multi-extension
  array implementation co-fix)
- Type ID cross-TU consistency (no more per-TU local static counter)

## [v2.1.0] - 2026-06-05

Initial tagged snapshot (pre-refactor baseline).
