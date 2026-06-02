# Legacy Modules (Port-based communication)

> ⚠️ **DEPRECATED** as of CppTLM v2.1

These modules use the legacy PortPair/PortManager communication model.
They are kept for backward compatibility with v2.0 projects only.

**New development should use ChStream/StreamAdapter pattern** and place
new modules in `../include/tlm/` instead.

## Migration Map (v2.0 → v2.1)

| Legacy Module | Replacement |
|---------------|-------------|
| `CrossbarV2`  | `CrossbarTLM` (in `include/tlm/crossbar_tlm.hh`) |
| `CacheV2`     | `CacheTLM` (in `include/tlm/cache_tlm.hh`) |
| `MemoryV2`    | `MemoryTLM` (in `include/tlm/memory_tlm.hh`) |

## Why Deprecated?

- **Architecture**: ChStream/StreamAdapter pattern is the v2.1 standard
- **Performance**: New TLM modules are cycle-accurate and port-composable
- **Testing**: Legacy modules have known compatibility issues with v2.1 plugins

## Status

- ❌ **No new features** will be added
- ✅ **Critical bug fixes only** will be merged
- 📦 **Will be removed** in v3.0 (planned)
