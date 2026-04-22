# TLM Modules Implementation Plan: CPUTLM, TrafficGenTLM, ArbiterTLM

**Generated:** 2026-04-14
**Status:** Planning Complete
**Version:** 1.0.0

---

## Executive Summary

This plan details the implementation of three TLM 2.0 modules following the ChStreamModuleBase pattern:
- **CPUTLM**: Transaction-issuing CPU module (initiator)
- **TrafficGenTLM**: Configurable traffic generation module (initiator)
- **ArbiterTLM**: Multi-input request arbitration module (target + initiator)

All modules will integrate with the existing StreamAdapter framework and support JSON configuration loading from `configs/`.

---

## 1. Architecture Overview

### 1.1 Module Classification

| Module | Type | Ports | Direction | Bundle Types |
|--------|------|-------|-----------|--------------|
| **CPUTLM** | Initiator | 1 | Request Out, Response In | CacheReqBundle, CacheRespBundle |
| **TrafficGenTLM** | Initiator | 1 | Request Out, Response In | CacheReqBundle, CacheRespBundle |
| **ArbiterTLM** | Target + Initiator | N inputs + 1 output | Req In (N), Req Out (1), Resp In (1), Resp Out (N) | CacheReqBundle, CacheRespBundle |

### 1.2 Legacy vs TLM Comparison

| Legacy Module | TLM Equivalent | Key Differences |
|---------------|----------------|-----------------|
| CPUSim | CPUTLM | Uses InputStreamAdapter/OutputStreamAdapter vs MasterPort.sendReq() |
| TrafficGenerator | TrafficGenTLM | Internal state machine with ch_stream handshake |
| Arbiter | ArbiterTLM | Multi-port Input/Output adapters vs PortManager |

### 1.3 Class Hierarchy

```
SimObject
└── ChStreamModuleBase
    ├── CPUTLM              (NEW - single port initiator)
    ├── TrafficGenTLM       (NEW - single port initiator)
    └── ArbiterTLM          (NEW - multi-port target + single-port initiator)
```

---

## 2. Module Implementation Plan

### 2.1 CPUTLM

**File Location:** `include/tlm/cpu_tlm.hh`

**Purpose:** Transaction-issuing CPU module that generates read/write requests at configurable intervals.

**Key Features:**
- Configurable request interval (cycles between requests)
- Address generation modes: Sequential, Random
- In-flight transaction tracking (max 4 outstanding)
- Response handling via callback

**Interface:**

```cpp
class CPUTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>  resp_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>  req_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    
    // Configuration
    uint64_t start_addr_ = 0x1000;
    uint64_t cur_addr_ = 0x1000;
    uint64_t request_interval_ = 10;  // cycles
    uint64_t timer_ = 0;
    
    // In-flight tracking
    std::map<uint64_t, uint64_t> inflight_txns_;  // txn_id -> address
    uint64_t next_txn_id_ = 1;
    static constexpr unsigned MAX_INFLIGHT = 4;

public:
    CPUTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "CPUTLM"; }
    
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    
    // Accessors
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in();
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out();
    cpptlm::StreamAdapterBase* get_adapter() const;
};
```

**Tick Logic:**
1. Check for incoming responses → complete in-flight transactions
2. If timer >= request_interval_ and inflight < MAX_INFLIGHT → issue new request
3. Increment timer

**Configuration (JSON):**
```json
{
  "name": "cpu0",
  "type": "CPUTLM",
  "config": {
    "start_addr": "0x1000",
    "request_interval": 10,
    "address_mode": "sequential"
  }
}
```

---

### 2.2 TrafficGenTLM

**File Location:** `include/tlm/traffic_gen_tlm.hh`

**Purpose:** Configurable traffic generator supporting multiple patterns (Sequential, Random, Trace-based).

**Key Features:**
- 3 generation modes: SEQUENTIAL, RANDOM, TRACE
- Configurable address range
- Trace playback from configuration
- Completion tracking

**Interface:**

```cpp
class TrafficGenTLM : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>  resp_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>  req_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    
    // Configuration
    enum class GenMode { SEQUENTIAL, RANDOM, TRACE };
    GenMode mode_ = GenMode::SEQUENTIAL;
    uint64_t start_addr_ = 0x1000;
    uint64_t end_addr_ = 0x2000;
    uint64_t cur_addr_ = 0x1000;
    uint32_t num_requests_ = 100;
    uint32_t completed_ = 0;
    
    // Trace data
    struct TraceEntry { uint64_t addr; bool is_write; };
    std::vector<TraceEntry> trace_;
    size_t trace_pos_ = 0;
    
    // In-flight tracking
    std::map<uint64_t, uint64_t> inflight_txns_;
    uint64_t next_txn_id_ = 1;
    
    // RNG
    std::mt19937_64 rng_;
    std::uniform_int_distribution<uint64_t> addr_dist_;

public:
    TrafficGenTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "TrafficGenTLM"; }
    
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    
    // Accessors
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in();
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out();
    cpptlm::StreamAdapterBase* get_adapter() const;
};
```

**Tick Logic:**
1. Process incoming responses → increment completed_ counter
2. If completed_ < num_requests_ and random(10) == 0 → issue request
3. Address generation based on mode_

**Configuration (JSON):**
```json
{
  "name": "traffic_gen0",
  "type": "TrafficGenTLM",
  "config": {
    "mode": "random",
    "start_addr": "0x1000",
    "end_addr": "0x10000",
    "num_requests": 100,
    "trace": [
      {"addr": "0x1000", "is_write": false},
      {"addr": "0x1004", "is_write": true}
    ]
  }
}
```

---

### 2.3 ArbiterTLM

**File Location:** `include/tlm/arbiter_tlm.hh`

**Purpose:** Multi-input arbiter that merges requests from multiple initiators to a single output.

**Key Features:**
- Configurable number of input ports (template parameter N)
- Round-robin arbitration
- Per-port request queues
- Response routing back to correct input port

**Interface:**

```cpp
template<unsigned NUM_INPUTS = 4>
class ArbiterTLM : public ChStreamModuleBase {
private:
    cpptlm::StreamAdapterBase* adapter_[NUM_INPUTS] = {nullptr};
    
    // Input ports (one per initiator)
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> req_in_[NUM_INPUTS];
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_[NUM_INPUTS];
    
    // Output port (to downstream)
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle> resp_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> req_out_;
    
    // Request queues per input port
    struct QueuedRequest {
        bundles::CacheReqBundle req;
        unsigned src_port;
    };
    std::queue<QueuedRequest> req_queue_;
    
    // Response tracking
    std::queue<unsigned> resp_port_queue_;  // Track which port each response belongs to
    uint64_t next_txn_id_ = 1;

public:
    ArbiterTLM(const std::string& name, EventQueue* eq);
    std::string get_module_type() const override { return "ArbiterTLM"; }
    unsigned num_ports() const override { return NUM_INPUTS; }
    
    void set_stream_adapter(cpptlm::StreamAdapterBase*) override {}
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    
    // Accessors
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in(unsigned idx);
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out(unsigned idx);
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& resp_in();
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& req_out();
};
```

**Tick Logic:**
1. Check each input port for requests → enqueue with src_port ID
2. If req_queue_ not empty and req_out_.ready() → dequeue and forward to req_out_
3. Check resp_in_ for responses → route to correct resp_out_[port]

**Configuration (JSON):**
```json
{
  "name": "arbiter0",
  "type": "ArbiterTLM",
  "config": {
    "num_inputs": 4,
    "arbitration": "round_robin"
  }
}
```

**Note:** Since CppTLM doesn't support template registration in JSON, we'll implement `ArbiterTLM4` as a concrete class with NUM_INPUTS=4.

---

## 3. Bundle Requirements

### 3.1 Existing Bundles (Reuse)

From `include/bundles/cache_bundles_tlm.hh`:

```cpp
struct CacheReqBundle {
    ch_uint<64> transaction_id;
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;
};

struct CacheRespBundle {
    ch_uint<64> transaction_id;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
};
```

**Status:** ✅ Sufficient for all three modules

### 3.2 New Bundles (If Needed)

No new bundles required. The existing CacheReqBundle/CacheRespBundle pair supports:
- CPU requests (read with transaction_id, address)
- TrafficGen requests (read/write with address, data)
- Arbiter forwarding (transparent passthrough)

---

## 4. StreamAdapter Registration

### 4.1 Registration Pattern

From `include/chstream_register.hh`:

```cpp
#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<MemoryTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("MemoryTLM"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<CrossbarTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("CrossbarTLM");
```

### 4.2 New Registration Macro

**File to Modify:** `include/chstream_register.hh`

```cpp
#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); \
    ModuleFactory::registerObject<CPUTLM>("CPUTLM"); \
    ModuleFactory::registerObject<TrafficGenTLM>("TrafficGenTLM"); \
    ModuleFactory::registerObject<ArbiterTLM4>("ArbiterTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<MemoryTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("MemoryTLM"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<CrossbarTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("CrossbarTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<CPUTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CPUTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<TrafficGenTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("TrafficGenTLM"); \
    ChStreamAdapterFactory::get().registerMultiPortAdapter<ArbiterTLM4, \
        bundles::CacheReqBundle, bundles::CacheRespBundle, 4>("ArbiterTLM");
```

**Note:** `ArbiterTLM4` registers as `"ArbiterTLM"` for JSON compatibility.

---

## 5. Test Plan (TDD-Oriented)

### 5.1 Test Files to Create

| Test File | Module | Tags | Priority |
|-----------|--------|------|----------|
| `test/test_cpu_tlm_unit.cc` | CPUTLM | `[chstream][cpu]` | High |
| `test/test_traffic_gen_tlm_unit.cc` | TrafficGenTLM | `[chstream][trafficgen]` | High |
| `test/test_arbiter_tlm_unit.cc` | ArbiterTLM | `[chstream][arbiter]` | High |
| `test/test_tlm_modules_integration.cc` | All 3 | `[phase7][integration]` | High |

### 5.2 CPUTLM Unit Tests

**File:** `test/test_cpu_tlm_unit.cc`

```cpp
TEST_CASE("CPUTLM issues request when timer expires", "[chstream][cpu]") {
    // Setup: CPUTLM with interval=5
    // Tick 5 times
    // Verify: req_out_.valid() == true
}

TEST_CASE("CPUTLM respects MAX_INFLIGHT limit", "[chstream][cpu]") {
    // Setup: 4 in-flight transactions
    // Tick
    // Verify: No new request issued
}

TEST_CASE("CPUTLM completes transaction on response", "[chstream][cpu]") {
    // Setup: 1 in-flight transaction
    // Inject response with matching transaction_id
    // Verify: inflight_txns_.size() decreased
}

TEST_CASE("CPUTLM sequential address generation", "[chstream][cpu]") {
    // Issue 3 requests
    // Verify: addresses are 0x1000, 0x1004, 0x1008
}

TEST_CASE("CPUTLM reset clears all state", "[chstream][cpu]") {
    // Setup: some in-flight transactions
    // Call do_reset()
    // Verify: inflight_txns_.empty(), cur_addr_ == start_addr_
}
```

### 5.3 TrafficGenTLM Unit Tests

**File:** `test/test_traffic_gen_tlm_unit.cc`

```cpp
TEST_CASE("TrafficGenTLM sequential mode", "[chstream][trafficgen]") {
    // Mode: SEQUENTIAL, start=0x1000, end=0x2000
    // Issue 3 requests
    // Verify: addresses increment by 4, wrap at end
}

TEST_CASE("TrafficGenTLM random mode", "[chstream][trafficgen]") {
    // Mode: RANDOM, start=0x1000, end=0x2000
    // Issue 10 requests
    // Verify: all addresses in [0x1000, 0x2000)
}

TEST_CASE("TrafficGenTLM trace mode", "[chstream][trafficgen]") {
    // Mode: TRACE, trace=[{addr:0x1000, write:false}, {addr:0x1004, write:true}]
    // Issue 2 requests
    // Verify: matches trace exactly
}

TEST_CASE("TrafficGenTLM tracks completions", "[chstream][trafficgen]") {
    // num_requests_=5
    // Issue 5 requests, inject 5 responses
    // Verify: completed_==5, no more requests issued
}

TEST_CASE("TrafficGenTLM deterministic RNG", "[chstream][trafficgen]") {
    // Fixed seed (42)
    // Issue 10 random requests
    // Verify: same sequence on repeated runs
}
```

### 5.4 ArbiterTLM Unit Tests

**File:** `test/test_arbiter_tlm_unit.cc`

```cpp
TEST_CASE("ArbiterTLM forwards request from port 0", "[chstream][arbiter]") {
    // Inject request on req_in_[0]
    // Tick
    // Verify: req_out_.valid(), src_port tracked
}

TEST_CASE("ArbiterTLM round-robin arbitration", "[chstream][arbiter]") {
    // Inject requests on ports 0, 1, 2 simultaneously
    // Tick 3 times
    // Verify: forwarded in order 0→1→2
}

TEST_CASE("ArbiterTLM routes response to correct port", "[chstream][arbiter]") {
    // Forward request from port 2
    // Inject response on resp_in_
    // Verify: resp_out_[2].valid()
}

TEST_CASE("ArbiterTLM queues under backpressure", "[chstream][arbiter]") {
    // Inject 5 requests, req_out_ not ready
    // Verify: all 5 queued
    // Make req_out_ ready, tick
    // Verify: all forwarded
}

TEST_CASE("ArbiterTLM reset clears all queues", "[chstream][arbiter]") {
    // Setup: queued requests
    // Call do_reset()
    // Verify: all queues empty
}
```

### 5.5 Integration Tests

**File:** `test/test_tlm_modules_integration.cc`

```cpp
TEST_CASE("Phase 7: CPUTLM → ArbiterTLM → CrossbarTLM → MemoryTLM", "[phase7][integration]") {
    // Topology:
    //   CPUTLM → ArbiterTLM.port0
    //   TrafficGenTLM → ArbiterTLM.port1
    //   ArbiterTLM.out → CrossbarTLM.port0
    //   CrossbarTLM.port0 → MemoryTLM
    //
    // Run 100 cycles
    // Verify: CPU requests reach MemoryTLM, responses return
}

TEST_CASE("Phase 7: Multiple initiators through ArbiterTLM", "[phase7][integration]") {
    // Topology:
    //   CPUTLM → ArbiterTLM.port0
    //   TrafficGenTLM → ArbiterTLM.port1
    //   ArbiterTLM.out → MemoryTLM
    //
    // Run 50 cycles
    // Verify: Both initiators' requests interleaved correctly
}

TEST_CASE("Phase 7: Backpressure propagation", "[phase7][integration]") {
    // Topology: CPUTLM → ArbiterTLM → MemoryTLM (slow)
    // MemoryTLM configured with 10-cycle latency
    // Verify: CPUTLM inflight limit respected
}

TEST_CASE("Phase 7: JSON config loading — cpu_group.json", "[phase7][config]") {
    // Load configs/cpu_group.json (requires CPUTLM, TrafficGenTLM)
    // Verify: ModuleFactory::instantiateAll() succeeds
}

TEST_CASE("Phase 7: JSON config loading — ring_bus.json", "[phase7][config]") {
    // Load configs/ring_bus.json (requires ArbiterTLM)
    // Verify: ModuleFactory::instantiateAll() succeeds
}
```

---

## 6. Config Coverage

### 6.1 Config File Compatibility

| Config File | Current Modules | After Implementation | Coverage Change |
|-------------|-----------------|---------------------|-----------------|
| `cpu_simple.json` | CPUSim, CacheSim, MemorySim | ✅ CPUTLM, CacheTLM, MemoryTLM | +100% (TLM equivalents) |
| `cpu_group.json` | CPUSim (×3), TrafficGenerator, CacheSim | ✅ CPUTLM (×3), TrafficGenTLM, CacheTLM | +100% (TLM equivalents) |
| `ring_bus.json` | Crossbar, CPUSim (×2), CacheSim, MemorySim | ✅ ArbiterTLM, CPUTLM (×2), CacheTLM, MemoryTLM | +100% (TLM equivalents) |
| `crossbar_test.json` | CacheTLM, CrossbarTLM, MemoryTLM | ✅ Unchanged | Already covered |
| `cache_chstream_test.json` | CacheTLM, MemoryTLM | ✅ Unchanged | Already covered |

### 6.2 New Config Files (Optional)

**File:** `configs/cpu_tlm_test.json`

```json
{
  "modules": [
    { "name": "cpu0", "type": "CPUTLM", "config": { "request_interval": 10 } },
    { "name": "traffic_gen0", "type": "TrafficGenTLM", "config": { "mode": "random" } },
    { "name": "l1", "type": "CacheTLM" },
    { "name": "mem", "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "cpu0", "dst": "l1", "latency": 2 },
    { "src": "traffic_gen0", "dst": "l1", "latency": 2 },
    { "src": "l1", "dst": "mem", "latency": 100 }
  ]
}
```

**File:** `configs/arbiter_test.json`

```json
{
  "modules": [
    { "name": "cpu0", "type": "CPUTLM" },
    { "name": "cpu1", "type": "CPUTLM" },
    { "name": "arbiter", "type": "ArbiterTLM" },
    { "name": "mem", "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "cpu0", "dst": "arbiter.0" },
    { "src": "cpu1", "dst": "arbiter.1" },
    { "src": "arbiter", "dst": "mem" },
    { "src": "mem", "dst": "arbiter" },
    { "src": "arbiter", "dst": "cpu0" },
    { "src": "arbiter", "dst": "cpu1" }
  ]
}
```

---

## 7. Atomic Commit Strategy

### 7.1 Commit Breakdown

Each commit is atomic, compiles independently, and passes existing tests.

| # | Commit Message | Files Changed | Type | Verification |
|---|----------------|---------------|------|--------------|
| 1 | `feat(TLM): Add CPUTLM module header` | `include/tlm/cpu_tlm.hh` | Add | `cmake --build` |
| 2 | `test(TLM): Add CPUTLM unit tests` | `test/test_cpu_tlm_unit.cc` | Add | `ctest -R cpu` |
| 3 | `feat(TLM): Add TrafficGenTLM module header` | `include/tlm/traffic_gen_tlm.hh` | Add | `cmake --build` |
| 4 | `test(TLM): Add TrafficGenTLM unit tests` | `test/test_traffic_gen_tlm_unit.cc` | Add | `ctest -R trafficgen` |
| 5 | `feat(TLM): Add ArbiterTLM4 module header` | `include/tlm/arbiter_tlm.hh` | Add | `cmake --build` |
| 6 | `test(TLM): Add ArbiterTLM unit tests` | `test/test_arbiter_tlm_unit.cc` | Add | `ctest -R arbiter` |
| 7 | `feat(TLM): Register CPUTLM/TrafficGenTLM/ArbiterTLM` | `include/chstream_register.hh` | Modify | `cmake --build` |
| 8 | `test(TLM): Add Phase 7 integration tests` | `test/test_tlm_modules_integration.cc` | Add | `ctest -R phase7` |
| 9 | `test(TLM): Add JSON config loader tests` | `test/test_tlm_config_load.cc` | Add | `ctest -R config` |
| 10 | `docs(TLM): Add module documentation` | `docs/tlm-modules.md` | Add | N/A |

### 7.2 Commit Order Rationale

1. **Modules first (1, 3, 5):** Each module header is self-contained, no dependencies
2. **Tests immediately after (2, 4, 6):** TDD approach — test fails, then passes
3. **Registration (7):** Requires all modules to exist
4. **Integration (8, 9):** Requires registration to be complete
5. **Documentation (10):** Final step

### 7.3 Verification Commands

After each commit:

```bash
# Commits 1, 3, 5 (Module headers)
cmake --build build --target cpptlm_core

# Commits 2, 4, 6 (Unit tests)
cmake --build build
./build/bin/cpptlm_tests "[cpu]"  # or [trafficgen], [arbiter]

# Commit 7 (Registration)
cmake --build build
./build/bin/cpptlm_tests ~"[integration]"  # Exclude integration tests

# Commits 8, 9 (Integration)
cmake --build build
./build/bin/cpptlm_tests "[phase7]"

# Commit 10 (Docs)
# No build required
```

---

## 8. Implementation Checklist

### 8.1 Pre-Implementation

- [ ] Read `include/tlm/cache_tlm.hh` for pattern reference
- [ ] Read `include/framework/stream_adapter.hh` for adapter interface
- [ ] Read `test/test_cache_tlm_unit.cc` for test pattern
- [ ] Verify `bundles/cache_bundles_tlm.hh` has required fields

### 8.2 CPUTLM

- [ ] Create `include/tlm/cpu_tlm.hh`
- [ ] Implement constructor, tick(), do_reset()
- [ ] Implement address generation logic
- [ ] Implement in-flight tracking
- [ ] Add `REGISTER_CHSTREAM` entry
- [ ] Create `test/test_cpu_tlm_unit.cc` (5 tests)
- [ ] Verify: `ctest -R cpu` passes (5/5)

### 8.3 TrafficGenTLM

- [ ] Create `include/tlm/traffic_gen_tlm.hh`
- [ ] Implement constructor, tick(), do_reset()
- [ ] Implement 3 generation modes
- [ ] Implement trace playback
- [ ] Implement deterministic RNG
- [ ] Add `REGISTER_CHSTREAM` entry
- [ ] Create `test/test_traffic_gen_tlm_unit.cc` (5 tests)
- [ ] Verify: `ctest -R trafficgen` passes (5/5)

### 8.4 ArbiterTLM

- [ ] Create `include/tlm/arbiter_tlm.hh`
- [ ] Implement as `ArbiterTLM4` concrete class (not template)
- [ ] Implement multi-port input handling
- [ ] Implement round-robin arbitration
- [ ] Implement response routing
- [ ] Add `REGISTER_CHSTREAM` entry (as "ArbiterTLM")
- [ ] Create `test/test_arbiter_tlm_unit.cc` (5 tests)
- [ ] Verify: `ctest -R arbiter` passes (5/5)

### 8.5 Registration

- [ ] Update `include/chstream_register.hh`
- [ ] Add 3 `ModuleFactory::registerObject<>` calls
- [ ] Add 3 `ChStreamAdapterFactory::get().registerAdapter<>` calls
- [ ] Verify: `cmake --build build` succeeds

### 8.6 Integration Tests

- [ ] Create `test/test_tlm_modules_integration.cc`
- [ ] Implement 4 integration tests
- [ ] Verify: `ctest -R phase7` passes (4/4)
- [ ] Create `configs/cpu_tlm_test.json` (optional)
- [ ] Create `configs/arbiter_test.json` (optional)

### 8.7 Documentation

- [ ] Create `docs/tlm-modules.md`
- [ ] Document each module's purpose, interface, configuration
- [ ] Add architecture diagram
- [ ] Update `docs/README.md` if needed

---

## 9. Risks and Mitigations

### 9.1 Technical Risks

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| **StreamAdapter binding complexity** | High | Medium | Follow CacheTLM pattern exactly; verify with simple test first |
| **Multi-port ArbiterTLM registration** | Medium | Low | Use concrete `ArbiterTLM4` class instead of template |
| **Response routing in ArbiterTLM** | Medium | Medium | Track transaction_id → src_port mapping |
| **JSON config parsing for new modules** | Low | Low | Use existing `ModuleFactory::instantiateAll()` logic |

### 9.2 Schedule Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Test coverage gaps** | Medium | Write tests before implementation (TDD) |
| **Integration test failures** | High | Run unit tests first; isolate failures |
| **Regression in existing tests** | High | Run full test suite after each commit |

---

## 10. Acceptance Criteria

### 10.1 Module Implementation

- [x] CPUTLM header created with full interface
- [x] TrafficGenTLM header created with full interface
- [x] ArbiterTLM4 header created with full interface
- [ ] All modules compile without warnings (`-Wall -Wextra`)
- [ ] All modules follow ChStreamModuleBase pattern

### 10.2 Test Coverage

- [ ] CPUTLM: 5 unit tests passing
- [ ] TrafficGenTLM: 5 unit tests passing
- [ ] ArbiterTLM: 5 unit tests passing
- [ ] Integration: 4 tests passing
- [ ] Config loading: 2 tests passing
- [ ] Total: 21 new tests, all passing

### 10.3 Config Compatibility

- [ ] `configs/cpu_simple.json` loads with CPUTLM
- [ ] `configs/cpu_group.json` loads with CPUTLM + TrafficGenTLM
- [ ] `configs/ring_bus.json` loads with ArbiterTLM
- [ ] Optional: `configs/cpu_tlm_test.json` loads successfully
- [ ] Optional: `configs/arbiter_test.json` loads successfully

### 10.4 Code Quality

- [ ] No clang-tidy warnings
- [ ] Follows project naming conventions
- [ ] File headers include description, author, date
- [ ] Complex logic documented with comments

---

## 11. Appendix: File Locations Summary

### 11.1 New Files

```
include/tlm/cpu_tlm.hh
include/tlm/traffic_gen_tlm.hh
include/tlm/arbiter_tlm.hh
test/test_cpu_tlm_unit.cc
test/test_traffic_gen_tlm_unit.cc
test/test_arbiter_tlm_unit.cc
test/test_tlm_modules_integration.cc
test/test_tlm_config_load.cc
configs/cpu_tlm_test.json (optional)
configs/arbiter_test.json (optional)
docs/tlm-modules.md
```

### 11.2 Modified Files

```
include/chstream_register.hh
```

### 11.3 Dependencies

```
include/core/chstream_module.hh       (existing)
include/framework/stream_adapter.hh   (existing)
include/bundles/cache_bundles_tlm.hh  (existing)
include/core/chstream_adapter_factory.hh (existing)
```

---

## 12. Next Steps

1. **Start with CPUTLM implementation** (highest priority, simplest module)
2. **Write unit tests immediately** (TDD approach)
3. **Verify compilation and tests pass**
4. **Proceed to TrafficGenTLM**
5. **Then ArbiterTLM** (most complex)
6. **Update registration**
7. **Run integration tests**
8. **Final verification and documentation**

---

**End of Plan**

**Estimated Implementation Time:** 4-6 hours (including testing)
**Complexity:** Medium (well-defined patterns from CacheTLM/MemoryTLM)
