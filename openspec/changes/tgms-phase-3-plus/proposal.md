## Why

The CppTLM framework has completed Phase 0-7 implementation, establishing core module registration, JSON-driven instantiation, and basic NoC modeling. However, analysis in ARCH-012 (Gap Analysis) identified 5 critical ModuleFactory defects (DEF-01 to DEF-05) and significant gaps in configuration表达能力, parameter systems, port management, and toolchain completeness. Phase 3+ addresses these gaps through systematic defect resolution and capability enhancement.

## What Changes

### Phase 3.1: Defect Fixes and Foundation
- **DEF-01**: Fix ModuleGroup wildcard expansion (`group:nics` returns literal `["nic_*"]` instead of expanded instance list)
- **DEF-02**: Eliminate Step 6/7b duplicate connection processing (same connection creates two PortPairs)
- **DEF-03**: Fix BidirectionalPortAdapter binding path (RouterTLM incorrectly uses `set_stream_adapter(array)` instead of `bind_port_pair()`)
- **DEF-04**: Strengthen port index parsing strictness (reject `xbar.0abc`)
- **DEF-05**: Fix Python toolchain type mapping (`generate_mesh()` outputs `"Router"` instead of `"RouterTLM"`)
- **CFG-04**: Add configuration inheritance mechanism (`extends` syntax)
- **CFG-06**: Add variable reference syntax (`${modules[0].name}`)
- **PARAM-03**: Parameter default value declaration (move from constructor hardcoding to declarative default table)
- **PORT-05**: Dual port side rule validation (NI PE-side port not allowed to connect to Router)
- **TOOL-06**: Topology config debug mode (detailed output of config parsing process and error locations)

### Phase 3.2: Port Management System
- **PORT-02**: Port type registration system (modules declare port direction, type, bundle)
- **PORT-01**: Port direction checking (validate src is output, dst is input before connection)
- **PORT-03**: Bundle type matching check (flit bundle types at both connection ends must be compatible)
- **PORT-04**: Data width checking (bit width consistency or declared converter at both ends)
- **PORT-06**: Port alias system (`{"0": "NORTH"}` for readability)
- **Dual connection detection**: Report error when already-connected port connects again)
- **Port type declaration DSL**: Modules declare port specifications through DSL)

### Phase 3.3: Configuration Capability Enhancement
- **CFG-03**: Dynamic parameter derivation (auto-derive flit_width, vc_count based on scale)
- **PARAM-04**: Parameter range validation (numeric range, enum legality checking)
- **PARAM-06**: Parameter dependency declaration (constraint relationships like `mesh_x * mesh_y == num_routers`)
- **PARAM-07**: Mutually exclusive parameter detection (params and config same-field conflict detection and warning)
- **CFG-05**: Command-line parameter override (CLI can override any field in JSON)
- **PARAM-02**: Type auto-conversion (string-to-address, latency unit parsing)
- **CFG-02**: Python configuration script support (gem5-like `.py` config files)

### Phase 3.4: Topology Validation and Toolchain
- **VALID-05**: Static load analysis (estimate each link traffic load based on topology structure)
- **VALID-07**: Clock domain declaration (modules/subnetworks associated with specific clock domains)
- **VALID-08**: Cross-clock domain connection validation (check if cross-clock domain connections have synchronizers)
- **TOOL-04**: ARCH-009 visualization integration (topology generator output directly fed into visualization pipeline)
- **TOOL-07**: Connection path tracing (given src/dst, output complete path)
- **TOOL-08**: Configuration lint tool (check configuration best practices)
- **TOOL-09**: Configuration format upgrade tool (auto-upgrade old JSON to new Schema)
- **SIM-06**: Topology-level traffic pattern configuration (declare global traffic patterns in routing section)
- **SIM-09**: Performance counter declaration (declare performance metrics to collect in topology config)

## Capabilities

### New Capabilities

- `module-group-wildcard`: ModuleGroup wildcard expansion resolving pattern strings against registered instances
- `config-inheritance`: Configuration file inheritance via `extends` field with deep merge semantics
- `variable-reference`: JSON variable reference syntax `${path}` for referencing other field values
- `port-type-system`: Port type registration and compatibility checking (PortRole, BundleType, width)
- `port-aliases`: Port alias system allowing named port references like `router.NORTH` instead of `router.0`
- `param-derivation`: Dynamic parameter derivation rules based on other parameter values
- `param-validation`: Parameter range and constraint validation framework
- `param-type-conversion`: Type auto-conversion for addresses ("0x10000000", "256MB") and latencies ("3ns", "100ps")
- `static-load-analysis`: Static traffic load analysis for identifying hotspot links
- `connection-path-tracer`: Path tracing from src module to dst module through the topology
- `config-lint`: Configuration best practices checking and validation
- `config-upgrade`: Legacy configuration format auto-upgrade tool

### Modified Capabilities

- (none - all new capabilities, no existing spec behavior changes)

## Impact

### Code Impacts
- `src/core/module_factory.cc`: DEF-01~05 fixes + extends/$ref support
- `include/core/port_types.hh`: New port type definitions
- `include/core/port_compatibility.hh`: Port compatibility matrix
- `include/core/param_parser.hh`: Parameter type parsing
- `include/core/param_rules.hh`: Parameter declaration and validation framework
- `include/utils/path_tracer.hh`: Connection path tracing

### Test Impacts
- `test/test_module_factory_fixes.cc`: DEF-01~05 verification (10 tests)
- `test/test_config_inheritance.cc`: extends/$ref support (8 tests)
- `test/test_port_management.cc`: Port type/direction/bundle checking (15 tests)
- `test/test_param_system.cc`: Parameter derivation/range/dependency (12 tests)
- `test/test_topology_analysis.cc`: Load analysis/path tracing/lint (10 tests)
- `test/test_e2e_phase3.cc`: Phase 3+ E2E verification (5 tests)

### Toolchain Impacts
- `scripts/topology_analyzer.py`: Static load analysis
- `scripts/config_lint.py`: Configuration linting
- `scripts/config_upgrade.py`: Configuration format upgrade

### Documentation Impacts
- `docs/architecture/13-port-management-system.md`: Port management system architecture
- `docs/architecture/14-parameter-system.md`: Parameter system architecture
- `docs/guide/CONFIGURATION_GUIDE.md`: Configuration guide (extends/$ref/port aliases)
- `docs/guide/PORT_MANAGEMENT_GUIDE.md`: Port management guide
- `docs/adr/ADR-X.8-port-type-system.md`: Port type system ADR
- `docs/adr/ADR-X.9-parameter-framework.md`: Parameter framework ADR