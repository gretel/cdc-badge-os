---
title: "[MEDIUM] Hardcoded battery voltage thresholds"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Multiple magic values for battery voltage thresholds are used in `components/cdc_hal/src/BQ25895Power.cpp` without named constants. These thresholds (3200mV, 4200mV, 2800mV, 4000mV, 4250mV) define battery state-of-charge and detection logic.

**Locations:**
- `components/cdc_hal/src/BQ25895Power.cpp:457-460` - Battery percentage calculation
- `components/cdc_hal/src/BQ25895Power.cpp:357` - Battery detection logic

## Impact
- **Maintainability**: Different LiPo batteries have different voltage characteristics. Hardcoded values make it difficult to adapt to different battery chemistries.
- **Clarity**: The meaning of values like `4000`, `4250`, `2800` is only clear from context.
- **Consistency**: `BQ_SYS_MIN_MV = 3300` is defined as a constant, but similar thresholds are hardcoded inline.

## Evidence

**Lines 457-460 - Battery percentage calculation:**
```cpp
// Linear approximation: 3200mV=0%, 4200mV=100%
if (mv <= 3200) return 0;
if (mv >= 4200) return 100;

return (uint8_t)(((uint32_t)(mv - 3200) * 100) / 1000);
```

**Lines 357-361 - Battery detection:**
```cpp
} else if (cachedChargeStatus_ == ChargeStatus::CHARGE_DONE && cachedUsbConnected_) {
    // "Charge done" with USB - could be full battery OR no battery
    // No battery: ICHGR=0, VBAT tracks VSYS (around 4.1-4.2V from USB)
    // Full battery: ICHGR may show small trickle or 0, VBAT stable at 4.15-4.2V
    // Best indicator: if no charge current and voltage exactly at USB-derived level
    if (!hasChargeCurrent && vbat >= 4000 && vbat <= 4250) {
        // Likely no battery - USB passthrough gives ~4.1-4.2V on BATV
        cachedBatteryPresent_ = false;
    } else {
        cachedBatteryPresent_ = true;
    }
```

**Lines 370-372:**
```cpp
} else {
    // Not charging, not USB - check if voltage is reasonable for a battery
    cachedBatteryPresent_ = (vbat >= 2800 && vbat <= 4250);
}
```

Note: `CHARGE_CURRENT_SLOW`, `CHARGE_CURRENT_FAST`, `BQ_SYS_MIN_MV` are defined as constants, but voltage thresholds for battery state are not.

## Recommended Fix

1. Define named constants for battery voltage characteristics:
```cpp
/** \brief Battery voltage thresholds for state estimation. */
static constexpr uint16_t BATTERY_VOLTAGE_MIN_MV = 2800;   // Minimum usable voltage
static constexpr uint16_t BATTERY_VOLTAGE_EMPTY_MV = 3200; // 0% calculation point
static constexpr uint16_t BATTERY_VOLTAGE_FULL_MV = 4200;  // 100% calculation point
static constexpr uint16_t BATTERY_VOLTAGE_MAX_MV = 4250;   // Maximum safe voltage
static constexpr uint16_t BATTERY_USB_PASSTHRU_MIN_MV = 4000; // USB passthrough range
static constexpr uint16_t BATTERY_USB_PASSTHRU_MAX_MV = 4250;
```

2. Add a comment explaining the battery chemistry:
```cpp
/** \brief LiPo battery voltage thresholds (3.7V nominal, 4.2V max). */
```

3. Update the code to use these constants:
```cpp
// Line 457-460
if (mv <= BATTERY_VOLTAGE_EMPTY_MV) return 0;
if (mv >= BATTERY_VOLTAGE_FULL_MV) return 100;
return (uint8_t)(((uint32_t)(mv - BATTERY_VOLTAGE_EMPTY_MV) * 100) / 
                 (BATTERY_VOLTAGE_FULL_MV - BATTERY_VOLTAGE_EMPTY_MV));

// Line 361
if (!hasChargeCurrent && vbat >= BATTERY_USB_PASSTHRU_MIN_MV && vbat <= BATTERY_USB_PASSTHRU_MAX_MV) {
```

## References
- [LiPo Battery Characteristics](https://www.batteryspace.com/prospects/1014.pdf)
- `components/cdc_hal/src/BQ25895Power.cpp` - Battery charger HAL implementation
