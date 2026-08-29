# release-gate Specification

## Purpose
TBD - created by archiving change 2026-08-31-cpptlm-v05-mvp-release-gate. Update Purpose after archive.
## Requirements
### Requirement: release-gate-validation
The system MUST pass `validate_topology` target + full test suite (≥978 cases) before `v0.5.0-MVP` tag is created.

#### Scenario: All gates pass
- WHEN `cmake --build build --target validate_topology` and `./build/bin/cpptlm_tests` both succeed
- THEN `git tag -a v0.5.0-MVP` MUST be permitted

