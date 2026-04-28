---
title: "[MEDIUM] Hardcoded TOTP time periods and Base32 constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
Time periods for TOTP generation and Base32 encoding constants are used directly in the TOTP module implementation. These are standard values defined by RFC 6238 but lack named constants with explanatory comments.

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp:379,622,631-633,643,865`
- `components/mod_totp/src/TotpStore.cpp`

**Magic values found:**
- `30` - Default TOTP period (seconds)
- `60` - Alternative TOTP period (seconds)
- `1000` - Update interval in milliseconds
- `5` - Bits per Base32 character
- `0x1F` - Base32 mask (5 bits)
- `"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"` - Base32 alphabet
- `128` - Secret buffer size
- `24` - Label buffer size

## Impact
- **Standards compliance**: RFC 6238 defines TOTP period of 30 seconds, but this is not documented
- **Maintainability**: Changing period requires code changes
- **Readability**: Magic number `30` doesn't clearly indicate "TOTP time step"
- **Base32 encoding**: The alphabet and bit manipulation are not self-documenting

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:379
if (nowMs - lastUpdateMs_ >= 1000) {
    // Magic: Update display every 1000ms (1 second)
}

// components/mod_totp/src/TotpModule.cpp:622
static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
// Magic: Base32 alphabet (RFC 4648)

// components/mod_totp/src/TotpModule.cpp:631-633,643
while (bitsLeft >= 5) {
    uint8_t index = (buffer >> (bitsLeft - 5)) & 0x1F;
    bitsLeft -= 5;
    // Magic: 5 bits per Base32 character
}

// components/mod_totp/src/TotpModule.cpp:865
s_wizard.period = (index == 0) ? 30 : 60;
// Magic: TOTP period in seconds (30s or 60s)

// components/mod_totp/src/TotpModule.cpp:229,561
char secret[128] = {};  // Magic: Base32 secret buffer
static char (*s_listLabels)[24] = nullptr;  // Magic: Label buffer
```

**Already defined as constants:**
```cpp
// components/mod_totp/include/mod_totp/TotpStore.h:28-32
static constexpr uint8_t NAME_LEN = 16;
static constexpr uint8_t ISSUER_LEN = 32;
static constexpr uint8_t SECRET_LEN = 32;
static constexpr uint32_t DEFAULT_PERIOD = 30;
```

## Recommended Fix
1. **Create TOTP-specific constants** in `components/mod_totp/include/mod_totp/TotpConstants.h`:
   ```cpp
   #pragma once
   #include <cstdint>
   
   // TOTP time periods (RFC 6238)
   static constexpr uint32_t TOTP_PERIOD_DEFAULT_S = 30;
   static constexpr uint32_t TOTP_PERIOD_ALT_S = 60;
   static constexpr uint32_t TOTP_UPDATE_INTERVAL_MS = 1000;
   
   // Base32 encoding (RFC 4648)
   static constexpr const char* BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
   static constexpr uint8_t BASE32_BITS_PER_CHAR = 5;
   static constexpr uint8_t BASE32_MASK = 0x1F;  // 5 bits
   static constexpr uint8_t BASE32_ALPHABET_SIZE = 32;
   
   // Display buffers
   static constexpr uint8_t TOTP_SECRET_BUF_LEN = 128;
   static constexpr uint8_t TOTP_LABEL_BUF_LEN = 24;
   ```

2. **Refactor usage**:
   ```cpp
   // Before:
   if (nowMs - lastUpdateMs_ >= 1000) { ... }
   
   // After:
   if (nowMs - lastUpdateMs_ >= TOTP_UPDATE_INTERVAL_MS) { ... }
   
   // Before:
   s_wizard.period = (index == 0) ? 30 : 60;
   
   // After:
   s_wizard.period = (index == 0) ? TOTP_PERIOD_DEFAULT_S : TOTP_PERIOD_ALT_S;
   
   // Before:
   while (bitsLeft >= 5) {
       uint8_t index = (buffer >> (bitsLeft - 5)) & 0x1F;
       bitsLeft -= 5;
   }
   
   // After:
   while (bitsLeft >= BASE32_BITS_PER_CHAR) {
       uint8_t index = (buffer >> (bitsLeft - BASE32_BITS_PER_CHAR)) & BASE32_MASK;
       bitsLeft -= BASE32_BITS_PER_CHAR;
   }
   ```

3. **Add RFC references** in comments:
   ```cpp
   // TOTP time step (RFC 6238 Section 4)
   static constexpr uint32_t TOTP_PERIOD_DEFAULT_S = 30;
   
   // Base32 alphabet (RFC 4648 Section 2)
   static constexpr const char* BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
   ```

## References
- [RFC 6238 - TOTP: Time-Based One-Time Password Algorithm](https://datatracker.ietf.org/doc/html/rfc6238)
- [RFC 4648 - The Base16, Base32, and Base64 Data Encodings](https://datatracker.ietf.org/doc/html/rfc4648)
