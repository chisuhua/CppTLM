# CppTLM Roadmap

> **Version**: 1.0
> **Created**: 2026-05-28
> **Status**: 🔄 Active

## Current Phase: Phase 4

---

## Phase 4: Hierarchy Core

**目标**: Support hierarchical topology tree and CoherenceDomain
**工期**: 3-4 weeks
**状态**: 🔄 进行中

### Tasks

#### 4.1 Hierarchy Tree Parser ✅ COMPLETE

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

---

#### 4.2 CoherenceDomain C++ Module ⏳ PENDING

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

---

#### 4.3 Domain Boundary Validation ⏳ PENDING

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

---

#### 4.4 Snoop Routing Logic ⏳ PENDING

**Files**: `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc`

**Steps**:
1. Implement `get_snoop_targets()` for broadcast/multicast
2. Support snoop fanout configuration
3. Add unit tests for snoop routing

**Acceptance Criteria**:
- [ ] Snoop messages reach all domain members
- [ ] Fanout configuration respected
- [ ] Unit tests pass

---

#### 4.5 Directory Protocol Stub ⏳ PENDING

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

---

#### 4.6 Python Hierarchy Generator ⏳ PENDING

**Files**: `scripts/topology_generator.py`

**Steps**:
1. Add `HierarchicalTopologyGenerator` class
2. Implement `add_cluster()`, `add_coherence_domain()` methods
3. Output v4.0 JSON format with `hierarchy` and `coherence_domains`
4. Add integration tests

**Acceptance Criteria**:
- [ ] HierarchicalTopologyGenerator class implemented
- [ ] Correct v4.0 JSON output with hierarchy
- [ ] Integration tests pass

---

### Phase Gate (门控条件)

- [x] 4.1 Hierarchy tree parser 完成
- [ ] 4.2 CoherenceDomain C++ 模块完成
- [ ] 4.3 Domain boundary validation 完成
- [ ] 4.4 Snoop routing logic 完成
- [ ] 4.5 Directory protocol stub 完成
- [ ] 4.6 Python hierarchy generator 完成

---

## Future Phases (预览)

### Phase 5: Protocol Bridge (待开始)

- 5.1 ProtocolBridge C++ module
- 5.2 Address translation engine
- 5.3 Protocol conversion logic
- 5.4 Cross-protocol validation
- 5.5 Python bridge config generator

### Phase 6: Multi-Cluster SoC Validation (待开始)

- 6.1 2x CPU Cluster + GPU Cluster config
- 6.2 Cross-cluster coherence validation
- 6.3 Protocol bridge integration test
- 6.4 Full SoC example (4 CPU + 1 GPU)

---

## Metadata

- **Architecture Reference**: `docs/architecture/10-topology-generation-management-system.md`
- **Specifications**: `docs/architecture/10-tgms-specifications.md`
- **Implementation Plan**: `docs/implementation/11-tgms-v4-implementation-plan.md`