---
title: "[MEDIUM] DEBUG_MODE Enabled by Default Disables Lockouts"
severity: MEDIUM
domain: cdc-badge-os
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The `DEBUG_MODE` feature flag is set to `1` (enabled) by default in `feature_flags.h`. When enabled, this mode disables security lockouts, making the device vulnerable to brute-force PIN attacks during development and potentially in production if not properly configured.

**Location:** `components/cdc_core/include/cdc_core/feature_flags.h:29-32`

```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // <-- Enabled by default!
#endif
```

## Impact

**Security Impact:**
- **PIN lockouts can be bypassed in DEBUG_MODE**
- Default build configuration has lockouts disabled
- Any developer building without explicit `DEBUG_MODE=0` gets a less-secure device
- Production firmware might accidentally be built with DEBUG_MODE=1

**What DEBUG_MODE affects:**
Based on code analysis, `DEBUG_MODE` controls:
1. FIDO2 PIN protocol debug logging (detailed key material exposure)
2. ECDH shared secret logging (exposes cryptographic material)
3. PIN hash decryption logging (shows decrypted PIN hashes)
4. Potentially lockout bypass logic (needs verification)

**From `components/mod_fido2/src/ctap2.cpp:2066-2123`:**
```cpp
#if DEBUG_MODE
    // Full debug output for manual verification
    LOG_I("PIN", "=== ECDH DEBUG (full 32-byte values) ===");
    LOG_I("PIN", "Z (ECDH x-coord):");
    LOG_I("PIN", "  %02X%02X%02X%02X %02X%02X%02X%02X ...",
          ecdh_z[0], ecdh_z[1], ...);  // Logs full ECDH shared secret!
#endif
```

**From `components/mod_fido2/src/ctap2.cpp:2545-2550`:**
```cpp
#if DEBUG_MODE
    LOG_D("PIN", "Decrypted PIN hash: %02X%02X%02X%02X ...",
          decrypted_pin_hash[0], ...);  // Logs decrypted PIN hash!
#endif
```

## Evidence

1. **File:** `components/cdc_core/include/cdc_core/feature_flags.h:29-32`
   - DEBUG_MODE defaults to 1
   - Comment says "disables lockouts, useful for development"

2. **File:** `components/mod_fido2/src/ctap2.cpp:1559, 2066, 2100, 2115, 2279, 2463, 2485, 2545`
   - 8 locations where DEBUG_MODE logs sensitive cryptographic material

3. **File:** `components/usb_badge/usb_cdc.cpp:129-130`
   - Uses "early debug mode" terminology for USB initialization

**Risk scenarios:**
1. Developer builds firmware without thinking about `DEBUG_MODE` → gets debug build
2. GitHub Actions CI builds with default settings → debug build artifact
3. Production build forgets to set `DEBUG_MODE=0` → production device has debug features

## Recommended Fix

**Option 1 (Recommended): Default to DEBUG_MODE=0**

```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 0  // Changed to 0 - secure by default
#endif
```

**Option 2: Require explicit enable**

Move DEBUG_MODE to Kconfig and require explicit selection:
```cpp
#ifdef CONFIG_DEBUG_MODE
#define DEBUG_MODE 1
#else
#define DEBUG_MODE 0
#endif
```

**Option 3: Add build warning**

```cpp
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#warning "DEBUG_MODE enabled by default - ensure this is intended for production"
#endif
```

**Option 4: Add DEBUG_MODE to README/build docs**

Document that production builds require `-DDEBUG_MODE=0`:
```markdown
## Production Build

```bash
pio run -DDEBUG_MODE=0
```
```

**Also fix:**
- Add DEBUG_MODE=0 to `sdkconfig.defaults` for production builds
- Add CI check to warn if DEBUG_MODE=1 in release builds

## References

- [OWASP Secure by Default](https://cheatsheetseries.owasp.org/cheatsheets/Secure_by_Default_Cheat_Sheet.html)
- [NIST SP 800-53 - Configuration Control](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-53r5.pdf)
- Common security practice: Debug features should be OFF by default
