---
title: "[LOW] FIDO2 PIN retry count (8) is higher than Badge PIN (3), inconsistent security"
severity: LOW
domain: rate-abuse
lens: rate-abuse-fido2
labels:
  - "audit:security/rate-abuse"
---

## Summary
The FIDO2 PIN implementation allows 8 retry attempts (`PIN_RETRIES_MAX = 8`), while the Badge PIN only allows 3 attempts (`MAX_RETRIES = 3`). This inconsistency means FIDO2 authentication is more susceptible to brute-force attacks than the main device PIN.

**Location:** `components/mod_fido2/src/ctap2.cpp:77`

```c
#define PIN_RETRIES_MAX 8  // FIDO2: 8 attempts
```

**Location:** `components/cdc_core/src/PinManager.cpp:108`

```cpp
static constexpr uint8_t MAX_RETRIES = 3;  // Badge PIN: 3 attempts
```

## Impact
**Inconsistent security posture:**
- Badge PIN: 3 attempts + 60s lockout = stronger protection
- FIDO2 PIN: 8 attempts + no persistence = weaker protection
- An attacker can try 8 FIDO2 PINs vs. only 3 Badge PINs before lockout

**Attack surface expansion:**
- FIDO2 interface is more exposed (USB HID, Chrome/FF integration)
- More attempts allowed on the more accessible interface
- 2.67x more attempts than the primary Badge PIN

**User confusion:**
- Users may assume consistent security across all interfaces
- FIDO2 PIN may be set to same value as Badge PIN (common pattern)
- Weaker FIDO2 lockout undermines overall device security

## Evidence
- **File:** `components/mod_fido2/src/ctap2.cpp`
- **Line 77:** `#define PIN_RETRIES_MAX 8`
- **Lines 2012, 2586, 2817, 2876:** FIDO2 retries initialized/reset
- **File:** `components/cdc_core/src/PinManager.cpp`
- **Line 108:** `static constexpr uint8_t MAX_RETRIES = 3;`
- **Lines 323, 376, 425, 498, 531, 592:** Badge PIN retries persisted to storage

## Recommended Fix
Align FIDO2 PIN retries with Badge PIN for consistent security:

**Option 1: Reduce FIDO2 retries to 3**
- Change `PIN_RETRIES_MAX` from 8 to 3 in `ctap2.cpp`
- Matches Badge PIN policy
- Simple change, no other code modifications needed

**Option 2: Use shared constant**
- Define `PIN_RETRIES_MAX` in `feature_flags.h` or a shared header
- Both FIDO2 and Badge PIN use the same constant
- Ensures future consistency

**Implementation steps (Option 1):**
1. Edit `components/mod_fido2/src/ctap2.cpp`
2. Change line 77 from `#define PIN_RETRIES_MAX 8` to `#define PIN_RETRIES_MAX 3`
3. Update any related documentation

**Implementation steps (Option 2):**
1. Create `components/cdc_core/include/cdc_core/pin_config.h`
2. Add: `#define PIN_RETRIES_MAX 3`
3. Include in both `ctap2.cpp` and `PinManager.cpp`
4. Remove local definitions

## References
- NIST SP 800-63B: Authentication and Lifecycle Management
- FIDO CTAP2 spec: PIN policy recommendations
