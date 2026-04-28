---
title: "[MEDIUM] PinChangeView PIN change flow lacks integration tests"
severity: MEDIUM
domain: security
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_os_ui"
  - "area:pin-flow"
---

## Summary
The `PinChangeView` component (`components/cdc_os_ui/src/views/PinChangeView.cpp`) handles the complete PIN change workflow (current PIN entry, new PIN entry, confirmation), but **no integration tests** verify that PINs change correctly and validation works.

## Impact
- **PIN validation**: Old PIN may not be verified correctly
- **New PIN**: New PIN may not meet constraints
- **Confirmation**: PIN mismatch may not be detected
- **PIN change**: PIN may not be stored correctly

## Evidence

**PinChangeView structure** (`components/cdc_os_ui/src/views/PinChangeView.cpp`):
```cpp
class PinChangeView {
    // States:
    // 1. Current PIN entry
    // 2. New PIN entry
    // 3. Confirm PIN entry
    // 4. Success/Failure
    
    void onCurrentPinComplete(const char* pin);
    void onNewPinComplete(const char* pin);
    void onConfirmPinComplete(const char* pin);
};
```

**PIN constraints** (`components/cdc_core/include/cdc_core/PinManager.h:34-40`):
```cpp
static constexpr uint8_t BADGE_PIN_MIN = 4;
static constexpr uint8_t BADGE_PIN_MAX = 8;
static constexpr uint8_t PW1_MIN = 6;
static constexpr uint8_t PW3_MIN = 8;
```

**PIN change API** (`components/cdc_core/src/PinManager.cpp:200-300`):
```cpp
bool PinManager::changeBadgePin(const char* currentPin, const char* newPin) {
    // Verify current PIN
    if (!verifyBadgePin(currentPin)) {
        return false;
    }
    
    // Validate new PIN length
    if (strlen(newPin) < BADGE_PIN_MIN) {
        return false;
    }
    
    // Store new PIN
    return storeBadgePin(newPin);
}
```

**View flow** (`components/cdc_os_ui/src/AppUi.cpp:400-500`):
```cpp
// PinChangeView integrated into settings menu
void showPinChangeView() {
    PinChangeView* view = new PinChangeView();
    view->init();
    ViewStack::instance().push(view);
}
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_pin_change_flow/` that verifies:

1. **Current PIN verification**: Wrong current PIN rejected
2. **New PIN validation**: Short PIN rejected
3. **Confirmation**: Mismatch detected
4. **PIN change**: New PIN stored and verified
5. **Cancel flow**: Can cancel at any step

**Test structure** (example):
```cpp
// test/test_pin_change_flow/test_pin_change.cpp
#include "cdc_core/PinManager.h"
#include "cdc_os_ui/views/PinChangeView.h"

void test_pin_change_flow() {
    PinManager& pm = PinManager::instance();
    pm.init();
    
    // Change PIN
    bool success = pm.changeBadgePin("123456", "654321");
    ASSERT_TRUE(success);
    
    // Verify old PIN fails
    ASSERT_FALSE(pm.verifyBadgePin("123456"));
    
    // Verify new PIN works
    ASSERT_TRUE(pm.verifyBadgePin("654321"));
}

void test_pin_change_wrong_current() {
    PinManager& pm = PinManager::instance();
    pm.init();
    
    // Wrong current PIN
    bool success = pm.changeBadgePin("000000", "654321");
    ASSERT_FALSE(success);
}

void test_pin_change_too_short() {
    PinManager& pm = PinManager::instance();
    pm.init();
    
    // New PIN too short (3 digits, min is 4)
    bool success = pm.changeBadgePin("123456", "123");
    ASSERT_FALSE(success);
}

void test_pin_change_view_flow() {
    PinChangeView view;
    view.init();
    
    // Simulate flow
    view.onCurrentPinComplete("123456");
    view.onNewPinComplete("654321");
    view.onConfirmPinComplete("654321");
    
    // Should succeed
    ASSERT_TRUE(view.isSuccess());
}
```

## References
- [PinChangeView implementation](components/cdc_os_ui/src/views/PinChangeView.cpp)
- [PinManager API](components/cdc_core/src/PinManager.cpp)
- [AppUi integration](components/cdc_os_ui/src/AppUi.cpp)

</content>