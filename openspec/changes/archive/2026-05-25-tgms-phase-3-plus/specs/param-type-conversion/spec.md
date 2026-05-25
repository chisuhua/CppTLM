## ADDED Requirements

### Requirement: Latency unit parsing

The ParamParser SHALL support latency values with unit suffixes (ns, ps, cycle) and convert to cycle count.

#### Scenario: Nanosecond suffix parsing
- **WHEN** parse_latency("3ns") is called
- **THEN** it SHALL return 3000 (converting to ps as base unit)

#### Scenario: Picosecond suffix parsing
- **WHEN** parse_latency("100ps") is called
- **THEN** it SHALL return 100

#### Scenario: Cycle suffix parsing
- **WHEN** parse_latency("5cycle") is called
- **THEN** it SHALL return 5

#### Scenario: Plain number defaults to cycles
- **WHEN** parse_latency("42") is called
- **THEN** it SHALL return 42

### Requirement: Address unit parsing

The ParamParser SHALL support address values with unit suffixes (MB, GB, or hex prefix 0x).

#### Scenario: Hex address parsing
- **WHEN** parse_address("0x10000000") is called
- **THEN** it SHALL return 268435456 (0x10000000 in decimal)

#### Scenario: Megabyte suffix parsing
- **WHEN** parse_address("256MB") is called
- **THEN** it SHALL return 268435456 (256 * 1024 * 1024)

#### Scenario: Gigabyte suffix parsing
- **WHEN** parse_address("2GB") is called
- **THEN** it SHALL return 2147483648 (2 * 1024 * 1024 * 1024)

#### Scenario: Plain decimal parsing
- **WHEN** parse_address("1024") is called
- **THEN** it SHALL return 1024

### Requirement: Auto-conversion in param processing

Parameter values SHALL be automatically converted based on declared param type before validation.

#### Scenario: String latency converted during processing
- **WHEN** config has "latency": "100ns" for a connection
- **THEN** ModuleFactory SHALL convert to 100000 before storing

#### Scenario: String address converted during processing
- **WHEN** config has "base_address": "0x1000" for a MemoryTLM
- **THEN** ModuleFactory SHALL convert to 4096 before storing