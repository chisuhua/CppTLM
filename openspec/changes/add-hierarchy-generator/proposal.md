## Why

Phase 4.6 of TGMS v4.0 adds Python hierarchy generator support. The `HierarchicalTopologyGenerator` class enables programmatic generation of v4.0 JSON configurations with hierarchy and coherence domains.

**Problem**: Manual JSON editing is error-prone and doesn't scale for complex hierarchical topologies.

## What Changes

- Add `HierarchicalTopologyGenerator` class to `scripts/topology_generator.py`
- Implement `add_cluster()`, `add_coherence_domain()` methods
- Output v4.0 JSON format with `hierarchy` and `coherence_domains`
- Integration tests for generator

## Capabilities

### New Capabilities
- `hierarchy-generator`: Python class for programmatic generation of hierarchical topology configurations

### Modified Capabilities
(None)

## Impact

- **Modified Files**: `scripts/topology_generator.py`
- **New Class**: `HierarchicalTopologyGenerator`
- **New Methods**: `add_cluster()`, `add_coherence_domain()`
- **Dependencies**: Tasks 4.1-4.3 (hierarchy parser, coherence domain, boundary validation)