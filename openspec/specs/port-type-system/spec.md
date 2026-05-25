# port-type-system Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: Port role enum

The system SHALL define a PortRole enum with values INITIATOR, TARGET, BI_DIRECTIONAL, NETWORK, and PE to classify port communication directions.

#### Scenario: PortRole enum values present
- **WHEN** a module queries the PortRole of a port
- **THEN** the returned value SHALL be one of: INITIATOR, TARGET, BI_DIRECTIONAL, NETWORK, PE

### Requirement: Bundle type enum

The system SHALL define a BundleType enum with values CACHE_REQ, CACHE_RESP, NOC_FLIT, and GENERIC to classify flit bundle types.

#### Scenario: BundleType enum values present
- **WHEN** a module queries the BundleType of a port
- **THEN** the returned value SHALL be one of: CACHE_REQ, CACHE_RESP, NOC_FLIT, GENERIC

### Requirement: PortSpec static declaration

Each ChStream module SHALL declare its port specifications via a static `get_port_specs()` method returning a vector of PortSpec structs.

#### Scenario: RouterTLM declares 5 ports
- **WHEN** RouterTLM::get_port_specs() is called
- **THEN** it SHALL return vector containing specs for NORTH, EAST, SOUTH, WEST, LOCAL ports, all with role BI_DIRECTIONAL and bundle NOC_FLIT

#### Scenario: NICTLM declares PE and NETWORK ports
- **WHEN** NICTLM::get_port_specs() is called
- **THEN** it SHALL return vector containing spec for PE port (role PE, bundle CACHE_REQ) and NETWORK port (role NETWORK, bundle NOC_FLIT)

### Requirement: Port compatibility matrix

The system SHALL define and enforce a port compatibility matrix governing which port roles can connect to which other roles.

#### Scenario: INITIATOR connects to TARGET
- **WHEN** PortCompatibility::is_compatible(INITIATOR, TARGET) is called
- **THEN** it SHALL return true

#### Scenario: BI_DIRECTIONAL connects to BI_DIRECTIONAL, NETWORK, or PE
- **WHEN** PortCompatibility::is_compatible(BI_DIRECTIONAL, NETWORK) is called
- **THEN** it SHALL return true
- **AND** PortCompatibility::is_compatible(BI_DIRECTIONAL, PE) SHALL return true

#### Scenario: INITIATOR cannot connect to INITIATOR
- **WHEN** PortCompatibility::is_compatible(INITIATOR, INITIATOR) is called
- **THEN** it SHALL return false

### Requirement: Connection validation at bind time

The ModuleFactory SHALL validate port connections using the compatibility matrix before creating PortPairs.

#### Scenario: Valid connection passes validation
- **WHEN** attempting to connect a BI_DIRECTIONAL port to a NETWORK port
- **THEN** validate_connection() SHALL return true

#### Scenario: Invalid connection is rejected
- **WHEN** attempting to connect an INITIATOR port to another INITIATOR port
- **THEN** validate_connection() SHALL return false and print error message

#### Scenario: Bundle type mismatch is detected
- **WHEN** connecting two ports with incompatible BundleTypes and no registered converter
- **THEN** validation SHALL fail with "[BUNDLE ERROR]" message

