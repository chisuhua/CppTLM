# JSON Config E2E Test Coverage Plan

**Created:** 2026-04-14  
**Status:** Analysis Complete

---

## Module Coverage Analysis

### Legacy vs TLM Module Mapping

| Legacy Module | TLM Equivalent | TLM Status | Config Usage |
|---------------|----------------|------------|-------------|
| CPUSim | **None** | ❌ Missing | cpu_simple, cpu_group, noc_mesh, ring_bus |
| CacheSim | CacheTLM | ✅ Exists | cpu_simple, cpu_group, ring_bus |
| MemorySim | MemoryTLM | ✅ Exists | All configs |
| Crossbar | CrossbarTLM | ✅ Exists | ring_bus |
| Router | **None** | ❌ Missing | noc_mesh |
| Arbiter | **None** | ❌ Missing | None (internal use) |
| TrafficGenerator | **None** | ❌ Missing | cpu_group |

### Loadable Configs with Current TLM Modules

| Config | Modules | TLM Coverage | Testable? |
|--------|---------|--------------|-----------|
| `crossbar_test.json` | CacheTLM, CrossbarTLM, MemoryTLM | 100% | ✅ YES |
| `cache_chstream_test.json` | CacheTLM, MemoryTLM | 100% | ✅ YES |

### Configs Requiring Legacy Modules (NOT Testable with Pure TLM)

| Config | Missing TLM Modules | Reason |
|--------|-------------------|--------|
| `noc_mesh.json` | Router | No RouterTLM |
| `cpu_group.json` | CPUTLM, TrafficGeneratorTLM | No CPUTLM, No TrafficGenTLM |
| `cpu_simple.json` | CPUTLM | No CPUTLM |
| `ring_bus.json` | Crossbar (Legacy) | Uses Legacy Crossbar |
| `base.json` | CPUSim | Uses include, complex setup |

---

## Missing TLM Module Implementations

### Priority 1: TrafficGeneratorTLM
**Why:** `cpu_group.json` uses TrafficGenerator for memory traffic generation
**Purpose:** Generates read/write requests to simulate CPU traffic
**Required Interface:**
- Downstream port (sends requests)
- tick() generates requests based on mode (sequential/random/trace)
- handleDownstreamResponse() processes responses

### Priority 2: CPUTLM
**Why:** CPU is initiator in all benchmarks
**Purpose:** Issues memory requests and receives responses
**Required Interface:**
- Downstream port (sends requests)
- Upstream port (receives responses)
- tick() issues requests based on program counter

### Priority 3: RouterTLM
**Why:** Mesh NoC requires routing
**Purpose:** Routes packets by address to different ports
**Required Interface:**
- N input ports, N output ports
- tick() routes incoming packets by address

---

## Recommended Test Plan

### Phase A: Test Current TLM Configs (Immediate)

```
configs/crossbar_test.json     → test_json_config_e2e.cc ✅ Covered
configs/cache_chstream_test.json → test_json_config_e2e.cc ✅ Covered
```

### Phase B: Create TrafficGeneratorTLM (High Priority)

**File:** `include/tlm/traffic_gen_tlm.hh`

```cpp
class TrafficGeneratorTLM : public ChStreamModuleBase {
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> req_out_;
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle> resp_in_;
    
    void tick() override {
        // Issue requests based on mode (sequential/random/trace)
        // Handle responses
    }
};
```

**Test File:** `test/test_traffic_gen_tlm.cc`

### Phase C: Create CPUTLM (Medium Priority)

**File:** `include/tlm/cpu_tlm.hh`

**Test File:** `test/test_cpu_tlm.cc`

### Phase D: Create RouterTLM (Medium Priority)

**File:** `include/tlm/router_tlm.hh`

**Test File:** `test/test_router_tlm.cc`

---

## Config Test Matrix

| Config | Modules | Can Load? | Module Types Valid? | Can Run? | Notes |
|--------|---------|-----------|-------------------|----------|-------|
| `crossbar_test.json` | CacheTLM, CrossbarTLM, MemoryTLM | ✅ | ✅ | ✅ | Currently tested |
| `cache_chstream_test.json` | CacheTLM, MemoryTLM | ✅ | ✅ | ✅ | Currently tested |
| `noc_mesh.json` | Router, CPUSim | ⚠️ | ❌ | ❌ | Missing RouterTLM/CPUTLM |
| `cpu_group.json` | CPUSim, TrafficGenerator | ⚠️ | ❌ | ❌ | Missing CPUTLM/TrafficGenTLM |
| `cpu_simple.json` | CPUSim, CacheSim, MemorySim | ⚠️ | ❌ | ❌ | Missing CPUTLM |
| `ring_bus.json` | Crossbar (Legacy) | ⚠️ | ❌ | ❌ | Uses Legacy Crossbar |
| `base.json` | External include | ❓ | ❓ | ❓ | Needs investigation |

---

## Action Items

### Immediate (No New Modules Needed)
- [x] `crossbar_test.json` - Already tested in `test_json_config_e2e.cc`
- [x] `cache_chstream_test.json` - Already tested in `test_json_config_e2e.cc`

### Short Term (1-2 New Modules)
- [ ] Create `TrafficGeneratorTLM` - High value, used in `cpu_group.json`
- [ ] Create `CPUTLM` - High value, used in `cpu_simple.json`
- [ ] Add `noc_mesh.json` test after RouterTLM
- [ ] Add `cpu_simple.json` test after CPUTLM

### Medium Term (1 New Module)
- [ ] Create `RouterTLM` for NoC topologies

### Long Term (Architecture Decision)
- [ ] Decide: Keep Legacy modules or fully migrate to TLM?
- [ ] `ring_bus.json` uses Legacy Crossbar - need to decide if CrossbarTLM replaces it

---

## E2E Test Coverage Target

```
Total Configs: 7
Currently Testable: 2 (crossbar_test, cache_chstream_test)
After TrafficGeneratorTLM: 3 (add cpu_group)
After CPUTLM: 4 (add cpu_simple)
After RouterTLM: 5 (add noc_mesh)

Coverage: 5/7 (71%)
```

---

## Validation Commands

```bash
# Run TLM config tests
./build/bin/cpptlm_tests "[e2e][config][chstream]"

# Run all phase6 tests
./build/bin/cpptlm_tests "[phase6]"

# Run simulation communication tests
./build/bin/cpptlm_tests "[e2e][simulation]"

# Full test suite
./build/bin/cpptlm_tests
```
