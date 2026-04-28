---
title: "[MEDIUM] Hardcoded security parameters - KDF iterations, rate limits, retry counts"
severity: MEDIUM
domain: environment configuration
lens: env-config
labels:
  - "audit:devops/env-config"
---

## Summary
Critical security parameters are hardcoded in source code with no mechanism to configure them per environment or deployment:

1. **KDF iteration count** (100,000) - Controls password hashing strength
2. **Rate limiting** (200 commands/second) - FIDO2 CTAPHID rate limit
3. **Retry limits** (3 attempts) - PIN retry counters
4. **Lockout duration** (60 seconds) - PIN lockout time

These values should be configurable for different security profiles (e.g., high-security vs. convenience modes).

## Impact
- **Security tuning impossible**: Cannot increase KDF iterations for higher security without code changes
- **Environment-specific tuning**: Different environments may need different rate limits (dev vs. prod)
- **User experience**: Cannot adjust retry counts/lockout times for different user preferences
- **Compliance**: May need to adjust parameters for specific compliance requirements

## Evidence

**KDF Iterations** - File: `components/cdc_core/include/cdc_core/PinManager.h` line 50:
```cpp
static constexpr uint32_t DEFAULT_ITERATIONS = 100000;
```
Line 99: `uint32_t getIterationCount() const { return iterations_; }`

**Rate Limits** - File: `components/mod_fido2/src/ctaphid.cpp` lines 35-36:
```cpp
#define CTAPHID_RATE_LIMIT_WINDOW_MS  1000
#define CTAPHID_RATE_LIMIT_MAX_CMDS   200
```

**Retry Limits** - File: `components/cdc_core/include/cdc_core/PinManager.h` line 108:
```cpp
static constexpr uint8_t MAX_RETRIES = 3;
```

**Lockout Duration** - File: `components/cdc_core/include/cdc_core/PinManager.h` line 71:
```cpp
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
```

**WiFi Retry** - File: `components/cdc_hal/src/WifiController.cpp` line 94:
```cpp
static constexpr uint8_t MAX_RETRY = 5;
```

## Recommended Fix

Create a centralized security configuration header with build-flag overrides:

1. **Create `components/cdc_core/include/cdc_core/SecurityConfig.h`**:
```cpp
#pragma once

// KDF Configuration
#ifndef KDF_DEFAULT_ITERATIONS
#define KDF_DEFAULT_ITERATIONS 100000
#endif

#ifndef KDF_MIN_ITERATIONS
#define KDF_MIN_ITERATIONS 50000
#endif

#ifndef KDF_MAX_ITERATIONS
#define KDF_MAX_ITERATIONS 1000000
#endif

// PIN Lockout Configuration
#ifndef PIN_LOCKOUT_DURATION_MS
#define PIN_LOCKOUT_DURATION_MS 60000
#endif

#ifndef PIN_MAX_RETRIES
#define PIN_MAX_RETRIES 3
#endif

// Rate Limiting Configuration
#ifndef RATE_LIMIT_WINDOW_MS
#define RATE_LIMIT_WINDOW_MS 1000
#endif

#ifndef RATE_LIMIT_MAX_CMDS
#define RATE_LIMIT_MAX_CMDS 200
#endif

// WiFi Configuration
#ifndef WIFI_MAX_RETRY
#define WIFI_MAX_RETRY 5
#endif
```

2. **Update `PinManager.h` to use macros**:
```cpp
static constexpr uint32_t DEFAULT_ITERATIONS = KDF_DEFAULT_ITERATIONS;
static constexpr uint32_t LOCKOUT_DURATION_MS = PIN_LOCKOUT_DURATION_MS;
static constexpr uint8_t MAX_RETRIES = PIN_MAX_RETRIES;
```

3. **Update `ctaphid.cpp`**:
```cpp
#include "cdc_core/SecurityConfig.h"
#define CTAPHID_RATE_LIMIT_WINDOW_MS  RATE_LIMIT_WINDOW_MS
#define CTAPHID_RATE_LIMIT_MAX_CMDS   RATE_LIMIT_MAX_CMDS
```

4. **Document in `platformio.ini`**:
```ini
build_flags =
    ; Security configuration
    -DKDF_DEFAULT_ITERATIONS=100000
    -DPIN_LOCKOUT_DURATION_MS=60000
    -DPIN_MAX_RETRIES=3
    -DRATE_LIMIT_MAX_CMDS=200
```

5. **Add to `CONFIGURATION.md`** (see finding #1):
   - Document each parameter with recommended values
   - Explain trade-offs (security vs. performance)
   - Provide examples for different security profiles

## References
- [NIST SP 800-63B - Password Guidelines](https://pages.nist.gov/800-63-3/sp800-63b.html#memauthpwd)
- [FIDO2 Rate Limiting Best Practices](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#rate-limiting)
- [OWASP Authentication Cheatsheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html)
