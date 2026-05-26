# TGMS v4.0 Implementation Plan

> **Document ID**: IMPL-010-v4.0
> **Version**: 4.0
> **Date**: 2026-05-27
> **Status**: 🟡 Planning

## Overview

TGMS v3.0 (Phase 1-3) has been completed. This document defines the implementation plan for TGMS v4.0 - hierarchical heterogeneous SoC with coherence domains.

**Architecture reference**: [10-topology-generation-management-system.md](../architecture/10-topology-generation-management-system.md)
**Specifications**: [10-tgms-specifications.md](../architecture/10-tgms-specifications.md)

---

## 1. Phase 4: Hierarchy Core (3-4 weeks)

**Goal**: Support hierarchical topology tree and CoherenceDomain

### 1.1 Tasks

| Task | Gap | Effort | Dependencies | Status |
|------|-----|--------|-------------|--------|
| 4.1 Hierarchy tree parser | G8 | 3 days | Phase 1 | 🟡 Planning |
| 4.2 CoherenceDomain C++ module | G9 | 5 days | 4.1 | 🟡 Planning |
| 4.3 Domain boundary validation | G9 | 2 days | 4.2 | 🟡 Planning |
| 4.4 Snoop routing logic | G9 | 3 days | 4.2 | 🟡 Planning |
| 4.5 Directory protocol stub | G9 | 5 days | 4.2 | 🟡 Planning |
| 4.6 Python hierarchy generator | G8 | 5 days | 4.1-4.3 | 🟡 Planning |

### 1.2 Detailed Tasks

#### Task 4.1: Hierarchy Tree Parser

**Files**: `src/core/module_factory.cc`

**Steps**:
1. Add Step 0 to parse `hierarchy` object in JSON config
2. Create `TopologyNode` class for tree structure
3. Implement `parse_hierarchy_tree()` function
4. Add unit tests for hierarchy parsing

**Acceptance Criteria**:
- [ ] `hierarchy` JSON object correctly parsed into tree structure
- [ ] Parent-child relationships correctly established
- [ ] Unit tests pass

#### Task 4.2: CoherenceDomain C++ Module

**Files**: `include/core/coherence_domain.hh` (NEW)

**Steps**:
1. Create `CoherenceDomain` class inheriting from `SimObject`
2. Implement `set_protocol()`, `set_members()`, `set_snoop_fanout()`
3. Implement `is_member()`, `get_snoop_targets()`, `lookup_home_node()`
4. Integrate with ModuleFactory (Step 0.5)

**Acceptance Criteria**:
- [ ] CoherenceDomain class created with all specified methods
- [ ] Protocol support: MESI, MOESI
- [ ] Directory-based home node lookup working
- [ ] Unit tests pass

#### Task 4.3: Domain Boundary Validation

**Files**: `src/core/module_factory.cc`, `include/core/coherence_domain.hh`

**Steps**:
1. Add `validate_domain_boundary()` function
2. Reject cross-domain connections without ProtocolBridge
3. Add validation in ModuleFactory Step 3
4. Add unit tests

**Acceptance Criteria**:
- [ ] Cross-domain connections without bridge are rejected
- [ ] WARNING/ERROR logged for boundary violations
- [ ] Unit tests pass

#### Task 4.4: Snoop Routing Logic

**Files**: `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc`

**Steps**:
1. Implement `get_snoop_targets()` for broadcast/multicast
2. Support snoop fanout configuration
3. Add unit tests for snoop routing

**Acceptance Criteria**:
- [ ] Snoop messages reach all domain members
- [ ] Fanout configuration respected
- [ ] Unit tests pass

#### Task 4.5: Directory Protocol Stub

**Files**: `include/core/directory.hh` (NEW), `src/core/directory.cc`

**Steps**:
1. Create `Directory` class
2. Implement `lookup_home_node()` with address mapping
3. Support directory entry states (M, O, S)
4. Add unit tests

**Acceptance Criteria**:
- [ ] Directory class created
- [ ] Home node correctly determined for address
- [ ] Unit tests pass

#### Task 4.6: Python Hierarchy Generator

**Files**: `scripts/topology_generator.py`

**Steps**:
1. Add `HierarchicalTopologyGenerator` class
2. Implement `add_cluster()`, `add_coherence_domain()` methods
3. Output v4.0 JSON format with `hierarchy` and `coherence_domains`
4. Add integration tests

**Acceptance Criteria**:
- [ ] HierarchicalTopologyGenerator class implemented
- [ ] Correct v4.0 JSON output with hierarchy tree
- [ ] Integration tests pass

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
- [ ] Address translation working for all rule types
- [ ] Unit tests pass

#### Task 5.3: Protocol Conversion Logic

**Files**: `src/core/protocol_bridge.cc`

**Steps**:
1. Implement protocol conversion (MESI ↔ MOESI, etc.)
2. Handle transaction type mapping
3. Add unit tests for conversion logic

**Acceptance Criteria**:
- [ ] MESI to MOESI conversion working
- [ ] Transaction types correctly mapped
- [ ] Unit tests pass

#### Task 5.4: Cross-Protocol Validation

**Files**: `src/core/module_factory.cc`

**Steps**:
1. Add `validate_protocol_compatibility()` function
2. Validate protocol bridge configuration
3. Add unit tests

**Acceptance Criteria**:
- [ ] Protocol compatibility validated
- [ ] Invalid configurations rejected with clear error
- [ ] Unit tests pass

#### Task 5.5: Python Bridge Config Generator

**Files**: `scripts/topology_generator.py`

**Steps**:
1. Add `ProtocolBridgeGenerator` class
2. Implement bridge configuration generation
3. Add integration tests

**Acceptance Criteria**:
- [ ] ProtocolBridge configuration correctly generated
- [ ] Address translation rules correctly output
- [ ] Integration tests pass

---

## 3. Phase 6: Multi-Cluster SoC Validation (2-3 weeks)

**Goal**: End-to-end validation with realistic SoC config

### 3.1 Tasks

| Task | Gap | Effort | Dependencies | Status |
|------|-----|--------|-------------|--------|
| 6.1 2x CPU Cluster + GPU Cluster config | G8, G9 | 3 days | Phase 4, 5 | 🟡 Planning |
| 6.2 Cross-cluster coherence validation | G9 | 5 days | 6.1 | 🟡 Planning |
| 6.3 Protocol bridge integration test | G10 | 3 days | 6.1 | 🟡 Planning |
| 6.4 Full SoC example (4 CPU + 1 GPU) | All | 5 days | 6.2-6.3 | 🟡 Planning |

### 3.2 Detailed Tasks

#### Task 6.1: 2x CPU Cluster + GPU Cluster Config

**Files**: `configs/hierarchical_2x2_tlm.json`, `configs/soc_2cpu_1gpu.json`

**Steps**:
1. Create hierarchical 2x2 cluster config
2. Add CPU cluster with 2 cores
3. Add GPU cluster with 1 device
4. Configure coherence domains for each cluster

**Acceptance Criteria**:
- [ ] Config file created with correct hierarchy
- [ ] Both CPU and GPU clusters defined
- [ ] Coherence domains correctly configured

#### Task 6.2: Cross-Cluster Coherence Validation

**Files**: `test/test_coherence_domain.cc`

**Steps**:
1. Add integration tests for cross-cluster coherence
2. Test snoop traffic between clusters
3. Validate directory lookup across clusters

**Acceptance Criteria**:
- [ ] Cross-cluster coherence tests implemented
- [ ] All coherence scenarios validated
- [ ] Tests pass

#### Task 6.3: Protocol Bridge Integration Test

**Files**: `test/test_protocol_bridge.cc`

**Steps**:
1. Add integration tests for ProtocolBridge
2. Test address translation through bridge
3. Test protocol conversion through bridge

**Acceptance Criteria**:
- [ ] ProtocolBridge integration tests implemented
- [ ] Address translation verified
- [ ] Protocol conversion verified
- [ ] Tests pass

#### Task 6.4: Full SoC Example (4 CPU + 1 GPU)

**Files**: `configs/soc_4cpu_1gpu.json`, `configs/`

**Steps**:
1. Create full SoC configuration (4 CPU cores + 1 GPU)
2. Configure hierarchical topology
3. Configure all coherence domains and bridges
4. Run end-to-end simulation

**Acceptance Criteria**:
- [ ] Full SoC config created
- [ ] All modules correctly configured
- [ ] End-to-end simulation passes

---

## 4. Implementation Order

```
Phase 4 ──────────────────────────────────────────
  4.1 (3d) ──→ 4.2 (5d) ──→ 4.3 (2d)
                       ──→ 4.4 (3d)
                       ──→ 4.5 (5d)
               ──→ 4.6 (5d) (parallel with 4.2-4.5 after 4.1)

Phase 5 ──────────────────────────────────────────
  5.1 (5d) ──→ 5.2 (3d)
           ──→ 5.3 (5d)
           ──→ 5.4 (3d) (after 5.1-5.3)
           ──→ 5.5 (3d) (parallel with 5.4)

Phase 6 ──────────────────────────────────────────
  6.1 (3d) ──→ 6.2 (5d)
           ──→ 6.3 (3d) (parallel with 6.2)
           ──→ 6.4 (5d) (after 6.2-6.3)
```

**Critical Path**: 4.1 → 4.2 → 4.3 → 5.1 → 5.4 → 6.2 → 6.4
**Estimated Duration**: 8-10 weeks

---

## 5. File Summary

### New Files

| File | Purpose |
|------|---------|
| `include/core/coherence_domain.hh` | CoherenceDomain class declaration |
| `src/core/coherence_domain.cc` | CoherenceDomain implementation |
| `include/core/directory.hh` | Directory class declaration |
| `src/core/directory.cc` | Directory implementation |
| `include/core/protocol_bridge.hh` | ProtocolBridge class declaration |
| `src/core/protocol_bridge.cc` | ProtocolBridge implementation |
| `test/test_coherence_domain.cc` | CoherenceDomain unit tests |
| `test/test_protocol_bridge.cc` | ProtocolBridge unit tests |

### Modified Files

| File | Changes |
|------|---------|
| `src/core/module_factory.cc` | Add hierarchy parsing (Step 0), coherence domain integration |
| `scripts/topology_generator.py` | Add HierarchicalTopologyGenerator, ProtocolBridgeGenerator |

---

## 6. Acceptance Criteria

### Phase 4 Acceptance
- [ ] Hierarchy tree correctly parsed
- [ ] CoherenceDomain class functional with MESI/MOESI support
- [ ] Domain boundary validation working
- [ ] Snoop routing within domain working
- [ ] Directory-based home node lookup working
- [ ] Python hierarchy generator producing correct v4.0 JSON

### Phase 5 Acceptance
- [ ] ProtocolBridge class functional
- [ ] Address translation engine working
- [ ] Protocol conversion logic working
- [ ] Cross-protocol validation working
- [ ] Python bridge config generator working

### Phase 6 Acceptance
- [ ] 2x CPU + GPU cluster config created
- [ ] Cross-cluster coherence validated
- [ ] Protocol bridge integration tested
- [ ] Full SoC example (4 CPU + 1 GPU) working