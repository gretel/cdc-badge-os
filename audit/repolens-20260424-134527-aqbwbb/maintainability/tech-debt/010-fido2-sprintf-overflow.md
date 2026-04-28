---
title: "[HIGH] Buffer overflow in FIDO2 pin_hash_enc debug logging"
severity: HIGH
domain: maintainability
lens: tech-debt/safety
labels:
  - safety
  - buffer-overflow
  - fido2
---

## Summary
The FIDO2 module's debug logging for `pin_hash_enc` uses a fixed 64-byte buffer that can overflow when logging 32-byte encrypted PIN hashes. The buffer is adequate for 16 bytes (49 chars needed) but insufficient for 32 bytes (97 chars needed).

## Impact
- **Stack buffer overflow**: 32-byte `pin_hash_enc` requires 97 bytes but buffer is only 64
- **Stack corruption**: Overwrites adjacent stack variables
- **Security risk**: Buffer overflows can lead to code execution vulnerabilities
- **Debug mode only**: Only affects DEBUG_MODE builds, but still critical

## Evidence

File: `components/mod_fido2/src/ctap2.cpp`, Line 2468

```cpp
#if DEBUG_MODE
    // Log received pinHashEnc for debugging
    LOG_I("PIN", "Received pinHashEnc (%zu bytes):", pin_hash_enc_len);
    for (size_t i = 0; i < pin_hash_enc_len; i += 16) {
        size_t row_len = (pin_hash_enc_len - i < 16) ? (pin_hash_enc_len - i) : 16;
        char hex[64];  // <-- Buffer too small for 32-byte pin_hash_enc
        char *p = hex;
        for (size_t j = 0; j < row_len; j++) {
            p += sprintf(p, "%02X ", pin_hash_enc[i + j]);  // No bounds checking
        }
        LOG_I("PIN", "  %s", hex);
    }
#endif
```

**Buffer size analysis:**
- `pin_hash_enc` can be 16, 32, or 64 bytes (see line 2341)
- For 16 bytes: 16 iterations × 3 chars ("XX ") = 48 + null = 49 bytes ✓
- For 32 bytes: 32 iterations × 3 chars = 96 + null = 97 bytes ✗ (64 buffer overflows by 33!)
- For 64 bytes: Loop processes in 16-byte rows, so OK

**Context:**
```cpp
uint8_t pin_hash_enc[64] = {0};  // Can be 16 or 32 bytes (with padding)
size_t pin_hash_enc_len = 0;
// ...
if (pin_hash_enc_len == 16 || pin_hash_enc_len == 32 || pin_hash_enc_len == 64) {
    has_pin = true;
}
```

## Recommended Fix

### Option 1 - Increase buffer size (quick fix):
```cpp
char hex[100];  // Enough for 32 bytes: 32 × 3 + null = 97
```

### Option 2 - Use snprintf with sizeof (recommended):
```cpp
char hex[100];
char *p = hex;
size_t remaining = sizeof(hex) - 1;
for (size_t j = 0; j < row_len && remaining > 2; j++) {
    int written = snprintf(p, remaining, "%02X ", pin_hash_enc[i + j]);
    p += written;
    remaining -= written;
}
hex[sizeof(hex) - 1] = '\0';  // Ensure null termination
```

### Option 3 - Process smaller chunks:
```cpp
// Process 16 bytes at a time with smaller buffer
for (size_t i = 0; i < pin_hash_enc_len; i += 16) {
    size_t row_len = (pin_hash_enc_len - i < 16) ? (pin_hash_enc_len - i) : 16;
    char hex[50];  // 16 × 3 + null = 49
    char *p = hex;
    for (size_t j = 0; j < row_len; j++) {
        p += snprintf(p, sizeof(hex) - (p - hex), "%02X ", pin_hash_enc[i + j]);
    }
    LOG_I("PIN", "  %s", hex);
}
```

### Option 4 - Log as hex dump without formatting:
```cpp
// Simpler approach - just log raw bytes
LOG_I("PIN", "Received pinHashEnc (%zu bytes):", pin_hash_enc_len);
for (size_t i = 0; i < pin_hash_enc_len; i++) {
    if (i % 16 == 0) LOG_I("PIN", "  %02X", pin_hash_enc[i]);
    else LOG_I("PIN", " %02X", pin_hash_enc[i]);
}
```

## References
- Buffer overflow prevention: https://www.securecoding.cert.org/confluence/display/c/STR34-C.+Use+snprintf%28%29+instead+of+sprintf%28%29
- FIDO2 Client PIN protocol: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#authenticatorGetAssertion
