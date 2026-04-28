---
title: "[MEDIUM] Hardcoded Li-Ion battery voltage thresholds"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The BQ25895 power management implementation uses hardcoded Li-Ion battery voltage thresholds (2800, 3200, 4000, 4200, 4250 mV) without named constants. These are standard Li-Ion battery characteristics but should be defined for clarity.

**Location:** `components/cdc_hal/src/BQ25895Power.cpp:355, 363, 464-467`

## Impact
- **Clarity**: Magic numbers don't convey "battery empty", "battery full", "Li-Ion chemistry".
- **Maintainability**: Changing battery chemistry requires finding all voltage values.
- **Self-documentation**: Named constants would explain the battery characteristics.

## Evidence

**Lines 355-363:**
```cpp
// Line 355: No battery detection (USB passthrough level)
if (!hasChargeCurrent && vbat >= 4000 && vbat <= 4250) {
    // Likely no battery - USB passthrough gives ~4.1-4.2V on BATV
    cachedBatteryPresent_ = false;
}

// Line 363: Battery presence check (reasonable voltage range)
cachedBatteryPresent_ = (vbat >= 2800 && vbat <= 4250);
```

**Lines 463-467:**
```cpp
// Linear approximation: 3200mV=0%, 4200mV=100%
if (mv <= 3200) return 0;
if (mv >= 4200) return 100;

return (uint8_t)(((uint32_t)(mv - 3200) * 100) / 1000);
```

## Recommended Fix

1. **Define battery voltage constants** near other BQ25895 constants (lines 44-49):
    ```cpp
    /** \brief Li-Ion battery voltage thresholds (millivolts) */
    static constexpr uint16_t BATTERY_EMPTY_MV = 2800;      // Below this = no battery
    static constexpr uint16_t BATTERY_MIN_MV = 3200;         // 0% capacity
    static constexpr uint16_t BATTERY_MAX_MV = 4200;         // 100% capacity
    static constexpr uint16_t BATTERY_FULL_MV = 4250;        // Max allowed (with tolerance)
    static constexpr uint16_t USB_PASSTHROUGH_MIN_MV = 4000; // USB-derived voltage level
    ```

2. **Update usage** to use constants:
    ```cpp
    // Before:
    if (!hasChargeCurrent && vbat >= 4000 && vbat <= 4250) {
        cachedBatteryPresent_ = false;
    }
    
    // After:
    if (!hasChargeCurrent && vbat >= USB_PASSTHROUGH_MIN_MV && vbat <= BATTERY_FULL_MV) {
        cachedBatteryPresent_ = false;
    }
    
    // Before:
    if (mv <= 3200) return 0;
    if (mv >= 4200) return 100;
    return (uint8_t)(((uint32_t)(mv - 3200) * 100) / 1000);
    
    // After:
    if (mv <= BATTERY_MIN_MV) return 0;
    if (mv >= BATTERY_MAX_MV) return 100;
    return (uint8_t)(((uint32_t)(mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV));
    ```

## References
- [Li-Ion Battery Characteristics](https://en.wikipedia.org/wiki/Lithium-ion_battery)
- [BQ25895 Datasheet](https://www.ti.com/product/BQ25895)
