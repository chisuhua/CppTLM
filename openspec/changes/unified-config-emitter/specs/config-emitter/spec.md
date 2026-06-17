# config-emitter Specification

## Purpose

Define how the Python `CxxCompatibleEmitter` expands an in-memory `TopoLayer` tree into a JSON dict that the C++ `ModuleFactory::instantiateAll()` can load. The emitter is the single source of truth for Python → C++ JSON schema translation; the C++ side accepts a fixed schema (modules, connections, groups, hierarchy, etc.) and this spec documents the Python mapping rules.

## ADDED Requirements

### Requirements

### Requirement: Emitter produces C++-compatible JSON

The `CxxCompatibleEmitter.emit()` method SHALL return a JSON-serializable Python `dict` whose top-level keys are a strict subset of: `name`, `description`, `version`, `metadata`, `modules`, `connections`, `groups`, `hierarchy`, `coherence_domains`, `extends`, `include`, `plugin`. The emitter SHALL NOT emit `module_groups`, `sublayers`, or any other key that the C++ `ModuleFactory` does not read.

#### Scenario: Basic flat layer emission
- **WHEN** `emit(TopoLayer(name="test", modules=[ModuleSpec(name="cpu0", type="CPUTLM")], connections=[]))` is called
- **THEN** the returned dict SHALL contain top-level keys: `name="test"`, `modules=[{name:"cpu0", type:"CPUTLM"}]`, `connections=[]`
- **AND** the dict SHALL NOT contain `module_groups`, `sublayers`, or any other unknown key

#### Scenario: Reserved module name detection
- **WHEN** the layer contains a module named `groups`, `modules`, `connections`, `hierarchy`, or `coherence_domains`
- **THEN** `emit()` SHALL raise `TopoEmitError` with message identifying the conflicting name

### Requirement: Python tags expand to C++ groups dict

The emitter SHALL convert each `TopoLayer.tags` entry into a `groups` entry where the key is the tag name and the value is the list of module names within that layer (and all sublayers, recursively). The expanded groups SHALL be merged into the top-level `groups` dict. A module name SHALL appear in a group's member list at most once even if the layer tree contributes it via multiple sublayers.

#### Scenario: Single tag on a single-layer root
- **WHEN** the root layer has `tags={"compute"}` and contains one module `cpu0`
- **THEN** the emitted `groups` SHALL contain `"compute": ["cpu0"]`

#### Scenario: Multiple tags on a multi-module layer
- **WHEN** the root layer has `tags={"compute", "shared"}` and contains modules `[cpu0, l1_0]`
- **THEN** the emitted `groups` SHALL contain `"compute": ["cpu0", "l1_0"]` AND `"shared": ["cpu0", "l1_0"]`

#### Scenario: Tag with sublayer aggregation
- **WHEN** the root layer has `tags={"all"}` and contains sublayers with modules `[cpu0]` and `[l1_0]`
- **THEN** the emitted `groups["all"]` SHALL equal `["cpu0", "l1_0"]` (deduplicated)

### Requirement: Sublayer nesting becomes hierarchy tree

The emitter SHALL recursively convert `TopoLayer.sublayers` into a `hierarchy` tree of the form `{"name": <layer.name>, "children": [<recursive>] }`. Leaf layers (no sublayers) SHALL emit `{"name": <layer.name>, "children": []}`. The hierarchy SHALL mirror the layer tree structure exactly.

#### Scenario: Three-level hierarchy
- **WHEN** the root layer has sublayers `[layer_a, layer_b]` and each sublayer has sublayer `[leaf_0]`
- **THEN** the emitted `hierarchy` SHALL equal `{"name": <root.name>, "children": [{"name":"layer_a","children":[{"name":"leaf_0","children":[]}]}, {"name":"layer_b","children":[{"name":"leaf_0","children":[]}]}]}`

#### Scenario: Flat single layer
- **WHEN** the root layer has no sublayers
- **THEN** the emitted `hierarchy` SHALL be `{"name": <root.name>, "children": []}`

### Requirement: Module layout coordinates propagate to JSON

The emitter SHALL include `modules[].layout` in the output for any module whose `metadata` contains a `layout` key. If `metadata["layout"]` is missing, the emitter SHALL NOT add a `layout` key (it is optional in the C++ schema).

#### Scenario: Module with explicit layout
- **WHEN** a module has `metadata = {"layout": {"x": 100, "y": 200}}`
- **THEN** the emitted module dict SHALL include `"layout": {"x": 100, "y": 200}`

#### Scenario: Module without layout
- **WHEN** a module has no `layout` in its metadata
- **THEN** the emitted module dict SHALL NOT contain a `layout` key

### Requirement: Module params propagate as-is

The emitter SHALL include `modules[].params` in the output for any module whose `params` dict is non-empty. The emitter SHALL NOT modify the `params` dict contents (it is opaque to the emitter).

#### Scenario: Module with non-empty params
- **WHEN** a module has `params = {"size": "32KB", "associativity": 4}`
- **THEN** the emitted module dict SHALL include `"params": {"size": "32KB", "associativity": 4}`

#### Scenario: Module with empty params
- **WHEN** a module has `params = {}`
- **THEN** the emitter MAY omit the `params` key (C++ side tolerates missing `params`)

### Requirement: Group placement defaults to grid

For every group emitted via tag expansion, the emitter SHALL include `"placement": "grid"` under the group key, UNLESS the originating layer explicitly overrides it via `metadata["placement"]`. Valid placement values are: `grid`, `linear`, `radial`, `circle`.

#### Scenario: Default grid placement
- **WHEN** a group is emitted from a tag and the layer has no explicit `placement` metadata
- **THEN** the group entry SHALL be `{"<tag>": {"members": [...], "placement": "grid"}}`

#### Scenario: Custom radial placement
- **WHEN** a layer has `metadata = {"placement": "radial"}` and is tagged `"compute"`
- **THEN** the emitted group SHALL be `{"compute": {"members": [...], "placement": "radial"}}`

### Requirement: Hierarchy binding validation

The emitter SHALL validate that every name appearing in the emitted `hierarchy` tree (both internal and leaf nodes) corresponds to a module in the emitted `modules` list. The emitter SHALL raise `TopoEmitError` with the offending name and a hint otherwise.

#### Scenario: Hierarchy node matches a module
- **WHEN** `hierarchy.children[0].name == "cpu0"` and `modules` contains a module named `cpu0`
- **THEN** `emit()` SHALL succeed without error

#### Scenario: Orphan hierarchy node detected
- **WHEN** `hierarchy.children[0].name == "phantom"` and no module with that name exists in `modules`
- **THEN** `emit()` SHALL raise `TopoEmitError` with message containing `"phantom"` and the hint `"hierarchy node 'phantom' has no matching module"`

#### Scenario: Cluster name as hierarchy node
- **WHEN** a `TopoLayer` with `name="cluster0"` is added as a sublayer of root
- **THEN** the emitted `hierarchy` SHALL contain a node `"cluster0"` even if no module is named `cluster0` directly, provided the validation rule allows cluster-level naming (i.e., a cluster's name appears as a hierarchy node and is treated as a structural grouping, not a module reference)

### Requirement: Coherence domains pass through with stub warning

The emitter SHALL include `coherence_domains` in the output as a verbatim copy of the source layer's `coherence_domains` list. The emitter SHALL NOT validate the members field. The C++ side currently treats `coherence_domains` as a stub (see ADR-X.14); the emitter does not enforce that limitation but does not implement runtime behavior for it either.

#### Scenario: Empty coherence domains
- **WHEN** the layer has no `coherence_domains`
- **THEN** the emitter SHALL NOT include the key in the output

#### Scenario: Populated coherence domains
- **WHEN** the layer has `coherence_domains = [{"name": "d0", "members": ["cpu0"], "protocol": "MESI"}]`
- **THEN** the emitted dict SHALL include `"coherence_domains": [{"name": "d0", "members": ["cpu0"], "protocol": "MESI"}]`
