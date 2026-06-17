# params-config-semantics Specification

## Purpose
Define the semantic boundary between the `params` and `config` JSON fields on module entries in CppTLM topology configurations, and require all C++ / Python tooling to honor this boundary.
## Requirements

### Requirement: params is the canonical field for module configuration

The `params` field on a module entry SHALL be the canonical location for module configuration, holding a JSON object whose keys are parameter names and values are parameter values (numbers, strings, arrays, or nested objects as defined by the module's parameter rules).

#### Scenario: SimObject consumes params via set_config

- **WHEN** a SimObject module (e.g. `TrafficGenTLM`, `CacheTLM`, `CrossbarTLM`) has `"params": { "pattern": "SEQUENTIAL", "num_requests": 10000 }`
- **THEN** ModuleFactory SHALL call `obj->set_config(*cfg_src)` with the params object
- **AND** the module SHALL use these parameters during simulation

#### Scenario: Python emitter outputs params

- **WHEN** `CxxCompatibleEmitter.emit()` is called on a `TopoLayer` whose modules have `params: dict` attribute
- **THEN** the emitted JSON SHALL include `params` field on each module entry
- **AND** SHALL NOT include any other field for the same purpose

#### Scenario: TopoLayer attribute is named params

- **WHEN** inspecting `cpptlm/topo/layer.py`
- **THEN** the `TopoLayer` module spec dataclass SHALL use field name `params` (not `config`)
- **AND** the type SHALL be `dict` (default empty)

### Requirement: config is reserved for external file path reference

The `config` field on a module entry SHALL be reserved exclusively for a JSON string referencing an external configuration file path. It SHALL NOT be used as a parameter dict.

#### Scenario: config as file path loads external config

- **WHEN** a module has `"config": "/absolute/path/to/cfg.json"` and the file exists and is readable
- **THEN** ModuleFactory SHALL parse the file as JSON
- **AND** SHALL call `sim_mod->instantiate(internal_cfg)` for SimModule types
- **AND** the module SHALL use the loaded configuration during simulation

#### Scenario: config as file path with missing file logs error

- **WHEN** a module has `"config": "/missing/file.json"` and the file cannot be opened
- **THEN** ModuleFactory SHALL log DPRINTF error: "[ERROR] Cannot open config: /missing/file.json"
- **AND** the module SHALL continue with default configuration (not crash)

#### Scenario: config as object is a misuse

- **WHEN** a module has `"config": { "any": "value" }` (object instead of string)
- **THEN** this SHALL be reported as LINT005 ERROR by config-linter
- **AND** ModuleFactory SHALL NOT attempt to load this as a file path (which would throw `json::type_error`)
- **AND** if the current code path would attempt to load, it SHALL instead emit a DPRINTF warning at MODULE log level

### Requirement: Documentation and tooling maintain the boundary

All CppTLM documentation, examples, and tooling (Python emitter, Pydantic models, visualization scripts) SHALL honor the `params` / `config` semantic boundary.

#### Scenario: AGENTS.md files document the boundary

- **WHEN** reading `configs/AGENTS.md` and `cpptlm/AGENTS.md`
- **THEN** the schema example SHALL show `params` as the parameter field
- **AND** SHALL explicitly note that `config` is reserved for external file path

#### Scenario: Python scripts do not confuse the fields

- **WHEN** Python code reads module configuration from a C++ JSON file
- **THEN** it SHALL use `module["params"]` (not `module["config"]`) for parameter access
- **AND** the only legitimate use of `config` is the API endpoint path `/api/runs/<id>/config` (unrelated)

#### Scenario: cpptlm_config legacy models use params

- **WHEN** inspecting `cpptlm_config/models.py` `ModuleSpec` Pydantic model
- **THEN** the parameter field SHALL be named `params` (not `config`)
- **AND** the type SHALL be `dict` (default empty)
