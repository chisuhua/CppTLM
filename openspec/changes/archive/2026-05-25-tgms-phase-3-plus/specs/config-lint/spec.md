## ADDED Requirements

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