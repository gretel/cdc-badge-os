---
title: "[LOW] Mixed C/C++ Style Functions: Inconsistent use of std::memcpy vs memcpy"
severity: LOW
domain: code-style
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "c-cpp-style"
---

## Summary

The codebase uses both C-style and C++-style memory/string functions inconsistently:

**C-style functions (mostly in HAL):**
- `memcpy()`, `memset()`, `strncpy()` - from `<string.h>`
- Used in: `Tropic01Element.cpp`, `BluetoothController.cpp`, `EspHardware.cpp`

**C++-style functions (mixed usage):**
- `std::memcpy()`, `std::memcmp()`, `std::min()` - from `<cstring>`, `<algorithm>`
- Used in: `BleAdvParser.cpp`, `EspHardware.cpp`, `BluetoothController.cpp`

**Examples:**

```cpp
// Tropic01Element.cpp: Uses C-style
#include <cstring>
memset(&handle_, 0, sizeof(handle_));
memcpy(buffer, &header, sizeof(header));

// BleAdvParser.cpp: Uses C++-style
#include <cstring>
std::memcmp(ptr, uuid128, UUID128_SIZE);
std::memcpy(name, payload, copyLen);

// EspHardware.cpp: Mixed
#include <limits>
std::numeric_limits<float>::quiet_NaN();  // C++ style
```

## Impact

- **Cognitive overhead**: Developers need to remember which style is "correct" for each file
- **Inconsistent code reviews**: Harder to enforce style guidelines
- **Maintenance friction**: New contributors may use either style without knowing preference

## Evidence

**Files using C-style:**
- `components/cdc_hal/src/Tropic01Element.cpp`: 8 uses of `memset`, `memcpy`, `strncpy`
- `components/cdc_hal/src/BluetoothController.cpp`: 15+ uses of C-style functions
- `components/cdc_core/src/ServiceRegistry.cpp`: Uses `strcmp()`

**Files using C++-style:**
- `components/cdc_hal/src/BleAdvParser.cpp`: `std::memcmp()`, `std::memcpy()`
- `components/cdc_hal/src/EspHardware.cpp`: `std::numeric_limits<>()`
- `components/cdc_hal/src/SpiBus.cpp`: `std::atomic<>()`

**Include patterns:**
- Both `<string.h>` and `<cstring>` are used
- Both `<limits.h>` and `<limits>` are used

## Recommended Fix

Choose one convention and apply consistently:

**Option A (C++ style - recommended for C++ files):**
```cpp
#include <cstring>
#include <algorithm>
#include <limits>

std::memcpy(dst, src, len);
std::memset(buf, 0, size);
std::strncmp(a, b, n);
std::min(a, b);
```

**Option B (C style - simpler for embedded):**
```cpp
#include <string.h>
#include <algorithm>

memcpy(dst, src, len);
memset(buf, 0, size);
strncmp(a, b, n);
min(a, b);  // or use ternary
```

**Recommendation:** Since the project is C++17, use C++-style (`std::`) for C++ files to leverage type safety and namespace clarity.

## References

- C++ Core Guidelines: [F.6: Use `std::` functions for C library functions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f6-use-std-functions-for-c-library-functions)
- ESP32-S3 supports both C and C++ standard libraries
