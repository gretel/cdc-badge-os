# API Response Consistency Audit Findings

## Overview

This audit examined the serial command API in the CDC Badge OS codebase for response consistency issues. The serial command interface is the primary programmatic access point for the device, used via USB CDC or BLE UART at 115200 baud.

## Summary of Findings

| # | Severity | Finding | Impact |
|---|----------|---------|--------|
| 001 | HIGH | Boolean return type masks specific error codes in FIDO2 storage | Error context loss, incorrect CTAP2 status codes |
| 002 | MEDIUM | Inconsistent return types for similar find functions in FIDO2 storage | API confusion, error-prone usage |
| 003 | HIGH | GPG, Password, and TOTP modules use bare `OK`/`ERROR` without context | Reduced user feedback, debugging difficulty |
| 004 | MEDIUM | Inconsistent field naming across serial command responses | Parser complexity, inconsistent UX |
| 005 | LOW | Date/time display formats inconsistent across UI components | Visual inconsistency, localization issues |
| 006 | MEDIUM | Password module serial commands don't handle empty fields consistently with UI | Parser complexity, inconsistent UX |
| 007 | LOW | Usage messages use inconsistent argument naming conventions | Learning curve, documentation complexity |

## Key Patterns Identified

### Error Response Formats
- **Main serial commands**: Descriptive (`ERROR: PIN locked. Wait 30 seconds.`)
- **vCard module**: Descriptive (`ERROR: Invalid vCard`)
- **GPG/Password/TOTP modules**: Bare (`ERROR\r\n`)

### Success Response Formats
- **Main serial commands**: Action + details (`OK: Time set to 14:30:00`)
- **vCard module**: Action + details (`OK: vCard updated`)
- **GPG/Password/TOTP modules**: Bare (`OK\r\n`)

### Field Naming
- **GPG**: `User-ID:`, `Sign Count:`, `Created:`
- **Password**: `Username:`, `Title:`, `TOTP Slot:`
- **TOTP**: `name:`, `slot:` (lowercase in lists)

## Recommended Standards

### Error/Success Response Format
```
SUCCESS: <action> [<details>]
ERROR: <action> [<details>]
```

### Field Naming Convention
- Use PascalCase for all field names
- No hyphens, no spaces
- Consistent terminology across modules

### Date/Time Format
- Time: `%02d:%02d` (HH:MM, compact)
- Date: `%02d.%02d.%04d` (DD.MM.YYYY, compact)

## Files

- `001-boolean-error-masking.md` - FIDO2 error code masking
- `002-find-function-return-types.md` - FIDO2 find function inconsistencies
- `003-module-success-error-patterns.md` - Bare OK/ERROR responses
- `004-field-naming-inconsistencies.md` - Field naming patterns
- `005-date-time-format-inconsistencies.md` - UI date/time formats
- `006-null-empty-field-handling.md` - Empty field handling
- `007-usage-message-format.md` - Usage message conventions

## Related Standards

- REST API Best Practices: https://datatracker.ietf.org/doc/html/rfc7807
- ISO 8601 Date/Time: https://en.wikipedia.org/wiki/ISO_8601
- HTTP Status Codes: https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
