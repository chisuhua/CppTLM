## ADDED Requirements

### Requirement: Wildcard pattern expansion

The ModuleGroup::resolve() SHALL expand wildcard patterns (using `*` and `?`) against the set of currently registered module instances, returning only the matched instance names rather than the raw pattern strings.

#### Scenario: Single wildcard at end
- **WHEN** ModuleGroup has `define("nics", {"nic_*"})` and instances "nic_0_0", "nic_0_1", "nic_1_0", "nic_1_1" are registered
- **THEN** resolve("group:nics") SHALL return ["nic_0_0", "nic_0_1", "nic_1_0", "nic_1_1"]

#### Scenario: Multiple wildcards in pattern
- **WHEN** ModuleGroup has `define("matching", {"*_0_*"})` and instances "a_0_x", "b_0_y", "c_1_z" are registered
- **THEN** resolve("group:matching") SHALL return ["a_0_x", "b_0_y"]

#### Scenario: Question mark single character wildcard
- **WHEN** ModuleGroup has `define("routes", {"router_?_?"})` and instances "router_0_1", "router_1_2", "router_2_0" are registered
- **THEN** resolve("group:routes") SHALL return ["router_0_1", "router_1_2", "router_2_0"]

#### Scenario: No matches returns empty
- **WHEN** ModuleGroup has `define("empty", {"xyz_*"})` and no instances starting with "xyz_" exist
- **THEN** resolve("group:empty") SHALL return []

#### Scenario: Mixed literal and wildcard patterns
- **WHEN** ModuleGroup has `define("mixed", {"type_a", "type_b_*"})` and instances "type_a", "type_b_0", "type_b_1", "type_c" exist
- **THEN** resolve("group:mixed") SHALL return ["type_a", "type_b_0", "type_b_1"]

#### Scenario: Non-wildcard pattern passes through unchanged
- **WHEN** ModuleGroup has `define("exact", {"specific_name"})` and no wildcard characters present
- **THEN** resolve("group:exact") SHALL return ["specific_name"]

### Requirement: Instance registration tracking

The ModuleGroup SHALL maintain a registry of all registered module instances accessible during wildcard resolution.

#### Scenario: New instance registration affects subsequent resolve
- **WHEN** ModuleGroup has `define("dynamic", {"dyn_*"})`, resolve is called with only "dyn_0" registered, then "dyn_1" is registered, and resolve is called again
- **THEN** first resolve returns ["dyn_0"] and second resolve returns ["dyn_0", "dyn_1"]

#### Scenario: Instance removal does not affect already-resolved groups
- **WHEN** ModuleGroup has `define("persistent", {"perm_*"})` and resolve returns ["perm_0", "perm_1"]; then "perm_0" is removed
- **THEN** the previously returned list remains valid; subsequent resolve call returns ["perm_1"]