---
title: "[MEDIUM] Session PIN not cleared after use in OpenPGP module"
severity: MEDIUM
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The OpenPGP module stores the verified PIN in a global `s_session_pin` buffer for use in PSO:DECIPHER, but never clears it after use, leaving the PIN in memory indefinitely.

**Location:** `components/mod_gpg/src/openpgp/openpgp.cpp:145, 1086-1087`

## Impact

**The session PIN persists in memory:**
1. After PW1 verification, the PIN is stored in `s_session_pin`
2. The PIN remains there until:
   - Next PIN verification (overwritten)
   - Module reset/reinitialization
   - Power cycle
3. No explicit clearing after PSO:DECIPHER operation

**Example flow:**
```cpp
// Line 1086-1087: PIN stored after verification
strncpy(s_session_pin, pin_str, OPENPGP_MAX_LEN);
s_session_pin[OPENPGP_PIN_MAX_LEN] = '\0';

// PIN is used later in PSO:DECIPHER (line 1472 checks pw3_verified)
// But s_session_pin is never cleared!
```

**Risk scenarios:**
1. **Stack inspection**: An attacker with memory access can read the global buffer
2. **Core dumps**: ESP32 core dumps will contain the PIN
3. **Debug access**: JTAG/UART debugging can reveal the PIN
4. **Multi-tenant scenarios**: If multiple processes share RAM

**Additional concern**: The `pin_str` buffer on line 1069 is also not cleared:
```cpp
char pin_str[OPENPGP_PIN_MAX_LEN + 1];
memcpy(pin_str, apdu->data, apdu->lc);
pin_str[apdu->lc] = '\0';
// ... used for verification ...
// Not cleared after use!
```

## Evidence

**File: `components/mod_gpg/src/openpgp/openpgp.cpp`**

Line 145 (global storage):
```cpp
static char s_session_pin[OPENPGP_PIN_MAX_LEN + 1] = {};
```

Line 1069-1087 (PIN stored but not cleared):
```cpp
char pin_str[OPENPGP_PIN_MAX_LEN + 1];
if (apdu->lc > OPENPGP_PIN_MAX_LEN) {
    return apdu_sw(resp, SW_WRONG_LENGTH);
}
memcpy(pin_str, apdu->data, apdu->lc);
pin_str[apdu->lc] = '\0';

// Verify PIN via TROPIC01 storage
bool verified = false;
if (pw_ref == 0x81 || pw_ref == 0x82) {
    verified = pin_storage_openpgp_verify_pw1(pin_str);
    if (verified) {
        pw1_verified = true;
        // Store session PIN for PSO:DECIPHER (ECDH decryption)
        strncpy(s_session_pin, pin_str, OPENPGP_PIN_MAX_LEN);
        s_session_pin[OPENPGP_PIN_MAX_LEN] = '\0';
        ESP_LOGI(TAG, "PW1 verified successfully");
    }
    // ...
}
// pin_str and s_session_pin not cleared!
```

Line 1472 (PIN used later):
```cpp
if (!pw3_verified) {
    // Check session PIN for DEC key
    if (strlen(s_session_pin) == 0) {
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }
    // ... use s_session_pin for decryption ...
}
```

## Recommended Fix

Clear PIN buffers after use:

```cpp
// After verification (line 1087)
if (verified) {
    return apdu_sw(resp, SW_OK);
}

// Clear PIN buffers on success
memset(pin_str, 0, sizeof(pin_str));
// s_session_pin will be used later, so don't clear it yet

// After PSO:DECIPHER completes (add new function)
void openpgp_clear_session_pin(void) {
    memset(s_session_pin, 0, sizeof(s_session_pin));
}

// Call after PSO:DECIPHER
static uint8_t openpgp_pso_decipher(const apdu_t* apdu, apdu_response_t* resp) {
    // ... use s_session_pin ...
    
    // Clear after use
    openpgp_clear_session_pin();
    
    return SW_OK;
}
```

Or clear `pin_str` immediately after verification:
```cpp
if (pw_ref == 0x81 || pw_ref == 0x82) {
    verified = pin_storage_openpgp_verify_pw1(pin_str);
    if (verified) {
        pw1_verified = true;
        strncpy(s_session_pin, pin_str, OPENPGP_PIN_MAX_LEN);
        s_session_pin[OPENPGP_PIN_MAX_LEN] = '\0';
        ESP_LOGI(TAG, "PW1 verified successfully");
    }
    // Clear local buffer immediately
    memset(pin_str, 0, sizeof(pin_str));
    retries = pin_storage_openpgp_pw1_retries();
}
```

## References

- CWE-311: Missing Encryption of Sensitive Data (in-memory)
- [OWASP: Clear Sensitive Data from Memory](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html#erase-passwords-from-memory)
- [OpenPGP Card Specification](https://www.openpgp-card.org/documentation/for-developers/apdu-reference.html)
