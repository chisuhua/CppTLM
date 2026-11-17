# param-type-conversion Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.

## Requirements

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

对**已准入**类型，参数值的类型判定与转换 SHALL 在**模块内**（`load_param` 的声明点）完成，**不**由 `ModuleFactory` 侧承担 —— 否则同一参数会存在第二个转换真值点（`ModuleFactory` 一份 + 模块一份 ⇒ 覆盖顺序决定行为）。

类型不符 SHALL 使装配失败。未准入类型继续沿用 `ModuleFactory` 侧既有转换路径（逐类型退役）。

#### Scenario: String latency converted during processing

- **WHEN** config has "latency": "100ns" for a connection（**连接**级字段，不属模块参数）
- **THEN** ModuleFactory SHALL convert to 100000 before storing（连接解析路径不变）

#### Scenario: String address converted during processing

- **WHEN** config has "base_address": "0x1000" for a MemoryTLM（**未准入**类型，沿用既有路径）
- **THEN** ModuleFactory SHALL convert to 4096 before storing（本 change 不改该路径）

#### Scenario: Non-integer JSON rejected for integer param

- **WHEN** 已准入类型的整型参数收到 `8.0`（JSON double）或 `true`（JSON bool）
- **THEN** 装配 SHALL 失败（⛔ 不得依赖 `get<int64_t>()` 的隐式转换）
- **AND** 错误信息 SHALL 含该参数名与期望类型

#### Scenario: Unsigned param rejects negative value

- **WHEN** 已准入类型的无符号整型参数收到 `-1`
- **THEN** 装配 SHALL 失败
- **AND** 错误信息 SHALL 含该参数名
