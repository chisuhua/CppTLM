# config-lint Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: Port index consistency checking

The ConfigLinter SHALL check that Router-to-Router connections specify port indices on both ends.

#### Scenario: Missing port index warning
- **WHEN** linting config with connection "router_0_0" -> "router_0_1" (no port index)
- **THEN** linter SHALL report WARN level issue: "Router-to-Router connection missing port index"

#### Scenario: Present port index passes
- **WHEN** linting config with connection "router_0_0.0" -> "router_0_1.2" (both have port index)
- **THEN** no warning SHALL be generated for this connection

### Requirement: Module naming convention checking

The ConfigLinter SHALL verify that modules follow naming conventions based on their type.

#### Scenario: RouterTLM naming convention enforcement
- **WHEN** linting module {"name": "my_router", "type": "RouterTLM"}
- **THEN** linter SHALL report WARN level issue: "RouterTLM should use router_x_y naming"

#### Scenario: Correct naming passes
- **WHEN** linting module {"name": "router_0_1", "type": "RouterTLM"}
- **THEN** no warning SHALL be generated

### Requirement: Connection redundancy detection

The ConfigLinter SHALL detect duplicate connections that may indicate configuration errors.

#### Scenario: Duplicate connection detection
- **WHEN** config has two connections with identical src/dst pairs
- **THEN** linter SHALL report WARN level issue about redundant connection

#### Scenario: Reverse duplicate detection
- **WHEN** config has connection "a->b" and also "b->a"
- **THEN** linter MAY report INFO or WARN about bidirectional redundant connection

### Requirement: Params completeness checking

The ConfigLinter SHALL verify that required params are present for each module type.

#### Scenario: Missing required param warning
- **WHEN** config has RouterTLM without "mesh_x" param
- **THEN** linter SHALL report WARN level issue: "RouterTLM missing required param: mesh_x"

#### Scenario: All required params present passes
- **WHEN** config has RouterTLM with mesh_x, mesh_y, node_x, node_y
- **THEN** no warning SHALL be generated for params completeness

### Requirement: Config field semantic enforcement

The ConfigLinter SHALL enforce the semantic boundary between the `params` and `config` fields on every module entry: `params` is the canonical location for module configuration (a JSON object), and `config` is reserved for an external configuration file path (a JSON string).

#### Scenario: Config used as parameter dict reports LINT005 error

- **WHEN** linting a module that contains `"config": { "pattern": "SEQUENTIAL", "num_requests": 10000 }`
- **THEN** linter SHALL report a LINT005 ERROR with message "module 'X' uses 'config' for module configuration; use 'params' instead (LINT005)"
- **AND** the error SHALL include a remediation hint pointing to the `params` field convention

#### Scenario: Config used as file path passes lint

- **WHEN** linting a module that contains `"config": "/etc/cpputlm/cpu_cluster.json"`
- **THEN** no LINT005 error SHALL be generated
- **AND** the file path SHALL be loaded by the ModuleFactory SimModule path (existing behavior)

#### Scenario: Module with neither params nor config passes lint

- **WHEN** linting a module that contains neither `params` nor `config`
- **THEN** no LINT005 error SHALL be generated
- **AND** the module SHALL be allowed to use module-level defaults

#### Scenario: Module with only params passes lint

- **WHEN** linting a module that contains `"params": { ... }` and no `config`
- **THEN** no LINT005 error SHALL be generated
- **AND** the params SHALL be consumed by `set_config()` (the main SimObject path)

#### Scenario: Both params and config set on same module

- **WHEN** linting a module that contains both `"params": { ... }` and `"config": { ... }` (config as object)
- **THEN** linter SHALL report LINT005 ERROR (config as object is invalid regardless of params presence)

#### Scenario: config null value passes lint

- **WHEN** linting a module that contains `"config": null` or omits `config` entirely
- **THEN** no LINT005 error SHALL be generated
- **AND** the module SHALL be treated as having no external config file reference

