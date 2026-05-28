## Why

Phase 4.4 of TGMS v4.0 implements snoop routing logic for broadcast/multicast within coherence domains. Proper snoop routing ensures all domain members receive snoop requests.

**Problem**: Without snoop routing, cache coherency cannot be maintained in multi-processor systems.

## What Changes

- Implement `get_snoop_targets()` for broadcast/multicast
- Support configurable snoop fanout
- Add fanout configuration per domain member
- Unit tests for snoop routing

## Capabilities

### New Capabilities
- `snoop-routing`: Logic for routing snoop requests to all domain members with configurable fanout

### Modified Capabilities
(None)

## Impact

- **Modified Files**: `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc`
- **New Method**: `get_snoop_targets()` with broadcast/multicast support
- **Dependencies**: Task 4.2 (CoherenceDomain C++ module)