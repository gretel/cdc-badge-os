---
title: "[HIGH] GPG module mixes enum and macro constants for key types"
severity: HIGH
domain: mod_gpg
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The GPG module uses two different representations for key types:
1. An enum `key_type_t` with values `KEY_TYPE_SIG`, `KEY_TYPE_DEC`, `KEY_TYPE_AUT`
2. Macros `KEY_SIG`, `KEY_DEC`, `KEY_AUT` (defined as `0xB6`, `0xB8`, `0xA4`)

These are used interchangeably in switch statements, creating a risk of:
- **Logic bugs**: Using the wrong constant type in comparisons
- **Silent failures**: Macros expand to integer literals that don't match enum values
- **Maintenance confusion**: Developers may not realize there are two different representations

**Locations:**
- `components/mod_gpg/src/openpgp/openpgp.cpp:322` - Switch uses `KEY_TYPE_SIG/DEC/AUT`
- `components/mod_gpg/src/openpgp/openpgp.cpp:656` - Switch uses `KEY_SIG/DEC/AUT` (macros)
- `components/mod_gpg/src/openpgp/openpgp.cpp:1420` - Switch uses `KEY_SIG/DEC/AUT` (macros)
- `components/mod_gpg/include/mod_gpg/openpgp/openpgp.h:37-39` - Macro definitions

## Impact
**Runtime logic errors**: The enum values are `0, 1, 2` while the macros are `0xB6, 0xB8, 0xA4`. If a function expecting one type receives the other, comparisons will fail silently.

**Example vulnerability:**
```cpp
// In get_algo_attr(), key_type is key_type_t (enum)
switch (key_type) {
    case KEY_TYPE_SIG:  // 0 - correct
    case KEY_TYPE_DEC:  // 1 - correct
    case KEY_TYPE_AUT:  // 2 - correct
}

// In set_key_fingerprint(), key_type is uint8_t
switch (key_type) {
    case KEY_SIG:       // 0xB6 - different from KEY_TYPE_SIG!
    case KEY_DEC:       // 0xB8 - different from KEY_TYPE_DEC!
    case KEY_AUT:       // 0xA4 - different from KEY_TYPE_AUT!
}
```

If code passes `KEY_TYPE_SIG` (value 0) where `KEY_SIG` (value 0xB6) is expected, the switch will fall through to default.

## Evidence
In `components/mod_gpg/src/openpgp/openpgp.cpp`:

```cpp
// Line 308-312: Enum definition
typedef enum {
    KEY_TYPE_SIG = 0,  // Signature (ECDSA/EdDSA)
    KEY_TYPE_DEC = 1,  // Decryption (ECDH)
    KEY_TYPE_AUT = 2   // Authentication (ECDSA/EdDSA)
} key_type_t;

// Line 322-326: Switch using enum values
static const uint8_t* get_algo_attr(key_type_t key_type, size_t *len) {
    switch (key_type) {
        case KEY_TYPE_SIG: slot = gpg_storage_sig_slot(); break;
        case KEY_TYPE_DEC: slot = gpg_storage_dec_slot(); break;
        case KEY_TYPE_AUT: slot = gpg_storage_aut_slot(); break;
        default:           slot = gpg_storage_sig_slot(); break;
    }
}

// Line 656-672: Switch using macro values (different type!)
static bool set_key_fingerprint(uint8_t key_type, const uint8_t *fingerprint, const uint8_t *ts) {
    switch (key_type) {
        case KEY_SIG:     // 0xB6 - NOT the same as KEY_TYPE_SIG (0)!
        case KEY_DEC:     // 0xB8 - NOT the same as KEY_TYPE_DEC (1)!
        case KEY_AUT:     // 0xA4 - NOT the same as KEY_TYPE_AUT (2)!
        default:
            ESP_LOGE(TAG, "Invalid key type: 0x%02X", key_type);
            return false;
    }
}
```

In `components/mod_gpg/include/mod_gpg/openpgp/openpgp.h`:

```cpp
// Line 37-39: Macro definitions (different values!)
#define KEY_SIG         0xB6  // Signature key
#define KEY_DEC         0xB8  // Decryption key
#define KEY_AUT         0xA4  // Authentication key
```

## Recommended Fix
Consolidate to a single type-safe representation. Use the enum everywhere and map to APDU constants only at the boundary:

```cpp
// In openpgp.h
typedef enum {
    KEY_TYPE_SIG = 0,  // Signature (ECDSA/EdDSA)
    KEY_TYPE_DEC = 1,  // Decryption (ECDH)
    KEY_TYPE_AUT = 2   // Authentication (ECDSA/EdDSA)
} key_type_t;

// Add conversion helper
static inline uint8_t key_type_to_apdu(key_type_t kt) {
    switch (kt) {
        case KEY_TYPE_SIG: return 0xB6;
        case KEY_TYPE_DEC: return 0xB8;
        case KEY_TYPE_AUT: return 0xA4;
    }
    return 0xB6; // Default
}

static inline key_type_t apdu_to_key_type(uint8_t apdu) {
    switch (apdu) {
        case 0xB6: return KEY_TYPE_SIG;
        case 0xB8: return KEY_TYPE_DEC;
        case 0xA4: return KEY_TYPE_AUT;
    }
    return KEY_TYPE_SIG; // Default
}

// Remove old macros or rename them:
// #define KEY_SIG -> KEY_TYPE_SIG (but this breaks APDU usage)
// Better: Keep macros only for APDU layer
#define KEY_APDU_SIG  0xB6
#define KEY_APDU_DEC  0xB8
#define KEY_APDU_AUT  0xA4
```

Update `set_key_fingerprint` to use enum:
```cpp
static bool set_key_fingerprint(key_type_t key_type, const uint8_t *fingerprint, const uint8_t *ts) {
    switch (key_type) {
        case KEY_TYPE_SIG:
        case KEY_TYPE_DEC:
        case KEY_TYPE_AUT:
            // ...
    }
}
```

For APDU boundary code, use conversion:
```cpp
// In APDU handler
key_type_t kt = apdu_to_key_type(apdu_key_ref);
if (kt == KEY_TYPE_SIG) {
    // Handle signature key
}
```

## References
- C++ Core Guidelines, C.16: "If a class has a simple set of values, use an enum"
- C++ Core Guidelines, C.9: "Use class enum (scoped enum) for type-safe enumerations"
- MISRA C:2012 Rule 7.2: "A "lower-case" character shall not be mixed with an "upper-case" character in a string"

</content>