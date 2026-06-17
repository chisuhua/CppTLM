# ADR-X.14: Mark `coherence_domains` field as stub

## Status

Accepted — 2026-06-17

## Context

The C++ `ModuleFactory::instantiateAll()` accepts a `coherence_domains` array field in JSON configs. The field is parsed by `src/core/topology_parser.cc:40-56` (which extracts the domain names) and `src/core/topology_parser.cc:106-111` `parse_hierarchy_tree_with_validation()` (which accepts the array as a parameter).

However, the `coherence_json` parameter in `parse_hierarchy_tree_with_validation` is **completely unused** in the current implementation — the function returns the hierarchy tree root without consulting the coherence domains at all. Furthermore, `module_factory.cc` has no `CoherenceDomain` instantiation logic; the only `CoherenceDomain`-related entry is the static helper `ModuleFactory::validate_domain_boundary()` at line 451-483, which is **not called from any code path**.

The net effect: 4 JSON configs in the repository (`configs/hierarchy_tree_3level.json`, `configs/apu_soc_phase7a.json`, `configs/apu_soc_phase7b.json`, `configs/apu_soc_full.json`) declare `coherence_domains` arrays with MESI / MOESI_AMD_6_STATE / NONE protocols, but these declarations have **zero runtime effect**. Users reading these configs would reasonably believe that coherence is enforced; in reality, no snoop filter, no invalidation messages, and no protocol state machines are activated.

Full implementation of `coherence_domains` would require 2-4 weeks of work:
- Implement `CoherenceDomain` instantiation in `module_factory.cc` flow
- Wire up MESI / MOESI state machines in CacheTLM
- Implement snoop filter and broadcast invalidation
- Define bridging semantics between domains
- Update tests with multi-cache scenarios

This is significantly out of scope for the current Phase (unified config emitter + `module_groups` cleanup).

## Decision

We mark the `coherence_domains` field as a **stub** for the current Phase, with the following actions:

1. **C++ side (`src/core/topology_parser.cc:106-111`):** Add a `DPRINTF(PARSER, "[STUB] ...")` log when `coherence_json` is provided but ignored. The log fires only when the field is non-empty and non-null, so it does not pollute the output of configs that don't use the field.

2. **Documentation:**
   - `configs/AGENTS.md` adds a `⚠️ STUB` note on the `coherence_domains` field description.
   - `docs/architecture/02-transaction-architecture.md` (or nearest relevant doc) gains a "Coherence Domains Stub" section.
   - `docs/user-guide/python-usage.md` notes the stub status in §3.5 (co-located with the `module_groups` → `groups` rewrite).

3. **No code is removed or behavior is changed.** Existing configs continue to load and run; the stub log is the only observable difference.

4. **No removal of `coherence_domains` from any JSON config.** The four APU configs and `hierarchy_tree_3level.json` keep their coherence declarations as forward-looking documentation of intended behavior.

## Consequences

**Positive:**
- Users are no longer misled into believing coherence is enforced
- Future implementation work has a clear scope and starting point
- The stub log helps future debugging — if a config author wonders why their `coherence_domains` declarations have no effect, the log answers the question

**Negative:**
- The stub log is a low-noise distraction in debug builds (it fires once per config load, but only when `coherence_domains` is non-empty)
- We do not validate that `coherence_domains.members` reference existing modules (a future improvement)

**Out of scope (deferred):**
- Implementing actual coherence protocol state machines
- Wiring `validate_domain_boundary` into the module factory flow
- Adding `coherence_domains` validation in `cpptlm_config/validator.py`

## Alternatives Considered

**A. Remove `coherence_domains` from JSON schema entirely.**
- Pro: Eliminates confusion.
- Con: Breaks 4 existing configs; requires migration; loses forward-looking documentation.

**B. Implement full `coherence_domains` runtime behavior.**
- Pro: Eliminates stub; makes 4 configs actually meaningful.
- Con: 2-4 weeks of work, out of scope for current Phase.

**C. Mark as stub, do nothing else (chosen).**
- Pro: Honest, minimal, preserves forward-looking intent, low cost.
- Con: Future cleanup still required.

## References

- `src/core/topology_parser.cc:40-56` — coherence domain name extraction
- `src/core/topology_parser.cc:106-111` — `parse_hierarchy_tree_with_validation()` (where stub log is added)
- `src/core/module_factory.cc:451-483` — `ModuleFactory::validate_domain_boundary()` (unused)
- `configs/hierarchy_tree_3level.json:48-73` — first config to declare `coherence_domains`
- `configs/apu_soc_phase7a.json:53-54`, `apu_soc_phase7b.json:111-115`, `apu_soc_full.json:210-214` — APU configs with `coherence_domains`
- Unified config emitter change: `openspec/changes/unified-config-emitter/`
