# Tasks: add-hierarchy-generator

## Phase 4.6: Python Hierarchy Generator

### Task List

- [ ] 1. Add `HierarchicalTopologyGenerator` class to `scripts/topology_generator.py`
- [ ] 2. Implement `add_cluster(name, children)` method
- [ ] 3. Implement `add_coherence_domain(name, protocol, members)` method
- [ ] 4. Implement `to_v4_json()` output method
- [ ] 5. Output v4.0 JSON format with `hierarchy` and `coherence_domains`
- [ ] 6. Add integration tests in `test/python/test_hierarchy_generator.py`

### Acceptance Criteria

- [ ] HierarchicalTopologyGenerator class implemented
- [ ] Correct v4.0 JSON output with hierarchy
- [ ] Integration tests pass

### Files

- `scripts/topology_generator.py` (MODIFIED)
- `test/python/test_hierarchy_generator.py` (NEW)