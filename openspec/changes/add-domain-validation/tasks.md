# Tasks: add-domain-validation

## Phase 4.3: Domain Boundary Validation

### Task List

- [ ] 1. Add `validate_domain_boundary()` function in ModuleFactory
- [ ] 2. Check for ProtocolBridge when cross-domain connection detected
- [ ] 3. Reject connection if no ProtocolBridge present
- [ ] 4. Log WARNING for boundary violations
- [ ] 5. Log ERROR for invalid configuration
- [ ] 6. Add unit tests in `test/test_domain_boundary.cc`

### Acceptance Criteria

- [ ] Cross-domain connections without bridge are rejected
- [ ] WARNING/ERROR logged for boundary violations
- [ ] Unit tests pass

### Files

- `src/core/module_factory.cc` (MODIFIED)
- `include/core/coherence_domain.hh` (MODIFIED)
- `test/test_domain_boundary.cc` (NEW)