# Phase 1.5 — MemoryV2 Error Path: Explicit TransactionContextExt Release

**Date**: 2026-06-06
**Commit**: (pending)
**Blocked By**: Phase 1d (✅ done)
**Blocks**: Phase 1.9

## Summary

Make MemoryV2 error path semantics **explicit** by releasing
`TransactionContextExt` before calling `set_error_code()`. With the
multi-extension array (Phase 1c/1d), `set_error_code` no longer
silently destroys `TransactionContextExt` — but leaving it in its own
slot creates an implicit slot-retention situation that the plan 1.5
audit wants surfaced and explicitly handled at the call site.

## Code Changes

### 1. `include/modules/legacy/modules_v2.hh:79`

Added explicit `release_extension<TransactionContextExt>()` before
`set_error_code()` in the `addr >= memory_size` branch, with a
block-comment explaining the rationale:

```cpp
if (req->payload) {
    req->payload->release_extension<TransactionContextExt>();
}
req->set_error_code(ErrorCode::TRANSPORT_INVALID_ADDRESS);
```

The transaction_id remains accessible because `set_transaction_id()`
already mirrored it to `Packet::stream_id` (see
`include/core/packet.hh:96`), and `get_transaction_id()` falls back
to `stream_id` when no extension is present (see
`include/core/packet.hh:88`).

### 2. `test/test_phase3_critical_fixes.cc:184-193`

Added a `Phase 1.5` annotation comment in the
`"Integration: Full packet lifecycle with errors"` test explaining
the before/after semantics, **without** changing the existing
`REQUIRE(pkt->get_transaction_id() == 5000)` (which was already
present and validates the stream_id fallback path).

## Caller Audit (`set_error_code` / `create_error_context`)

Grep across `include/`, `src/`, `test/`, `examples/`:

| Location | Status | Reason |
|----------|--------|--------|
| `include/core/packet.hh:198` | KEEP | `Packet::set_error_code` definition itself |
| `include/core/packet.hh:206` | KEEP | `Packet::set_error_code` auto-creates `ErrorContextExt` only — does NOT touch `TransactionContextExt` slot (Phase 1c/1d) |
| `include/framework/debug_tracker.hh:157` | KEEP | `DebugTracker::record_error` uses `create_error_context` directly for instrumentation, no `TransactionContextExt` to release |
| `include/modules/legacy/modules_v2.hh:79` | **FIXED** | MemoryV2 error path — added explicit release per Phase 1.5 spec |
| `include/ext/error_context_ext.hh:190` | KEEP | `create_error_context` definition |
| `examples/example_error_handling.cc:35,40` | KEEP | Demo code, uses `Packet::set_error_code` + direct `create_error_context` |
| `test/test_phase3_critical_fixes.cc:64,79,182` | KEEP | Tests — do not need release in test fixtures (no transaction_id needed post-error) |
| `test/test_tlm_multi_extension.cc:186` | KEEP | Test name only (the actual case is the multi-ext regression test) |
| `test/test_phase6_regression.cc:132,141,243` | KEEP | Comments only |
| `test/test_phase7_transaction_lifecycle.cc:207-465` | KEEP | Tests use `create_error_context` directly to seed error extensions |
| `test/test_phase8_performance_stress.cc:80` | KEEP | Commented out |

**Conclusion**: Only `MemoryV2::tick()` (modules_v2.hh:79) is in the
production error path that retains a `TransactionContextExt` after
error. The other call sites either (a) only create the error
extension without a prior transaction context, or (b) are tests
that intentionally test the `Packet::set_error_code` API in
isolation.

## Verification

```
[phase3]    -> 16/16 passed (24 assertions)
[phase4]    ->  4/4  passed (10 assertions)
[multi_ext] -> 12/12 passed (61 assertions)
ALL         -> 579/581 passed (2 pre-existing NIC failures in test_phase0_stats_registration.cc:95,133)
```

Build: clean, no new warnings. Only pre-existing warnings about
`unused variable 'child_tid'` in modules_v2.hh:165 and
`unused structured binding` in tlm_module.hh:106 remain — both
unrelated to this fix.

## Behavioral Notes

1. **Pre-Phase 1c behavior**: `set_error_code()` would internally
   destroy `TransactionContextExt` because there was a single
   extension slot (or destructive replace).
2. **Post-Phase 1c/1d behavior**: `set_error_code()` writes
   `ErrorContextExt` to its own `T::ID` slot, leaving
   `TransactionContextExt` in its own `T::ID` slot. Both coexist.
3. **Phase 1.5 fix**: MemoryV2 explicitly releases the
   `TransactionContextExt` slot on the error path for semantic
   clarity. No code change is needed in other call sites because
   none of them pre-create a `TransactionContextExt` that needs
   cleanup.
4. **Downstream correctness**: `Packet::get_transaction_id()` still
   returns the correct value via the `stream_id` fallback path
   (set by `set_transaction_id()` at packet.hh:96, read at
   packet.hh:88).

## Not Modified

- `include/core/packet.hh` — `set_error_code` semantics unchanged
  (intentionally not adding `release_extension<TransactionContextExt>`
  inside; the explicit release belongs at the call site, not the API)
- `include/ext/*` — extension API unchanged
- `examples/*` — examples unchanged
- `test/test_tlm_multi_extension.cc` — already has the multi-ext
  coexistence test from Phase 1c, no additional case needed
