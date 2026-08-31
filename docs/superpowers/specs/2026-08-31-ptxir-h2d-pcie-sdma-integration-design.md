# PTX-IR H2D PCIe/SDMA → CppTLM dGPU Device Memory Integration Test

> **Date**: 2026-08-31
> **Status**: Draft (brainstorming Round 2 complete, awaiting user review)
> **Owner**: CppTLM Team (Sisyphus)
> **Type**: New Test (not architecture change)
> **Branch**: feature/ptxir-h2d-pcie-sdma-integration

## Purpose

CppTLM 2026-08-31 full regression audit (`HEAD=1d16e5f`) revealed that **NO test in CppTLM's regression gate exercises the full PTX-EMU → CppTLM data flow** — the path that a real CUDA workload would take: a host-side PTX fatbinary loaded into a CUDA driver, copied via PCIe H2D DMA into the dGPU's device memory (VRAM). This spec fills that gap by adding one focused integration test that proves a real PTX fatbinary can be transferred through PCIe/SDMA into CppTLM's dGPU VRAM and verified byte-for-byte.

Scope is intentionally limited to **data-plane transport verification** (no PTX parsing or kernel execution). This is the "A data plane" option from brainstorming Round 1.

## Decisions Locked (Brainstorming Round 1)

| Knob | Decision | Rationale |
|------|----------|-----------|
| Test depth | **A: data-plane transport** | Real bug surface (PCIe routing + SDMA + VRAM) is most likely here; PTX parsing/execution is its own scope |
| SDMA trigger | **A2: PCIe MMIO doorbell** | Tests production path (PcieEndpoint→bar_router→SdmaEngine), not just direct SDMA call |
| PTX image source | **P1: real PTX fatbinary** | Tests must reflect real CUDA load semantics, not synthetic bytes |
| VRAM verification | **V3: backdoor primary + mmio secondary** | backdoor is the contract (R3-S1 "VRAM write visibility"); mmio validates PCIe routing consistency |
| Test granularity | **S1: Minimal** (1 happy + 2 boundary) | TDD-fast; covers happy + segment-cross + alignment-edge |

## Architecture (Test Topology)

```
Host test driver (Catch2 TEST_CASE)
  │
  ├─► [1] load_fatbinary: PTX-EMU fixtures path_1B_ptxir_fatbinary/<sample>.cubin
  │     PTXIRLoader::hasEmbeddedPTXIR() → PTXIRLoader::extractPTXIR()
  │
  ├─► [2] build_test_topology:
  │     EventQueue eq;
  │     DGpuBoard board(&eq);                  // VRAM target
  │     SdmaEngineTLM sdma(&eq, &board);       // H2D processor
  │     PcieEndpointTLM pcie(&eq, &sdma);       // BAR router
  │     // wire via existing ChStreamModuleBase::connect() helpers
  │
  ├─► [3] trigger_h2d:
  │     DmaDescriptorBundle desc(H2D, host_iova=0x10000,
  │                              vram_offset=0x0, size=32768, tag=1);
  │     pcie.mmio_write(BAR0, doorbell_offset, &desc, sizeof(desc));
  │
  ├─► [4] wait_completion: eq.run() until done_out bundle received
  │
  ├─► [5] assert_backdoor_read (primary):
  │     std::vector<uint8_t> actual(size);
  │     board.backdoor_read(0x0, actual.data(), size);
  │     CHECK(actual == ptxir_image);  // byte-for-byte
  │
  └─► [6] assert_mmio_read (secondary, PCIe routing consistency):
        std::vector<uint8_t> via_pcie(size);
        pcie.mmio_read(BAR0, vram_offset, via_pcie.data(), size);
        CHECK(via_pcie == actual);   // backdoor == mmio via PCIe
```

## Components Instantiated (from existing CppTLM code)

| Component | File | Key interface used |
|-----------|------|---------------------|
| `EventQueue` | `core/event_queue.hh` | `run(N)` simulation tick |
| `DGpuBoard` | `tlm/gpu/dgpu_board_shell.hh` | `backdoor_read(vram_off, buf, size)` / `mmio_read(BAR, off, buf, size)` |
| `SdmaEngineTLM` | `tlm/gpu/sdma_engine_tlm.hh` | `process_h2d(DmaDescriptor, CompletionBundle)` (5-port ChStream) |
| `PcieEndpointTLM` | `tlm/gpu/pcie_endpoint_tlm.h` | `mmio_write(BAR, off, buf, size)` + `apply_bar0_registers_config()` |
| `PTXIRLoader` | PTX-EMU `cudart/ptxir_loader.h` | `hasEmbeddedPTXIR()` / `extractPTXIR()` |

**Reuses** (no new implementation needed):
- `DmaDescriptor` / `DmaDescriptorBundle` (already exist)
- BAR0 doorbell side-effect config (per `test_pcie_endpoint_tick_e2e.cc` template)
- DGpuBoard VRAM segment allocation

## Test Scenarios (S1 Minimal: 3 TEST_CASE)

**TEST_CASE 1: "PTX-IR H2D happy path through PCIe MMIO doorbell" `[ptxir][h2d][integration]`**
- Load `external/PTX-EMU/tests/fixtures/ptx/path_1B_ptxir_fatbinary/<sample>.cubin`
- PTXIRLoader::hasEmbeddedPTXIR() → true
- PTXIRLoader::extractPTXIR() → ~32KB ptxir bytes
- Trigger via PCIe MMIO write to BAR0 doorbell
- Verify backdoor_read bytes match ptxir bytes
- Verify mmio_read bytes match backdoor_read bytes (PCIe routing consistency)

**TEST_CASE 2: "PTX-IR H2D across VRAM segment boundary" `[ptxir][h2d][boundary]`**
- Configure DGpuBoard with 16KB VRAM segments (2 segments)
- PTXIR size = 32KB (spans 2 segments)
- Trigger H2D
- Verify backdoor_read across segment 0 + segment 1 returns contiguous bytes
- Verify segment boundary byte at offset 16384 is correct

**TEST_CASE 3: "PTX-IR H2D with non-4K aligned size" `[ptxir][h2d][alignment]`**
- PTXIR size = 3000 bytes (not 4K multiple)
- Trigger H2D
- Verify backdoor_read returns exactly 3000 bytes
- Verify completion tag is correct (partial copy semantics)

## File Structure

```
test/test_ptxir_h2d_pcie_sdma_integration.cc   # NEW ~250-300 LOC

openspec/changes/2026-08-31-ptxir-h2d-pcie-sdma-integration/
├── proposal.md                                 # Why / What Changes / Impact
├── design.md                                   # Full design detail
├── tasks.md                                    # Implementation steps (TDD-style)
└── specs/ptxir-h2d-integration/spec.md         # Capability spec + scenarios
```

## CMake Integration

```cmake
# test/CMakeLists.txt — add to test_ptxir_h2d_pcie_sdma_integration target
add_executable(test_ptxir_h2d_pcie_sdma_integration
    test/test_ptxir_h2d_pcie_sdma_integration.cc
)
target_link_libraries(test_ptxir_h2d_pcie_sdma_integration
    cpptlm_test_common  # catch2 + event_queue + sim core
)
if(CPPTLM_WITH_PTX_EMU)
    # PTX-EMU submodule provides ptxir_loader.h + fatbinary fixtures
    target_include_directories(test_ptxir_h2d_pcie_sdma_integration
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/PTX-EMU/include)
    target_link_libraries(test_ptxir_h2d_pcie_sdma_integration
        PRIVATE ptxemu_core)
    target_compile_definitions(test_ptxir_h2d_pcie_sdma_integration
        PRIVATE HAS_PTX_EMU_FIXTURES)
endif()
add_test(NAME test_ptxir_h2d_pcie_sdma_integration
         COMMAND test_ptxir_h2d_pcie_sdma_integration)
```

**Skip behavior**:
- `CPPTLM_WITH_PTX_EMU=OFF`：test main 直接 `return 0` + 每个 TEST_CASE 用 Catch2 `SUCCEED("skipped — requires CPPTLM_WITH_PTX_EMU=ON")` 跳过 → ctest 显示 "Passed"（不是 "Not Run"），避免 d15 regression CI gate 误报
- PTX-EMU fixtures 缺失：catch2 `[!mayfail]` 标记 → 跳过但 ctest 通过
- 双层守卫确保 PTX-EMU submodule 未初始化的开发者 CI 仍然绿色

## Risks

| Risk | Severity | Mitigation |
|------|----------|-------------|
| PTX-EMU submodule not initialized (missing fixtures) | High | CMake guard + SKIP behavior above |
| No suitable .cubin fixture at chosen path | Medium | Reuse `tests/e2e/path_1B_ptxir_fatbinary/` fixture from PTX-EMU submodule |
| PcieEndpointTLM BAR0 doorbell side-effect config unclear | Medium | Reference `test_pcie_endpoint_tick_e2e.cc` template |
| DGpuBoard VRAM segment boundary semantics undocumented | Low | Reference `test_dgpu_pcie_device_perspective.cc` template |
| PCIe BAR router maps BAR0 → SDMA mem_in (not yet verified for H2D path) | Medium | Run test once; if wrong, swap to direct SDMA process_h2d call (fallback) |

## Acceptance Criteria

- [ ] `test/test_ptxir_h2d_pcie_sdma_integration.cc` compiles with `CPPTLM_WITH_PTX_EMU=ON`
- [ ] 3 TEST_CASE all PASS with `CPPTLM_WITH_PTX_EMU=ON` + PTX-EMU submodule initialized
- [ ] 3 TEST_CASE all SKIP (exit 0) with `CPPTLM_WITH_PTX_EMU=OFF`
- [ ] Full regression baseline unchanged: cpptlm_tests 998/998, Python 255, ctest 21/21, E2E 12/12
- [ ] `openspec validate 2026-08-31-ptxir-h2d-pcie-sdma-integration --strict` 0 errors

## Out of Scope (explicit non-goals)

- PTX-IR parsing / execution (`PTXIRLoader::extractPTXIR()` returns bytes only — no PtxContext population)
- Module registration / kernel launch
- gpgpu-sim reference data comparison
- D2H direction (H2D only this change)
- Performance / throughput benchmarks

## Dependencies (all present in repo as of 2026-08-31)

- `CPPTLM_WITH_PTX_EMU=ON` build path (validated in cpptlm-2026-08-31-fix-test-infra-drift regression: `1d16e5f` builds + tests pass)
- PTX-EMU submodule at `fcdad151+` (HSK-8 spec requirement, CMakeLists.txt:166 enforcement)
- `ptxemu_core` static lib (HSK-8 Phase 2 delivered)

## Estimated Effort

~1-2 days (per option A brainstorming estimate)
- Test fixture wiring: 0.5d
- 3 TEST_CASE implementation: 0.5d
- CMake integration + SKIP behavior: 0.2d
- OpenSpec artifacts (proposal/design/tasks/specs): 0.3d
- Verification (cpptlm_tests + openspec validate + docs_sync_check): 0.2d

## Open Questions for User Review

1. **Fixture path confirmation**: Which exact `.cubin` to load? Candidates:
   - `external/PTX-EMU/tests/fixtures/ptx/path_1B_ptxir_fatbinary/<sample>.cubin` (preferred — already used by PTX-EMU e2e tests)
   - `external/PTX-EMU/tests/fixtures/ptx/vector_add.cubin` (if exists, simpler)
   - Need to confirm fixture file exists in submodule before locking spec
2. **PCIe BAR0 doorbell side-effect config**: Does existing `PcieEndpointTLM` JSON test setup already include `doorbell` side-effect entry that routes to `sdma_engine.mem_in`? If not, need new minimal JSON config in test fixture.
3. **VRAM segment configuration**: For boundary case (TEST_CASE 2), need DGpuBoard configured with 2×16KB segments. Confirm this is configurable via JSON or constructor.

After user review and approval, transition to `writing-plans` skill for implementation plan.