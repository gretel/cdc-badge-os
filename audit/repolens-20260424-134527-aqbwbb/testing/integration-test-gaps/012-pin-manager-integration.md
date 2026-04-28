---
title: "[LOW] PinManager and PIN storage integration lacks tests"
severity: LOW
domain: security
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_core"
  - "area:authentication"
---

## Summary
The `PinManager` component handles user PIN authentication for unlocking the device, but **no integration tests** verify that PIN verification, change, and timeout work correctly.

## Evidence

**PinManager API** (`components/cdc_core/include/cdc_core/PinManager.h`):
```cpp
class PinManager {
    bool verifyPin(const char* pin);
    bool changePin(const char* oldPin, const char* newPin);
    bool isLocked() const;
    void lock();
    void unlock();
    uint8_t getAttemptsLeft() const;
};
```

**Implementation** (`components/cdc_core/src/PinManager.cpp`):
- PIN stored in TROPIC01 R-Memory slot 0
- 3 attempts before lockout
- Timeout-based auto-lock

**Usage in modules** (`components/mod_gpg/src/GpgModule.cpp`):
```cpp
// Modules check PIN before sensitive operations
auto& pinManager = PinManager::instance();
if (pinManager.isLocked()) {
    // Show lock screen
}
```

**UI integration** (`components/cdc_views/src/PinEntryView.cpp`):
- PIN entry view for user input
- No integration tests

**Current test coverage**: None

## Impact
- **PIN verification**: May accept wrong PINs or reject correct ones
- **Lockout logic**: Attempts counter may not work correctly
- **Auto-lock**: Timeout may not trigger
- **PIN change**: Old PIN verification may be bypassed

## Recommended Fix

Create integration test `test_pin_manager/` that verifies:

1. **PIN verification**: Correct PIN accepted, wrong PIN rejected
2. **Attempt tracking**: Attempts decrease on wrong PIN
3. **Lockout**: Locks after 3 attempts
4. **PIN change**: Old PIN verified, new PIN stored
5. **Auto-lock**: Locks after timeout

**Test structure** (example):
```cpp
// test/test_pin_manager/test_pin_ops.cpp
#include "cdc_core/PinManager.h"

void test_pin_verification() {
    PinManager& pm = PinManager::instance();
    pm.init();
    
    // Set PIN
    pm.changePin("", "1234");
    
    // Verify correct PIN
    ASSERT_TRUE(pm.verifyPin("1234"));
    
    // Verify wrong PIN
    ASSERT_FALSE(pm.verifyPin("5678"));
    ASSERT_EQ(pm.getAttemptsLeft(), 2);
}

void test_pin_lockout() {
    PinManager& pm = PinManager::instance();
    
    // Wrong PIN 3 times
    pm.verifyPin("0000");
    pm.verifyPin("0000");
    pm.verifyPin("0000");
    
    ASSERT_TRUE(pm.isLocked());
}
```

## References
- [PinManager header](components/cdc_core/include/cdc_core/PinManager.h)
- [PinManager implementation](components/cdc_core/src/PinManager.cpp)
- [PinEntryView](components/cdc_views/src/PinEntryView.cpp)
