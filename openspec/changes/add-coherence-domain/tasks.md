# Tasks: add-coherence-domain

## Phase 4.2: CoherenceDomain C++ Module

### Task List

- [ ] 1. Create `CoherenceDomain` class inheriting from `SimObject` in `include/core/coherence_domain.hh`
- [ ] 2. Implement `set_protocol(Protocol)` method (MESI, MOESI support)
- [ ] 3. Implement `set_members(std::vector<std::string>)` method
- [ ] 4. Implement `set_snoop_fanout(int)` method
- [ ] 5. Implement `is_member(const std::string& id)` method
- [ ] 6. Implement `get_snoop_targets()` method for broadcast
- [ ] 7. Implement `lookup_home_node(uint64_t addr)` using directory
- [ ] 8. Integrate with ModuleFactory (Step 0.5)
- [ ] 9. Add unit tests in `test/test_coherence_domain.cc`

### Acceptance Criteria

- [ ] CoherenceDomain class created with all specified methods
- [ ] Protocol support: MESI, MOESI
- [ ] Directory-based home node lookup working
- [ ] Unit tests pass

### Files

- `include/core/coherence_domain.hh` (NEW)
- `src/core/coherence_domain.cc` (NEW)
- `test/test_coherence_domain.cc` (NEW)
- `src/core/module_factory.cc` (MODIFIED)