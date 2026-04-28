---
title: "[HIGH] DEBUG_MODE Defaults to 1 - Production Configuration Gap"
severity: HIGH
domain: ICT Risk Management
lens: dora-configuration-management
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The `DEBUG_MODE` feature flag defaults to `1` (enabled) in `components/cdc_core/include/cdc_core/feature_flags.h:31`, and there is no explicit setting in `sdkconfig.defaults` or `platformio.ini` to override it for production builds.

When `DEBUG_MODE=1`:
- PIN lockouts are disabled (critical security feature bypassed)
- Verbose debug logging is enabled (potential information leakage)
- FIDO2 debug packets are enabled (see `components/mod_fido2/src/ctaphid.cpp:26`)

Key files:
- `components/cdc_core/include/cdc_core/feature_flags.h:30-31` - Default DEBUG_MODE=1
- `sdkconfig.defaults` - No DEBUG_MODE override
- `platformio.ini` - No DEBUG_MODE build flag

## Impact
For financial entities deploying this firmware:
- Devices may ship with DEBUG_MODE=1 if build configuration is not carefully managed
- Brute-force attacks on PIN become possible (3 attempt limit disabled)
- Debug output may leak sensitive information (keys, PINs, transaction data)
- FIDO2 AAGUID and other metadata may be exposed
- Hard to justify production deployment without explicit configuration review

From `README.md` line 7:
> "See [SECURITY.md](SECURITY.md) for hardening steps required before production use."

But SECURITY.md does not exist, and there's no clear production configuration guide.

## Evidence
From `components/cdc_core/include/cdc_core/feature_flags.h:30-31`:
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // <-- Default is ENABLED
#endif
```

From `components/mod_fido2/src/ctaphid.cpp:26`:
```cpp
#define CTAPHID_DEBUG_PACKETS       DEBUG_MODE  // Controlled by feature_flags.h
```

From `components/mod_fido2/src/ctap2.cpp:1559` (example):
```cpp
#if DEBUG_MODE
    // Debug code that may expose internal state
#endif
```

From `sdkconfig.defaults` - No `DEBUG_MODE` setting found (checked lines 1-78)

From `platformio.ini` - No `-DDEBUG_MODE=0` in build_flags (checked lines 1-43)

## Recommended Fix
Create a clear production-ready configuration:

1. **Change default in `feature_flags.h:31`**:
   ```cpp
   #ifndef DEBUG_MODE
   #define DEBUG_MODE 0  // Production default
   #endif
   ```

2. **Add explicit production configuration in `sdkconfig.defaults`**:
   ```
   # Production configuration
   CONFIG_DEBUG_MODE=0
   ```

3. **Create `docs/PRODUCTION_CONFIGURATION.md`** with:
   - List of all feature flags for production
   - Security-critical flags (DEBUG_MODE, FEATURE_SECURE_SERIAL, etc.)
   - Step-by-step build instructions for production
   - Verification checklist before flashing

4. **Add CI/CD check** to verify DEBUG_MODE=0 for release builds:
   ```bash
   # Check in build pipeline
   grep -q "DEBUG_MODE=0" sdkconfig.defaults || exit 1
   ```

## References
- DORA Regulation (EU) 2022/2554, Article 6(1)(e) - ICT configuration management
- EBA Guidelines on ICT risk management (EBA-GL-2021-07), Section 4.3 - Configuration baselines
- NIST SP 800-123 - General Guide to Server Configuration
- OWASP Configuration Checklist
