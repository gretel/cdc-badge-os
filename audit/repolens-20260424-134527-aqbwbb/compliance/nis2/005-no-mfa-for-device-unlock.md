---
title: "[MEDIUM] No multi-factor authentication for device unlock"
severity: MEDIUM
domain: access-control
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The device uses single-factor authentication (PIN only) for unlocking. NIS2 requires "multi-factor authentication" for access to critical systems (Art. 20). While the device is a hardware security key itself, accessing the badge's own features (FIDO2, GPG, passwords, TOTP) requires only a PIN - no second factor.

## Impact
**NIS2 Art. 20 MFA Gap**:
- Single point of failure: compromised PIN gives full access
- No defense-in-depth for device unlock
- PIN can be brute-forced (limited but possible)
- No biometric or hardware token as second factor

## Evidence
1. **PIN-only authentication**:
   - File: `README.md:71-75`
   ```
   | PIN | Purpose | Max Retries | Lockout |
   |-----|---------|-------------|---------|
   | Badge PIN | Device unlock, serial auth | 3 | 60 seconds |
   ```
   - Only PIN required for badge unlock

2. **Multiple PINs, but all single-factor**:
   - File: `README.md:71-84`
   ```
   | PIN | Purpose | Max Retries | Lockout |
   |-----|---------|-------------|---------|
   | Badge PIN | Device unlock, serial auth | 3 | 60 seconds |
   | PW1 | FIDO2/GPG user operations | 3 | 60 seconds |
   | PW3 | GPG admin operations | 3 | 60 seconds |
   ```
   - Multiple PINs but no combination of factors

3. **No second factor mechanism**:
   - No fingerprint sensor
   - No second hardware token requirement
   - No biometric option
   - No "something you have" + "something you know" combination

4. **DEBUG_MODE can disable lockouts**:
   - File: `components/cdc_core/include/cdc_core/feature_flags.h:28-30`
   ```cpp
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 1
   #endif
   ```
   - Development mode removes even basic PIN protection

## Recommended Fix
1. **Add Hardware Token Second Factor**:
   - Require physical button press for unlock (already exists, but not enforced as MFA)
   - Make button press mandatory after PIN entry
   - Document as "something you know" (PIN) + "something you have" (badge + button)

2. **Implement Time-Based Re-authentication**:
   - Require PIN re-entry after N minutes of inactivity
   - Shorter timeout for sensitive operations (password view, export)

3. **Add Biometric Option** (future):
   - Design for external biometric sensor
   - BLE-based phone unlock with phone's biometric

4. **Enforce PIN Complexity**:
   - Minimum 6 digits (currently allows 4-8)
   - No sequential patterns (123456, 654321)
   - No repeated patterns (111111, 121212)

5. **Document MFA Trade-offs**:
   - Explain why PIN+button is considered MFA for hardware key
   - Document threat model for single-factor vs two-factor

## References
- [NIS2 Directive Art. 20 - Multi-factor authentication](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [NIST SP 800-63B - Authentication](https://pages.nist.gov/800-63-3/sp800-63b.html)
- [FIDO2 WebAuthn MFA](https://www.fidoalliance.org/)
