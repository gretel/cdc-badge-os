---
title: "[MEDIUM] GPG module stores session PIN in plaintext RAM"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The GPG module caches the verified PIN in a global variable `s_session_pin` stored in plaintext RAM. This allows the PIN to be used for PSO:DECIPHER (ECDH decryption) operations without re-verification, but the PIN remains in memory until explicitly cleared.

**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:145`
```cpp
static char s_session_pin[OPENPGP_PIN_MAX_LEN + 1] = {};  // Plaintext PIN in RAM
```

**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:1084-1088`
```cpp
if (verified) {
    pw1_verified = true;
    // Store session PIN for PSO:DECIPHER (ECDH decryption)
    strncpy(s_session_pin, pin_str, OPENPGP_PIN_MAX_LEN);
    s_session_pin[OPENPGP_PIN_MAX_LEN] = '\0';  // PIN stored in plaintext
    ESP_LOGI(TAG, "PW1 verified successfully");
}
```

The session PIN is cleared only in specific scenarios:
**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:690-700`
```cpp
static void cmd_reset_status(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    pw1_verified = false;
    pw3_verified = false;
    // Clear session PIN
    if (s_session_pin[0] != '\0') {
        mbedtls_platform_zeroize(s_session_pin, sizeof(s_session_pin));
    }
    return apdu_sw(resp, SW_OK);
}

static void cmd_select_application(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // Clear session PIN on app re-selection
    if (s_session_pin[0] != '\0') {
        mbedtls_platform_zeroize(s_session_pin, sizeof(s_session_pin));
    }
}
```

The session PIN is used for PSO:DECIPHER operations:
**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:1295-1310`
```cpp
// PW1 must be verified for decryption operations
if (!pw1_verified) {
    ESP_LOGW(TAG, "PW1 not verified for PSO:DECIPHER");
    return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
}

// Use session PIN to decrypt data key
// ...
```

## Impact
- **RAM Exposure**: The PIN remains in plaintext RAM until cleared, vulnerable to memory dump attacks
- **Extended Lifetime**: The PIN persists across multiple operations until explicitly cleared or app re-selected
- **No Automatic Expiry**: Unlike typical session tokens, there's no automatic timeout for the session PIN
- **Decryption Access**: Anyone with access to the GPG module can use the session PIN for decryption without re-verification
- **No PIN Token**: Unlike FIDO2, the session PIN is the actual PIN, not an encrypted token

## Evidence
**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:145`
```cpp
static char s_session_pin[OPENPGP_PIN_MAX_LEN + 1] = {};  // Plaintext string
```

**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:1084-1088`
PIN is copied directly from user input without encryption.

**File**: `components/mod_gpg/src/openpgp/openpgp.cpp:690-700`
PIN is only cleared on explicit reset or app re-selection, not automatically.

**Search for automatic clear**:
```
grep -n "s_session_pin\|timeout\|expiry" components/mod_gpg/src/openpgp/openpgp.cpp
# No automatic timeout found
```

## Recommended Fix
1. **Clear after use**: Clear `s_session_pin` after each PSO:DECIPHER operation
2. **Add timeout**: Implement automatic expiration after a configurable period (e.g., 2-5 minutes)
3. **Use encrypted token**: Instead of storing the PIN, generate an encrypted session token similar to FIDO2

Example implementation for clearing after use:
```cpp
static int cmd_pso_decipher(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    // PW1 must be verified for decryption operations
    if (!pw1_verified) {
        ESP_LOGW(TAG, "PW1 not verified for PSO:DECIPHER");
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    // Use session PIN to decrypt data key
    // ... decryption logic ...

    // Clear session PIN after use (one-time use)
    if (s_session_pin[0] != '\0') {
        mbedtls_platform_zeroize(s_session_pin, sizeof(s_session_pin));
    }
    pw1_verified = false;  // Reset verification flag

    return apdu_sw(resp, SW_OK);
}
```

Example implementation for timeout:
```cpp
static struct {
    char session_pin[OPENPGP_PIN_MAX_LEN + 1];
    uint32_t expiry_time_ms;  // Timestamp when PIN expires
} s_session = {};

#define SESSION_TIMEOUT_MS (5 * 60 * 1000)  // 5 minutes

static bool is_session_expired(void) {
    if (s_session.session_pin[0] == '\0') return true;
    uint32_t now = esp_timer_get_time() / 1000;
    return now >= s_session.expiry_time_ms;
}

static void set_session_expiry(void) {
    s_session.expiry_time_ms = esp_timer_get_time() / 1000 + SESSION_TIMEOUT_MS;
}

static void clear_session(void) {
    if (s_session.session_pin[0] != '\0') {
        mbedtls_platform_zeroize(s_session.session_pin, sizeof(s_session.session_pin));
    }
    pw1_verified = false;
}

// In cmd_verify() after successful PIN verification
strncpy(s_session.session_pin, pin_str, OPENPGP_PIN_MAX_LEN);
set_session_expiry();

// In cmd_pso_decipher()
if (is_session_expired()) {
    clear_session();
    ESP_LOGW(TAG, "Session expired");
    return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
}
```

## References
- NIST SP 800-63B - Session Management
- OpenPGP Card Specification - Resetting Verification Status
- OWASP Session Management Cheat Sheet
- CWE-613: Insufficient Session Expiration

</content>