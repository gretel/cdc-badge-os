---
title: "[LOW] Default PINs documented in code (acceptable with user change requirement)"
severity: LOW
domain: cyber-resilience-act
lens: default-credentials
labels:
  - "default-credentials"
  - "pin-management"
  - "cra-2026"
---

## Summary
Default PINs are defined in code (`components/cdc_core/include/cdc_core/PinManager.h`). While this is acceptable for initial device setup, the documentation should be clearer about the requirement for users to change these defaults.

**Files affected:**
- `components/cdc_core/include/cdc_core/PinManager.h` (line 53-55)
- `components/cdc_core/src/PinManager.cpp` (uses default PINs)
- `README.md` - Mentions default PIN change but not prominently

## Impact
Under CRA (Annex I - Default configuration):
- **User awareness**: Users must clearly understand they need to change defaults
- **Best practice**: Default credentials should be unique per device or require immediate change

**Current PIN definitions:**
```cpp
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

**Current documentation** (README.md):
```markdown
1. **Change the default PIN** (Settings -> Change PIN)
   - Default PIN: `123456`
   - FIDO2 requires a non-default PIN
```

## Recommended Fix
Improve default PIN handling and documentation:

1. **Add warning in PinManager.h**:
```cpp
/**
 * Default PINs for initial device setup.
 * 
 * CRITICAL: Users MUST change these defaults before production use.
 * FIDO2 requires a non-default PIN for registration.
 */
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
```

2. **Add runtime warning on first boot** (if not already present):
```cpp
// On first boot, display warning to change PIN
if (isFirstBoot()) {
    LOG_W(TAG, "Default PIN active - please change in Settings");
}
```

3. **Add lockout for default PIN** (optional enhancement):
```cpp
// Require PIN change after N uses of default PIN
if (PIN == DEFAULT_BADGE_PIN && usageCount > 5) {
    forcePINChange();
}
```

4. **Strengthen documentation** in README.md:
```markdown
> **CRITICAL**: Change the default PIN within 24 hours of first use.
> The default PIN `123456` is well-known and should not be used long-term.
```

## References
- [EU CRA Annex I - Default configuration](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [OWASP Default Passwords](https://cheatsheetseries.owasp.org/cheatsheets/Default_Passwords_Cheat_Sheet.html)
- [FIDO2 PIN requirements](https://fidoalliance.org/specs/)
