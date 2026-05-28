## Why

Phase 4.3 of TGMS v4.0 requires domain boundary validation to prevent invalid cross-domain connections. This prevents coherence violations and ensures protocol bridge is used for inter-domain communication.

**Problem**: Without boundary validation, cross-domain connections without ProtocolBridge can cause incorrect coherence behavior and data integrity issues.

## What Changes

- Add `validate_domain_boundary()` function to ModuleFactory
- Reject cross-domain connections without ProtocolBridge
- Log WARNING/ERROR for boundary violations
- Integrate validation in ModuleFactory Step 3

## Capabilities

### New Capabilities
- `domain-boundary-validation`: Validation of cross-domain connections to ensure proper ProtocolBridge usage

### Modified Capabilities
(None)

## Impact

- **Modified Files**: `src/core/module_factory.cc`, `include/core/coherence_domain.hh`
- **New Function**: `validate_domain_boundary()`
- **Integration**: ModuleFactory Step 3
- **Dependencies**: Task 4.2 (CoherenceDomain C++ module)