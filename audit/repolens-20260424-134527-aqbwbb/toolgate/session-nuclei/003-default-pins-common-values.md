---
title: "[LOW] Default PINs use common values that should be changed on first use"
severity: LOW
domain: security
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

The default PINs in `components/cdc_core/include/cdc_core/PinManager.h` are well-known common values:

```cpp
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

While the README instructs users to change the default PIN, there is no enforcement mechanism to ensure this happens.

## Impact

- **First-time setup risk**: Users who skip the "change default PIN" step have their device protected by a universally known PIN.
- **FIDO2 requirement**: FIDO2 requires a non-default PIN, but the badge PIN check only verifies if it differs from default, not if it's strong.
- **Common pattern**: "123456" is one of the most commonly used passwords/PINs.

## Evidence

**File**: `components/cdc_core/include/cdc_core/PinManager.h` (lines 27-29)

```cpp
Defaults:
- Badge/FIDO2: "123456"
- OpenPGP PW1 (User): "123456" (min 6 digits)
- OpenPGP PW3 (Admin): "12345678" (min 8 digits)
```

**File**: `components/cdc_core/include/cdc_core/PinManager.h` (lines 59-61)

```cpp
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

**File**: `README.md` (lines 193-195)

```
1. **Change the default PIN** (Settings -> Change PIN)
   - Default PIN: `123456`
   - FIDO2 requires a non-default PIN
```

## Recommended Fix

This is a documentation/enforcement enhancement. Options include:

**Option 1: Add visual indicator for default PIN**
Add a status indicator in the UI when the PIN is still at default value, making it more obvious to users.

**Option 2: Add warning on first FIDO2 registration**
When registering the first FIDO2 credential with the default PIN, show a prominent warning to change it.

**Option 3: Require PIN change after first boot**
Implement a "first-time setup" flow that requires changing the PIN before other features are accessible.

For immediate improvement, update the README to be more prominent:

```markdown
> **CRITICAL**: Change the default PIN (`123456`) immediately after first boot!
> Devices with the default PIN are vulnerable to brute-force attacks.
```

## References

- FIDO2 ClientPIN specification
- Common password lists (123456 is typically #1)
- NIST SP 800-63B Digital Identity Guidelines
