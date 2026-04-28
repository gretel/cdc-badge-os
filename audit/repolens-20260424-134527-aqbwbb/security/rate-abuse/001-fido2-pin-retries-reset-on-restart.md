---
title: "[HIGH] FIDO2 PIN retry counter resets on device restart, enabling brute-force attacks"
severity: HIGH
domain: rate-abuse
lens: rate-abuse-fido2
labels:
  - "audit:security/rate-abuse"
---

## Summary
The FIDO2 ClientPIN implementation in `components/mod_fido2/src/ctap2.cpp` stores PIN retry counters in RAM only (`g_client_pin.pin_retries`). The counter is reset to `PIN_RETRIES_MAX` (8) whenever `ctap2_client_pin()` is called with `g_client_pin.initialized == false`, which happens after every device power-cycle or restart.

**Location:** `components/mod_fido2/src/ctap2.cpp:2872-2879`

```cpp
uint8_t ctap2_client_pin(const uint8_t *params, uint16_t params_len,
                          uint8_t *response, uint16_t *response_len) {
    // Initialize if needed
    if (!g_client_pin.initialized) {
        g_client_pin.pin_retries = PIN_RETRIES_MAX;  // Reset to 8!
        g_client_pin.uv_retries = PIN_UV_RETRIES_MAX;
        g_client_pin.initialized = true;
    }
    ...
}
```

The retry counter is decremented on failed PIN attempts (lines 2573, 2810) but this state is never persisted to the TROPIC01 secure element's R-Memory.

## Impact
An attacker can brute-force the FIDO2 PIN by:
1. Sending 8 PIN guesses via the FIDO2 `getPinToken` or `getPinUvAuthToken` commands
2. When all retries are exhausted (PIN blocked), power-cycle the badge
3. On restart, retry counters reset to 8, allowing another 8 guesses
4. Repeat until the PIN is found

With an 8-digit PIN (10^8 combinations) and 8 attempts per power-cycle, this reduces the effective security significantly. A determined attacker with automated power-control (e.g., smart outlet) could exhaust the key space.

## Evidence
- **File:** `components/mod_fido2/src/ctap2.cpp`
- **Line 77:** `#define PIN_RETRIES_MAX 8`
- **Lines 2875-2878:** Retry counter reset on initialization
- **Lines 2573-2576, 2810-2811:** Decrement on failure, but no persistence
- **Line 2012:** Initial value set to `PIN_RETRIES_MAX`

Compare with Badge PIN in `components/cdc_core/src/PinManager.cpp:323` which persists retry count to storage:
```cpp
badgeRetries_--;
saveToStorage();  // Persist retry count
```

## Recommended Fix
Persist FIDO2 PIN retry counters to TROPIC01 R-Memory, similar to how Badge PIN retries are stored in R-Memory slot 0.

**Option 1: Store in existing PIN storage (R-Memory slot 0)**
- Add 1 byte for FIDO2 PIN retries after the existing Badge PIN data
- Load/save FIDO2 retries alongside Badge PIN data in `PinManager`
- Update FIDO2 module to read/write retries from `PinManager`

**Option 2: Dedicated storage in FIDO2 module**
- Add R-Memory slot for FIDO2 state (retries + last timestamp)
- Implement persistence in `fido2_storage.cpp`
- Load retries on module init, save after each failed attempt

**Implementation steps:**
1. Add FIDO2 retry field to PIN storage format (1 byte)
2. Update `PinManager::saveToStorage()` and `loadFromStorage()` to include FIDO2 retries
3. Add accessor methods: `getFido2Retries()`, `setFido2Retries()`
4. Modify `ctap2_client_pin()` to load retries from storage instead of hard-coding
5. Update retry decrement logic to call `setFido2Retries()` after each failure

## References
- FIDO CTAP2 spec: [Client PIN](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#authenticating-a-user)
- Similar pattern in codebase: `components/cdc_core/src/PinManager.cpp:323` (Badge PIN persistence)
