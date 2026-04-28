---
title: "[MEDIUM] DEBUG_MODE enabled by default for production builds"
severity: MEDIUM
domain: cyber-resilience-act
lens: secure-by-default
labels:
  - "debug-mode"
  - "secure-by-default"
  - "cra-2026"
---

## Summary
The `DEBUG_MODE` feature flag is set to `1` (enabled) by default in `components/cdc_core/include/cdc_core/feature_flags.h`. This disables PIN lockouts and increases log verbosity, which is suitable for development but insecure for production releases.

**Files affected:**
- `components/cdc_core/include/cdc_core/feature_flags.h` (line 30-31)
- `components/mod_fido2/src/ctap2.cpp` (multiple locations using DEBUG_MODE)
- `platformio.ini` - No default build flag to override DEBUG_MODE

## Impact
When `DEBUG_MODE=1`:
- **PIN lockouts disabled**: Brute-force attacks can try unlimited PIN combinations
- **Verbose logging**: May expose sensitive information via serial output
- **Production risk**: Developers may forget to disable it before release

**Evidence from feature_flags.h (line 30-31):**
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

**Evidence from ctap2.cpp (multiple locations):**
```cpp
#if DEBUG_MODE
// Debug code that bypasses security checks
#endif
```

## Recommended Fix
Change the default to `DEBUG_MODE=0` for production-ready builds:

1. **Update feature_flags.h**:
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 0  // Changed from 1 to 0 for secure defaults
#endif
```

2. **Add explicit build flag documentation** in `platformio.ini`:
```ini
build_flags =
    ; Security settings for production
    -DDEBUG_MODE=0
    -DFEATURE_SECURE_SERIAL=1
```

3. **Add CI check** to warn if DEBUG_MODE=1 in release builds:
```yaml
- name: Check DEBUG_MODE for releases
  if: startsWith(github.ref, 'refs/tags/v')
  run: |
    if grep -q "DEBUG_MODE 1" components/cdc_core/include/cdc_core/feature_flags.h; then
      echo "::warning::DEBUG_MODE is enabled in release build"
    fi
```

## References
- [EU CRA Annex I - Secure by default](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [OWASP Secure Defaults](https://cheatsheetseries.owasp.org/cheatsheets/Default_Passwords_Cheat_Sheet.html)
