---
title: "[MEDIUM] `ble_vcard.cpp` lacks namespace wrapper"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `ble_vcard.cpp` file (1160 lines) implements BLE vCard exchange functionality but does not wrap its functions in a module namespace. All functions like `ble_vcard_init()`, `ble_vcard_start()`, `ble_vcard_poll_nearby()` are in the global namespace.

**Evidence:**
- File: `components/mod_vcard/src/ble_vcard.cpp` (1160 lines)
- Uses `using namespace cdc::hal;` (line 22) for controller access
- No `namespace mod_vcard { }` wrapper
- Functions declared in `mod_vcard/ble_vcard.h` should be namespaced

## Impact
1. **Global namespace pollution**: BLE vCard functions are accessible globally
2. **Name collision risk**: Common names like `init()`, `start()`, `poll()` could collide
3. **Inconsistent API**: Other mod_vcard files use namespace, this one doesn't
4. **Discoverability**: Harder to identify which module a function belongs to

## Evidence
**ble_vcard.cpp** (lines 10-60, no namespace):
```cpp
#include "mod_vcard/ble_vcard.h"
#include "mod_vcard/vcard_store.h"
#include "cdc_hal/IBluetoothController.h"
#include "cdc_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <cstring>
#include <cstdio>

using namespace cdc::hal;  // Only imports hal namespace

static const char* TAG = "VCARD_BLE";

// Functions are in GLOBAL namespace!
bool ble_vcard_init(void) {
    // ...
}

void ble_vcard_start(void) {
    // ...
}

bool ble_vcard_poll_nearby(vcard_peer_t* out) {
    // ...
}
```

**Compare with VcardModule.cpp** (properly namespaced):
```cpp
namespace cdc::mod_vcard {

class VcardModule : public core::IModule {
    // ...
};

} // namespace cdc::mod_vcard
```

## Recommended Fix
1. Wrap `ble_vcard.h` declarations in `namespace cdc::mod_vcard { }` (5 minutes)
2. Wrap `ble_vcard.cpp` functions in `namespace cdc::mod_vcard { }` (10 minutes)
   - Add `namespace cdc::mod_vcard {` after includes and static variables
   - Add `} // namespace cdc::mod_vcard` at end of file
3. Update `VcardModule.cpp` includes if needed (5 minutes)
4. Verify compilation (5 minutes)

**Total estimated time: ~25 minutes**

## References
- Project pattern: All modules use `namespace cdc::<module_name>`
- C++ Core Guidelines [R.10](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-namespace)
