## ADDED Requirements

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