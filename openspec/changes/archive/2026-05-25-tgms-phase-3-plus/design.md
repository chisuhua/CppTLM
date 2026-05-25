## Context

The CppTLM framework has completed Phase 0-7, establishing core TLM simulation infrastructure. However, analysis in ARCH-012 identified 5 critical ModuleFactory defects and significant capability gaps across configuration expression, parameter systems, port management, and toolchain completeness. Phase 3+ implements four sub-phases: defect fixes (3.1), port management (3.2), configuration enhancement (3.3), and topology validation (3.4).

The design must maintain backward compatibility with existing JSON configurations while adding new capabilities. All changes must pass existing test suites (434/434 tests).

## Goals / Non-Goals

**Goals:**
- Fix all 5 known ModuleFactory defects (DEF-01 to DEF-05)
- Implement port type registration and compatibility checking system
- Add configuration inheritance (`extends`) and variable reference (`${}`) syntax
- Implement dynamic parameter derivation and validation framework
- Create topology analysis toolchain (load analysis, path tracing, lint)
- Maintain 100% backward compatibility with existing configurations

**Non-Goals:**
- Do not implement full gem5-style param system in one phase
- Do not modify existing working module implementations (CacheTLM, CrossbarTLM, etc.)
- Do not add visualization rendering - only integration points for external tools

## Decisions

### Decision 1: Port Type System Architecture

**Choice**: Use static `get_port_specs()` method on each module class instead of runtime registration.

**Rationale**: Statically declared port specs allow compile-time validation and avoid runtime registration complexity. The alternative (runtime registration via `registerPort()`) adds initialization order dependencies.

### Decision 2: Configuration Inheritance Merge Strategy

**Choice**: Deep merge with override semantics for modules by name, append for connections.

**Rationale**: This matches user expectations - base config provides common elements, override adds/modifies specific parts. Connections are append-only to allow extending topology without overriding base connections.

### Decision 3: Parameter Derivation Expression Format

**Choice**: Use string-based expression syntax `"(condition) ? value1 : value2"` stored in ParamRule.

**Rationale**: Simple ternary expressions cover 90% of derivation needs without full expression parser complexity. Complex cases can be handled by explicit parameter declaration.

### Decision 4: ModuleGroup Wildcard Resolution Timing

**Choice**: Resolve wildcards at `resolve()` call time against current registered instances.

**Rationale**: Late binding allows flexibility - instances can be registered in any order, and wildcard resolution always reflects current state. Alternative (early binding at `define()`) would require re-resolution after registration changes.

### Decision 5: Port Alias Resolution Location

**Choice**: Resolve aliases in `parsePortSpec()` before connection processing.

**Rationale**: Centralizing alias resolution in one place avoids scattered `resolve_port_alias()` calls throughout the codebase. Aliases are resolved once at parse time.

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| Port type system introduces ODR violations with inline static methods | High | Use inline functions and ensure header-only PortSpec structs |
| Config extends creates circular reference | Medium | Limit extends depth to 3, detect cycles before processing |
| Wildcard expansion performance with large instance counts | Low | Cache resolved results, invalidate on new registration |
| Bundle type checking adds instantiation overhead | Low | Perform checks only during bind phase, not per-packet |
| Python config script eval security | High | Use restricted expression parser, no Python eval |

## Migration Plan

1. **Phase 3.1**: Deploy defect fixes first - these are self-contained and don't affect other components
2. **Phase 3.2**: Add port type declarations to RouterTLM and NICTLM as reference implementation
3. **Phase 3.3**: Implement extends/$ref - existing configs work without changes, new syntax is optional
4. **Phase 3.4**: Add toolchain scripts - can be deployed independently of core changes

No migration needed for existing configs - all Phase 3+ features are additive.

## Open Questions

1. Should port aliases be module-scoped or global? (Currently designed as config-global)
2. Should parameter derivation expressions support external function references?
3. Should we add deprecation warnings for configs using old patterns?