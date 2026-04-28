---
title: "[MEDIUM] Hardcoded PIN storage format constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Magic values for PIN storage format (magic byte `0xDD`, storage size `106`, iteration count `100000`) are used in `components/cdc_core/include/cdc_core/PinManager.h`. While some are defined as constants, the rationale for these specific values could be better documented.

**Location:** `components/cdc_core/include/cdc_core/PinManager.h:12,17,108-110`

## Impact
- **Maintainability**: Changing the storage format requires understanding the layout and versioning.
- **Clarity**: The magic byte `0xDD` and size `106` lack documentation about why these values were chosen.
- **Extensibility**: Adding new fields to the storage format requires careful recalculation of sizes.

## Evidence

**Line 12 (comment):**
```cpp
* [Magic 0xDD]           (1)  - Format identifier
```

**Line 17 (comment):**
```cpp
* [Iteration Count]      (4)  - Default 100000
```

**Lines 108-110:**
```cpp
static constexpr uint8_t MAX_RETRIES = 3;
static constexpr uint8_t MAGIC_V3 = 0xDD;
static constexpr uint8_t STORAGE_SIZE = 106;
```

The storage format is documented in the header but the calculation of `106` bytes is implicit:
```
1 (Magic) + 16 (Badge Hash) + 1 (Badge Retries) + 1 (KDF) + 1 (Hash) + 4 (Iterations) +
8 (PW1 Salt) + 8 (PW3 Salt) + 32 (PW1 Hash) + 32 (PW3 Hash) + 1 (PW1 Retries) + 1 (PW3 Retries) = 106
```

## Recommended Fix

1. Define the storage format as a packed struct with explicit size:
```cpp
#pragma pack(push, 1)
struct PinStorageFormat {
    uint8_t magic;           // 1 byte: Magic marker (0xDD)
    uint8_t badgeHash[16];   // 16 bytes: LEFT(SHA256, 16)
    uint8_t badgeRetries;    // 1 byte: Remaining Badge PIN attempts
    uint8_t kdfAlgorithm;    // 1 byte: KDF_ITERSALTED_S2K (0x03)
    uint8_t hashAlgorithm;   // 1 byte: SHA256 (0x08)
    uint32_t iterations;     // 4 bytes: KDF iteration count
    uint8_t pw1Salt[8];      // 8 bytes: User PIN salt
    uint8_t pw3Salt[8];      // 8 bytes: Admin PIN salt
    uint8_t pw1Hash[32];     // 32 bytes: User PIN hash
    uint8_t pw3Hash[32];     // 32 bytes: Admin PIN hash
    uint8_t pw1Retries;      // 1 byte: Remaining User PIN attempts
    uint8_t pw3Retries;      // 1 byte: Remaining Admin PIN attempts
};
#pragma pack(pop)

static_assert(sizeof(PinStorageFormat) == 106, "PinStorageFormat size mismatch");
```

2. Replace magic values with named constants:
```cpp
static constexpr uint8_t PIN_STORAGE_MAGIC_V3 = 0xDD;
static constexpr uint8_t PIN_STORAGE_SIZE = sizeof(PinStorageFormat);
static constexpr uint32_t PIN_KDF_DEFAULT_ITERATIONS = 100000;  // OpenPGP spec recommendation
```

3. Add format versioning for future compatibility:
```cpp
enum class PinStorageVersion : uint8_t {
    V1 = 0xCD,
    V2 = 0xDE,
    V3 = 0xDD,  // Current version
    LATEST = V3
};
```

## References
- [OpenPGP Card Specification](https://g10code.com/p-card.html)
- `components/cdc_core/include/cdc_core/PinManager.h` - PIN manager implementation
