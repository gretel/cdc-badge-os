---
title: "[HIGH] No business/operational metrics for core functions"
severity: HIGH
domain: observability/metrics
lens: business-metrics
labels:
  - "metrics"
  - "observability"
  - "fido2"
  - "totp"
---

## Summary
The firmware lacks counters for core business operations. Key functions like FIDO2 authentications, TOTP code generations, PIN verifications, and secure element operations have no instrumentation to track volume, frequency, or success rates.

**Evidence:**
- `components/mod_fido2/src/Fido2Module.cpp` - No metrics for CTAPHID operations, credential count, authentication successes/failures
- `components/mod_totp/src/TotpModule.cpp` - No metrics for code generations, account additions/deletions
- `components/cdc_core/src/PinManager.cpp` - No metrics for PIN attempts (success/failure)
- `components/cdc_hal/src/Tropic01Element.cpp` - No metrics for secure element operations (ECC key generation, R-Memory reads/writes)

## Impact
Without business metrics:
- **Usage analytics impossible**: Cannot answer "how many FIDO2 authentications per day?"
- **Feature adoption unclear**: Cannot track which modules are actively used
- **Troubleshooting blind**: Hard to detect if a feature broke (no baseline to compare)
- **Capacity planning missing**: Cannot predict when slot storage will be full

## Evidence
**FIDO2 Module** (`components/mod_fido2/src/Fido2Module.cpp`):
- CTAPHID packet processing with no counter increments
- HID interface registration but no authentication tracking

**TOTP Module** (`components/mod_totp/src/TotpModule.cpp`):
- `generateCode()` function but no metrics for code generations
- `addAccount()`/`deleteAccount()` but no tracking of account management

**PIN Manager** (`components/cdc_core/src/PinManager.cpp`):
- Retry counters for security but no operational metrics
- No distinction between successful vs failed attempts in metrics

## Recommended Fix
Add simple counter API calls to key operations:

1. **Define metrics** (add to `components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   // FIDO2 metrics
   #define METRIC_FIDO_AUTH_SUCCESS "fido_auth_success"
   #define METRIC_FIDO_AUTH_FAILURE "fido_auth_failure"
   #define METRIC_FIDO_CREDENTIALS "fido_credentials_total"
   
   // TOTP metrics
   #define METRIC_TOTP_GENERATIONS "totp_generations_total"
   #define METRIC_TOTP_ACCOUNTS "totp_accounts_total"
   
   // PIN metrics
   #define METRIC_PIN_ATTEMPTS "pin_attempts_total"
   #define METRIC_PIN_SUCCESSES "pin_successes_total"
   ```

2. **Instrument FIDO2** (`components/mod_fido2/src/fido2.cpp`):
   ```cpp
   // In authentication success path
   metrics_increment(METRIC_FIDO_AUTH_SUCCESS);
   
   // In authentication failure path
   metrics_increment(METRIC_FIDO_AUTH_FAILURE);
   ```

3. **Instrument TOTP** (`components/mod_totp/src/TotpModule.cpp`):
   ```cpp
   // In generateCode()
   metrics_increment(METRIC_TOTP_GENERATIONS);
   
   // In addAccount()/deleteAccount()
   metrics_set_gauge(METRIC_TOTP_ACCOUNTS, currentCount);
   ```

4. **Instrument PIN manager** (`components/cdc_core/src/PinManager.cpp`):
   ```cpp
   // In verifyBadgePin()
   metrics_increment(METRIC_PIN_ATTEMPTS);
   if (success) metrics_increment(METRIC_PIN_SUCCESSES);
   ```

## References
- Current FIDO2 implementation: `components/mod_fido2/src/fido2.cpp`
- Current TOTP implementation: `components/mod_totp/src/TotpStore.cpp`
- PIN management: `components/cdc_core/src/PinManager.cpp`
