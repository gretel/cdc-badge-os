---
title: "[MEDIUM] Silent failures in TROPIC01 auto-sleep configuration"
severity: MEDIUM
domain: hardware-abstraction
lens: error-handling
labels:
  - "audit:error-handling/unhandled-errors"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp`, the `sessionStart()` function configures TROPIC01 auto-sleep mode using nested `if` statements that silently ignore failures. If `lt_r_config_read()` or `lt_r_config_write()` fails, no error is logged.

## Impact
If auto-sleep configuration fails silently:
- Chip might not enter auto-sleep mode, draining battery
- Chip might enter sleep unexpectedly, causing session timeouts
- Hard to debug since initialization appears to succeed
- User experiences shorter battery life or unexpected wakeups

## Evidence
File: `components/cdc_hal/src/Tropic01Element.cpp:216-227`

```cpp
// Enable chip auto-sleep mode
uint32_t sleepCfg = 0;
if (lt_r_config_read(&handle_, TR01_CFG_SLEEP_MODE_ADDR, &sleepCfg) == LT_OK) {
    if (!(sleepCfg & 0x01)) {
        sleepCfg |= 0x01;
        if (lt_r_config_write(&handle_, TR01_CFG_SLEEP_MODE_ADDR, sleepCfg) == LT_OK) {
            LOG_I(TAG, "Auto-sleep enabled");
        }
        // <-- No else branch, write failure is silent
    }
    // <-- No else branch, read failure is silent
}
```

The nested `if` structure means:
- If `lt_r_config_read()` fails: nothing logged
- If `lt_r_config_write()` fails: nothing logged
- Only success is logged, failures are invisible

## Recommended Fix
Add explicit error logging for each operation:

```cpp
// Enable chip auto-sleep mode
uint32_t sleepCfg = 0;
lt_ret_t ret = lt_r_config_read(&handle_, TR01_CFG_SLEEP_MODE_ADDR, &sleepCfg);
if (ret != LT_OK) {
    LOG_W(TAG, "Failed to read sleep config: %s", lt_ret_verbose(ret));
} else {
    if (!(sleepCfg & 0x01)) {
        sleepCfg |= 0x01;
        ret = lt_r_config_write(&handle_, TR01_CFG_SLEEP_MODE_ADDR, sleepCfg);
        if (ret != LT_OK) {
            LOG_W(TAG, "Failed to write sleep config: %s", lt_ret_verbose(ret));
        } else {
            LOG_I(TAG, "Auto-sleep enabled");
        }
    }
}
```

## References
- libtropic documentation: `lt_r_config_read()` and `lt_r_config_write()` return `lt_ret_t`
- TROPIC01 datasheet: Configuration register layout
