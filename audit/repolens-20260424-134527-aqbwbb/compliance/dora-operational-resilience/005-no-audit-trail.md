---
title: "[MEDIUM] No Audit Trail for Administrative Actions and Key Operations"
severity: MEDIUM
domain: Audit Trail
lens: dora-audit-trail
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The firmware lacks immutable audit logging for:
- Administrative actions (PIN changes, module configuration)
- Key generation events
- Key import/export operations
- Authentication events (FIDO2, SSH, GPG)
- Security-relevant state changes

Key searches performed:
- `grep -rn 'auditLog\|audit_log\|immutable\|append.*only'` - no results in source code
- `grep -rn 'logKey\|logEntry\|logTransaction\|logEvent'` - no results in source code

## Impact
For financial entities:
- Cannot trace administrative actions for compliance audits
- No immutable record of key lifecycle events
- Difficult to investigate security incidents
- Missing audit trail for regulatory reporting

## Evidence
Current logging implementation:
- `components/cdc_log/` - Provides basic logging via `LOG_I()`, `LOG_E()`, etc.
- Logs routed through USB CDC for serial output
- **No persistent storage** of log entries
- **No immutable audit trail** - logs can be lost on power cycle

From `README.md` line 71:
```
| **PIN Protection** | 4-8 digit PIN with 3 attempt lockout |
```
PIN lockout exists but failed attempts are not logged persistently.

From `components/cdc_log/include/cdc_log.h` (inferred structure):
- Basic logging to serial output
- No audit-specific logging functions
- No persistent storage mechanism

## Recommended Fix

1. **Create `components/audit_log/`** with:
   - `AuditLogger.h/cpp` - Class for immutable audit logging
   - `AuditEvent.h` - Struct for audit event data (timestamp, event_type, actor, details)
   - Storage in NVS with append-only semantics

2. **Define audit event types**:
   - `AUDIT_PIN_CHANGE` - PIN modification
   - `AUDIT_KEY_GENERATED` - New key created
   - `AUDIT_KEY_IMPORTED` - Key imported
   - `AUDIT_AUTH_SUCCESS` / `AUDIT_AUTH_FAILURE` - Authentication events
   - `AUDIT_MODULE_CONFIG` - Module configuration change

3. **Integrate audit logging into modules**:
   - `mod_fido2/`: Log authentication events
   - `mod_gpg/`: Log key generation, import, export
   - `mod_password/`: Log vault operations
   - `cdc_os_ui/`: Log PIN changes

4. **Create `docs/AUDIT_LOGGING.md`** with:
   - Audit event catalog
   - Log retention policy
   - Log export format

## References
- DORA Regulation (EU) 2022/2554, Article 6 - ICT risk management (audit trails)
- EBA Guidelines on internal governance (EBA-GL-2021-06)
- ISO 27001:2013 A.12.4 - Logging and monitoring
