---
title: "[INFO] No Employee Time Tracking Feature Found"
severity: INFO
domain: compliance/time-tracking
lens: arbzg
labels:
  - scope:app
---

## Summary
The CDC Badge OS firmware is a **hardware security key** application, not an employee time tracking system. After comprehensive analysis of all modules and source files, no employee-facing time tracking, shift management, or workforce management functionality was found.

## Impact
Since this is not a time tracking application, ArbZG (German Working Time Act) compliance requirements for:
- Mandatory time recording
- Daily rest period enforcement (11 hours)
- Weekly maximum hours (48h average)
- Break time enforcement
- Overtime tracking
- 2-year retention

...are **not applicable** to this codebase.

## Evidence
**Codebase Analysis:**
- **TOTP Module** (`components/mod_totp/`): Uses Unix timestamps for cryptographic TOTP code generation, NOT for tracking working hours.
- **Password Vault** (`components/mod_password/`): Stores password entries with optional TOTP slot linkage.
- **FIDO2 Module** (`components/mod_fido2/`): WebAuthn authentication.
- **GPG Module** (`components/mod_gpg/`): OpenPGP smartcard functionality.
- **UI Components**: Lock screen clock display, settings menus, Bluetooth/WiFi menus.

**Features Identified:**
| Feature | Purpose |
|---------|---------|
| FIDO2/WebAuthn | Passwordless authentication |
| SSH Keys | Hardware key storage |
| TOTP | Time-based one-time passwords (cryptographic, not work hours) |
| Password Vault | Secure password storage |
| GPG/CCID | OpenPGP smartcard |
| BLE vCard | Contact exchange |

**Time-related code found:**
- `TotpStore::isTimeValid()` - Validates system time for TOTP (year 2024+)
- `TotpStore::timeRemaining()` - Seconds remaining in TOTP time step
- Lock screen clock display (UI feature)
- NTP time synchronization via WiFi

All time-related functionality is for **cryptographic purposes** (TOTP generation) or **UI display** (clock on lock screen), not employee time tracking.

## Recommended Fix
**None required.** This is the expected behavior for a hardware security key firmware.

If time tracking functionality is intended to be added in the future, it would require:
1. A new module for employee time entry (clock-in/out)
2. Database/storage for time records
3. Rest period validation logic
4. Overtime calculation
5. Export/retention mechanisms

## References
- [ECJ Ruling C-55/18](https://curia.europa.eu/juris/document/document.jsf?text=&docid=206178&pageIndex=0&doclang=EN&mode=req&dir=&occ=first&part=1&cid=1068406) - Mandatory time tracking ruling
- [German ArbZG](https://www.gesetze-im-internet.de/arbzg/) - Working Time Act
- [CDC Badge README](/input/20260423-132359-oj8ayc/cdc-badge-os/README.md) - Project documentation
