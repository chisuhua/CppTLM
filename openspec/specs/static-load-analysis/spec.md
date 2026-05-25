# static-load-analysis Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: Topology graph construction

The StaticLoadAnalyzer SHALL build a graph representation of the topology from configuration, with nodes representing routers and edges representing physical links.

#### Scenario: Mesh topology creates correct graph
- **WHEN** analyze_uniform_traffic() is called on mesh_4x4 config
- **THEN** the resulting graph SHALL have 16 router nodes with edges representing the mesh connectivity

#### Scenario: Ring topology creates correct graph
- **WHEN** analyze_uniform_traffic() is called on ring_8 config
- **THEN** the resulting graph SHALL have 8 router nodes with each node connected to exactly 2 neighbors

### Requirement: Uniform traffic load calculation

The analyzer SHALL calculate expected link load assuming uniform traffic distribution across all node pairs.

#### Scenario: Uniform traffic load formula
- **WHEN** analyze_uniform_traffic() is called on topology with N PE nodes
- **THEN** each link's load SHALL be calculated as paths_through_link / (N * (N-1))

#### Scenario: Load values are between 0 and 1
- **WHEN** analyze_uniform_traffic() returns load values
- **THEN** all load values SHALL be in range [0.0, 1.0]

### Requirement: Hotspot identification

The analyzer SHALL identify links with load exceeding a configurable threshold as hotspots.

#### Scenario: Default threshold hotspot detection
- **WHEN** identify_hotspots() is called with default threshold 0.15 on a topology
- **THEN** it SHALL return list of links where load > 0.15

#### Scenario: Custom threshold hotspot detection
- **WHEN** identify_hotspots(0.20) is called
- **THEN** it SHALL return only links where load > 0.20

#### Scenario: No hotspots when all loads low
- **WHEN** identify_hotspots() is called on a topology where all links have load < 0.15
- **THEN** returned list SHALL be empty

### Requirement: XY routing path computation

The analyzer SHALL compute paths using XY routing algorithm (route horizontally first, then vertically).

#### Scenario: XY path through intermediate routers
- **WHEN** computing path from router (0,0) to router (2,2) in 4x4 mesh
- **THEN** path SHALL go: (0,0) -> (1,0) -> (2,0) -> (2,1) -> (2,2)

