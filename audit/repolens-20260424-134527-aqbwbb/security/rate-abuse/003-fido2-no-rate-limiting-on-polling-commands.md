---
title: "[MEDIUM] No rate limiting on FIDO2 CTAP2 commands (getPINRetries, getKeyAgreement)"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-fido2
labels:
  - "audit:security/rate-abuse"
---

## Summary
The FIDO2 CTAP2 implementation allows unlimited rapid-fire requests to `getPINRetries` (0x01) and `getKeyAgreement` (0x02) commands without any rate limiting or throttling. These commands can be used to:
1. Enumerate PIN retry state (`getPINRetries`)
2. Generate ECDH keys and consume CPU/battery (`getKeyAgreement`)

**Location:** `components/mod_fido2/src/ctap2.cpp:2242-2259, 2267-2320`

```cpp
static uint8_t client_pin_get_retries(uint8_t *response, uint16_t *response_len) {
    cbor_writer_t w;
    cbor_writer_init(&w, response + 1, *response_len - 1);
    cbor_encode_map(&w, 2);
    cbor_encode_uint(&w, 0x03);
    cbor_encode_uint(&w, g_client_pin.pin_retries);  // Returns current retries
    ...
}

static uint8_t client_pin_get_key_agreement(uint8_t *response, uint16_t *response_len) {
    if (!client_pin_init_ecdh()) {  // Generates new ECDH key pair!
        response[0] = CTAP2_ERR_OTHER;
        return CTAP2_ERR_OTHER;
    }
    ...
}
```

## Impact
**Enumeration attacks:**
- An attacker can poll `getPINRetries` to determine when a PIN attempt was successful (retries reset)
- Can detect if PIN is blocked (retries=0) without triggering lockout

**Resource exhaustion:**
- `getKeyAgreement` generates a new ECDH key pair each time (mbedtls_ecp_gen_key)
- ECP key generation is CPU-intensive (~100-500ms per key on ESP32-S3)
- An attacker can flood the device with `getKeyAgreement` requests:
  - Deplete battery (1000 requests = ~100-500 seconds of CPU time)
  - Block legitimate FIDO2 operations (serial/USB HID bottleneck)
  - Cause thermal stress on the badge

**No request throttling:**
- Commands can be sent at USB/HID maximum throughput (~16ms intervals for 64-byte HID reports)
- No delay between `getPINRetries` calls to check lockout status

## Evidence
- **File:** `components/mod_fido2/src/ctap2.cpp`
- **Lines 2242-2259:** `getPINRetries` - no rate limiting
- **Lines 2267-2320:** `getKeyAgreement` - calls `client_pin_init_ecdh()` which generates new ECDH key
- **Lines 2001-2015:** ECDH key generation (`mbedtls_ecp_gen_key`)
- **Lines 2872-2941:** `ctap2_client_pin()` dispatch - no rate limiting wrapper

## Recommended Fix
Implement rate limiting at the CTAP2 command level:

**Option 1: Per-command rate limiting**
- Track last request time for each subcommand
- Enforce minimum delay: 100ms for `getPINRetries`, 500ms for `getKeyAgreement`
- Return `CTAP2_ERR_BUSY` if request too soon

**Option 2: Global request throttling**
- Track total CTAP2 requests per second
- Allow max 10 requests/second across all CTAP2 commands
- Return `CTAP2_ERR_BUSY` on overflow

**Implementation steps (Option 1):**
1. Add static variables in `ctap2_client_pin()`:
   ```cpp
   static uint32_t lastRetriesMs = 0;
   static uint32_t lastKeyAgreeMs = 0;
   ```
2. Add rate limit constants:
   ```cpp
   static constexpr uint32_t RETRIES_RATE_LIMIT_MS = 100;
   static constexpr uint32_t KEY_AGREE_RATE_LIMIT_MS = 500;
   ```
3. Add check before each command:
   ```cpp
   uint32_t now = esp_timer_get_time() / 1000;
   if (now - lastRetriesMs < RETRIES_RATE_LIMIT_MS) {
       return CTAP2_ERR_BUSY;
   }
   lastRetriesMs = now;
   ```
4. Return `CTAP2_ERR_BUSY` for rate-limited requests

## References
- FIDO CTAP2 spec: [Error codes](https://fidoalliance.org/specs/fido-v2.1-rd-20201209/fido-client-to-authenticator-protocol-v2.1-rd-20201209.html#error-codes) - CTAP2_ERR_BUSY
- ESP32-S3 ECP timing: mbedtls_ecp_gen_key typically 100-500ms
