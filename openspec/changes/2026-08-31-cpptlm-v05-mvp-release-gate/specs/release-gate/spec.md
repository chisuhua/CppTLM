# Spec: v0.5.0-MVP Release Gate

## ADDED Requirements

### Requirement: release-gate-validation
The system MUST pass `validate_topology` target + full test suite (≥978 cases) before `v0.5.0-MVP` tag is created.

#### Scenario: All gates pass
- WHEN `cmake --build build --target validate_topology` and `./build/bin/cpptlm_tests` both succeed
- THEN `git tag -a v0.5.0-MVP` MUST be permitted