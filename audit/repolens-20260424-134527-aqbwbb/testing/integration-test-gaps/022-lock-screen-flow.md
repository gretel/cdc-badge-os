---
title: "[MEDIUM] LockScreenView and auto-lock flow lacks integration tests"
severity: MEDIUM
domain: security
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_os_ui"
  - "area:lock-screen"
---

## Summary
The `LockScreenView` component (`components/cdc_os_ui/src/views/LockScreenView.cpp`) handles device locking and unlocking with auto-lock timeout, but **no integration tests** verify that auto-lock triggers correctly and lock state persists.

## Impact
- **Auto-lock**: Device may not lock after timeout
- **Lock state**: Lock state may not persist correctly
- **Wake handling**: Device may not wake correctly from sleep
- **PIN entry**: Locked device may not require PIN

## Evidence

**LockScreenView API** (`components/cdc_os_ui/src/views/LockScreenView.cpp`):
```cpp
class LockScreenView : public ViewBase {
    void init();
    void onUnlock();
    void onLock();
    
    // Keys: Any key to show PIN entry
};
```

**SleepManager** (`components/cdc_os_ui/src/SleepManager.cpp`):
```cpp
class SleepManager {
    void init();
    void update(uint32_t nowMs);
    void onUserActivity();
    
    // Auto-lock after inactivity
    static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 5 * 60 * 1000;
};
```

**Lock state** (`components/cdc_core/src/PinManager.cpp:100-200`):
```cpp
void PinManager::lock() {
    badgeRetries_ = MAX_RETRIES;
    lockoutStartMs_ = 0;
}

void PinManager::unlock() {
    // Unlock device
}

bool PinManager::isLocked() const {
    return badgeRetries_ == 0 || isLockoutActive();
}
```

**Event integration** (`components/cdc_core/include/cdc_core/EventBus.h`):
```cpp
enum class EventType {
    SYSTEM_UNLOCK,
    SYSTEM_LOCK,
};
```

**Usage in main** (`components/cdc_os_ui/src/AppUi.cpp:100-300`):
```cpp
// Lock screen shown on unlock
void showLockScreen() {
    LockScreenView* view = new LockScreenView();
    view->init();
    ViewStack::instance().push(view);
}

// Auto-lock in SleepManager
void SleepManager::update(uint32_t nowMs) {
    if (nowMs - lastActivity_ > INACTIVITY_TIMEOUT_MS) {
        PinManager::instance().lock();
        EventBus::instance().publish(EventType::SYSTEM_LOCK);
    }
}
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_lock_screen_flow/` that verifies:

1. **Lock screen display**: Shown when device locked
2. **PIN entry**: Any key triggers PIN entry
3. **Auto-lock**: Triggers after inactivity timeout
4. **Unlock flow**: Correct PIN unlocks device
5. **Lock state**: State persists across sleep

**Test structure** (example):
```cpp
// test/test_lock_screen_flow/test_lock_screen.cpp
#include "cdc_os_ui/views/LockScreenView.h"
#include "cdc_core/PinManager.h"
#include "cdc_os_ui/SleepManager.h"

void test_lock_screen_display() {
    PinManager& pm = PinManager::instance();
    pm.init();
    pm.lock();
    
    LockScreenView view;
    view.init();
    
    // Should be showing lock screen
    ASSERT_TRUE(view.isLocked());
}

void test_lock_screen_unlock() {
    PinManager& pm = PinManager::instance();
    pm.init();
    
    LockScreenView view;
    view.init();
    
    // Try wrong PIN
    view.onKey('1');
    // ... enter PIN ...
    view.onKey('Y');
    
    // Should still be locked
    ASSERT_TRUE(pm.isLocked());
    
    // Try correct PIN
    // ... enter correct PIN ...
    view.onUnlock();
    
    // Should be unlocked
    ASSERT_FALSE(pm.isLocked());
}

void test_auto_lock() {
    SleepManager sleepMgr;
    sleepMgr.init();
    
    uint32_t nowMs = 1000;
    sleepMgr.onUserActivity();
    
    // Advance time past timeout
    nowMs += 6 * 60 * 1000;  // 6 minutes
    sleepMgr.update(nowMs);
    
    // Should be locked
    ASSERT_TRUE(PinManager::instance().isLocked());
}
```

## References
- [LockScreenView implementation](components/cdc_os_ui/src/views/LockScreenView.cpp)
- [SleepManager](components/cdc_os_ui/src/SleepManager.cpp)
- [PinManager](components/cdc_core/src/PinManager.cpp)

</content>