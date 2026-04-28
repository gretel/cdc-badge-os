---
title: "[LOW] Missing early return in cmd_get_data() for repeated algorithm attribute queries"
severity: LOW
domain: mod_gpg
lens: algorithm-efficiency
labels:
  - "early-return"
  - "redundant-computation"
---

## Summary
In `components/mod_gpg/src/openpgp/openpgp.cpp`, the `cmd_get_data()` function (lines 708-888) calls `get_algo_attr()` separately for each algorithm attribute query (SIG, DEC, AUT). Each call independently reads from the secure element to determine the curve, even though all three are often requested in the same APDU response (e.g., for `GET DATA 0x6E`).

While this overlaps with issue #001, the specific issue here is the **lack of early return/caching within a single APDU processing cycle**. When handling `GET DATA 0x6E`, the function calls:
1. `get_algo_attr(KEY_TYPE_SIG)` - reads SIG slot
2. `get_algo_attr(KEY_TYPE_DEC)` - reads DEC slot  
3. `get_algo_attr(KEY_TYPE_AUT)` - reads AUT slot

But when individual algorithm attributes are queried separately (e.g., `GET DATA 0xC1` for SIG, then `GET DATA 0xC2` for DEC), each query re-reads from the secure element.

## Impact
- **Performance**: Repeated secure-element reads for the same data within a short time window
- **Latency**: Each additional query adds 1-5ms I2C transaction time
- **Power**: Unnecessary secure-element wakeups

## Evidence
**File: `components/mod_gpg/src/openpgp/openpgp.cpp`**

Lines 745-760 (cmd_get_data switch cases):
```cpp
case DO_ALGO_SIG: { // 0xC1: Algorithm Attributes - Signature
    size_t algo_len;
    const uint8_t *algo = get_algo_attr(KEY_TYPE_SIG, &algo_len);
    return apdu_build_response(resp, resp_max, algo, algo_len, SW_OK);
}

case DO_ALGO_DEC: { // 0xC2: Algorithm Attributes - Decryption
    size_t algo_len;
    const uint8_t *algo = get_algo_attr(KEY_TYPE_DEC, &algo_len);
    return apdu_build_response(resp, resp_max, algo, algo_len, SW_OK);
}

case DO_ALGO_AUT: { // 0xC3: Algorithm Attributes - Authentication
    size_t algo_len;
    const uint8_t *algo = get_algo_attr(KEY_TYPE_AUT, &algo_len);
    return apdu_build_response(resp, resp_max, algo, algo_len, SW_OK);
}
```

Each case calls `get_algo_attr()` which performs a fresh secure-element read, even if the same attribute was just queried moments before.

## Recommended Fix
**Add session-level cache that resets on application deselect**

```cpp
// Add to static state (near line 135)
static struct {
    uint8_t curves[3];      // [KEY_TYPE_SIG], [KEY_TYPE_DEC], [KEY_TYPE_AUT]
    uint8_t algo_attrs[3][8]; // Cached algorithm attribute arrays
    size_t algo_lens[3];     // Lengths
    bool valid;              // Cache valid for current session
} s_algo_cache = {};

// Modify get_algo_attr to use cache:
static const uint8_t* get_algo_attr(key_type_t key_type, size_t *len) {
    uint8_t cache_idx = static_cast<uint8_t>(key_type);
    
    // Check session cache first
    if (s_algo_cache.valid) {
        *len = s_algo_cache.algo_lens[cache_idx];
        return s_algo_cache.algo_attrs[cache_idx];
    }
    
    // Read from secure element
    uint8_t slot = gpg_storage_sig_slot();
    switch (key_type) {
        case KEY_TYPE_SIG: slot = gpg_storage_sig_slot(); break;
        case KEY_TYPE_DEC: slot = gpg_storage_dec_slot(); break;
        case KEY_TYPE_AUT: slot = gpg_storage_aut_slot(); break;
        default:           slot = gpg_storage_sig_slot(); break;
    }

    uint8_t pubkey[65];
    uint8_t curve = CDC_CURVE_P256;
    if (se_ecc_key_read(slot, pubkey, sizeof(pubkey), &curve)) {
        // Cache the result
        if (curve == CDC_CURVE_P256) {
            if (key_type == KEY_TYPE_DEC) {
                s_algo_cache.algo_lens[cache_idx] = sizeof(ALGO_ATTR_P256_ECDH);
                memcpy(s_algo_cache.algo_attrs[cache_idx], ALGO_ATTR_P256_ECDH, 
                       sizeof(ALGO_ATTR_P256_ECDH));
            } else {
                s_algo_cache.algo_lens[cache_idx] = sizeof(ALGO_ATTR_P256_ECDSA);
                memcpy(s_algo_cache.algo_attrs[cache_idx], ALGO_ATTR_P256_ECDSA, 
                       sizeof(ALGO_ATTR_P256_ECDSA));
            }
        } else {
            s_algo_cache.algo_lens[cache_idx] = sizeof(ALGO_ATTR_ED25519);
            memcpy(s_algo_cache.algo_attrs[cache_idx], ALGO_ATTR_ED25519, 
                   sizeof(ALGO_ATTR_ED25519));
        }
    }
    
    *len = s_algo_cache.algo_lens[cache_idx];
    return s_algo_cache.algo_attrs[cache_idx];
}

// Invalidate cache on SELECT (line 693)
static int cmd_select(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (apdu->lc >= 6 && memcmp(apdu->data, OPENPGP_AID, 6) == 0) {
        app_selected = true;
        pw1_verified = false;
        pw3_verified = false;
        mbedtls_platform_zeroize(s_session_pin, sizeof(s_session_pin));
        
        // Invalidate algorithm cache on new select
        s_algo_cache.valid = false;
        
        ESP_LOGI(TAG, "OpenPGP application selected");
        return apdu_sw(resp, SW_OK);
    }
    // ...
}

// Also invalidate on key generation (cmd_generate_keypair)
```

This approach:
- Caches results for the duration of an APDU session
- Automatically invalidates on application deselect
- Avoids repeated reads within a single command (0x6E) or across multiple commands

## References
- **Pattern**: "Session-level caching" - cache data for the lifetime of a logical session
- **Time Complexity**: O(n) reads per session → O(1) reads per session
- **Trade-off**: Adds ~20 bytes of state per key type (configurable with preprocessor)

---

</content>