# port-aliases Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: Port alias declaration

The configuration JSON SHALL support a `port_aliases` section mapping symbolic port names to numeric indices.

#### Scenario: Simple module port alias
- **WHEN** config contains `"port_aliases": {"router_0.N": "router_0.0", "router_0.E": "router_0.1"}`
- **THEN** connection `"src": "router_0.N"` SHALL be resolved to "router_0.0"

#### Scenario: Router cardinal direction alias
- **WHEN** config contains `"port_aliases": {"router_0_0.NORTH": "router_0_0.0", "router_0_0.EAST": "router_0_0.1", "router_0_0.SOUTH": "router_0_0.2", "router_0_0.WEST": "router_0_0.3"}`
- **THEN** `"dst": "router_0_0.NORTH"` SHALL resolve to "router_0_0.0"

#### Scenario: Alias in connection referencing another alias
- **WHEN** port_aliases maps "router_0.E" to "router_0.1" and a connection uses "router_0.E" as destination
- **THEN** the connection processor SHALL first resolve alias "router_0.E" to "router_0.1", then parse as module "router_0" port index 1

### Requirement: Alias resolution before port parsing

Port aliases SHALL be resolved in parsePortSpec() before any other port parsing logic.

#### Scenario: Alias resolution precedes parsing
- **WHEN** parsePortSpec() receives "router_0.NORTH" and alias exists
- **THEN** alias SHALL be resolved before extracting module name and port index

#### Scenario: Undefined alias passes through unchanged
- **WHEN** parsePortSpec() receives "module.unknown_alias" and no alias defined
- **THEN** the spec SHALL be processed as-is without alias resolution

#### Scenario: Alias to alias chain resolves correctly
- **WHEN** port_aliases contains "a" -> "b" and "b" -> "c", and "c" -> "module.2"
- **THEN** resolving "a" SHALL ultimately resolve to "module.2" (single-level alias resolution, not recursive chain)

