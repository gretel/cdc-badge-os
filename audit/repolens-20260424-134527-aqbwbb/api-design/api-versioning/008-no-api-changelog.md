---
title: "[LOW] No API Changelog or Migration Guide Documentation"
severity: LOW
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The project lacks an API changelog documenting breaking changes, new features, and migration paths. `docs/` directory has feature docs but no version history.

**Evidence:**
- `docs/` directory contains: `SERIAL_COMMANDS.md`, `ble_vcard_protocol.md`, `MODULE_DEVELOPMENT.md` - but no `CHANGELOG.md` or `API_VERSIONING.md`
- `docs/SERIAL_COMMANDS.md` - Command reference with no version history
- No file tracking API changes across firmware versions

## Impact
- Developers cannot track what changed between firmware versions
- Hard to plan upgrades without knowing breaking changes
- No migration guide for moving from v1 to v2 APIs

## Evidence
Docs directory contents:
```
docs/
├── GPG.md
├── MODULE_DEVELOPMENT.md
├── README.md
├── SERIAL_COMMANDS.md          # No version history
├── UI_FLOWS.md
├── ble_vcard_protocol.md       # No version history
└── plans/                      # Planning docs, not changelog
```

## Recommended Fix
Create API documentation structure:

1. Create `docs/API_VERSIONING.md`:
```markdown
# API Versioning Policy

## Version Scheme
- Serial commands: v1.0.0 (current)
- BLE protocol: v1.0.0 (current)
- USB HID: v1.0.0 (current)

## Compatibility Guarantees
- Major version change = breaking changes
- Minor version change = new features (backward compatible)
- Patch version change = bug fixes

## Change Log
### v1.0.0 (Current)
- Initial API release
```

2. Create `CHANGELOG.md` at root:
```markdown
# Changelog

## [1.0.0] - 2026-01-01
### Added
- Serial command interface
- BLE vCard exchange
- USB HID support

### API Changes
- `TOTP_ADD` now supports optional issuer parameter
```

3. Update `docs/SERIAL_COMMANDS.md`:
```markdown
# Serial Commands (v1.0.0)

## Deprecated Commands
| Command | Deprecated | Alternative |
|---------|------------|-------------|
| OLD_CMD | v1.1.0 | NEW_CMD |
```

## References
- Keep a Changelog: https://keepachangelog.com/
- Semantic Versioning: https://semver.org/
