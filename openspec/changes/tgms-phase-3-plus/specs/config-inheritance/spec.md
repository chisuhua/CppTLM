## ADDED Requirements

### Requirement: Extends field processing

The ModuleFactory SHALL support an `extends` field in configuration JSON that references a base configuration file. When present, the base configuration SHALL be loaded and merged with the current configuration.

#### Scenario: Basic extends loads base config
- **WHEN** config contains `"extends": "configs/base_mesh.json"` and base config contains `"modules": [{"name": "base_cpu", "type": "CPUSim"}]`
- **THEN** ModuleFactory SHALL load and parse the base config before processing the current config

#### Scenario: Module name matching merges params
- **WHEN** base config has module `{"name": "router", "type": "RouterTLM", "params": {"mesh_x": 4, "mesh_y": 4}}` and current config has `{"name": "router", "params": {"mesh_x": 8}}`
- **THEN** resulting config for "router" SHALL have merged params `{"mesh_x": 8, "mesh_y": 4}` (current overrides base)

#### Scenario: New modules in child config are appended
- **WHEN** base config has modules [{"name": "existing"}] and current config has modules [{"name": "new_module"}]
- **THEN** resulting module list SHALL contain both "existing" and "new_module"

#### Scenario: Connections are appended not merged
- **WHEN** base config has connections [{"src": "a", "dst": "b"}] and current config has connections [{"src": "c", "dst": "d"}]
- **THEN** resulting connection list SHALL contain all four connections in order

#### Scenario: Missing base file produces error
- **WHEN** config contains `"extends": "configs/nonexistent.json"` and file does not exist
- **THEN** ModuleFactory::instantiateAll SHALL return false and print error message

### Requirement: Deep merge semantics

The merge operation SHALL handle nested structures with override semantics.

#### Scenario: Module groups are merged by group name
- **WHEN** base config has `"groups": {"cluster_a": ["r0", "r1"]}` and current config has `"groups": {"cluster_b": ["r2"]}`
- **THEN** resulting groups SHALL contain both "cluster_a" and "cluster_b"

#### Scenario: Same group name in base and child merges member lists
- **WHEN** base config has `"groups": {"shared": ["a", "b"]}` and current config has `"groups": {"shared": ["c"]}`
- **THEN** resulting group "shared" SHALL contain ["a", "b", "c"]

#### Scenario: Top-level scalar fields are overridden
- **WHEN** base config has `"topology": "mesh"` and current config has `"topology": "ring"`
- **THEN** resulting config SHALL have `"topology": "ring"`