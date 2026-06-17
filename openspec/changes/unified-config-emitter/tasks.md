# Unified Config Emitter — Tasks

## 1. TopoLayer extension (Phase 1)

- [x] 1.1 Add `tags: set = field(default_factory=set)` to `TopoLayer` dataclass in `cpptlm/topo/layer.py`
- [x] 1.2 Add `tag(*tags) -> TopoLayer` chainable method to `TopoLayer`
- [x] 1.3 Add `clone(prefix: str = None) -> TopoLayer` method (deep copy with optional name prefixing)
- [x] 1.4 Add `layout_grid(dx, dy, x_offset=0, y_offset=0) -> TopoLayer` method (auto-assign `layout.x/y` to all modules recursively)
- [x] 1.5 Update `cpptlm/topo/__init__.py` to export new methods
- [x] 1.6 Add 5+ new test cases to `test/python/test_topo_layer.py` covering tag chainability, clone independence, layout_grid coordinates, recursive sublayer coverage
- [x] 1.7 Run `pytest test/python/test_topo_layer.py` and confirm all 19+5 = 24+ tests pass

## 2. CxxCompatibleEmitter core (Phase 2)

- [x] 2.1 Create `cpptlm/topo/emitter.py` with `TopoEmitError` exception class
- [x] 2.2 Implement `CxxCompatibleEmitter.emit(root: TopoLayer) -> dict` entry point
- [x] 2.3 Implement `_collect_modules(root)` — flatten all sublayers into flat module list with reserved-name detection
- [x] 2.4 Implement `_collect_connections(root)` — flatten all connections (no expansion; group: prefix already in place)
- [x] 2.5 Implement `_collect_groups(root)` — Python tags → `groups` dict with `placement: "grid"` default
- [x] 2.6 Implement `_collect_hierarchy(root)` — recursive sublayer → `hierarchy` tree
- [x] 2.7 Implement `_validate_hierarchy_binding(root)` — Python-side check that hierarchy node names map to modules (or are cluster-level structural names)
- [x] 2.8 Implement `ALLOWED_KEYS` whitelist enforcement (top-level keys only from C++ schema)
- [x] 2.9 Update `cpptlm/topo/__init__.py` to export `CxxCompatibleEmitter` and `TopoEmitError`
- [x] 2.10 Create `test/python/test_topo_emitter.py` with 10+ cases: basic emit, tag→groups, sublayer→hierarchy, layout propagation, reserved name detection, hierarchy binding validation, placement default, coherence_domains passthrough

## 3. cluster-factory library (Phase 3)

- [x] 3.1 Create `cpptlm/library/__init__.py` with public exports
- [x] 3.2 Create `cpptlm/library/standard.py` with `cpu_l1_cluster(idx, n_cores=2, l1_size="32KB") -> TopoLayer`
- [x] 3.3 Add `memory_cluster(name, n_banks=1) -> TopoLayer` to `standard.py`
- [x] 3.4 Add `crossbar_cluster(n_ports=4, name="xbar") -> TopoLayer` to `standard.py`
- [x] 3.5 Create `cpptlm/library/interconnect.py` with `mesh_cluster(rows, cols, name_prefix="noc") -> TopoLayer` (XY-routed routers)
- [x] 3.6 Add `ring_cluster(n_nodes, name_prefix="ring") -> TopoLayer` to `interconnect.py`
- [x] 3.7 Create `cpptlm/library/soc.py` with `SoC` class — fluent API: `add_cluster`, `add_module`, `connect`, `connect_group`, `tag`, `save`
- [x] 3.8 Implement `SoC.save(path)` that calls `CxxCompatibleEmitter.emit()` and writes JSON
- [x] 3.9 Implement reserved-name collision check in all factory functions
- [x] 3.10 Update `cpptlm/__init__.py` to re-export `cpu_l1_cluster`, `mesh_cluster`, `SoC` (top-level convenience)
- [x] 3.11 Create `test/python/test_library.py` with 6+ cases: each factory produces expected modules/connections, SoC composes multi-cluster, connect_group emits group: prefix, save produces valid JSON, reserved name raises

## 4. Dead-link fixes + module_groups cleanup (Phase 4)

- [x] 4.1 Wrap `from scripts.topology_generator import TopologyGenerator` in `cpptlm_config/topology_adapter.py` with try/except + DeprecationWarning
- [x] 4.2 Wrap same import in `cpptlm/config/topologies.py` with try/except + warning
- [x] 4.3 Wrap same import in `cpptlm/config/generator.py` with try/except + warning
- [x] 4.4 Add `import warnings; warnings.warn(DeprecationWarning, stacklevel=2)` at top of `cpptlm_config/__init__.py`
- [x] 4.5 Migrate `configs/soc_cluster_a.json`: convert `module_groups` array to `groups` dict format
- [x] 4.6 Verify `configs/soc_cluster_b.json` `extends` merge still produces correct output after base change
- [x] 4.7 Migrate `examples/demo_configs/soc_cluster.json`: convert `module_groups` array to `groups` dict
- [x] 4.8 Verify `examples/demo_configs/dual_cluster_soc.json` `extends` merge is correct
- [x] 4.9 Add DeprecationWarning comment to `module_groups` field in `cpptlm_config/models.py` ConfigSchema
- [x] 4.10 Add `warnings.warn(DeprecationWarning)` to `cpptlm_config/builder.py` when emitting `module_groups` (still emit, for backward compat)
- [x] 4.11 Update `cpptlm_config/AGENTS.md` with Legacy/Deprecation status section
- [x] 4.12 Move `configs/include_chain_demo.json` to `docs-archived/dead-configs-2026-q2/include_chain_demo.json` (or appropriate subdir)
- [x] 4.13 Run C++ smoke test: load `soc_cluster_b.json` and `dual_cluster_soc.json` with `cpptlm_sim` and confirm successful run
- [x] 4.14 Run `cmake --build build && ./build/bin/cpptlm_tests` to confirm 610/610 pass

## 5. coherence_domains stub marking (Phase 6)

- [x] 5.1 Add stub warning log to `src/core/topology_parser.cc:106-111` in `parse_hierarchy_tree_with_validation`
- [x] 5.2 Create `docs/adr/ADR-X.14-coherence-domains-stub.md` (Status: Accepted, Date: 2026-06-17)
- [x] 5.3 Update `configs/AGENTS.md` with "⚠️ STUB" note on `coherence_domains` field
- [x] 5.4 Update `docs/architecture/02-transaction-architecture.md` (or nearest relevant doc) to mark stub
- [x] 5.5 Update `docs/user-guide/python-usage.md` with stub note (separate from the §3.5 module_groups rewrite)
- [x] 5.6 Verify `cmake --build build` still compiles with new log line
- [x] 5.7 Verify `./build/bin/cpptlm_tests` still 610/610 (no behavior change, only DPRINTF log)

## 6. Examples and documentation (Phase 7)

- [x] 6.1 Create `examples/generate_via_emitter.py` (~80 lines): end-to-end demo of SoC + cluster + tag + save
- [x] 6.2 Run `examples/generate_via_emitter.py` to produce `configs/example_emitter_soc.json` (~50 lines)
- [x] 6.3 Verify the generated JSON loads in C++ smoke test (load + parse + run, no error)
- [x] 6.4 Create `cpptlm/AGENTS.md` (~30 lines, currently missing) — package overview, link to library/ and topo/ submodules
- [x] 6.5 Rewrite `docs/user-guide/python-usage.md` §3.5 (module_groups → groups) with full example using `groups` dict and `group:` prefix
- [x] 6.6 Update `configs/AGENTS.md` core schema example: replace `module_groups` with `groups` dict form
- [x] 6.7 Add archive note to `configs/AGENTS.md` explaining `include_chain_demo.json` is moved to `docs-archived/`
- [x] 6.8 Verify `grep -r "module_groups" docs/ configs/ examples/demo_configs/` returns no stale references (except in deprecation comments)

## 7. Final validation (Phase 8)

- [x] 7.1 Run `cmake --build build -j$(nproc)` — must succeed with zero warnings
- [x] 7.2 Run `./build/bin/cpptlm_tests` — must be 610/610 pass
- [x] 7.3 Run `pytest test/python/ -v` — all old 207 + new ~25 cases pass
- [x] 7.4 Run `./scripts/build/format.sh --check` — clang-format clean
- [x] 7.5 Run `./scripts/test/docs_sync_check.sh --strict` — 365/365 paths valid
- [x] 7.6 Run end-to-end: `python3 examples/generate_via_emitter.py && ./build/bin/cpptlm_sim configs/example_emitter_soc.json` — no errors
- [x] 7.7 Run C++ smoke tests for migrated configs: `soc_cluster_b.json`, `dual_cluster_soc.json` — successful load
- [x] 7.8 Verify `import cpptlm_config` triggers `DeprecationWarning` but does not raise
- [x] 7.9 Verify no `module_groups` references remain in active code paths (only in `cpptlm_config/builder.py` deprecation comment)
- [x] 7.10 Commit all changes with structured commit message referencing `unified-config-emitter` change
