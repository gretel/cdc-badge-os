---
title: "[LOW] std:: Qualifier Inconsistency: Mixed use of qualified vs unqualified standard library calls"
severity: LOW
domain: code-style
lens: consistency
labels:
  - "audit:code-quality/consistency"
  - "c-cpp-style"
---

## Summary

The codebase shows inconsistent use of `std::` qualifiers for standard library types and functions:

**With `std::` qualifier:**
- `std::atomic<bool>` in `SpiBus.cpp`
- `std::numeric_limits<float>()` in `EspHardware.cpp`
- `std::min()` in `BluetoothController.cpp`
- `std::memcpy()`, `std::memcmp()` in `BleAdvParser.cpp`

**Without `std::` qualifier:**
- `memset()`, `memcpy()`, `strncpy()` in `Tropic01Element.cpp`
- `strcmp()` in `ServiceRegistry.cpp`
- `size_t`, `uint8_t` types (implicitly using C types or `using namespace std;` context)

**Examples:**

```cpp
// SpiBus.cpp: Uses std::
#include <atomic>
static std::atomic<bool> g_spiInitialized{false};

// EspHardware.cpp: Uses std::
#include <limits>
float tempC = std::numeric_limits<float>::quiet_NaN();

// BluetoothController.cpp: Mixed
#include <algorithm>
uint8_t numChars = std::min(service.numCharacteristics, (uint8_t)MAX_CHARS_PER_HANDLE);
// But uses: memcpy(), memset() without std::

// Tropic01Element.cpp: No std::
#include <cstring>
memset(&handle_, 0, sizeof(handle_));
memcpy(buffer, &header, sizeof(header));
```

## Impact

- **Style inconsistency**: Same file may use both styles
- **Namespace pollution**: Some files rely on ADL (Argument Dependent Lookup) while others explicitly qualify
- **Readability variance**: Explicit `std::` makes dependencies clearer but adds verbosity

## Evidence

**Files using `std::` prefix:**
- `components/cdc_hal/src/SpiBus.cpp`: `std::atomic<>()`
- `components/cdc_hal/src/EspHardware.cpp`: `std::numeric_limits<>()`
- `components/cdc_hal/src/BluetoothController.cpp`: `std::min()`
- `components/cdc_hal/src/BleAdvParser.cpp`: `std::memcpy()`, `std::memcmp()`

**Files NOT using `std::` prefix:**
- `components/cdc_hal/src/Tropic01Element.cpp`: All C-style functions
- `components/cdc_core/src/ServiceRegistry.cpp`: `strcmp()`, `strlen()`
- `components/cdc_core/src/EventBus.cpp`: C-style functions

## Recommended Fix

Choose one convention:

**Option A (Explicit `std::` - more C++ idiomatic):**
```cpp
#include <cstring>
#include <algorithm>

std::memcpy(dst, src, len);
std::memset(buf, 0, size);
std::min(a, b);
```

**Option B (Implicit via using declarations - shorter):**
```cpp
#include <cstring>
#include <algorithm>
using std::memcpy;
using std::memset;
using std::min;

memcpy(dst, src, len);
memset(buf, 0, size);
min(a, b);
```

**Option C (C-style for embedded - simplest):**
```cpp
#include <string.h>
#include <limits.h>

memcpy(dst, src, len);
memset(buf, 0, size);
```

**Recommendation:** Use explicit `std::` for C++17 code to make dependencies clear and avoid namespace collisions.

## References

- C++ Core Guidelines: [Namespace rules](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#n-namespaces)
- ESP32-S3 C++17 support documentation
