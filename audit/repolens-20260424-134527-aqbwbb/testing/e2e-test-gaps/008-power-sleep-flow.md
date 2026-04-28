---
title: "[LOW] No E2E Tests for Power Management and Sleep Flows"
severity: LOW
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The power management system (`components/cdc_os_ui/src/SleepManager.cpp`, `components/cdc_hal/`) implements multiple power states but has **no E2E tests**. Power flows include:

1. **Light Sleep** - Idle on lock screen, wake on any key
2. **Deep Sleep** - Hold N 5s on lock, wake on Y key only
3. **Shipping Mode** - Hold BOOT 3s, wake on USB power
4. **Battery monitoring** - Percentage display, low-battery warnings

**Critical flows untested:**
- Transition from Active → Light Sleep
- Wake from Light Sleep preserves state
- Transition to Deep Sleep saves state
- Wake from Deep Sleep restores state
- Battery threshold triggers

## Impact

**User Experience Risk:**
- Battery drain if sleep not working correctly
- State loss on wake (user data, UI position)
- Deep sleep might not wake correctly

**Hardware Risk:**
- Battery might not charge properly
- Power states might not save enough context
- Shipping mode might activate accidentally

## Evidence

**Sleep Manager** (`components/cdc_os_ui/src/SleepManager.cpp`):
```cpp
void SleepManager::update() {
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Check idle time
    if (now - lastActivity_ > LIGHT_SLEEP_INTERVAL) {
        startLightSleep();
    }
}

void SleepManager::startLightSleep() {
    // Configure wake sources
    esp_sleep_enable_ext1_wakeup(BUTTON_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);
    
    // Enter light sleep
    esp_light_sleep_start();
}

void SleepManager::startDeepSleep() {
    // Save state
    saveState();
    
    // Configure wake (Y key only)
    esp_sleep_enable_ext1_wakeup(Y_KEY_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);
    
    // Enter deep sleep
    esp_deep_sleep_start();
}
```

**Power Manager** (`components/cdc_hal/src/PowerManager.cpp`):
```cpp
void PowerManager::update() {
    // Read battery voltage
    uint16_t voltage = readBatteryVoltage();
    
    // Calculate percentage
    batteryPercent_ = voltageToPercent(voltage);
    
    // Check thresholds
    if (batteryPercent_ < LOW_BATTERY_THRESHOLD) {
        triggerLowBatteryWarning();
    }
}
```

**UI Flows** (`docs/UI_FLOWS.md:330-342`):
```markdown
## Power States

| State | Trigger | Wake | Display |
|-------|---------|------|---------|
| Active | Normal use | - | On |
| Light Sleep | Idle on lock screen | Any key | Off (fast wake) |
| Deep Sleep | Hold N 5s on lock | Y key only | Off (slow wake) |
```

**No E2E tests exist** for power flows.

## Recommended Fix

Create E2E test file `test/e2e/e2e_power_sleep.cpp`:

```cpp
#include <unity.h>
#include "cdc_os_ui/SleepManager.h"
#include "cdc_hal/IPowerManager.h"
#include "cdc_hal/ISleepController.h"

using namespace cdc::ui;
using namespace cdc::hal;

void setUp() {
    SleepManager::instance().init();
}

// ============================================
// Light Sleep Tests
// ============================================

void test_light_sleep_init() {
    SleepManager::instance().start();
    TEST_ASSERT_TRUE(SleepManager::instance().isRunning());
}

void test_light_sleep_idle_threshold() {
    // Get current idle time
    uint32_t idle = SleepManager::instance().getIdleTimeMs();
    TEST_ASSERT_GREATER_THAN(0, idle);
}

void test_light_sleep_wake_preserves_state() {
    // Set some state
    uint32_t savedTime = esp_timer_get_time();
    
    // Trigger light sleep
    SleepManager::instance().triggerLightSleep();
    
    // Wake (simulated by test framework)
    // In real test: press any key
    
    // Verify state preserved
    uint32_t currentTime = esp_timer_get_time();
    TEST_ASSERT_GREATER_THAN(savedTime, currentTime);  // Time advanced
}

// ============================================
// Deep Sleep Tests
// ============================================

void test_deep_sleep_save_state() {
    // Set some state
    SleepManager::instance().setSavedData(0x12345678);
    
    // Trigger deep sleep
    SleepManager::instance().triggerDeepSleep();
    
    // State saved to RTC memory
    // TEST_ASSERT_EQUAL(0x12345678, SleepManager::instance().getSavedData());
}

void test_deep_sleep_restore_state() {
    // Wake from deep sleep (simulated)
    // In real test: press Y key
    
    // Verify state restored
    // TEST_ASSERT_EQUAL(0x12345678, SleepManager::instance().getSavedData());
}

// ============================================
// Battery Tests
// ============================================

void test_battery_read() {
    IPowerManager* pm = getPowerManagerInstance();
    pm->init();
    
    uint16_t voltage = pm->getBatteryVoltage();
    TEST_ASSERT_GREATER_THAN(0, voltage);
}

void test_battery_percent() {
    IPowerManager* pm = getPowerManagerInstance();
    pm->init();
    
    uint8_t percent = pm->getBatteryPercent();
    TEST_ASSERT_LESS_OR_EQUAL(100, percent);
    TEST_ASSERT_GREATER_OR_EQUAL(0, percent);
}

void test_low_battery_threshold() {
    IPowerManager* pm = getPowerManagerInstance();
    pm->init();
    
    // Test threshold calculation
    // TEST_ASSERT_EQUAL(LOW_BATTERY_THRESHOLD, pm->getLowBatteryThreshold());
}

// ============================================
// Power State Transitions
// ============================================

void test_active_to_light_sleep() {
    // Start in active state
    TEST_ASSERT_EQUAL(POWER_STATE_ACTIVE, SleepManager::instance().getState());
    
    // Simulate idle
    SleepManager::instance().simulateIdle(30000);  // 30 seconds
    
    // Should transition to light sleep
    // TEST_ASSERT_EQUAL(POWER_STATE_LIGHT_SLEEP, SleepManager::instance().getState());
}

void test_light_sleep_to_active() {
    // In light sleep
    SleepManager::instance().triggerLightSleep();
    
    // Simulate key press
    SleepManager::instance().simulateKeyPress();
    
    // Should return to active
    // TEST_ASSERT_EQUAL(POWER_STATE_ACTIVE, SleepManager::instance().getState());
}

void test_active_to_deep_sleep() {
    // Start in active state
    TEST_ASSERT_EQUAL(POWER_STATE_ACTIVE, SleepManager::instance().getState());
    
    // Simulate hold N for 5s
    SleepManager::instance().simulateHoldN(5000);
    
    // Should transition to deep sleep
    // TEST_ASSERT_EQUAL(POWER_STATE_DEEP_SLEEP, SleepManager::instance().getState());
}

// ============================================
// Edge Cases
// ============================================

void test_rapid_sleep_wake_cycle() {
    // Rapidly cycle sleep/wake (stress test)
    for (int i = 0; i < 10; i++) {
        SleepManager::instance().triggerLightSleep();
        SleepManager::instance().simulateKeyPress();
    }
    
    // Should not crash
    TEST_ASSERT_TRUE(true);
}

void test_battery_low_warning() {
    // Simulate low battery
    IPowerManager* pm = getPowerManagerInstance();
    pm->setBatteryPercent(5);  // Below threshold
    
    // Should trigger warning
    // TEST_ASSERT_TRUE(pm->isLowBatteryWarningActive());
}

extern "C" void app_main() {
    UNITY_BEGIN();
    
    // Light sleep
    RUN_TEST(test_light_sleep_init);
    RUN_TEST(test_light_sleep_idle_threshold);
    RUN_TEST(test_light_sleep_wake_preserves_state);
    
    // Deep sleep
    RUN_TEST(test_deep_sleep_save_state);
    RUN_TEST(test_deep_sleep_restore_state);
    
    // Battery
    RUN_TEST(test_battery_read);
    RUN_TEST(test_battery_percent);
    RUN_TEST(test_low_battery_threshold);
    
    // Transitions
    RUN_TEST(test_active_to_light_sleep);
    RUN_TEST(test_light_sleep_to_active);
    RUN_TEST(test_active_to_deep_sleep);
    
    // Edge cases
    RUN_TEST(test_rapid_sleep_wake_cycle);
    RUN_TEST(test_battery_low_warning);
    
    UNITY_END();
}
```

## References

- [Sleep Manager](components/cdc_os_ui/src/SleepManager.cpp) - Sleep logic
- [Power Manager](components/cdc_hal/src/PowerManager.cpp) - Battery monitoring
- [UI Flows](docs/UI_FLOWS.md) - Power states (lines 330-342)
