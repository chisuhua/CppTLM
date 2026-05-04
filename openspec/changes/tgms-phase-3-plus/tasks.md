## 1. Phase 3.1 - Defect Fixes

- [x] 1.1 Fix ModuleGroup::resolve() wildcard expansion (DEF-01)
- [x] 1.2 Add connection deduplication in Step 6/7b (DEF-02)
- [x] 1.3 Fix BidirectionalPortAdapter binding path for RouterTLM (DEF-03)
- [x] 1.4 Strengthen port index parsing to reject "xbar.0abc" (DEF-04)
- [x] 1.5 Fix Python generate_mesh() to output "RouterTLM" not "Router" (DEF-05)

## 2. Phase 3.1 - Config Inheritance

- [x] 2.1 Implement `extends` field processing in ModuleFactory::instantiateAll()
- [x] 2.2 Implement deep merge for modules by name (params override)
- [x] 2.3 Implement connections append (not merge)
- [x] 2.4 Implement groups merge by group name
- [x] 2.5 Add cycle detection and depth limiting for extends

## 3. Phase 3.1 - Variable Reference

- [ ] 3.1 Implement `${path}` syntax parsing
- [ ] 3.2 Implement module name reference resolution
- [ ] 3.3 Implement array element reference (${modules[0].name})
- [ ] 3.4 Implement nested path reference (${settings.delay})
- [ ] 3.5 Add unresolved reference warning

## 4. Phase 3.1 - Parameter Defaults

- [ ] 4.1 Add ParamRule structure with type, required, default, max, derive_expr
- [ ] 4.2 Add get_param_rules() static method to RouterTLM
- [ ] 4.3 Implement default value assignment when param not provided

## 5. Phase 3.1 - Debug Mode & Side Rules

- [x] 5.1 Add `--debug-config` CLI option for verbose parsing output
- [ ] 5.2 Implement NI PE-side port to Router connection validation (PORT-05)

## 6. Phase 3.2 - Port Type System

- [ ] 6.1 Define PortRole enum (INITIATOR, TARGET, BI_DIRECTIONAL, NETWORK, PE)
- [ ] 6.2 Define BundleType enum (CACHE_REQ, CACHE_RESP, NOC_FLIT, GENERIC)
- [ ] 6.3 Define PortSpec struct with name, role, bundle, width, is_multi
- [ ] 6.4 Add get_port_specs() to RouterTLM returning 5 cardinal ports
- [ ] 6.5 Add get_port_specs() to NICTLM returning PE and NETWORK ports
- [ ] 6.6 Implement PortCompatibility::is_compatible() matrix
- [ ] 6.7 Add validate_connection() in ModuleFactory::bindPorts()

## 7. Phase 3.2 - Port Validation

- [ ] 7.1 Implement port direction checking (INITIATOR->TARGET)
- [ ] 7.2 Implement Bundle type matching check
- [ ] 7.3 Implement data width checking
- [ ] 7.4 Implement dual connection detection (already-connected port error)

## 8. Phase 3.2 - Port Aliases

- [ ] 8.1 Support `port_aliases` section in config JSON
- [ ] 8.2 Implement alias resolution in parsePortSpec()
- [ ] 8.3 Add test for router cardinal direction aliases (N/E/S/W/LOCAL)

## 9. Phase 3.3 - Parameter Derivation

- [ ] 9.1 Implement derive_expr evaluation for flit_width based on mesh size
- [ ] 9.2 Implement derive_expr evaluation for vc_count based on mesh size
- [ ] 9.3 Add explicit value override of derivation

## 10. Phase 3.3 - Parameter Validation

- [ ] 10.1 Implement parameter range validation (mesh_x/y: 1-16, vc_count: 1-8)
- [ ] 10.2 Implement ParamConstraint declarations for RouterTLM
- [ ] 10.3 Add required parameter enforcement
- [ ] 10.4 Implement ParamParser with latency unit parsing (ns, ps, cycle)
- [ ] 10.5 Implement ParamParser with address parsing (0x, MB, GB)

## 11. Phase 3.4 - Topology Analysis

- [ ] 11.1 Implement StaticLoadAnalyzer with graph construction
- [ ] 11.2 Implement uniform traffic load calculation
- [ ] 11.3 Implement hotspot identification with configurable threshold
- [ ] 11.4 Implement XY routing path computation

## 12. Phase 3.4 - Toolchain Scripts

- [ ] 12.1 Implement PathTracer with BFS shortest path
- [ ] 12.2 Implement path reconstruction and print_path()
- [ ] 12.3 Create scripts/config_lint.py for best practices checking
- [ ] 12.4 Create scripts/config_upgrade.py for legacy config migration
- [ ] 12.5 Create scripts/topology_analyzer.py for static load analysis

## 13. Phase 3.4 - Documentation

- [ ] 13.1 Create docs/architecture/13-port-management-system.md
- [ ] 13.2 Create docs/architecture/14-parameter-system.md
- [ ] 13.3 Create docs/adr/ADR-X.8-port-type-system.md
- [ ] 13.4 Create docs/adr/ADR-X.9-parameter-framework.md

## 14. Testing

- [ ] 14.1 Add test/test_module_factory_fixes.cc (DEF-01~05, 10 tests)
- [ ] 14.2 Add test/test_config_inheritance.cc (extends/$ref, 8 tests)
- [ ] 14.3 Add test/test_port_management.cc (port type/direction/bundle, 15 tests)
- [ ] 14.4 Add test/test_param_system.cc (derivation/range/dependency, 12 tests)
- [ ] 14.5 Add test/test_topology_analysis.cc (load/path/lint, 10 tests)
- [ ] 14.6 Add test/test_e2e_phase3.cc (Phase 3+ E2E, 5 tests)
- [ ] 14.7 Verify all 434 existing tests still pass