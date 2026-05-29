# TGMS v4.0 Implementation Plan

> **Document ID**: IMPL-010-v4.0
> **Version**: 4.0
> **Date**: 2026-05-27
> **Status**: 🔄 In Progress (Phase 4 Complete)

## Overview

TGMS v3.0 (Phase 1-3) has been completed. This document defines the implementation plan for TGMS v4.0 - hierarchical heterogeneous SoC with coherence domains.

**Architecture reference**: [10-topology-generation-management-system.md](../architecture/10-topology-generation-management-system.md)
**Specifications**: [10-tgms-specifications.md](../architecture/10-tgms-specifications.md)

---

## Task Tracking

### Phase 4 Task Status

| Task | Status | Completion Date | Notes |
|------|--------|-----------------|-------|
| **4.1 Hierarchy tree parser** | ✅ Complete | 2026-05-27 | TopologyNode + parse_hierarchy_tree() implemented |
| **4.2 CoherenceDomain C++ module** | ✅ Complete | 2026-05-28 | SimObject-based, MESI/MOESI protocol support |
| **4.3 Domain boundary validation** | ✅ Complete | 2026-05-28 | ModuleFactory::validate_domain_boundary() implemented |
| **4.4 Snoop routing logic** | ✅ No-op | 2026-05-29 | get_snoop_targets() already in CoherenceDomain (Phase 4.2) |
| **4.5 Directory protocol stub** | ✅ Complete | 2026-05-29 | Directory class with M/O/S states, delegates to CoherenceDomain |
| **4.6 Python hierarchy generator** | ⚠️ Deferred | — | Deferred to Phase 5 - requires complete builder API design |
| **4.7 HierarchicalTopologyGenerator** | 🔄 Planned | — | NEW - Python builder API for v4.0 JSON generation |

### Phase 5 Task Status

| Task | Status | Dependencies |
|------|--------|--------------|
| 5.1 ProtocolBridge C++ module | ⏳ Pending | Phase 4 |
| 5.2 Address translation engine | ⏳ Pending | 5.1 |
| 5.3 Protocol conversion logic | ⏳ Pending | 5.1 |
| 5.4 Cross-protocol validation | ⏳ Pending | 5.1-5.3 |
| 5.5 Python bridge config generator | ⏳ Pending | 5.1-5.4 |
| **5.6 HierarchicalTopologyGenerator** | 🔄 Planned | 4.6 Deferred |

### Phase 6 Task Status

| Task | Status | Dependencies |
|------|--------|--------------|
| 6.1 2x CPU Cluster + GPU Cluster config | ⏳ Pending | Phase 4, 5 |
| 6.2 Cross-cluster coherence validation | ⏳ Pending | 6.1 |
| 6.3 Protocol bridge integration test | ⏳ Pending | 6.1 |
| 6.4 Full SoC example (4 CPU + 1 GPU) | ⏳ Pending | 6.2-6.3 |

---

## 1. Phase 4: Hierarchy Core (3-4 weeks)

**Goal**: Support hierarchical topology tree and CoherenceDomain

### 1.1 Tasks

| Task | Gap | Effort | Dependencies | Status |
|------|-----|--------|-------------|--------|
| 4.1 Hierarchy tree parser | G8 | 3 days | Phase 1 | ✅ Complete |
| 4.2 CoherenceDomain C++ module | G9 | 5 days | 4.1 | ✅ Complete |
| 4.3 Domain boundary validation | G9 | 2 days | 4.2 | ✅ Complete |
| 4.4 Snoop routing logic | G9 | 3 days | 4.2 | ✅ No-op |
| 4.5 Directory protocol stub | G9 | 5 days | 4.2 | ✅ Complete |
| 4.6 Python hierarchy generator | G8 | 5 days | 4.1-4.3 | ⚠️ Deferred |
| 4.7 HierarchicalTopologyGenerator | G8 | 5 days | 4.6 | 🔄 Planned |

### 1.2 Detailed Tasks

#### Task 4.1: Hierarchy Tree Parser ✅ COMPLETE

**Files**: `src/core/module_factory.cc`, `include/core/topology_node.hh`, `include/core/topology_parser.hh`

**Steps**:
1. [x] Add Step 0 to parse `hierarchy` object in JSON config
2. [x] Create `TopologyNode` class for tree structure
3. [x] Implement `parse_hierarchy_tree()` function
4. [x] Add unit tests for hierarchy parsing

**Acceptance Criteria**:
- [x] `hierarchy` JSON object correctly parsed into tree structure
- [x] Parent-child relationships correctly established
- [x] Unit tests pass

**Implementation Notes**:
- `TopologyNode` class: `include/core/topology_node.hh`
- `parse_hierarchy_tree()`: `include/core/topology_parser.hh` + `src/core/topology_parser.cc`
- ModuleFactory integration: Step 0.5 in `src/core/module_factory.cc`
- Tests: `test/test_tgms_v4_hierarchy_integration.cc` (10 test cases)
- Example: `examples/demo_configs/hierarchy_tgms_v4.json`

#### Task 4.2: CoherenceDomain C++ Module ✅ COMPLETE

**Files**: `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc`

**Steps**:
1. [x] Create `CoherenceDomain` class inheriting from `SimObject`
2. [x] Implement `set_protocol()`, `set_members()`, `set_snoop_fanout()`
3. [x] Implement `is_member()`, `get_snoop_targets()`, `lookup_home_node()`
4. [x] Integrate with ModuleFactory (Step 0.5)

**Acceptance Criteria**:
- [x] CoherenceDomain class created with all specified methods
- [x] Protocol support: MESI, MOESI
- [x] Directory-based home node lookup working
- [x] Unit tests pass

**Implementation Notes**:
- Commit: `371aa99 feat(phase-4.2): implement CoherenceDomain C++ module with TDD`
- Supports bridge mapping via `register_bridge()`, `has_bridge_to()`, `get_bridge()`
- DomainRegistry singleton for domain management

#### Task 4.3: Domain Boundary Validation ✅ COMPLETE

**Files**: `src/core/module_factory.cc`, `include/core/coherence_domain.hh`

**Steps**:
1. [x] Add `validate_domain_boundary()` function in ModuleFactory
2. [x] Check for ProtocolBridge when cross-domain connection detected
3. [x] Reject connection if no ProtocolBridge present
4. [x] Log WARNING for boundary violations
5. [x] Log ERROR for invalid configuration
6. [x] Add unit tests in `test/test_domain_boundary.cc`

**Acceptance Criteria**:
- [x] Cross-domain connections without bridge are rejected
- [x] WARNING/ERROR logged for boundary violations
- [x] Unit tests pass

**Implementation Notes**:
- Commit: `e5766f6 feat(domain): add domain boundary validation (Phase 4.3)`
- Merge: `32ba0dd Merge add-domain-validation (Phase 4.3)`
- Uses DomainRegistry to lookup domain and check bridge availability

#### Task 4.4: Snoop Routing Logic ✅ NO-OP

**Files**: `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc`

**Analysis**:
- `get_snoop_targets()` was already implemented in Phase 4.2 as part of CoherenceDomain
- `snoop_fanout_` configuration was already available via `set_snoop_fanout()`
- Task 3 (multicast target list) was correctly identified as belonging to RouterTLM layer, not CoherenceDomain

**Conclusion**:
- Tasks 1 & 2: Already implemented in Phase 4.2
- Task 3: Deferred to RouterTLM/CrossbarTLM layer (architectural decision)
- No code changes needed

#### Task 4.5: Directory Protocol Stub ✅ COMPLETE

**Files**: `include/core/directory.hh` (NEW), `src/core/directory.cc` (NEW), `test/test_directory.cc` (NEW)

**Steps**:
1. [x] Create `Directory` class
2. [x] Delegate `lookup_home_node()` to `CoherenceDomain` (reuse Phase 4.2)
3. [x] Support directory entry states (M, O, S - Modified, Owned, Shared)
4. [x] Add basic directory entry data structure with sharers/owner tracking
5. [x] Add unit tests

**Acceptance Criteria**:
- [x] Directory class created
- [x] Home node correctly determined via CoherenceDomain delegation
- [x] Directory entry states (M/O/S) properly tracked
- [x] Unit tests pass

**Implementation Notes**:
- Commit: `b8146fe feat(directory): add Directory class (Phase 4.5)`
- Directory delegates `lookup_home_node()` to CoherenceDomain to avoid duplication
- `DirectoryEntry` struct tracks: state (M/O/S/Uncached), sharers set, owner node

#### Task 4.6: Python Hierarchy Generator ⚠️ DEFERRED

**Files**: `scripts/topology_generator.py`

**Analysis**:
- Existing `TopologyGenerator` class has `generate_hierarchical()` method
- Existing `export_json_config()` outputs `modules` + `connections` only
- Missing: `hierarchy` and `coherence_domains` JSON fields
- Missing: `HierarchicalTopologyGenerator` class with builder API

**Deferred Reason**:
- Requires complete builder API design for `HierarchicalTopologyGenerator`
- Requires v4.0 JSON format specification with hierarchy + coherence_domains
- Current gap is in API design, not implementation

**Next Steps** (Phase 5):
1. Design complete builder pattern API for `HierarchicalTopologyGenerator`
2. Define v4.0 JSON format specification
3. Implement `add_cluster()` and `add_coherence_domain()` methods
4. Extend `to_v4_json()` output to include hierarchy and coherence_domains fields

#### Task 4.7: HierarchicalTopologyGenerator (NEW) 🔄 PLANNED

**Files**: `scripts/topology_generator.py` (MODIFIED)

**Goal**: Python builder API for programmatic generation of v4.0 JSON configurations with hierarchy and coherence domains

**Steps**:
1. [ ] Create `HierarchicalTopologyGenerator` class
2. [ ] Implement `add_cluster(name, children)` method
3. [ ] Implement `add_coherence_domain(name, protocol, members)` method
4. [ ] Implement `to_v4_json()` output method
5. [ ] Output v4.0 JSON format with `hierarchy` and `coherence_domains` fields
6. [ ] Add integration tests in `test/python/test_hierarchy_generator.py`

**Acceptance Criteria**:
- [ ] HierarchicalTopologyGenerator class implemented
- [ ] `add_cluster()` correctly builds hierarchy tree
- [ ] `add_coherence_domain()` registers domain with protocol and members
- [ ] `to_v4_json()` outputs complete v4.0 JSON format
- [ ] Integration tests pass

**v4.0 JSON Format**:
```json
{
  "name": "...",
  "modules": [...],
  "connections": [...],
  "hierarchy": {
    "name": "system",
    "children": [
      {"name": "cluster1", "children": [...]},
      {"name": "cluster2"}
    ]
  },
  "coherence_domains": [
    {"name": "cpu_domain", "protocol": "MESI", "members": ["cpu0", "cpu1"]}
  ]
}
```

**Dependencies**: Task 4.6 (deferred)

---

## 2. Phase 5: Protocol Bridge (2-3 weeks)

**Goal**: ProtocolBridge module and cross-protocol routing

### 2.1 Tasks

| Task | Gap | Effort | Dependencies | Status |
|------|-----|--------|-------------|--------|
| 5.1 ProtocolBridge C++ module | G10 | 5 days | Phase 4 | 🟡 Planning |
| 5.2 Address translation engine | G10 | 3 days | 5.1 | 🟡 Planning |
| 5.3 Protocol conversion logic | G10 | 5 days | 5.1 | 🟡 Planning |
| 5.4 Cross-protocol validation | G10 | 3 days | 5.1-5.3 | 🟡 Planning |
| 5.5 Python bridge config generator | G10 | 3 days | 5.1-5.4 | 🟡 Planning |
| **5.6 HierarchicalTopologyGenerator** | G8 | 5 days | 4.6 | 🔄 Planned |

### 2.2 Detailed Tasks

#### Task 5.1: ProtocolBridge C++ Module

**Files**: `include/core/protocol_bridge.hh` (NEW), `src/core/protocol_bridge.cc`

**Steps**:
1. Create `ProtocolBridge` class inheriting from `ChStreamModuleBase`
2. Implement `set_config()` with protocol and domain parameters
3. Implement `translate_address()`, `get_output_protocol()`
4. Add unit tests

**Acceptance Criteria**:
- [ ] ProtocolBridge class created with all specified methods
- [ ] Cross-domain traffic correctly routed
- [ ] Unit tests pass

#### Task 5.2: Address Translation Engine

**Files**: `include/core/protocol_bridge.hh`, `src/core/protocol_bridge.cc`

**Steps**:
1. Implement `AddressTranslationRule` structure
2. Parse `address_translation` array in config
3. Implement `translate_address()` with rule matching
4. Add unit tests

**Acceptance Criteria**:
- [ ] Address translation rules correctly parsed
- [ ] Address translation working for all configured rules
- [ ] Unit tests pass

#### Task 5.3: Protocol Conversion Logic

**Files**: `include/core/protocol_bridge.hh`, `src/core/protocol_bridge.cc`

**Steps**:
1. Implement MESI → MOESI conversion
2. Implement MOESI → MESI conversion
3. Implement directory state machine
4. Add unit tests

**Acceptance Criteria**:
- [ ] Protocol conversion correctly handles all state transitions
- [ ] Directory state machine correctly tracks sharing
- [ ] Unit tests pass

#### Task 5.4: Cross-Protocol Validation

**Files**: `include/core/protocol_bridge.hh`, `src/core/protocol_bridge.cc`

**Steps**:
1. Add `validate_cross_protocol()` function
2. Reject invalid protocol transitions
3. Add validation in ModuleFactory Step 3
4. Add unit tests

**Acceptance Criteria**:
- [ ] Invalid protocol transitions are rejected
- [ ] Validation errors logged with clear messages
- [ ] Unit tests pass

#### Task 5.5: Python Bridge Config Generator

**Files**: `scripts/bridge_config_generator.py` (NEW)

**Steps**:
1. Create `BridgeConfigGenerator` class
2. Implement `add_bridge()` method
3. Implement `add_translation_rule()` method
4. Output bridge configuration JSON
5. Add integration tests

**Acceptance Criteria**:
- [ ] BridgeConfigGenerator class implemented
- [ ] Bridge configuration JSON correctly generated
- [ ] Integration tests pass

#### Task 5.6: HierarchicalTopologyGenerator 🔄 PLANNED

**Files**: `scripts/topology_generator.py`

**Goal**: Python builder API for programmatic generation of v4.0 JSON configurations

**Steps**:
1. [ ] Create `HierarchicalTopologyGenerator` class (distinct from existing `TopologyGenerator`)
2. [ ] Implement `add_cluster(name, children)` method for hierarchy building
3. [ ] Implement `add_coherence_domain(name, protocol, members)` method
4. [ ] Implement `to_v4_json()` output method
5. [ ] Output v4.0 JSON format with `hierarchy` and `coherence_domains` fields
6. [ ] Add integration tests in `test/python/test_hierarchy_generator.py`

**Acceptance Criteria**:
- [ ] HierarchicalTopologyGenerator class implemented with builder pattern
- [ ] `add_cluster()` correctly builds hierarchy tree structure
- [ ] `add_coherence_domain()` registers coherence domain with protocol (MESI/MOESI) and members
- [ ] `to_v4_json()` outputs complete v4.0 JSON format including hierarchy and coherence_domains
- [ ] Integration tests pass

**v4.0 JSON Output Format**:
```json
{
  "name": "topology_name",
  "modules": [...],
  "connections": [...],
  "hierarchy": {
    "name": "system",
    "children": [
      {"name": "cluster0", "children": [{"name": "cpu0"}, {"name": "cpu1"}]},
      {"name": "cluster1", "children": [{"name": "cpu2"}, {"name": "cpu3"}]}
    ]
  },
  "coherence_domains": [
    {"name": "cpu_cluster", "protocol": "MESI", "members": ["cpu0", "cpu1", "cpu2", "cpu3"]}
  ]
}
```

**API Design**:
```python
gen = HierarchicalTopologyGenerator("my_soc")
gen.add_cluster("cluster0", ["cpu0", "cpu1", "l2_cache0"])
gen.add_cluster("cluster1", ["cpu2", "cpu3", "l2_cache1"])
gen.add_coherence_domain("cpu_cluster", "MESI", ["cpu0", "cpu1", "cpu2", "cpu3"])
config = gen.to_v4_json()
```

---

## 3. Phase 6: Integration (2 weeks)

**Goal**: Full SoC integration with coherence

### 3.1 Tasks

| Task | Gap | Effort | Dependencies | Status |
|------|-----|--------|-------------|--------|
| 6.1 2x CPU Cluster + GPU Cluster config | G10 | 3 days | Phase 4, 5 | 🟡 Planning |
| 6.2 Cross-cluster coherence validation | G10 | 3 days | 6.1 | 🟡 Planning |
| 6.3 Protocol bridge integration test | G10 | 3 days | 6.1 | 🟡 Planning |
| 6.4 Full SoC example (4 CPU + 1 GPU) | G10 | 5 days | 6.2-6.3 | 🟡 Planning |

### 3.2 Detailed Tasks

#### Task 6.1: 2x CPU Cluster + GPU Cluster Config

**Files**: `configs/soc_2x_cpu_gpu.json` (NEW)

**Steps**:
1. Create 2 CPU clusters with 2 CPUs each
2. Create 1 GPU cluster
3. Configure cross-cluster connections
4. Add integration tests

**Acceptance Criteria**:
- [ ] 2 CPU clusters correctly configured
- [ ] GPU cluster correctly configured
- [ ] Cross-cluster connections established
- [ ] Integration tests pass

#### Task 6.2: Cross-Cluster Coherence Validation

**Files**: `test/test_cross_cluster_coherence.cc` (NEW)

**Steps**:
1. Add coherence validation for cross-cluster traffic
2. Validate ProtocolBridge usage
3. Add unit tests

**Acceptance Criteria**:
- [ ] Cross-cluster coherence correctly validated
- [ ] ProtocolBridge correctly used for cross-cluster traffic
- [ ] Unit tests pass

#### Task 6.3: Protocol Bridge Integration Test

**Files**: `test/test_protocol_bridge_integration.cc` (NEW)

**Steps**:
1. Create integration test for ProtocolBridge
2. Test address translation
3. Test protocol conversion
4. All tests pass

**Acceptance Criteria**:
- [ ] ProtocolBridge integration test created
- [ ] Address translation tested
- [ ] Protocol conversion tested
- [ ] All tests pass

#### Task 6.4: Full SoC Example (4 CPU + 1 GPU)

**Files**: `configs/soc_4cpu_1gpu.json` (NEW), `docs/examples/soc_4cpu_1gpu.md` (NEW)

**Steps**:
1. Create full SoC configuration with 4 CPUs and 1 GPU
2. Document configuration
3. Add end-to-end test

**Acceptance Criteria**:
- [ ] Full SoC configuration created
- [ ] Documentation complete
- [ ] End-to-end test passes

---

## Appendix: Gap Reference

| Gap ID | Description |
|--------|-------------|
| G8 | Hierarchy tree parsing and generation |
| G9 | Coherence domain management |
| G10 | Cross-domain protocol bridge |

---

## Change Log

| Date | Version | Changes |
|------|---------|---------|
| 2026-05-27 | 4.0 | Initial version |
| 2026-05-29 | 4.1 | Phase 4.1-4.7 status updates, Task 4.4 marked no-op, Task 4.6 deferred, Task 4.7 added |