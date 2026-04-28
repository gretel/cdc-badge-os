---
title: "[HIGH] Default PINs hardcoded and documented in source"
severity: HIGH
domain: compliance/product-liability
lens: product-liability
labels:
  - hardcoded-credentials
  - default-values
---

## Summary
Default PINs are hardcoded in the source code (`components/cdc_core/include/cdc_core/PinManager.h`) and well-documented in README. The default PIN "123456" is used for Badge/FIDO2, PW1 (User), and PW3 (Admin) with slight variations.

**Evidence:**
- `PinManager.h:53-55`:
  ```cpp
  static constexpr const char* DEFAULT_BADGE_PIN = "123456";
  static constexpr const char* DEFAULT_PW1 = "123456";
  static constexpr const char* DEFAULT_PW3 = "12345678";
  ```
- `README.md:187`: "Default PIN: `123456`"
- `docs/README.md`: "Default PIN: `123456` (change immediately!)"

## Impact
Under Product Liability Directive, default credentials are considered a "defect" if users don't change them:
- Users who don't change the default PIN have a weak security posture
- "123456" is the most common PIN, easily guessable
- Brute-force protection (3 attempts) helps, but first 3 attempts always succeed with default
- If device is lost/stolen before PIN change, attacker can access all credentials
- For a **security key**, default PINs are particularly risky

## Evidence
Source code:
```cpp
components/cdc_core/include/cdc_core/PinManager.h:53:    static constexpr const char* DEFAULT_BADGE_PIN = "123456";
components/cdc_core/include/cdc_core/PinManager.h:54:    static constexpr const char* DEFAULT_PW1 = "123456";
components/cdc_core/include/cdc_core/PinManager.h:55:    static constexpr const char* DEFAULT_PW3 = "12345678";
```

Documentation:
```
README.md:187: 1. **Change the default PIN** (Settings -> Change PIN)
README.md:188:   - Default PIN: `123456`
```

## Recommended Fix
1. **Force PIN change on first boot**:
   - Add a "First Setup" mode that requires setting a new PIN before accessing other features
   - Similar to "Change PIN" but mandatory on first unlock

2. **Generate random default PIN**:
   - Instead of "123456", generate a random 6-digit PIN on first boot
   - Display on screen (user must write it down)
   - More secure but slightly more friction

3. **Add warning on first boot**:
   - Show prominent warning: "Device uses default PIN '123456'. Go to Settings -> Change PIN immediately."
   - Show this warning every time until PIN is changed

4. **Document in SECURITY.md**:
   - Emphasize that not changing default PIN is a known risk
   - Add to "Production Hardening Checklist"

Minimum fix (1 hour): Add mandatory first-setup PIN change flow.

## References
- OWASP Default Credentials: https://cheatsheetseries.owasp.org/cheatsheets/Default_Passwords_Cheat_Sheet.html
- NIST SP 800-63B (Digital Identity): https://pages.nist.gov/SP800-63B/
