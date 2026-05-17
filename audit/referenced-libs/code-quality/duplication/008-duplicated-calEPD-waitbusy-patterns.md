---
title: "[MEDIUM] Duplicated _waitBusy implementation patterns across CalEPD display models"
severity: MEDIUM
domain: Code Duplication
lens: code-quality/duplication
labels:
  - "audit:code-quality/duplication"
---

## Summary

The CalEPD component contains **20+ display model files** that each implement nearly identical `_waitBusy()` methods for waiting on display ready status. The core logic is 95%+ identical across all implementations, with only minor variations in function signatures (some have `busy_time` parameter, some don't) and logging tags.

**Affected Files** (20+ models):
- `components/CalEPD/models/gdeh0154d67.cpp`
- `components/CalEPD/models/gdeh0213b73.cpp`
- `components/CalEPD/models/gdem029E97.cpp`
- `components/CalEPD/models/gdep015OC1.cpp`
- `components/CalEPD/models/gdew0213i5f.cpp`
- `components/CalEPD/models/gdew027w3.cpp`
- `components/CalEPD/models/gdew027w3T.cpp`
- `components/CalEPD/models/gdew075HD.cpp`
- `components/CalEPD/models/wave12i48.cpp` (with M1/M2/S1/S2 variants)
- `components/CalEPD/models/color/dke075z83.cpp`
- `components/CalEPD/models/color/gdew075z09.cpp`
- `components/CalEPD/models/color/gdeh042Z98.cpp`
- `components/CalEPD/models/color/gdew0583z21.cpp`
- `components/CalEPD/models/color/wave5i7Color.cpp`
- And 6+ more models...

### Code Comparison

**Pattern 1: Standard _waitBusy (appears in 15+ files):**

```cpp
// gdeh0154d67.cpp
void Gdeh0154d67::_waitBusy(const char* message){
  if (debug_enabled) {
    ESP_LOGI(TAG, "_waitBusy for %s", message);
  }
  int64_t time_since_boot = esp_timer_get_time();

  while (1){
    // On low is not busy anymore
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 0) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>7000000)
    {
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}
```

```cpp
// gdeh0213b73.cpp - Byte-for-byte identical
void Gdeh0213b73::_waitBusy(const char* message){
  if (debug_enabled) {
    ESP_LOGI(TAG, "_waitBusy for %s", message);
  }
  int64_t time_since_boot = esp_timer_get_time();

  while (1){
    // On low is not busy anymore
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 0) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>7000000)
    {
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}
```

```cpp
// gdem029E97.cpp - Byte-for-byte identical
void Gdem029E97::_waitBusy(const char* message){
  if (debug_enabled) {
    ESP_LOGI(TAG, "_waitBusy for %s", message);
  }
  int64_t time_since_boot = esp_timer_get_time();

  while (1){
    // On low is not busy anymore
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 0) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>7000000)
    {
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}
```

**Pattern 2: _waitBusy with busy_time parameter (appears in 5+ files):**

```cpp
// gdeh0154d67.cpp
void Gdeh0154d67::_waitBusy(const char* message, uint16_t busy_time){
  if (debug_enabled) {
    ESP_LOGI(TAG, "_waitBusy for %s", message);
  }
  int64_t time_since_boot = esp_timer_get_time();
  // On high is busy
  if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) {
  while (1){
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 0) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>7000000)
    {
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
  }
}
```

**Pattern 3: Multi-channel variants (wave12i48.cpp with M1, M2, S1, S2):**

```cpp
// wave12i48.cpp - 4 nearly identical variants
void Wave12I48::_waitBusyM1(const char* message){
  if (debug_enabled) ESP_LOGI(TAG, "_waitBusyM1 for %s", message);
  int64_t time_since_boot = esp_timer_get_time();
  while (1){
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY_M1) == 0) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>7000000){
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}

void Wave12I48::_waitBusyM2(const char* message){
  if (debug_enabled) ESP_LOGI(TAG, "_waitBusyM2 for %s", message);
  int64_t time_since_boot = esp_timer_get_time();
  while (1){
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY_M2) == 0) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>7000000){
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}
// ... S1 and S2 variants follow same pattern
```

## Impact

- **Maintenance Burden:** Any bug fix or improvement to the wait logic must be applied across 20+ files.
- **Code Bloat:** Approximately 400-600 lines of duplicated code (20 files × 20-30 lines each).
- **Binary Size:** Each duplicate adds to the firmware binary.
- **Inconsistency Risk:** Different models might implement timeout handling or logging differently over time.
- **Onboarding Complexity:** New developers must understand 20+ implementations of the same algorithm.
- **Testing Overhead:** Same logic needs testing across multiple display models.

## Evidence

**Files Affected (20+ models):**
1. `components/CalEPD/models/gdeh0154d67.cpp` - Lines ~200-220
2. `components/CalEPD/models/gdeh0213b73.cpp` - Lines ~150-170
3. `components/CalEPD/models/gdem029E97.cpp` - Lines ~180-200
4. `components/CalEPD/models/gdep015OC1.cpp` - Lines ~160-180
5. `components/CalEPD/models/gdew0213i5f.cpp` - Lines ~140-160
6. `components/CalEPD/models/gdew027w3.cpp` - Lines ~130-150
7. `components/CalEPD/models/gdew027w3T.cpp` - Lines ~135-155
8. `components/CalEPD/models/gdew075HD.cpp` - Lines ~170-190
9. `components/CalEPD/models/wave12i48.cpp` - Lines ~280-340 (4 variants)
10. `components/CalEPD/models/color/dke075z83.cpp`
11. `components/CalEPD/models/color/gdew075z09.cpp`
12. `components/CalEPD/models/color/gdeh042Z98.cpp`
13. `components/CalEPD/models/color/gdew0583z21.cpp`
14. `components/CalEPD/models/color/wave5i7Color.cpp`
15-20. Plus 6+ additional models...

**Duplicated Logic:**
1. **Debug logging check:** `if (debug_enabled) ESP_LOGI(TAG, "_waitBusy for %s", message);`
2. **Timer initialization:** `int64_t time_since_boot = esp_timer_get_time();`
3. **Busy wait loop:** `while (1) { if (gpio_get_level(...) == 0) break; vTaskDelay(1); ... }`
4. **Timeout check:** `if (esp_timer_get_time()-time_since_boot>7000000)`
5. **Timeout logging:** `if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");`

**Common Pattern Structure:**
```cpp
void ClassName::_waitBusy(const char* message) {
    if (debug_enabled) ESP_LOGI(TAG, "_waitBusy for %s", message);
    int64_t time_since_boot = esp_timer_get_time();
    while (1) {
        if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 0) break;
        vTaskDelay(1);
        if (esp_timer_get_time()-time_since_boot>7000000) {
            if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
            break;
        }
    }
}
```

## Recommended Fix

### Create a Base EPD Class with Shared _waitBusy

Create `components/CalEPD/include/CalEPD/EpdBase.h`:

```cpp
#pragma once
#include <cstdint>
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_log.h"

/**
 * \brief Base class providing common EPD display functionality.
 */
class EpdBase {
protected:
    bool debug_enabled = false;
    const char* TAG = "Epd";

    /**
     * \brief Wait for display busy signal to clear.
     * \param message Context message for logging.
     * \param busyGpio GPIO pin for busy signal.
     * \param timeoutUs Timeout in microseconds (default 7s).
     */
    void waitBusy(const char* message, gpio_num_t busyGpio, int64_t timeoutUs = 7000000) {
        if (debug_enabled) {
            ESP_LOGI(TAG, "_waitBusy for %s", message);
        }
        int64_t time_since_boot = esp_timer_get_time();
        while (1) {
            if (gpio_get_level(busyGpio) == 0) break;
            vTaskDelay(1);
            if (esp_timer_get_time() - time_since_boot > timeoutUs) {
                if (debug_enabled) {
                    ESP_LOGI(TAG, "Busy Timeout");
                }
                break;
            }
        }
    }

    /**
     * \brief Wait with named GPIO macro support.
     * \param message Context message for logging.
     * \param busyMacro CONFIG_EINK_BUSY macro name.
     * \param timeoutUs Timeout in microseconds.
     */
    void waitBusyNamed(const char* message, int timeoutUs = 7000000) {
        waitBusy(message, (gpio_num_t)CONFIG_EINK_BUSY, timeoutUs);
    }
};
```

### Create Multi-Channel Helper for Wave12I48

```cpp
/**
 * \brief Multi-channel busy wait for displays with multiple busy lines.
 */
class MultiChannelEpdBase : public EpdBase {
protected:
    void waitBusyChannel(const char* prefix, gpio_num_t busyGpio, int64_t timeoutUs = 7000000) {
        if (debug_enabled) {
            ESP_LOGI(TAG, "%s for %s", prefix, "display refresh");
        }
        int64_t time_since_boot = esp_timer_get_time();
        while (1) {
            if (gpio_get_level(busyGpio) == 0) break;
            vTaskDelay(1);
            if (esp_timer_get_time() - time_since_boot > timeoutUs) {
                if (debug_enabled) {
                    ESP_LOGI(TAG, "Busy Timeout");
                }
                break;
            }
        }
    }
};
```

### Implementation Steps

1. **Create `components/CalEPD/include/CalEPD/EpdBase.h`** with base class above.

2. **Update each model file:**
   - Add `#include "CalEPD/EpdBase.h"`
   - Inherit from `EpdBase`: `class Gdeh0154d67 : public EpdBase`
   - Replace `_waitBusy()` with inherited `waitBusy()`
   - Remove local `_waitBusy()` implementations

3. **For multi-channel displays (wave12i48.cpp):**
   - Inherit from `MultiChannelEpdBase`
   - Replace `_waitBusyM1()`, `_waitBusyM2()`, etc. with `waitBusyChannel()`

4. **Update call sites:**
   - `_waitBusy("message")` → `waitBusy("message")`
   - `_waitBusy("message", time)` → `waitBusy("message", busyGpio, time)`

### Alternative: Template-Based Approach

For more flexibility, use a template:

```cpp
template<gpio_num_t BusyGpio>
void waitBusy(const char* message) {
    if (debug_enabled) ESP_LOGI(TAG, "_waitBusy for %s", message);
    int64_t time_since_boot = esp_timer_get_time();
    while (1) {
        if (gpio_get_level(BusyGpio) == 0) break;
        vTaskDelay(1);
        if (esp_timer_get_time() - time_since_boot > 7000000) {
            if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
            break;
        }
    }
}
```

## References

- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [Template Method Pattern](https://en.wikipedia.org/wiki/Template_method_pattern)
- [CRTP (Curiously Recurring Template Pattern)](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
- Related findings: #1 (token parsing), #2 (UI helpers), #3 (wizard state), #6 (storage layer patterns)

</content>