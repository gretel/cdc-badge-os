---
title: "[LOW] Array Naming Convention Inconsistency"
severity: LOW
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent naming conventions for constant arrays:

1. **cdc_core/mod_gpg**: k-prefix (e.g., `kSlotMap[]`) or descriptive names
2. **CalEPD**: Mixed styles (LUT_DATA, LUTDefault_VCOM, lut_25_LUTBD_partial)

### Evidence

**cdc_core (k-prefix for constants):**
```cpp
// components/cdc_core/src/TropicSlotMap.cpp:25
static const SlotMapEntry kSlotMap[] = {
    {0, 1, "SYSTEM", 1, 0},
    {1, 3, "GPG", 3, 1},
    // ...
};
```

**mod_gpg (descriptive names, no prefix):**
```cpp
// components/mod_gpg/src/openpgp/ccid.cpp
const uint8_t CCID_DESCRIPTOR[] = { ... };
static const uint8_t ATR[] = { ... };

// components/mod_gpg/src/openpgp/openpgp.cpp
static const uint8_t HIST_BYTES[] = { ... };
static const uint8_t ALGO_ATTR_ED25519[] = { ... };
static const uint8_t ALGO_ATTR_P256_ECDSA[] = { ... };
static const uint8_t EXT_CAPABILITIES[] = { ... };

// components/mod_gpg/src/gpg.cpp
static const uint8_t oid_ed25519[] = { ... };
static const uint8_t oid_p256[] = { ... };
```

**CalEPD (mixed styles):**
```cpp
// components/CalEPD/models/gdem029E97.cpp
const unsigned char LUT_DATA[]= { ... };       // All caps, no space before []
const unsigned char LUT_DATA_part[]={ ... };   // Underscore separator, no space

// components/CalEPD/models/dke/depg750bn.cpp
const uint8_t Depg750bn::LUTDefault_VCOM[] = { ... };
const uint8_t Depg750bn::LUTDefault_LUTWW[] = { ... };
const uint8_t Depg750bn::lut_25_LUTBD_partial[] = { ... };  // camelCase + UPPER

// components/CalEPD/include/dke/depg750bn.h
static const unsigned char lut_25_LUTBD_partial[];
static const uint8_t LUTDefault_VCOM[];
static const uint8_t LUTDefault_LUTWW[];
```

**Key inconsistencies:**
- `kSlotMap[]` vs. `LUT_DATA[]` vs. `CCID_DESCRIPTOR[]`
- `unsigned char` vs. `uint8_t` (different type names)
- `LUT_DATA[]` (no space before `[]`) vs. `LUTDefault_VCOM[] = {` (space and initializer)
- `LUT_DATA_part[]` (snake_case) vs. `LUTDefault_VCOM[]` (PascalCase)
- `lut_25_LUTBD_partial[]` (mixed case)

## Impact
- **Cognitive overhead**: Developers must remember multiple naming patterns
- **Search difficulty**: Harder to grep for all constant arrays
- **Code clarity**: Inconsistent naming makes code harder to read
- **Type consistency**: `unsigned char` vs. `uint8_t` confusion

## Recommended Fix

**Establish and document a single convention:**

1. **Use k-prefix for constant arrays** (consistent with cdc_core):
   - `kSlotMap` → keep
   - `CCID_DESCRIPTOR` → `kCcidDescriptor`
   - `ATR` → `kAtr`
   - `HIST_BYTES` → `kHistBytes`
   - `LUT_DATA` → `kLutData`

2. **Use `uint8_t` instead of `unsigned char`** for consistency

3. **Use camelCase for all array names**:
   - `LUT_DATA` → `kLutData`
   - `LUTDefault_VCOM` → `kLutDefaultVcom`
   - `lut_25_LUTBD_partial` → `kLut25LutbdPartial`

### Files to fix (scope for ~1 hour fix):
- `components/mod_gpg/src/openpgp/ccid.cpp`
- `components/mod_gpg/src/openpgp/openpgp.cpp`
- `components/CalEPD/models/gdem029E97.cpp`
- `components/CalEPD/models/dke/depg750bn.cpp`

### Rename pattern:
```cpp
// BEFORE (mixed styles)
const unsigned char LUT_DATA[]= { ... };
const uint8_t Depg750bn::LUTDefault_VCOM[] = { ... };
static const unsigned char lut_25_LUTBD_partial[];

// AFTER (consistent k-prefix, camelCase, uint8_t)
static const uint8_t kLutData[] = { ... };
static const uint8_t Depg750bn::kLutDefaultVcom[] = { ... };
static const uint8_t kLut25LutbdPartial[] = { ... };
```

## References
- C++ Core Guidelines: Naming rules
- ESP-IDF Style Guide: Constants and arrays
- cdc-badge-os project convention: k-prefix used in cdc_core

</content>