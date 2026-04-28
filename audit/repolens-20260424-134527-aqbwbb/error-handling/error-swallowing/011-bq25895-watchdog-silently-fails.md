---
title: "[LOW] BQ25895 kickWatchdog() silently ignores write failures"
severity: LOW
domain: cdc_hal
lens: error-handling
labels:
  - "BQ25895"
  - "watchdog"
  - "kickWatchdog"
---

## Summary
In `components/cdc_hal/src/BQ25895Power.cpp`, the `kickWatchdog()` method (lines 558-564) silently ignores I2C write failures when resetting the charger watchdog timer.

## Impact
While this is a less critical issue (watchdog is kicked every 30s with 40s timeout), repeated failures could lead to:
- Charger watchdog expiration
- Charging stops unexpectedly
- Battery may not charge fully

The silent failure makes diagnosis difficult.

## Evidence
```cpp
// Lines 558-564 in BQ25895Power.cpp
void BQ25895Power::kickWatchdog() {
    // REG03[6] = 1 resets the I2C watchdog timer
    // Silently kick without debug log (runs every 30s)
    uint8_t current = 0;
    if (readReg(BQ_REG_CHG_CTRL, &current)) {
        writeReg(BQ_REG_CHG_CTRL, current | (1 << 6));  // No error check!
    }
    // If write fails, no error is logged or returned
}
```

The comment acknowledges the silent behavior: "Silently kick without debug log (runs every 30s)"

## Recommended Fix
Add at least debug-level logging for write failures:

```cpp
void BQ25895Power::kickWatchdog() {
    // REG03[6] = 1 resets the I2C watchdog timer
    uint8_t current = 0;
    if (readReg(BQ_REG_CHG_CTRL, &current)) {
        if (!writeReg(BQ_REG_CHG_CTRL, current | (1 << 6))) {
            LOG_D(TAG, "Watchdog kick write failed");  // At least log the failure
        }
    }
}
```

Alternatively, consider making it return a bool and logging in `update()` if kicks fail repeatedly.

## References
- BQ25895 datasheet: Watchdog timer section
- Similar pattern in `updateRegBits()` (line 169) which correctly logs failures
