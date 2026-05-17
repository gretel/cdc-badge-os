---
title: "[MEDIUM] TOTP module uses atoi() without bounds validation"
severity: MEDIUM
domain: mod_totp
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The TOTP module uses `atoi()` to parse numeric values without validating that the result fits in the target type. This can lead to:
- **Integer overflow**: Large numbers wrap silently
- **Type truncation**: Values exceeding `uint8_t` or `uint16_t` range are truncated
- **Logic bugs**: Invalid algorithm numbers, digit counts, or indices accepted

**Locations:**
- `components/mod_totp/src/TotpModule.cpp:147` - `parseAlgo()` returns `atoi(buf)` cast to `uint8_t`
- `components/mod_totp/src/TotpModule.cpp:250` - `atoi(digitsBuf)` cast to `uint8_t`
- `components/mod_totp/src/TotpModule.cpp:274, 293` - `atoi(args)` cast to `uint16_t`

## Impact
**Silent data corruption**: If a user enters a large number (e.g., `999` for digits), `atoi()` returns `999`, which truncates to `999 % 256 = 231` when cast to `uint8_t`. The code proceeds with an invalid value.

**Algorithm confusion**: `parseAlgo()` falls back to `atoi()` for numeric algorithm values. If the user enters `99`, the code accepts it as a valid algorithm (returns `99`), but the switch in `hmacCompute()` only handles `0, 1, 2` (SHA1, SHA256, SHA512).

**Example vulnerability:**
```cpp
// User enters: TOTP_ADD Secret 6 999 99
// ParseAlgo returns 99 (from atoi)
uint8_t algo = parseAlgo("99");  // Returns 99

// Switch defaults to SHA1, but algo value is 99
switch (algo) {
    case 1: // SHA256
    case 2: // SHA512
    default: // SHA1 used, but algo == 99!
}
```

## Evidence
In `components/mod_totp/src/TotpModule.cpp`:

```cpp
// Line 136-148: parseAlgo() - no bounds checking
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    char buf[8] = {};
    size_t i = 0;
    for (; token[i] && i + 1 < sizeof(buf); i++) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
    }
    buf[i] = '\0';
    if (strcmp(buf, "sha1") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    if (strcmp(buf, "sha256") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA256);
    if (strcmp(buf, "sha512") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA512);
    return static_cast<uint8_t>(atoi(buf));  // ❌ No bounds check!
}

// Line 250-252: digits and period parsing - no bounds checking
uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
uint8_t algo = parseAlgo(algoBuf);

// Line 274, 293: index parsing - no bounds checking
uint16_t index = static_cast<uint16_t>(atoi(args));
```

In `components/mod_totp/src/TotpStore.cpp`:

```cpp
// Line 443-456: Switch only handles 3 values
switch (algo) {
    case TotpAlgorithm::SHA256:  // 1
        mdType = MBEDTLS_MD_SHA256;
        expectedLen = 32;
        break;
    case TotpAlgorithm::SHA512:  // 2
        mdType = MBEDTLS_MD_SHA512;
        expectedLen = 64;
        break;
    default:  // Includes any invalid value like 99!
        mdType = MBEDTLS_MD_SHA1;
        expectedLen = 20;
        break;
}
```

## Recommended Fix
Use `strtol()` with explicit bounds checking:

```cpp
#include <cstdlib>
#include <cerrno>

// Line 136-155: Updated parseAlgo()
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    char buf[8] = {};
    size_t i = 0;
    for (; token[i] && i + 1 < sizeof(buf); i++) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
    }
    buf[i] = '\0';
    if (strcmp(buf, "sha1") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    if (strcmp(buf, "sha256") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA256);
    if (strcmp(buf, "sha512") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA512);
    
    // Parse numeric with bounds check
    char* end;
    errno = 0;
    long val = strtol(buf, &end, 10);
    if (errno || *end != '\0' || val < 0 || val > 2) {
        return static_cast<uint8_t>(TotpAlgorithm::SHA1);  // Default on error
    }
    return static_cast<uint8_t>(val);
}

// Line 250-255: Updated digits/period parsing
uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
if (digits < 6 || digits > 8) digits = TotpStore::DEFAULT_DIGITS;  // Validate

uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
if (period < 10 || period > 60) period = TotpStore::DEFAULT_PERIOD;  // Validate

// Line 274, 293: Updated index parsing
uint16_t index = static_cast<uint16_t>(atoi(args));
if (index == 0 || index > s_accountCount) {
    cdc::serial::Console::printf("ERROR: index out of range\r\n");
    return;
}
```

Alternatively, add validation in `addAccount()`:
```cpp
bool TotpStore::addAccount(const char* name, const char* issuer, const char* secretBase32,
                           uint8_t digits, uint32_t period, uint8_t algorithm) {
    // Validate inputs
    if (digits < 6 || digits > 8) return false;
    if (period < 10 || period > 60) return false;
    if (algorithm > static_cast<uint8_t>(TotpAlgorithm::SHA512)) return false;
    // ...
}
```

## References
- C++ Core Guidelines I.1: "Use integers for counting, not for bit patterns"
- C++ Core Guidelines I.4: "Ensure that values fit in the chosen type"
- CWE-190: Integer overflow or wraparound

</content>