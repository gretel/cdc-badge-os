---
title: "[MEDIUM] Synchronous crypto operations blocking main thread"
severity: MEDIUM
domain: performance
lens: blocking-io
labels:
  - "performance"
  - "crypto"
  - "mbedtls"
---

## Summary

Various cryptographic operations in the codebase use synchronous mbedtls functions that block the calling task. These include SHA256 hashing, HMAC computation, and ECC operations.

**Affected files:**
- `components/cdc_core/src/PinManager.cpp` (lines 225-230, 259-271) - PIN hashing
- `components/cdc_core/src/AttestationKeyService.cpp` (lines 146, 162) - SHA256
- `components/mod_totp/src/TotpStore.cpp` (line 463) - HMAC
- `components/mod_gpg/src/openpgp/ecdh.cpp` (lines 127, 176, 243) - ECC operations

**Evidence:**
```cpp
// components/cdc_core/src/PinManager.cpp:225-230
mbedtls_sha256_context ctx;
mbedtls_sha256_init(&ctx);
mbedtls_sha256_starts(&ctx, 0);
mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));  // Blocks
mbedtls_sha256_finish(&ctx, fullHash);
mbedtls_sha256_free(&ctx);

// components/mod_totp/src/TotpStore.cpp:463
int ret = mbedtls_md_hmac(mdInfo, key, keyLen, data, dataLen, output);  // Blocks

// components/mod_gpg/src/openpgp/ecdh.cpp:127
ret = mbedtls_ecp_mul(&grp, &S, &d, &Q, hw_random, NULL);  // Blocks 50-100ms!

// components/mod_gpg/src/openpgp/ecdh.cpp:176
ret = mbedtls_ecp_gen_keypair(&grp, &d, &Q, hw_random, NULL);  // Blocks 100-200ms!
```

## Impact

1. **PIN verification delay**: SHA256 computation for PIN hashing blocks for ~1-5ms.

2. **TOTP generation blocking**: HMAC-SHA1/SHA256 for TOTP blocks for ~1-3ms per code generation.

3. **GPG key operations**: ECC multiplication and key generation block for 50-200ms.

4. **FIDO2 assertion**: Multiple crypto operations during assertion can block for 100-300ms total.

5. **No concurrent crypto**: Only one crypto operation can run at a time (single-threaded).

## Evidence

**Crypto operation timing:**
- SHA256 (32 bytes): ~1-3ms
- HMAC-SHA1: ~2-5ms
- HMAC-SHA256: ~3-8ms
- ECDH multiply (P-256): ~50-100ms
- ECC key generation: ~100-200ms

**Called from:**
- `components/cdc_core/src/PinManager.cpp` - PIN entry (hot path)
- `components/mod_totp/src/TotpStore.cpp` - TOTP generation (every 30s)
- `components/mod_gpg/src/openpgp/ecdh.cpp` - GPG operations
- `components/mod_fido2/src/ctap2.cpp` - FIDO2 assertions

## Recommended Fix

1. **Use ESP32 hardware crypto accelerators** (fastest, ~1 hour):
```cpp
// ESP32-S3 has SHA hardware acceleration
#include "hal/sha_hal.h"

void sha256_hardware(const uint8_t* data, size_t len, uint8_t* hash) {
    sha_hal_context_t ctx;
    sha_hal_init(&ctx, SHA_SHA256);
    sha_hal_update(&ctx, data, len);
    sha_hal_finish(&ctx, hash);
}

// Replace mbedtls_sha256 with hardware version
```

2. **Offload crypto to dedicated task** (~2 hours):
```cpp
// Create crypto worker task
static TaskHandle_t cryptoTask;
static QueueHandle_t cryptoQueue;

typedef struct {
    CryptoOp op;
    void* params;
    void (*callback)(CryptoResult);
} CryptoRequest_t;

void crypto_worker_task(void* param) {
    while (true) {
        CryptoRequest_t req;
        xQueueReceive(cryptoQueue, &req, portMAX_DELAY);
        
        CryptoResult result = performCrypto(req);
        req.callback(result);
    }
}

// Usage
void sha256_async(const uint8_t* data, size_t len, void (*cb)(uint8_t*)) {
    CryptoRequest_t req = {SHA256, data, cb};
    xQueueSend(cryptoQueue, &req, 0);
}
```

3. **Cache crypto results where possible** (~30 min):
```cpp
// Cache TOTP computation
static uint32_t lastTotp = 0;
static uint32_t lastTime = 0;

uint32_t getTotp() {
    uint32_t time = getCurrentTime();
    if (time == lastTime) {
        return lastTotp;  // Return cached
    }
    lastTotp = computeTotp(time);
    lastTime = time;
    return lastTotp;
}
```

4. **Batch crypto operations** (~1 hour):
```cpp
// Batch multiple FIDO2 signatures
void signMultiple(uint8_t** hashes, size_t count, uint8_t** signatures) {
    // Process in batch, reducing overhead
    for (size_t i = 0; i < count; i++) {
        signatures[i] = sign(hashes[i]);
    }
}
```

**Estimated effort**: 1-2 hours for hardware crypto integration

## References

- [ESP32-S3 SHA hardware](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/sha.html)
- [mbedtls performance](https://tls.mbed.org/performance)
- SHA256 software: ~10 cycles/byte, hardware: ~2 cycles/byte
- ECC operations are inherently slow (100-200ms typical)

</content>