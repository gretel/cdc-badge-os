---
title: "[MEDIUM] No latency/timing metrics for operations"
severity: MEDIUM
domain: observability/metrics
lens: latency-metrics
labels:
  - "metrics"
  - "observability"
  - "latency"
---

## Summary
The firmware has no latency tracking for operations. Critical operations like FIDO2 authentication, TOTP generation, secure element access, and PIN verification have no timing metrics to detect performance degradation or slow paths.

**Evidence:**
- `components/mod_fido2/src/Fido2Module.cpp` - CTAPHID packet processing with no timing
- `components/mod_totp/src/TotpStore.cpp` - Code generation with no latency tracking
- `components/cdc_core/src/PinManager.cpp` - PIN verification with no timing
- `components/cdc_hal/src/Tropic01Element.cpp` - Secure element operations with no latency metrics
- `esp_timer_get_time()` is used in `components/serial_cmd/src/SerialCmd.cpp:86` for auth timeout but not for operation timing

## Impact
Without latency metrics:
- **Performance regression detection impossible**: Cannot detect if operations are getting slower
- **Hardware issues hidden**: Cannot detect slow secure element or I2C bus issues
- **User experience blind**: Cannot measure actual authentication/generation times
- **Debug difficulty**: Hard to identify slow paths without timing data

## Evidence
**FIDO2 authentication flow** (`components/mod_fido2/src/fido2.cpp`):
- CTAPHID packet processing
- HMAC computation for signatures
- No timing captured

**TOTP generation** (`components/mod_totp/src/TotpStore.cpp`):
```cpp
bool TotpStore::generateCode(uint16_t slot, char* codeOut) {
    // Gets time, computes counter, HMAC, truncates
    // No timing metric captured
}
```

**PIN verification** (`components/cdc_core/src/PinManager.cpp`):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    // Compares PIN, updates retry counter
    // No timing metric captured
}
```

**Secure element operations** (`components/cdc_hal/src/Tropic01Element.cpp`):
- ECC key generation
- R-Memory read/write
- Session management
- No latency tracking

## Recommended Fix
1. **Add timing helper** (`components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   #include "esp_timer.h"
   
   // Simple timing helper
   class TimingScope {
   public:
       TimingScope(const char* metricName) : name_(metricName), start_(esp_timer_get_time()) {}
       ~TimingScope() {
           uint64_t durationUs = esp_timer_get_time() - start_;
           metrics_histogram(name_, durationUs);  // in microseconds
       }
   private:
       const char* name_;
       int64_t start_;
   };
   
   // Usage:
   // TimingScope timing("fido_auth");  // Automatically records on scope exit
   ```

2. **Instrument FIDO2** (`components/mod_fido2/src/Fido2Module.cpp`):
   ```cpp
   bool fido2_usb_write(const uint8_t* buffer) {
       TimingScope timing("fido_usb_write");
       if (!buffer) return false;
       return usb_hid_send_report(s_hid_instance, 0, buffer, CTAPHID_PACKET_SIZE);
   }
   ```

3. **Instrument TOTP** (`components/mod_totp/src/TotpStore.cpp`):
   ```cpp
   bool TotpStore::generateCode(uint16_t slot, char* codeOut) {
       TimingScope timing("totp_generate");
       // ... existing code ...
   }
   ```

4. **Instrument PIN** (`components/cdc_core/src/PinManager.cpp`):
   ```cpp
   bool PinManager::verifyBadgePin(const char* pin) {
       TimingScope timing("pin_verify");
       // ... existing code ...
   }
   ```

5. **Export latency metrics** in METRICS command:
   ```
   fido_auth_duration_us{quantile="50"} 1500
   fido_auth_duration_us{quantile="95"} 2500
   fido_auth_duration_us{quantile="99"} 3500
   totp_generate_duration_us 1200
   pin_verify_duration_us 50
   ```

## References
- Current timing usage: `components/serial_cmd/src/SerialCmd.cpp:86` (auth timeout)
- ESP timer API: `esp_timer_get_time()`
- Operation implementations:
  - FIDO2: `components/mod_fido2/src/fido2.cpp`
  - TOTP: `components/mod_totp/src/TotpStore.cpp`
  - PIN: `components/cdc_core/src/PinManager.cpp`
