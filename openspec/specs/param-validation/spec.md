# param-validation Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: Parameter range validation

The ModuleFactory SHALL validate numeric parameter values against declared ranges during configuration processing.

#### Scenario: Valid range passes validation
- **WHEN** RouterTLM config has mesh_x=4, mesh_y=4 (within valid range 1-16)
- **THEN** validation SHALL pass without error

#### Scenario: Below range value fails validation
- **WHEN** RouterTLM config has mesh_x=0 (below valid range 1-16)
- **THEN** validation SHALL fail with "[PARAM ERROR] mesh dimensions must be >= 1"

#### Scenario: Above range value produces warning
- **WHEN** RouterTLM config has mesh_x=32 (above valid range 1-16)
- **THEN** validation SHALL fail with "[PARAM ERROR] mesh dimensions must be <= 16" or similar

#### Scenario: vc_count range enforcement
- **WHEN** RouterTLM config has vc_count=16 (above valid range 1-8)
- **THEN** validation SHALL fail with "[PARAM ERROR] vc_count must be in [1, 8]"

### Requirement: Parameter constraint declarations

Modules SHALL be able to declare inter-parameter constraints that MUST be satisfied.

#### Scenario: Topology size constraint
- **WHEN** RouterTLM declares constraint "node_x < mesh_x && node_y < mesh_y" with message "node coordinates must be within mesh bounds"
- **THEN** config with node_x=5, mesh_x=4 SHALL fail validation with the constraint message

#### Scenario: Constraint satisfied passes
- **WHEN** RouterTLM constraint is "node_x < mesh_x && node_y < mesh_y"
- **THEN** config with node_x=3, mesh_x=4 SHALL pass validation

### Requirement: Required parameter enforcement

Parameters marked required SHALL be either explicitly provided or derivable; missing required params SHALL cause validation failure.

#### Scenario: Missing required parameter fails
- **WHEN** RouterTLM param rule for "node_x" has required=true and config provides mesh_x but not node_x and no derivation
- **THEN** validation SHALL fail with "[PARAM ERROR] Required parameter 'node_x' is missing"

#### Scenario: Derivable required parameter passes
- **WHEN** RouterTLM param rule for "node_x" has required=true and derive_expr provides path to compute it
- **THEN** missing but derivable node_x SHALL not cause validation failure

### Requirement: Param/config field separation is a validation precondition

The ModuleFactory param-validation pipeline SHALL treat the `params` / `config` field separation as a precondition: parameter validation logic operates on the `params` field, and the `config` field is reserved exclusively for external configuration file references.

#### Scenario: Validation reads params field only

- **WHEN** ModuleFactory::validateConfig() processes a module with both `params` and `config` fields
- **THEN** param-validation logic SHALL only read the `params` field for type-specific rules (range, constraints, required)
- **AND** SHALL NOT consult the `config` field for parameter values (it is a file path, not a parameter dict)

#### Scenario: SimObject with config dict produces no params

- **WHEN** a SimObject (e.g. `TrafficGenTLM`) module has `"config": { "pattern": "SEQUENTIAL" }` instead of `"params"`
- **THEN** ModuleFactory SHALL inject empty/default parameters into the module instance
- **AND** the user's intended configuration SHALL be silently ignored (CURRENT BUG)
- **AND** the `config-lint` LINT005 rule SHALL flag this configuration as an error

#### Scenario: SimModule with config as file path loads correctly

- **WHEN** a SimModule (e.g. `CpuCluster`) module has `"config": "/path/to/cfg.json"` and the file exists
- **THEN** ModuleFactory SHALL load the file and call `sim_mod->instantiate(internal_cfg)`
- **AND** the file's contents SHALL be merged into the module's configuration

#### Scenario: Param validation does not read config as fallback

- **WHEN** param-validation needs a value that is not in `params`
- **THEN** validation SHALL NOT fall back to reading the `config` field as a dict
- **AND** SHALL fail validation with the standard "missing required parameter" error

