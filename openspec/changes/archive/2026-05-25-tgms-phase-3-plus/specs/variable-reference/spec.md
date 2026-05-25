## ADDED Requirements

### Requirement: Variable reference syntax

The configuration JSON SHALL support `${path}` syntax for referencing other field values within the same configuration, enabling parameter reuse and dynamic values.

#### Scenario: Module name reference
- **WHEN** config contains `"module_name": "router_0"` and another field uses `"dst": "${module_name}.0"`
- **THEN** the reference SHALL be resolved to `"dst": "router_0.0"`

#### Scenario: Array element reference
- **WHEN** config has `"modules": [{"name": "first"}, {"name": "second"}]` and a connection uses `"src": "${modules[0].name}.0"`
- **THEN** the reference SHALL resolve to `"src": "first.0"`

#### Scenario: Nested path reference
- **WHEN** config has `"settings": {"delay": 5}` and a connection uses `"latency": "${settings.delay}"`
- **THEN** the reference SHALL resolve to `"latency": 5`

#### Scenario: Unresolved reference produces warning
- **WHEN** config contains `"reference": "${nonexistent.field}"`
- **THEN** ModuleFactory SHALL print a warning message and leave the reference unresolved

#### Scenario: Self-referential path handled gracefully
- **WHEN** a field contains `"circular": "${circular.value}"` which creates a cycle
- **THEN** ModuleFactory SHALL detect the cycle and either resolve to current value or warn

### Requirement: Cross-field references

The variable reference system SHALL support references across different sections of the configuration.

#### Scenario: Reference from params to module name
- **WHEN** a module has `"name": "cpu_0"` and its params contains `"owner": "${name}"`
- **THEN** params["owner"] SHALL resolve to "cpu_0"

#### Scenario: Reference in default value context
- **WHEN** a field uses `"default": "${settings.base_port}"` where settings.base_port exists
- **THEN** default SHALL be set to the referenced value