---
title: "[020] [MEDIUM] Inconsistent naming: camelCase vs snake_case"
severity: MEDIUM
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The codebase mixes camelCase and snake_case naming conventions within the same files and namespaces, creating visual inconsistency.

## Impact
- Makes the code harder to scan and read
- Developers must remember which naming style applies where
- Inconsistent naming in diffs and reviews
- Can cause confusion when searching for symbols

## Evidence
**File: `components/cdc_ui/src/I18n.cpp`**

Member variables use `camelCase` with trailing underscores:
```cpp
class I18n {
private:
    Language currentLang_ = Language::EN;  // camelCase with underscore
    uint16_t nextModuleId_ = CORE_STRINGS_END;  // camelCase with underscore
    const char* strings_[2][MAX_STRINGS] = {};  // camelCase with underscore
};
```

But the struct uses `snake_case`:
```cpp
struct Translation {
    uint16_t stringId;  // snake_case
    Language lang;      // snake_case
    const char* text;   // snake_case
};
```

**File: `components/cdc_hal/src/BleAdvParser.cpp`**

Constants use `UPPER_SNAKE_CASE`:
```cpp
static constexpr uint8_t AD_TYPE_SHORTENED_NAME = 0x08;
static constexpr uint8_t AD_TYPE_COMPLETE_NAME = 0x09;
static constexpr uint8_t UUID128_SIZE = 16;
```

But local variables use `camelCase`:
```cpp
bool findManufacturerData(const uint8_t* advData, uint8_t len,
                          uint16_t* companyId, const uint8_t** data, uint8_t* dataLen) {
    if (companyId == nullptr || data == nullptr || dataLen == nullptr) {
        return false;
    }

    return walkAdStructures(advData, len,
        [&](uint8_t adType, const uint8_t* payload, uint8_t payloadLen) -> bool {
            if (adType != AD_TYPE_MANUFACTURER_SPECIFIC) {
                return false;
            }
            // camelCase: adType, payload, payloadLen
```

**File: `components/mod_gpg/src/GpgModule.cpp`**

Member variables use `s_` prefix for static:
```cpp
static uint16_t s_strIdBase = 0;
static constexpr const char* CMD_MODULE = "gpg";
static bool s_commandsRegistered = false;
```

But the struct uses different style:
```cpp
typedef struct {
    uint16_t keyId;
    uint8_t eccSlot;
    uint16_t rmemSlot;
    uint8_t keySize;
} gpg_key_info_t;
```

## Recommended Fix
Establish and follow consistent naming conventions:

**Recommended convention for this codebase:**
- **Constants** (`static constexpr`): `UPPER_SNAKE_CASE` - already consistent
- **Member variables**: `camelCase` with trailing underscore (e.g., `currentLang_`) - already consistent in classes
- **Static module variables**: `s_camelCase` (e.g., `s_strIdBase`) - already used
- **Local variables**: `camelCase` (e.g., `adType`, `payloadLen`) - already consistent
- **Typedef structs**: Use `snake_case` for members (e.g., `gpg_key_info_t` with `key_id`, `ecc_slot`)

**Fix needed:**
1. Rename struct members in `GpgModule.cpp` to match:
```cpp
typedef struct {
    uint16_t key_id;      // was: keyId
    uint8_t ecc_slot;     // was: eccSlot
    uint16_t rmem_slot;   // was: rmemSlot
    uint8_t key_size;     // was: keySize
} gpg_key_info_t;
```

2. Document the naming convention in a CONTRIBUTING.md or CODE_STYLE.md file

## References
- C++ Core Guidelines: Naming conventions should be consistent within a project
- Google C++ Style Guide: "Choose one naming convention and stick to it"
