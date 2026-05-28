## Why

Phase 4.5 of TGMS v4.0 creates a Directory protocol stub for home node lookup. The Directory tracks which nodes have copies of data in a distributed memory system.

**Problem**: Without Directory, home node lookup for coherence transactions is not possible, breaking distributed coherence protocols.

## What Changes

- Create `Directory` class
- Implement `lookup_home_node()` with address mapping
- Support directory entry states: M (Modified), O (Owned), S (Shared)
- Unit tests for directory functionality

## Capabilities

### New Capabilities
- `directory-protocol-stub`: Directory-based home node lookup with entry state tracking

### Modified Capabilities
(None)

## Impact

- **New Files**: `include/core/directory.hh`, `src/core/directory.cc`
- **New Class**: `Directory`
- **Methods**: `lookup_home_node()`, entry state management
- **Dependencies**: Task 4.2 (CoherenceDomain C++ module)