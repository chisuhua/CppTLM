# param-derivation Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: ParamRule structure

The system SHALL define a ParamRule structure containing type, required flag, default value, max value, and optional derivation expression.

#### Scenario: ParamRule stores all fields
- **WHEN** ParamRule is created with type INTEGER, required=true, default=64, max=256, derive_expr="(mesh_x * mesh_y > 16) ? 128 : 64"
- **THEN** all fields SHALL be accessible via getter methods

### Requirement: Module param rules declaration

Each TLM module SHALL declare its parameter rules via a static `get_param_rules()` method.

#### Scenario: RouterTLM declares coordinate params as required
- **WHEN** RouterTLM::get_param_rules() is called
- **THEN** it SHALL return rules where "node_x", "node_y", "mesh_x", "mesh_y" are marked required=true

#### Scenario: RouterTLM declares flit_width with derivation
- **WHEN** RouterTLM::get_param_rules() returns rule for "flit_width" with derive_expr
- **THEN** when flit_width is not explicitly provided, system SHALL evaluate expression using other param values

### Requirement: Dynamic derivation evaluation

Parameter derivation expressions SHALL be evaluated when a parameter is not explicitly provided in the configuration.

#### Scenario: Derivation based on mesh size
- **WHEN** config provides mesh_x=8, mesh_y=8 but not flit_width, and derive_expr="(mesh_x * mesh_y > 16) ? 128 : 64"
- **THEN** flit_width SHALL be derived as 128

#### Scenario: Derivation with small mesh
- **WHEN** config provides mesh_x=2, mesh_y=2 but not flit_width, and derive_expr="(mesh_x * mesh_y > 16) ? 128 : 64"
- **THEN** flit_width SHALL be derived as 64

#### Scenario: Explicit value overrides derivation
- **WHEN** config provides both mesh_x=8 and flit_width=256 explicitly
- **THEN** flit_width SHALL use the explicit value 256, not the derived value

### Requirement: Default value assignment

Parameters with no explicit value and no derivation expression SHALL use their declared default value.

#### Scenario: Default value applied when not provided
- **WHEN** config does not include "vc_count" and vc_count rule has default_value=2
- **THEN** vc_count SHALL be set to 2

