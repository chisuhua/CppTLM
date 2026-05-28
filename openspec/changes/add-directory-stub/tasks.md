# Tasks: add-directory-stub

## Phase 4.5: Directory Protocol Stub

### Task List

- [ ] 1. Create `Directory` class in `include/core/directory.hh`
- [ ] 2. Implement `lookup_home_node(uint64_t addr)` with address mapping
- [ ] 3. Support directory entry states: M (Modified), O (Owned), S (Shared)
- [ ] 4. Add basic directory entry data structure
- [ ] 5. Add unit tests in `test/test_directory.cc`

### Acceptance Criteria

- [ ] Directory class created
- [ ] Home node correctly determined for address
- [ ] Unit tests pass

### Files

- `include/core/directory.hh` (NEW)
- `src/core/directory.cc` (NEW)
- `test/test_directory.cc` (NEW)