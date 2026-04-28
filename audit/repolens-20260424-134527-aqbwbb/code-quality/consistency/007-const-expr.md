---
title: "[LOW] Constant Declaration Style: Mixed use of `static constexpr`, `constexpr`, and `static`"
severity: LOW
domain: code-style
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "constants"
---

## Summary

The codebase uses different declaration styles for compile-time constants:

**Variations found:**

1. **`static constexpr`** (most common in C++ files):
```cpp
// Tropic01Element.cpp
static constexpr uint8_t RMEM_HEADER_MAGIC = 0xCD;

// I2cBus.cpp
static constexpr uint32_t I2C_FREQ_HZ = 100000;
static constexpr uint32_t I2C_TIMEOUT_MS = 100;
```

2. **`constexpr` only** (in header/namespace scope):
```cpp
// BleAdvParser.cpp (namespace scope)
static constexpr uint8_t AD_TYPE_SHORTENED_NAME = 0x08;
static constexpr uint8_t AD_TYPE_COMPLETE_NAME = 0x09;
```

3. **`static` with runtime initialization**:
```cpp
// Tropic01Element.cpp
static const char* TAG = "TR01";

// ServiceRegistry.cpp
static const char* TAG = "ServiceRegistry";
```

4. **`#define` macros** (legacy style):
```cpp
// Tropic01Element.cpp
#define PAIRING_KEY_PRIV sh0priv_prod0
#define PAIRING_KEY_PUB sh0pub_prod0
```

## Impact

- **Scope confusion**: `static constexpr` vs just `constexpr` have different linkage
- **Type safety**: `#define` macros have no type checking
- **Debugging**: `#define` constants don't appear in debug symbols

## Evidence

**Using `static constexpr`:**
- `components/cdc_hal/src/Tropic01Element.cpp`: Line 26
- `components/cdc_hal/src/I2cBus.cpp`: Lines 16-17
- `components/cdc_hal/src/SpiBus.cpp`: Line 14
- `components/cdc_views/src/ListView.cpp`: Lines 21-23, 32

**Using `static const char*`:**
- All 62 files with TAG declarations use this pattern

**Using `#define`:**
- `components/cdc_hal/src/Tropic01Element.cpp`: Lines 28-30
- `components/cdc_core/src/ServiceRegistry.cpp`: None (uses enum)
- `components/cdc_log/include/cdc_log.h`: Lines 36-37 (ERROR_LOG_MAX_ENTRIES)

## Recommended Fix

**Standardize on `static constexpr` for type-safe constants:**

```cpp
// Good (type-safe, scoped, debuggable):
static constexpr uint8_t RMEM_HEADER_MAGIC = 0xCD;
static constexpr uint32_t I2C_FREQ_HZ = 100000;

// Good for strings (still constexpr):
static constexpr const char* TAG = "TR01";

// Better (enum for related constants):
namespace {
    enum {
        PAIRING_KEY_PRIV = ...  // or use constexpr variables
    };
}
```

**Replace `#define` with `constexpr`:**
```cpp
// Before:
#define PAIRING_KEY_PRIV sh0priv_prod0

// After:
static constexpr auto PAIRING_KEY_PRIV = sh0priv_prod0;
```

## References

- C++ Core Guidelines: [I.6: Prefer `constexpr` for constants](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i6-prefer-constexpr-for-constants)
- Modern C++ style guides recommend `constexpr` over `#define`
