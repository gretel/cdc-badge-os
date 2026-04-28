---
title: "[HIGH] No incident response plan or alerting mechanism for security events"
severity: HIGH
domain: incident-management
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The CDC Badge OS codebase lacks any documented incident response plan, detection mechanism for security incidents, or escalation procedure as required by NIS2 Article 23. There is no mechanism to detect, classify, or escalate security incidents such as tamper detection, failed authentication spikes, or secure element alarms.

## Impact
**NIS2 Art. 23 Compliance Gap**: The directive requires "capabilities for early warning" and incident notification. Without an incident response plan:
- Security incidents (e.g., tamper detection, brute-force attacks) go unlogged and unescalated
- No defined procedure for classifying severity of incidents
- No mechanism for notifying stakeholders within 72 hours (NIS2 requirement)
- No post-incident review process to capture lessons learned

For a hardware security key handling FIDO2, GPG, and TOTP credentials, this gap is critical.

## Evidence
1. **Tamper detection exists but no incident logging**:
   - File: `components/cdc_hal/include/cdc_hal/ISecureElement.h:27`
   ```cpp
   enum class SeResult : uint8_t {
       OK,
       ERROR,
       SESSION_REQUIRED,
       SLOT_EMPTY,
       SLOT_OCCUPIED,
       INVALID_PARAM,
       ALARM_MODE,         // Chip in alarm mode (tamper detected)
       NOT_SUPPORTED
   };
   ```
   - `ALARM_MODE` is defined but no incident handling procedure exists

2. **PIN lockout exists but no incident tracking**:
   - File: `README.md:71-75`
   ```
   | Badge PIN | Device unlock, serial auth | 3 | 60 seconds |
   | PW1 | FIDO2/GPG user operations | 3 | 60 seconds |
   | PW3 | GPG admin operations | 3 | 60 seconds |
   ```
   - Lockouts occur but no incident log or alert mechanism

3. **No incident response documentation**:
   - No `INCIDENT_RESPONSE.md` or similar in `/docs/`
   - No runbooks for security events
   - No alerting configuration

4. **Limited logging infrastructure**:
   - File: `components/cdc_core/src/PinManager.cpp` - Lockout logged but not tracked over time
   - No centralized incident logging system

## Recommended Fix
1. **Create Incident Response Documentation** (`docs/INCIDENT_RESPONSE.md`):
   - Define incident categories (tamper, brute-force, key compromise, etc.)
   - Define severity levels (Critical, High, Medium, Low)
   - Define escalation paths and response times
   - Include post-incident review process

2. **Implement Incident Logging**:
   - Add incident counter to NVS (e.g., failed PIN attempts, tamper events)
   - Log incidents with timestamps to persistent storage
   - Add serial command to query incident log: `INCIDENTS LIST`, `INCIDENTS CLEAR`

3. **Add Alert Mechanism**:
   - Implement threshold-based alerts (e.g., 5 failed attempts in 1 hour)
   - Add visual/audio alert on device for critical incidents
   - Store alert state in NVS for persistence across reboots

4. **Create Security Event Handler**:
   - Centralize security event handling in `cdc_core/SecurityEventManager.h`
   - Provide API for modules to report incidents
   - Implement configurable thresholds

## References
- [NIS2 Directive Art. 23 - Incident handling](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-61 Rev. 2 - Computer Security Incident Handling Guide](https://csrc.nist.gov/publications/detail/sp/800-61/rev-2/final)
- [ENISA Incident Management Best Practices](https://www.enisa.europa.eu/topics/incident-management)
