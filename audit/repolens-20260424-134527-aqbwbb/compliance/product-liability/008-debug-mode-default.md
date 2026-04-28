---
title: "[HIGH] DEBUG_MODE enabled by default"
severity: HIGH
domain: compliance/product-liability
lens: product-liability
labels:
  - debug-mode
  - security-configuration
---

## Summary
`DEBUG_MODE` is set to `1` (enabled) by default in `feature_flags.h`. This disables PIN lockouts and increases log verbosity, creating a security risk if users build/flash without changing the flag.

**Evidence:**
- `components/cdc_core/include/cdc_core/feature_flags.h:27`: `#define DEBUG_MODE 1`
- `README.md:88`: "`DEBUG_MODE=1` disables lockouts for development. Set to 0 for production!"

## Impact
When `DEBUG_MODE=1`:
- PIN lockouts are disabled (users can try infinite PINs)
- More verbose logging (may expose sensitive data)
- If users build and flash without changing this, their device has weaker security
- Under Product Liability Directive, debug features in production firmware are considered a defect

## Evidence
```cpp
components/cdc_core/include/cdc_core/feature_flags.h:27: #ifndef DEBUG_MODE
components/cdc_core/include/cdc_core/feature_flags.h:28: #define DEBUG_MODE 1
components/cdc_core/include/cdc_core/feature_flags.h:29: #endif
```

Documentation warning:
```
README.md:88: **Note:** `DEBUG_MODE=1` disables lockouts for development. Set to 0 for production!
```

## Recommended Fix
1. **Change default to DEBUG_MODE=0**:
   ```cpp
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 0  // Production-safe default
   #endif
   ```

2. **Add build-time warning**:
   If `DEBUG_MODE=1`, add a compile-time warning:
   ```cpp
   #if DEBUG_MODE == 1
   #warning "DEBUG_MODE is enabled - PIN lockouts are disabled!"
   #endif
   ```

3. **Add runtime warning**:
   On boot, if `DEBUG_MODE=1`, show on display: "DEBUG MODE: Lockouts disabled"

4. **Update documentation**:
   Make it clear that `DEBUG_MODE=0` is required for production

Minimum fix (1 hour): Change default to `DEBUG_MODE=0`.

## References
- OWASP Debugging Features: https://cheatsheetseries.owasp.org/cheatsheets/Debugging_Features_Cheat_Sheet.html
- CWE-489: Active Debug Code: https://cwe.mitre.org/data/definitions/489.html
