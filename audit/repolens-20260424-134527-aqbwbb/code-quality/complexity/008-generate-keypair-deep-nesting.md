---
title: "[MEDIUM] Deep nesting and complex branching in cmd_generate_keypair() function"
severity: MEDIUM
domain: mod_gpg/openpgp
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `cmd_generate_keypair()` function in `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 1453-1620) has deep nesting (4+ levels) and complex branching logic that handles key generation for different key types (SIG, DEC, AUT) with different storage backends (TROPIC01 hardware vs. software ECDH).

**Estimated Cyclomatic Complexity: ~14**
**Maximum Nesting Depth: 4 levels**
**Function Length: ~170 lines**

## Impact

**Readability:**
- Deep nesting makes it hard to follow the control flow
- Multiple code paths for different key types create cognitive overhead
- Error handling is scattered across nested branches

**Maintenance:**
- Adding support for new key types or curves requires understanding all existing branches
- Bug fixes in one path may inadvertently affect other paths
- Testing requires covering all combinations of key types, curves, and storage backends

**Risk:**
- Private key handling code is nested deep within conditional logic
- Memory clearing (mbedtls_platform_zeroize) is conditional and may be missed

## Evidence

**File:** `components/mod_gpg/src/openpgp/openpgp.cpp:1453-1620`

**Code excerpt showing deep nesting:**
```cpp
static int cmd_generate_keypair(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t key_ref = KEY_SIG;

    if (apdu->lc >= 2) {  // Level 1
        key_ref = apdu->data[0];
    }

    uint8_t ecc_slot = get_ecc_slot_for_key_ref(key_ref);
    key_type_t key_type = get_key_for_ref(key_ref);

    if (apdu->p1 == 0x80) {  // Level 1 - Generate new key
        if (!pw3_verified) {  // Level 2
            return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
        }

        uint8_t curve = CDC_CURVE_P256;

        if (key_type == KEY_TYPE_DEC) {  // Level 2 - DEC key path
            uint8_t privkey[32];
            uint8_t pubkey_gen[65];

            if (!ecdh_p256_generate_keypair(privkey, pubkey_gen)) {  // Level 3
                ESP_LOGE(TAG, "Software key generation failed for DEC");
                return apdu_sw(resp, SW_UNKNOWN);
            }

            if (!gpg_storage_save_dec_privkey(privkey, nullptr)) {  // Level 3
                ESP_LOGE(TAG, "Failed to store DEC private key");
                mbedtls_platform_zeroize(privkey, sizeof(privkey));
                return apdu_sw(resp, SW_UNKNOWN);
            }

            mbedtls_platform_zeroize(privkey, sizeof(privkey));
            ESP_LOGI(TAG, "DEC key pair generated (software ECDH)");
        } else {  // Level 2 - SIG/AUT key path
            if (!se_ecc_key_generate(ecc_slot, curve)) {  // Level 3
                ESP_LOGE(TAG, "Key generation failed for slot %d", ecc_slot);
                return apdu_sw(resp, SW_UNKNOWN);
            }
            ESP_LOGI(TAG, "Key pair generated in slot %d (hardware)", ecc_type);
        }

        // Update generation timestamp with switch inside if
        uint32_t now = (uint32_t)time(NULL);
        uint8_t ts[4] = { ... };

        switch (key_ref) {  // Level 2 - switch inside if
            case KEY_SIG: memcpy(gen_time_sig, ts, 4); break;
            case KEY_DEC: memcpy(gen_time_dec, ts, 4); break;
            case KEY_AUT: memcpy(gen_time_aut, ts, 4); break;
        }
        save_state_to_nvs();
    }

    // Read public key - another complex branch
    uint8_t pubkey[65];
    bool pubkey_ok = false;

    if (key_type == KEY_TYPE_DEC) {  // Level 1
        if (gpg_storage_has_dec_privkey()) {  // Level 2
            uint8_t privkey[32];
            if (gpg_storage_load_dec_privkey(privkey, nullptr)) {  // Level 3
                pubkey_ok = ecdh_p256_derive_pubkey(privkey, pubkey);
                mbedtls_platform_zeroize(privkey, sizeof(privkey));
            }
        }
        if (!pubkey_ok) {
            return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
        }
    } else {  // Level 1
        if (!se_ecc_key_read(ecc_slot, pubkey, sizeof(pubkey), &read_curve)) {  // Level 2
            return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
        }
        pubkey_ok = true;
    }

    // Build TLV response with conditional logic
    uint8_t pubkey_with_prefix[65];
    size_t pubkey_len;

    if (read_curve == CDC_CURVE_P256) {  // Level 1
        if (pubkey[0] == 0x04) {  // Level 2
            memcpy(pubkey_with_prefix, pubkey, 65);
        } else {  // Level 2
            pubkey_with_prefix[0] = 0x04;
            memcpy(pubkey_with_prefix + 1, pubkey, 64);
        }
        pubkey_len = 65;
    } else {  // Level 1
        memcpy(pubkey_with_prefix, pubkey, 32);
        pubkey_len = 32;
    }
    // ... more code
}
```

**Branching count:**
- Line 1460: `if (apdu->lc >= 2)` (+1)
- Line 1470: `if (apdu->p1 == 0x80)` (+1)
- Line 1471: `if (!pw3_verified)` (+1)
- Line 1481: `if (key_type == KEY_TYPE_DEC)` (+1)
- Line 1486: `if (!ecdh_p256_generate_keypair(...))` (+1)
- Line 1493: `if (!gpg_storage_save_dec_privkey(...))` (+1)
- Line 1515: `switch (key_ref)` with 3 cases (+2)
- Line 1544: `if (key_type == KEY_TYPE_DEC)` (+1)
- Line 1545: `if (gpg_storage_has_dec_privkey())` (+1)
- Line 1547: `if (gpg_storage_load_dec_privkey(...))` (+1)
- Line 1554: `if (!pubkey_ok)` (+1)
- Line 1559: `if (!se_ecc_key_read(...))` (+1)
- Line 1576: `if (read_curve == CDC_CURVE_P256)` (+1)
- Line 1577: `if (pubkey[0] == 0x04)` (+1)

Total: ~14 independent paths

## Recommended Fix

**Extract key generation logic into separate functions:**

1. Create a function for DEC key generation:
```cpp
static int generate_dec_key(uint8_t *resp, size_t resp_max) {
    uint8_t privkey[32];
    uint8_t pubkey_gen[65];

    if (!ecdh_p256_generate_keypair(privkey, pubkey_gen)) {
        ESP_LOGE(TAG, "Software key generation failed for DEC");
        return apdu_sw(resp, SW_UNKNOWN);
    }

    if (!gpg_storage_save_dec_privkey(privkey, nullptr)) {
        ESP_LOGE(TAG, "Failed to store DEC private key");
        mbedtls_platform_zeroize(privkey, sizeof(privkey));
        return apdu_sw(resp, SW_UNKNOWN);
    }

    mbedtls_platform_zeroize(privkey, sizeof(privkey));
    ESP_LOGI(TAG, "DEC key pair generated (software ECDH)");
    return apdu_sw(resp, SW_OK);
}
```

2. Create a function for hardware key generation:
```cpp
static int generate_hardware_key(uint8_t ecc_slot, uint8_t curve, uint8_t *resp, size_t resp_max) {
    if (!se_ecc_key_generate(ecc_slot, curve)) {
        ESP_LOGE(TAG, "Key generation failed for slot %d", ecc_slot);
        return apdu_sw(resp, SW_UNKNOWN);
    }
    ESP_LOGI(TAG, "Key pair generated in slot %d (hardware)", ecc_slot);
    return apdu_sw(resp, SW_OK);
}
```

3. Create a function for timestamp update:
```cpp
static void update_generation_timestamp(uint8_t key_ref) {
    uint32_t now = (uint32_t)time(NULL);
    uint8_t ts[4] = {
        (uint8_t)((now >> 24) & 0xFF),
        (uint8_t)((now >> 16) & 0xFF),
        (uint8_t)((now >> 8) & 0xFF),
        (uint8_t)(now & 0xFF)
    };

    switch (key_ref) {
        case KEY_SIG: memcpy(gen_time_sig, ts, 4); break;
        case KEY_DEC: memcpy(gen_time_dec, ts, 4); break;
        case KEY_AUT: memcpy(gen_time_aut, ts, 4); break;
    }
    save_state_to_nvs();
}
```

4. Create a function for reading public key:
```cpp
static int read_public_key(key_type_t key_type, uint8_t ecc_slot,
                           uint8_t *pubkey, uint8_t *curve, size_t *len) {
    if (key_type == KEY_TYPE_DEC) {
        if (!gpg_storage_has_dec_privkey()) {
            return false;
        }
        uint8_t privkey[32];
        if (!gpg_storage_load_dec_privkey(privkey, nullptr)) {
            return false;
        }
        bool ok = ecdh_p256_derive_pubkey(privkey, pubkey);
        mbedtls_platform_zeroize(privkey, sizeof(privkey));
        *curve = CDC_CURVE_P256;
        *len = 65;
        return ok;
    }

    if (!se_ecc_key_read(ecc_slot, pubkey, 65, curve)) {
        return false;
    }
    *len = 65;
    return true;
}
```

5. Simplified main function:
```cpp
static int cmd_generate_keypair(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t key_ref = KEY_SIG;
    if (apdu->lc >= 2) {
        key_ref = apdu->data[0];
    }

    uint8_t ecc_slot = get_ecc_slot_for_key_ref(key_ref);
    key_type_t key_type = get_key_for_ref(key_ref);

    ESP_LOGI(TAG, "GENERATE_KEYPAIR: P1=0x%02X, key_ref=0x%02X, slot=%d, type=%d",
             apdu->p1, key_ref, ecc_slot, key_type);

    if (apdu->p1 == 0x80) {
        if (!pw3_verified) {
            return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
        }

        int gen_result;
        if (key_type == KEY_TYPE_DEC) {
            gen_result = generate_dec_key(resp, resp_max);
        } else {
            gen_result = generate_hardware_key(ecc_slot, CDC_CURVE_P256, resp, resp_max);
        }
        if (gen_result != SW_OK) {
            return gen_result;
        }

        update_generation_timestamp(key_ref);
    }

    // Read and return public key
    uint8_t pubkey[65];
    uint8_t read_curve;
    size_t pubkey_len;

    if (!read_public_key(key_type, ecc_slot, pubkey, &read_curve, &pubkey_len)) {
        return apdu_sw(resp, SW_CONDITIONS_NOT_SATISFIED);
    }

    // Build TLV response (existing code)
    // ...
}
```

**Expected result:**
- Main function reduced to ~60 lines
- Each helper function has complexity < 5
- Clear separation of concerns
- Easier to test each path independently

## References

- [Cyclomatic Complexity - Wikipedia](https://en.wikipedia.org/wiki/Cyclomatic_complexity)
- [Refactoring: Extract Function](https://refactoring.com/catalog/extractFunction.html)
- [McCabe Complexity Thresholds](https://www.confluence.atlassian.com/codeanalysis/cyclomatic-complexity-104391.html)
