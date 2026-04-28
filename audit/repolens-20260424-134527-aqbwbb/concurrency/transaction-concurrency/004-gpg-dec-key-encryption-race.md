---
title: "[MEDIUM] GPG DEC key save operation has race condition between session PIN and key storage"
severity: MEDIUM
domain: transaction-concurrency
lens: transaction-concurrency
labels:
  - audit:concurrency/transaction-concurrency
---

## Summary

In `components/mod_gpg/src/GpgStorage.cpp`, the `gpg_storage_save_dec_privkey()` method (lines 241-322) derives an encryption key from a PIN that is passed as a parameter. However, the session PIN is managed separately via `gpg_storage_set_session_pin()` (lines 453-464). If these two methods are called concurrently with different PINs, or if the session PIN changes between the call to set session and save key, the DEC key may be encrypted with the wrong PIN.

**Location:** `components/mod_gpg/src/GpgStorage.cpp:241-322, 453-464`

Additionally, the global storage state `s_storage` (lines 66-83) is accessed without synchronization:

```cpp
static struct {
    bool ready = false;
    // ...
    // Session state for verified PIN
    bool sessionActive = false;
    uint8_t sessionKey[32];  // HKDF-derived key from PIN
} s_storage;
```

## Impact

**Wrong PIN Encryption:**

1. User verifies PIN "123456" via UI
2. UI calls `gpg_storage_set_session_pin("123456")` - sets session key
3. Background task calls `gpg_storage_save_dec_privkey(key, "789012")` with different PIN
4. Key is encrypted with "789012" instead of "123456"
5. User cannot decrypt key later with expected PIN

**Session Key Staleness:**

1. User verifies PIN "123456" - session key derived
2. User changes PIN to "789012" - session key updated
3. Old code still holds reference to old session key
4. `gpg_storage_get_session_key()` returns stale key

**Race on sessionActive flag:**

1. Task A checks `s_storage.sessionActive` (true)
2. Task B calls `gpg_storage_clear_session()` (sets to false)
3. Task A calls `gpg_storage_get_session_key()` - may return wrong result

## Evidence

**gpg_storage_save_dec_privkey() - Uses parameter PIN, not session:**
```cpp
// Line 241-322: components/mod_gpg/src/GpgStorage.cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    if (!privkey) {
        LOG_E(TAG, "Invalid parameters for save_dec_privkey");
        return false;
    }
    // Note: pin can be NULL - derive_key_from_pin will use device key in that case

    auto* se = get_se();
    if (!se) {
        LOG_E(TAG, "Secure element not available");
        return false;
    }

    // Derive encryption key from PIN (parameter, not session)
    uint8_t enc_key[32];
    if (!derive_key_from_pin(pin, enc_key)) {  // Line 259 - Uses parameter pin
        LOG_E(TAG, "Failed to derive encryption key");
        return false;
    }

    // ... prepare storage ...
    
    // Derive encryption key from PIN
    uint8_t enc_key[32];
    if (!derive_key_from_pin(pin, enc_key)) {  // Line 259 - Uses parameter pin
        // ...
    }

    // ... encrypt and save ...
}
```

**gpg_storage_set_session_pin() - Sets global session state:**
```cpp
// Line 453-464: components/mod_gpg/src/GpgStorage.cpp
void gpg_storage_set_session_pin(const char* pin) {
    if (!pin) {
        gpg_storage_clear_session();
        return;
    }

    if (derive_key_from_pin(pin, s_storage.sessionKey)) {  // Line 462 - Writes global
        s_storage.sessionActive = true;  // Line 463 - Writes global
        LOG_D(TAG, "Session key derived from PIN");
    }
}
```

**gpg_storage_get_session_key() - Reads global session state:**
```cpp
// Line 467-479: components/mod_gpg/src/GpgStorage.cpp
bool gpg_storage_get_session_key(uint8_t* key_out) {
    if (!s_storage.sessionActive || !key_out) {  // Line 469 - Reads global
        return false;
    }

    memcpy(key_out, s_storage.sessionKey, 32);  // Line 477 - Reads global
    return true;
}
```

**gpg_storage_clear_session() - Clears global session state:**
```cpp
// Line 482-488: components/mod_gpg/src/GpgStorage.cpp
void gpg_storage_clear_session(void) {
    mbedtls_platform_zeroize(s_storage.sessionKey, sizeof(s_storage.sessionKey));  // Line 484
    s_storage.sessionActive = false;  // Line 485
    LOG_D(TAG, "Session cleared");
}
```

**Global storage state - No synchronization:**
```cpp
// Line 66-83: components/mod_gpg/src/GpgStorage.cpp
static struct {
    bool ready = false;
    uint16_t eccStart = 0;
    uint16_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t sigSlot = 0;
    uint8_t decSlot = 0;
    uint8_t autSlot = 0;

    // Session state for verified PIN
    bool sessionActive = false;
    uint8_t sessionKey[32];  // HKDF-derived key from PIN
} s_storage;
```

## Recommended Fix

**Option 1: Make session state thread-safe with mutex**

```cpp
// Add mutex to storage state
static struct {
    bool ready = false;
    uint16_t eccStart = 0;
    uint16_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t sigSlot = 0;
    uint8_t decSlot = 0;
    uint8_t autSlot = 0;

    // Session state for verified PIN
    bool sessionActive = false;
    uint8_t sessionKey[32];  // HKDF-derived key from PIN
} s_storage;

static SemaphoreHandle_t s_storageMutex = nullptr;

// In gpg_storage_init() or similar:
if (!s_storageMutex) {
    s_storageMutex = xSemaphoreCreateMutex();
}

// In gpg_storage_set_session_pin():
void gpg_storage_set_session_pin(const char* pin) {
    if (!s_storageMutex) return;
    
    xSemaphoreTake(s_storageMutex, portMAX_DELAY);
    
    if (!pin) {
        xSemaphoreGive(s_storageMutex);
        gpg_storage_clear_session();
        return;
    }

    uint8_t key[32];
    if (derive_key_from_pin(pin, key)) {
        memcpy(s_storage.sessionKey, key, 32);
        s_storage.sessionActive = true;
        LOG_D(TAG, "Session key derived from PIN");
    }
    
    xSemaphoreGive(s_storageMutex);
    
    mbedtls_platform_zeroize(key, sizeof(key));
}

// In gpg_storage_get_session_key():
bool gpg_storage_get_session_key(uint8_t* key_out) {
    if (!s_storageMutex || !key_out) return false;
    
    xSemaphoreTake(s_storageMutex, portMAX_DELAY);
    
    bool result = false;
    if (s_storage.sessionActive) {
        memcpy(key_out, s_storage.sessionKey, 32);
        result = true;
    }
    
    xSemaphoreGive(s_storageMutex);
    return result;
}
```

**Option 2: Use session PIN consistently**

If the design intent is to use the session PIN for saving DEC keys, change `gpg_storage_save_dec_privkey()` to use the session:

```cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    // If pin is NULL, use session PIN
    if (!pin) {
        uint8_t sessionKey[32];
        if (gpg_storage_get_session_key(sessionKey)) {
            // Use session key for encryption
            // ...
        } else {
            LOG_E(TAG, "No session PIN available");
            return false;
        }
    } else {
        // Use provided PIN
        // ...
    }
}
```

## References

1. **FreeRTOS Mutex** - [xSemaphoreCreateMutex](https://www.freertos.org/a00127.html)
2. **GPG Smart Card** - OpenPGP card session management
3. **Key Derivation Functions** - HKDF consistency requirements
