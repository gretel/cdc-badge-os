---
title: "[HIGH] No Incident Detection and Reporting Mechanism"
severity: HIGH
domain: Incident Management
lens: dora-incident-reporting
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The CDC Badge OS firmware lacks:
- Security incident detection mechanisms (no classification by severity)
- Automated incident reporting pipeline
- Incident response runbooks for security-relevant scenarios
- Root cause analysis process documentation

Key searches performed:
- `grep -rn 'incident\|security.*event\|breach\|detectIncident\|reportIncident'` - no results in source code
- `grep -rn 'auditLog\|audit_log\|immutable'` - no results in source code

## Impact
For financial entities using this firmware:
- Cannot detect security incidents within the 15-minute SLA required by DORA
- No automated reporting to regulatory authorities
- Difficult to perform root cause analysis after security events
- Limited visibility into device-level security events

## Evidence
- `components/cdc_log/` - Provides basic logging but no incident classification
- `components/cdc_core/EventBus.h` - Event bus exists but no incident-specific events defined
- No incident handling code found in `mod_fido2/`, `mod_gpg/`, `mod_password/`
- README.md line 68-70: PIN lockout exists but no logging of failed attempts for security analysis

Example from `README.md`:
```
| PIN | Purpose | Max Retries | Lockout |
| Badge PIN | Device unlock, serial auth | 3 | 60 seconds |
```
Lockout exists but failed attempts are not logged for incident detection.

## Recommended Fix
Implement incident detection and logging:

1. **Create `components/incident_mgmt/`** with:
   - `IncidentManager.h/cpp` - Class to classify and track incidents
   - `IncidentTypes.h` - Enum for incident types (PIN_BRUTE_FORCE, KEY_EXHAUSTION, etc.)
   - `IncidentSeverity.h` - Enum for severity levels (LOW, MEDIUM, HIGH, CRITICAL)

2. **Add incident logging to existing modules**:
   - In `mod_fido2/`: Log failed authentication attempts
   - In `mod_gpg/`: Log PIN verification failures
   - In `cdc_hal/`: Log hardware tamper detection

3. **Create `docs/INCIDENT_RESPONSE.md`** with:
   - Incident classification matrix
   - Response procedures for each incident type
   - Escalation paths

## References
- DORA Regulation (EU) 2022/2554, Article 10 - Major operational loss
- EBA Guidelines on ICT incident management (EBA-GL-2020-02)
- NIST SP 800-61 Rev. 2 - Computer Security Incident Handling Guide
