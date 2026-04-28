---
title: "[LOW] PIN hash exposed via public API without access control"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The `PinManager` class provides public methods to retrieve PIN hashes (`getBadgePinHash`, `getPW1Hash`, `getPW3Salt`, etc.) without any access control. Any component that includes the header can call these methods and obtain the hash values.

**File**: `components/cdc_core/include/cdc_core/PinManager.h:65-66`
```cpp
bool getBadgePinHash(uint8_t* hashOut) const;
bool verifyBadgePinHash(const uint8_t* hashIn) const;
```

**File**: `components/cdc_core/include/cdc_core/PinManager.h:73-74**
```cpp
bool getPW1Hash(uint8_t* hashOut) const;
bool getPW1Salt(uint8_t* saltOut) const;
```

**File**: `components/cdc_core/include/cdc_core/PinManager.h:80-81**
```cpp
bool getPW3Hash(uint8_t* hashOut) const;
bool getPW3Salt(uint8_t* saltOut) const;
```

These methods are public and can be called by any code that includes the header.

## Impact
- **Hash Exposure**: Any module can retrieve PIN hashes and potentially use them for offline brute-force attacks
- **Salt Exposure**: The salts are also exposed, allowing attackers to compute rainbow tables
- **FIDO2 PIN Hash**: The badge PIN hash is used by FIDO2, but could also be exposed to other modules
- **OpenPGP PIN Hashes**: PW1 and PW3 hashes could be extracted and used to brute-force the PINs

## Evidence
**File**: `components/cdc_core/include/cdc_core/PinManager.h:58-94`
All hash getter methods are public:
```cpp
// === Badge/FIDO2 PIN ===
bool getBadgePinHash(uint8_t* hashOut) const;
bool verifyBadgePinHash(const uint8_t* hashIn) const;

// === OpenPGP PW1 (User PIN) ===
bool getPW1Hash(uint8_t* hashOut) const;
bool getPW1Salt(uint8_t* saltOut) const;

// === OpenPGP PW3 (Admin PIN) ===
bool getPW3Hash(uint8_t* hashOut) const;
bool getPW3Salt(uint8_t* saltOut) const;
```

**File**: `components/mod_fido2/src/pin_storage.cpp:26-30**
```cpp
bool pin_storage_get_fido2_hash(uint8_t* hash_out) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.getBadgePinHash(hash_out);
}
```

The FIDO2 module uses these methods, but any other module could too.

## Recommended Fix
Restrict access to hash getters by:

**Option 1**: Make them private and only expose through friend classes:

```cpp
class PinManager {
public:
    // Public methods
    bool verifyBadgePin(const char* pin);
    bool verifyPW1(const char* pin);
    bool verifyPW3(const char* pin);
    
private:
    // Keep hash getters private
    bool getBadgePinHash(uint8_t* hashOut) const;
    bool getPW1Hash(uint8_t* hashOut) const;
    bool getPW3Hash(uint8_t* hashOut) const;

    // Allow specific modules to access
    friend struct Fido2PinStorage;
    friend struct GpgPinStorage;
};
```

**Option 2**: Add a permission check before returning hashes:

```cpp
bool PinManager::getBadgePinHash(uint8_t* hashOut) const {
    // Check if caller has permission (e.g., from FIDO2 module)
    if (!isCallerAuthorized("FIDO2")) {
        LOG_W(TAG, "Unauthorized hash retrieval");
        return false;
    }
    if (!hashOut) return false;
    memcpy(hashOut, badgeHash_, BADGE_HASH_SIZE);
    return true;
}
```

**Option 3**: Remove hash getters entirely and only expose verification:

```cpp
// Remove these methods:
// bool getBadgePinHash(uint8_t* hashOut) const;
// bool getPW1Hash(uint8_t* hashOut) const;
// bool getPW3Hash(uint8_t* hashOut) const;

// Keep only verification:
bool verifyBadgePin(const char* pin);
bool verifyPW1(const char* pin);
bool verifyPW3(const char* pin);
```

For FIDO2, use the hash internally without exposing it:

```cpp
// In FIDO2 module, compute hash and verify directly
bool pin_storage_verify_fido2_hash(const uint8_t* hash_in) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    return pm.verifyBadgePinHash(hash_in);  // Compare internally
}
```

## References
- OWASP Authentication Cheat Sheet - PIN Storage
- CWE-200: Information Exposure
- CWE-640: Weak Password Recovery Mechanism

</content>