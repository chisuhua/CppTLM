# cluster-factory Specification

## Purpose
TBD - created by archiving change unified-config-emitter. Update Purpose after archive.
## Requirements
### Requirement: Library exports standard cluster factories

The `cpptlm.library` package SHALL export the following factory functions, each returning a `TopoLayer`:

- `cpu_l1_cluster(idx: int, n_cores: int = 2, l1_size: str = "32KB") -> TopoLayer`
- `memory_cluster(name: str, n_banks: int = 1) -> TopoLayer`
- `crossbar_cluster(n_ports: int = 4, name: str = "xbar") -> TopoLayer`
- `mesh_cluster(rows: int, cols: int, name_prefix: str = "noc") -> TopoLayer`
- `ring_cluster(n_nodes: int, name_prefix: str = "ring") -> TopoLayer`

Each factory SHALL create a `TopoLayer` whose `name` and module names are prefixed with the cluster's identifier (e.g., `cpu_l1_cluster(0)` produces `name="cluster0"` and module names like `cluster0_cpu0`, `cluster0_l1`).

#### Scenario: Default cpu_l1_cluster
- **WHEN** `cpu_l1_cluster(idx=0, n_cores=2)` is called
- **THEN** the returned layer SHALL have `name="cluster0"`
- **AND** the layer SHALL contain 3 modules: `cluster0_cpu0`, `cluster0_cpu1`, `cluster0_l1` (all CPUTLM except the L1 which is CacheTLM)
- **AND** the layer SHALL contain 2 internal connections: `cluster0_cpu0 → cluster0_l1` and `cluster0_cpu1 → cluster0_l1`, both with `latency=1`

#### Scenario: Default memory_cluster
- **WHEN** `memory_cluster(name="mem_bank", n_banks=1)` is called
- **THEN** the returned layer SHALL have `name="mem_bank"`
- **AND** the layer SHALL contain 1 MemoryTLM module named `mem_bank_mem0`

#### Scenario: Crossbar cluster with custom port count
- **WHEN** `crossbar_cluster(n_ports=8, name="xbar8")` is called
- **THEN** the returned layer SHALL contain 1 CrossbarTLM module named `xbar8`
- **AND** the layer's `metadata["port_count"]` SHALL equal `8`

#### Scenario: Mesh cluster with non-square dimensions
- **WHEN** `mesh_cluster(rows=3, cols=2, name_prefix="noc")` is called
- **THEN** the returned layer SHALL contain 6 RouterTLM modules named `noc_router_0_0`, `noc_router_0_1`, `noc_router_1_0`, `noc_router_1_1`, `noc_router_2_0`, `noc_router_2_1`
- **AND** the layer SHALL contain 7 XY-routed connections (3 horizontal + 2 vertical + 2 wraparound optional)

### Requirement: SoC orchestrator composes clusters and modules

The `cpptlm.library.soc.SoC` class SHALL provide a fluent API for assembling a full topology:

- `add_cluster(layer: TopoLayer) -> SoC` (chainable)
- `add_module(name: str, type: str, **params) -> SoC` (chainable)
- `connect(src: str, dst: str, latency: int = 1) -> SoC` (chainable, literal name)
- `connect_group(tag: str, dst: str, latency: int = 1) -> SoC` (chainable, expands to `group:<tag>` reference)
- `tag(name: str) -> SoC` (chainable, applies a tag to the most recently added cluster or module)
- `save(path: str) -> None` (writes emitted JSON via `CxxCompatibleEmitter`)

`SoC.save(path)` SHALL call `CxxCompatibleEmitter.emit()` internally and write the result to `path` as a JSON file with `indent=2, ensure_ascii=False`.

#### Scenario: Add two clusters and a crossbar
- **WHEN** `SoC("my_soc")` is created and `add_cluster(cpu_l1_cluster(0))` then `add_cluster(cpu_l1_cluster(1))` then `add_module("xbar", "CrossbarTLM")` is called
- **THEN** the SoC's root layer SHALL contain 2 sublayers (the two clusters) and 1 direct module (`xbar`)

#### Scenario: connect_group emits group: prefix
- **WHEN** `SoC.connect_group("compute", "xbar.0", latency=5)` is called after tagging a cluster `"compute"`
- **THEN** the root layer's connections SHALL include `ConnectionSpec(src="group:compute", dst="xbar.0", latency=5)`

#### Scenario: save() produces valid JSON
- **WHEN** `SoC("my_soc").add_cluster(cpu_l1_cluster(0)).save("/tmp/test.json")` is called
- **THEN** `/tmp/test.json` SHALL exist
- **AND** its contents SHALL be valid JSON
- **AND** the JSON SHALL be loadable by `CxxCompatibleEmitter` round-trip (i.e., a second `SoC` constructed from the same `TopoLayer` data would produce the same output)

### Requirement: Cluster clone with prefix renaming

The `TopoLayer.clone(prefix: str = None) -> TopoLayer` method SHALL return a deep copy of the layer with all module names re-prefixed. If `prefix` is given, every module name in the clone SHALL be `<prefix>_<original_name>`. If `prefix` is `None`, the clone SHALL keep the original names and only deep-copy the structure.

The clone SHALL preserve: `tags`, `connections`, `coherence_domains`, `metadata`, `sublayers` (recursively). Module `params` and `metadata` SHALL also be deep-copied (no shared references with the original).

#### Scenario: Clone with prefix
- **WHEN** `cluster = cpu_l1_cluster(0)` is cloned with `prefix="cluster1"`
- **THEN** the clone's `name` SHALL be `"cluster1"`
- **AND** the clone SHALL contain modules `cluster1_cluster0_cpu0`, `cluster1_cluster0_cpu1`, `cluster1_cluster0_l1`
- **AND** modifying the clone's module `params` SHALL NOT affect the original cluster's module `params`

#### Scenario: Clone without prefix
- **WHEN** `cluster = cpu_l1_cluster(0)` is cloned with `prefix=None`
- **THEN** the clone SHALL have the same module names as the original
- **AND** modifying the clone's `tags` SHALL NOT affect the original's `tags`

#### Scenario: Clone preserves sublayers
- **WHEN** a layer with 2 sublayers is cloned
- **THEN** the clone SHALL also have 2 sublayers, each independently a deep copy

### Requirement: Auto-computed layout grid

The `TopoLayer.layout_grid(dx: int, dy: int, x_offset: int = 0, y_offset: int = 0) -> TopoLayer` method SHALL compute and assign `layout.x/y` to every module in the layer (recursively into sublayers). The layout SHALL arrange modules in a grid of `dx` columns × `dy` rows, with each cell `100×100` units wide, starting at `(x_offset, y_offset)`.

The method SHALL be chainable (return `self`).

#### Scenario: 4 modules in 2×2 grid
- **WHEN** a layer with 4 modules calls `layout_grid(dx=2, dy=2)`
- **THEN** the modules SHALL receive layouts: `(0, 0)`, `(100, 0)`, `(0, 100)`, `(100, 100)` (in module order)

#### Scenario: Offset grid
- **WHEN** a layer with 2 modules calls `layout_grid(dx=2, dy=1, x_offset=500, y_offset=300)`
- **THEN** the modules SHALL receive layouts: `(500, 300)`, `(600, 300)`

#### Scenario: Grid applied recursively
- **WHEN** a root layer with 2 sublayers (each with 2 modules) calls `layout_grid(dx=2, dy=1)`
- **THEN** all 4 modules (across both sublayers) SHALL have `layout.x/y` assigned

### Requirement: cluster tag propagates to all contained modules

When a user calls `SoC.tag("compute")` after `add_cluster(...)`, the tag SHALL be applied to the most recently added cluster, AND propagated to all modules in that cluster's hierarchy for the purpose of `CxxCompatibleEmitter` tag expansion. The tag SHALL NOT affect modules in other clusters.

#### Scenario: Tag scoped to most recent cluster
- **WHEN** two clusters are added, then `tag("compute")` is called
- **THEN** only the second cluster's modules SHALL be in the emitted `groups["compute"]` list
- **AND** the first cluster's modules SHALL NOT appear in `groups["compute"]`

### Requirement: Reserved module names enforcement

The library factory functions SHALL NOT produce module names that collide with reserved C++ schema keys: `groups`, `modules`, `connections`, `hierarchy`, `coherence_domains`, `name`, `description`, `extends`, `include`, `plugin`, `version`, `metadata`. If a user-supplied parameter would cause such a collision, the factory SHALL raise `ValueError` immediately.

#### Scenario: Reserved name detection in factory
- **WHEN** `cpu_l1_cluster(idx=0)` is called and the resulting module name `cluster0_modules` would collide with reserved `modules`
- **THEN** the factory SHALL detect this and raise `ValueError("reserved module name 'modules'")` during the call

