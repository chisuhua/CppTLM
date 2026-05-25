# connection-path-tracer Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: BFS path discovery

The PathTracer SHALL find the shortest path between two modules using breadth-first search.

#### Scenario: Direct connection path
- **WHEN** trace_path("cpu_0", "mem_0") is called where they are directly connected
- **THEN** result SHALL contain single hop with correct src/dst modules and ports

#### Scenario: Multi-hop path discovery
- **WHEN** trace_path("ni_0_0", "ni_2_2") is called in 4x4 mesh with 3 hops
- **THEN** result SHALL contain 3 hops with intermediate routers

#### Scenario: Path with latency information
- **WHEN** trace_path("a", "d") returns path with hops through "b" and "c"
- **THEN** each hop SHALL contain the latency value from the connection definition

#### Scenario: No path exists
- **WHEN** trace_path("isolated_node", "reachable_node") is called where no path exists
- **THEN** result SHALL be empty vector

### Requirement: Path reconstruction

The tracer SHALL reconstruct the full path from source to destination including all intermediate hops.

#### Scenario: Path includes all intermediate nodes
- **WHEN** trace_path("src", "dst") finds path through "mid1" and "mid2"
- **THEN** result vector SHALL have 3 hops: src->mid1, mid1->mid2, mid2->dst

#### Scenario: Port information preserved
- **WHEN** trace_path returns a hop
- **THEN** hop SHALL contain src_module, src_port, dst_module, dst_port, and latency

### Requirement: Path printing

The tracer SHALL provide a method to print the path in human-readable format.

#### Scenario: Print path with hop numbers
- **WHEN** print_path() is called with a 2-hop path
- **THEN** output SHALL show "Hop 1: src.port -> mid.port (latency=X)" and "Hop 2: mid.port -> dst.port (latency=Y)"

#### Scenario: Empty path printing
- **WHEN** print_path() is called with empty path
- **THEN** it SHALL print a message indicating no path exists

