# Tasks: add-snoop-routing

## Phase 4.4: Snoop Routing Logic

### Task List

- [ ] 1. Implement `get_snoop_targets()` for broadcast/multicast in `CoherenceDomain`
- [ ] 2. Support configurable snoop fanout per domain
- [ ] 3. Add multicast target list management
- [ ] 4. Add unit tests for snoop routing in `test/test_snoop_routing.cc`

### Acceptance Criteria

- [ ] Snoop messages reach all domain members
- [ ] Fanout configuration respected
- [ ] Unit tests pass

### Files

- `include/core/coherence_domain.hh` (MODIFIED)
- `src/core/coherence_domain.cc` (MODIFIED)
- `test/test_snoop_routing.cc` (NEW)