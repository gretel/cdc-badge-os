---
title: "[LOW] Default DEBUG_MODE May Ship in Production Builds"
severity: LOW
domain: api-security
lens: session-zap-api
labels:
  - audit:toolgate/session-zap-api
---

## Summary

The `DEBUG_MODE` feature flag defaults to `1` (enabled) in `feature_flags.h`. While primarily used for verbose cryptographic logging rather than disabling lockouts, the default "on" setting means production builds may inadvertently include debug features.

**Affected File:** `components/cdc_core/include/cdc_core/feature_flags.h`
**Lines:** 28-30

## Impact

When `DEBUG_MODE` is enabled (default):
- **Verbose crypto logging** - ECDH keys, AES session keys logged to serial (see related finding)
- **Build consistency** - Developers may forget to disable for production
- **Documentation gap** - No clear guidance on when to disable

## Evidence

```cpp
// components/cdc_core/include/cdc_core/feature_flags.h:28-30
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // Default is ON
#endif
```

Usage in FIDO2 module (`components/mod_fido2/src/ctap2.cpp`):
- ECDH shared secrets logged
- HKDF PRK values logged  
- AES session keys logged
- ECDH public keys logged

## Recommended Fix

1. **Document the flag clearly** with usage guidelines
2. **Add compile warning** when enabled
3. **Consider renaming** to `DEBUG_VERBOSE` for clarity

**Option 1 - Add warning:**
```cpp
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif

#if DEBUG_MODE
#warning "DEBUG_MODE=1: Verbose logging enabled (disable for production)"
#endif
```

**Option 2 - Add to README/CLAUDE.md:**
```markdown
## Build Flags

- `DEBUG_MODE` (default: 1) - Enable verbose debug logging
  - Set to 0 for production builds
  - May log cryptographic key material
```

**Option 3 - Split into granular flags:**
```cpp
// General debug logging
#ifndef DEBUG_LOG
#define DEBUG_LOG 1
#endif

// Verbose crypto logging (more sensitive)
#ifndef DEBUG_CRYPTO
#define DEBUG_CRYPTO 0  // Default OFF
#endif
```

## References

- CWE-489: Active debug code
- OWASP: Debugging enabled in production
