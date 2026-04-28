---
title: "[LOW] Hardcoded PIN policy constants in PinManager"
severity: LOW
domain: architecture/extensibility
lens: extensibility-plugin-points
labels:
  - "audit:architecture/extensibility"
---

## Summary
PIN policy constants (min/max length, retries, lockout duration) are hardcoded in `components/cdc_core/include/cdc_core/PinManager.h`. These security-relevant parameters should be centralized and potentially configurable.

**Evidence:**
- `components/cdc_core/include/cdc_core/PinManager.h:14-39` - PIN policy constants

## Impact
- **Security Tuning**: Changing PIN policy requires editing core header
- **Flexibility**: No runtime configuration for PIN policy
- **Maintenance**: Security parameters scattered across implementation

## Evidence
File: `components/cdc_core/include/cdc_core/PinManager.h:14-39`
```cpp
static constexpr uint8_t BADGE_PIN_MIN = 4;
static constexpr uint8_t BADGE_PIN_MAX = 8;
static constexpr uint8_t PW1_MIN = 6;
static constexpr uint8_t PW3_MIN = 8;
static constexpr uint8_t PIN_MAX = 16;
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
static constexpr uint8_t MAX_RETRIES = 3;
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

## Recommended Fix
Centralize PIN policy configuration:

1. **Create PIN config header**:
   ```cpp
   // components/cdc_core/include/cdc_core/pin_config.h
   #pragma once
   
   namespace cdc::core::config {
       // Badge PIN (for serial authentication)
       constexpr uint8_t BADGE_PIN_MIN_LEN = 4;
       constexpr uint8_t BADGE_PIN_MAX_LEN = 8;
       constexpr uint8_t BADGE_MAX_RETRIES = 3;
       constexpr uint32_t BADGE_LOCKOUT_MS = 60000;
       
       // PW1 (for GPG/SSH)
       constexpr uint8_t PW1_MIN_LEN = 6;
       
       // PW3 (for FIDO2)
       constexpr uint8_t PW3_MIN_LEN = 8;
       
       // General
       constexpr uint8_t PIN_MAX_LEN = 16;
   }
   ```

2. **Update PinManager to use config**:
   ```cpp
   #include "cdc_core/pin_config.h"
   using namespace cdc::core::config;
   
   class PinManager {
   public:
       static constexpr uint8_t BADGE_PIN_MIN = BADGE_PIN_MIN_LEN;
       static constexpr uint8_t BADGE_PIN_MAX = BADGE_PIN_MAX_LEN;
       static constexpr uint8_t MAX_RETRIES = BADGE_MAX_RETRIES;
       static constexpr uint32_t LOCKOUT_DURATION_MS = BADGE_LOCKOUT_MS;
   };
   ```

3. **Optional: Runtime configuration** (for different profiles):
   ```cpp
   struct PinPolicy {
       uint8_t minLength;
       uint8_t maxLength;
       uint8_t maxRetries;
       uint32_t lockoutMs;
   };
   
   class PinManager {
   public:
       void setPolicy(const PinPolicy& policy);
       const PinPolicy& getPolicy() const;
   };
   ```

This centralizes PIN policy for easier security tuning.

## References
- NIST SP 800-63B: Digital Identity Guidelines for PIN policies
- Security Configuration: Centralize security-relevant parameters
