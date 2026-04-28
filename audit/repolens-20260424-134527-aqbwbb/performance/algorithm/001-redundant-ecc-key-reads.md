---
title: "[MEDIUM] Redundant ECC key reads in get_algo_attr() - O(n) secure-element reads instead of cached lookup"
severity: MEDIUM
domain: mod_gpg
lens: algorithm-efficiency
labels:
  - "redundant-computation"
  - "secure-element-access"
---

## Summary
In `components/mod_gpg/src/openpgp/openpgp.cpp`, the `get_algo_attr()` function (lines 320-353) performs a **secure-element ECC key read** on every call to determine the curve type. This function is called **3 times** in `build_do_app_related()` (lines 383, 388, 393) and **3 more times** in `cmd_get_data()` (lines 746, 752, 758), resulting in **6 separate secure-element reads** per APDU response.

Each call to `get_algo_attr()` executes:
```cpp
if (se_ecc_key_read(slot, pubkey, sizeof(pubkey), &curve)) {
    // Key exists, curve is now set
}
```

Where `se_ecc_key_read()` performs an I2C transaction to the TROPIC01 secure element.

## Impact
- **Performance**: 6 I2C transactions per `GET DATA 0x6E` or algorithm-attribute query (each ~1-5ms depending on bus speed)
- **Latency**: Adds 6-30ms to APDU response time for application-related data
- **Power**: Unnecessary secure-element wakeups increase power consumption
- **Scalability**: As more algorithm attributes are queried, the I2C bus becomes a bottleneck

## Evidence
**File: `components/mod_gpg/src/openpgp/openpgp.cpp`**

Lines 320-335:
```cpp
static const uint8_t* get_algo_attr(key_type_t key_type, size_t *len) {
    uint8_t slot = gpg_storage_sig_slot();
    switch (key_type) {
        case KEY_TYPE_SIG: slot = gpg_storage_sig_slot(); break;
        case KEY_TYPE_DEC: slot = gpg_storage_dec_slot(); break;
        case KEY_TYPE_AUT: slot = gpg_storage_aut_slot(); break;
        default:           slot = gpg_storage_sig_slot(); break;
    }

    // Try to read existing key to get curve
    uint8_t pubkey[65];
    uint8_t curve = CDC_CURVE_P256;  // Default to P-256

    // If key exists, use its curve; otherwise use default
    if (se_ecc_key_read(slot, pubkey, sizeof(pubkey), &curve)) {
        // Key exists, curve is now set
    }
    // ...
}
```

Lines 381-395 (caller):
```cpp
// C1: Algorithm attributes - Signature (ECDSA/EdDSA)
size_t algo_len;
const uint8_t *algo = get_algo_attr(KEY_TYPE_SIG, &algo_len);
discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                         0xC1, algo, algo_len);

// C2: Algorithm attributes - Decryption (ECDH)
algo = get_algo_attr(KEY_TYPE_DEC, &algo_len);
discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                         0xC2, algo, algo_len);

// C3: Algorithm attributes - Authentication (ECDSA/EdDSA)
algo = get_algo_attr(KEY_TYPE_AUT, &algo_len);
discret_len += tlv_build(discret + discret_len, sizeof(discret) - discret_len,
                         0xC3, algo, algo_len);
```

## Recommended Fix
**Cache the curve information** for each key slot. The curve type is a **static property** that only changes when keys are regenerated.

### Option 1: Static cache variables
```cpp
static struct {
    uint8_t curve;      // CDC_CURVE_P256 or CDC_CURVE_ED25519
    bool valid;         // Has the curve been read?
} s_curve_cache[3];     // [KEY_TYPE_SIG], [KEY_TYPE_DEC], [KEY_TYPE_AUT]

static const uint8_t* get_algo_attr(key_type_t key_type, size_t *len) {
    uint8_t slot = gpg_storage_sig_slot();
    switch (key_type) {
        case KEY_TYPE_SIG: slot = gpg_storage_sig_slot(); break;
        case KEY_TYPE_DEC: slot = gpg_storage_dec_slot(); break;
        case KEY_TYPE_AUT: slot = gpg_storage_aut_slot(); break;
        default:           slot = gpg_storage_sig_slot(); break;
    }

    // Check cache first
    uint8_t cache_idx = static_cast<uint8_t>(key_type);
    uint8_t curve = CDC_CURVE_P256;  // Default
    if (s_curve_cache[cache_idx].valid) {
        curve = s_curve_cache[cache_idx].curve;
    } else {
        // Lazy load from secure element
        uint8_t pubkey[65];
        if (se_ecc_key_read(slot, pubkey, sizeof(pubkey), &curve)) {
            s_curve_cache[cache_idx].curve = curve;
            s_curve_cache[cache_idx].valid = true;
        }
    }

    // Return appropriate algorithm attributes
    // ...
}

// Invalidate cache when keys are generated
static void invalidate_curve_cache(uint8_t key_type) {
    s_curve_cache[key_type].valid = false;
}
```

### Option 2: Batch read all curves once
Cache the result at the start of `build_do_app_related()`:
```cpp
static int build_do_app_related(uint8_t *buf, size_t buf_max) {
    // Read all curves once
    uint8_t curves[3];
    for (int i = 0; i < 3; i++) {
        uint8_t slot = (i == 0) ? gpg_storage_sig_slot() : 
                        (i == 1) ? gpg_storage_dec_slot() : 
                                   gpg_storage_aut_slot();
        uint8_t pubkey[65];
        curves[i] = CDC_CURVE_P256;
        se_ecc_key_read(slot, pubkey, sizeof(pubkey), &curves[i]);
    }
    
    // Then use curves[0], curves[1], curves[2] instead of calling get_algo_attr()
    // ...
}
```

## References
- **Time Complexity**: O(n) I2C reads → O(1) cached lookup
- **ESP32-S3 I2C**: Typically 100kHz-400kHz, ~1-5ms per secure-element transaction
- **Pattern**: "Read-mostly data caching" - cache static properties that rarely change

---

</content>