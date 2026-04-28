---
title: "[MEDIUM] DEBUG_MODE defaults to enabled (1) in feature_flags.h"
severity: MEDIUM
domain: build
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `DEBUG_MODE` feature flag defaults to `1` (enabled) in `components/cdc_core/feature_flags.h`. This means that unless explicitly disabled during build, debug features remain active including:
- PIN lockout bypass
- Detailed debug logging
- Extended error information
- Various debug commands

Default value from `feature_flags.h:30-31`:
```cpp
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

## Impact
- **Production risk**: If build configuration is forgotten, debug features ship in production
- **Security bypass**: DEBUG_MODE affects lockout behavior for PIN verification
- **Verbose logging**: More data exposed via serial output when DEBUG_MODE=1
- **Multiple features affected**: DEBUG_MODE is used by multiple modules to enable debug output

## Evidence
File: `components/cdc_core/feature_flags.h:30-33`
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

Usage in `mod_fido2` (from existing findings):
```cpp
#if DEBUG_MODE
    LOG_D("FIDO2", "pinHash: ");
    log_hex("FIDO2", "pinHash", pinHash, 32);
#endif
```

## Recommended Fix
1. Change default to `0` for production-ready builds:
   ```cpp
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 0
   #endif
   ```

2. Add explicit documentation that DEBUG_MODE must be set via build flags:
   ```bash
   pio run -DDEBUG_MODE=1  # Explicitly enable for development
   ```

3. Add a CI check that warns if DEBUG_MODE=1 in release builds

4. Consider renaming to `FEATURE_DEBUG` for consistency with other feature flags

## References
- Feature flags: `components/cdc_core/feature_flags.h`
- Related to issue #3 (FIDO2 PIN hash in DEBUG_MODE logs)
- Related to issue #8 (FIDO2 ECDH in DEBUG_MODE logs)
- Build flags documentation: `docs/README.md`
