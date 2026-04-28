---
title: "[INFO] No KRITIS (Critical Infrastructure) Applicability Detected"
severity: INFO
domain: kritis-infrastructure
lens: IT-SiG-2.0
labels:
  - "audit:compliance/kritis-infrastructure"
  - "scope:applicability"
---

## Summary
The CDC Badge OS repository contains firmware for a personal hardware security key (ESP32-S3 + TROPIC01 secure element). After thorough analysis, no indicators of critical infrastructure (KRITIS) sector integration were found. The codebase does not integrate with:

- SCADA/ICS/OT systems (Modbus, DNP3, IEC 61850, OPC UA)
- Healthcare systems (HL7, FHIR, hospital management)
- Energy grid or utility management
- Financial transaction processing
- Telecommunications network management

**Evidence:**
- `grep -rni 'scada|ics|plc|modbus|dnp3|iec.*61850|opc.*ua|critical.*infra|kritis'` returned no relevant matches
- Documentation (README.md, docs/*.md) describes personal authentication features only
- Components: FIDO2/WebAuthn, SSH keys, TOTP, password vault, BLE vCard

## Impact
**Low/Info:** This finding is informational only. The software is a **general-purpose hardware security key** firmware, not a KRITIS system. IT-SiG 2.0 requirements for critical infrastructure operators do not apply.

## Evidence
- File: `README.md` - Describes FIDO2, SSH, TOTP, password vault features
- File: `docs/GPG.md` - OpenPGP smartcard functionality
- File: `components/` - 14 modules, all focused on personal authentication
- No references to industrial protocols, OT systems, or critical infrastructure in code or docs

## Recommended Fix
**No action required.** This is expected behavior for a personal hardware security key. If the badge is intended for use *within* a KRITIS environment (e.g., as an authentication token for SCADA operators), the **host systems** would need to comply with IT-SiG 2.0, not the badge firmware itself.

## References
- [IT-Sicherheitsgesetz 2.0 (IT-SiG 2.0)](https://www.gesetze-im-internet.de/itsig_2_0/)
- [BSI KRITIS Sector Definition](https://www.bsi.bund.de/DE/Themen/Kritische-Infrastrukturen/KRITIS-Infrastrukturen/KRITIS_node.html)
- [BSI IT-Grundschutz](https://www.bsi.bund.de/DE/Service-Navi/Grundschutz/grundschutz_node.html)

---

**Applicability Signal:** Not applicable - General-purpose hardware security key firmware with no critical infrastructure sector integration.
