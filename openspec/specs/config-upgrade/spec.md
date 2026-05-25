# config-upgrade Specification

## Purpose
TBD - created by archiving change tgms-phase-3-plus. Update Purpose after archive.
## Requirements
### Requirement: Old schema field migration

The config upgrade tool SHALL migrate configurations from old schema to current schema.

#### Scenario: Old module format migration
- **WHEN** old config has module {"type": "Router", "config": {...}} without explicit "type" field as current schema expects
- **THEN** upgrade tool SHALL transform to current format {"name": "...", "type": "RouterTLM", "params": {...}}

#### Scenario: Missing port indices added
- **WHEN** old config has connections without port indices
- **THEN** upgrade tool MAY add default port indices (.0) or flag for manual review

#### Scenario: Old param names mapped to new
- **WHEN** old config uses param name "num_vcs" but current schema expects "vc_count"
- **THEN** upgrade tool SHALL map "num_vcs" -> "vc_count"

### Requirement: Version marker insertion

The upgrade tool SHALL add version marker to migrated config to indicate schema version.

#### Scenario: Version field added
- **WHEN** old config without "config_version" field is upgraded
- **THEN** resulting config SHALL contain "config_version": "2.0" or current version

#### Scenario: Existing version preserved
- **WHEN** config already has "config_version" field
- **THEN** upgrade tool SHALL preserve existing version or update if target version differs

### Requirement: Backup before upgrade

The upgrade tool SHALL create backup of original config before making changes.

#### Scenario: Backup file created
- **WHEN** upgrade tool processes "mesh_4x4.json"
- **THEN** it SHALL create "mesh_4x4.json.bak" before writing upgraded version

#### Scenario: Upgrade fails leaves original intact
- **WHEN** upgrade tool encounters unrecoverable error during processing
- **THEN** original file SHALL remain unchanged and error message displayed

