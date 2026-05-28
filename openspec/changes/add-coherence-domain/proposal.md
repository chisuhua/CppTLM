## Why

Phase 4.2 of TGMS v4.0 requires a `CoherenceDomain` class to manage hierarchical coherence domains. This is essential for supporting multi-cluster SoC with cross-cluster coherence validation (Phase 6).

**Problem**: Without `CoherenceDomain`, the system cannot properly track which processors/shareability domain a memory transaction belongs to, leading to incorrect snoop routing and coherence violations.

## What Changes

- Create new `CoherenceDomain` class inheriting from `SimObject`
- Implement protocol support: MESI, MOESI
- Add member management (processors, caches in domain)
- Implement snoop fanout configuration
- Directory-based home node lookup for distributed coherence

## Capabilities

### New Capabilities
- `coherence-domain`: Core coherence domain abstraction with protocol support, member management, and home node lookup

### Modified Capabilities
(None - this is a new capability)

## Impact

- **New File**: `include/core/coherence_domain.hh`
- **New Methods**: `set_protocol()`, `set_members()`, `set_snoop_fanout()`, `is_member()`, `get_snoop_targets()`, `lookup_home_node()`
- **Protocol Support**: MESI, MOESI
- **Integration**: ModuleFactory (Step 0.5)
- **Dependencies**: Task 4.1 (Hierarchy tree parser) ✅ Complete