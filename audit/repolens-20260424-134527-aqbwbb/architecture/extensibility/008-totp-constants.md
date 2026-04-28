---
title: "[LOW] Hardcoded TOTP algorithm constants in TotpStore"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
TOTP algorithm constants (digits, period) are hardcoded in `components/mod_totp/include/mod_totp/TotpStore.h` as static constexpr members. While these are well-defined, they could be externalized for flexibility.

**Evidence:**
- `components/mod_totp/include/mod_totp/TotpStore.h:22-26` - Algorithm constants

## Impact
- **Flexibility**: Changing default TOTP parameters requires recompiling
- **Configuration**: No runtime configuration for TOTP defaults

## Evidence
File: `components/mod_totp/include/mod_totp/TotpStore.h:22-26`
```cpp
static constexpr uint8_t DEFAULT_DIGITS = 6;
static constexpr uint32_t DEFAULT_PERIOD = 30;
```

Usage in `TotpStore.cpp:388-400`:
```cpp
uint32_t TotpStore::generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                             uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const {
    // ...
    if (digits < 6 || digits > 8) {
        digits = DEFAULT_DIGITS;
    }
    
    if (period == 0) {
        period = DEFAULT_PERIOD;
    }
    // ...
}
```

## Recommended Fix
Externalize TOTP configuration:

1. **Create TOTP config header**:
   ```cpp
   // components/mod_totp/include/mod_totp/totp_config.h
   #pragma once
   
   namespace cdc::mod_totp::config {
       constexpr uint8_t DEFAULT_DIGITS = 6;
       constexpr uint32_t DEFAULT_PERIOD = 30;
       constexpr uint8_t MIN_DIGITS = 6;
       constexpr uint8_t MAX_DIGITS = 8;
       constexpr uint32_t MIN_PERIOD = 10;
       constexpr uint32_t MAX_PERIOD = 120;
       constexpr uint8_t MAX_SECRET_LEN = 32;
   }
   ```

2. **Update TotpStore to use config**:
   ```cpp
   #include "mod_totp/totp_config.h"
   
   using namespace cdc::mod_totp::config;
   
   uint32_t TotpStore::generate(...) const {
       if (digits < MIN_DIGITS || digits > MAX_DIGITS) {
           digits = DEFAULT_DIGITS;
       }
       
       if (period == 0) {
           period = DEFAULT_PERIOD;
       }
       // ...
   }
   ```

3. **Optional: Runtime configuration via NVS**:
   ```cpp
   struct TotpDefaults {
       uint8_t digits;
       uint32_t period;
       
       void loadFromNvs();
       void saveToNvs();
   };
   ```

This allows TOTP defaults to be configured without recompiling.

## References
- Configuration Management: Externalize tunable parameters
- TOTP RFC 6238: Standard defines 30-second period, 6-8 digits
