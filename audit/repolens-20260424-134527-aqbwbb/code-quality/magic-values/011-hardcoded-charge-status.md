---
title: "[MEDIUM] Hardcoded BQ25895 charge status codes in switch statement"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
BQ25895 charge status register values are used as magic numbers directly in a switch statement. While the register address is defined as a constant, the bit-field values representing charge states are not named, making the code less self-documenting.

**Files affected:**
- `components/cdc_hal/src/BQ25895Power.cpp:324-329`

**Magic values found:**
- `0` - Charge status: NOT_CHARGING
- `1` - Charge status: PRE_CHARGE
- `2` - Charge status: FAST_CHARGE
- `3` - Charge status: CHARGE_DONE
- `0x03` - Charge status mask (REG0B[4:3])
- `0x01` - Power Good flag mask (REG0B[2])
- `3` - Magic comparison in `reg0c == 0x80 && chrgStat == 3`

## Impact
- **Readability**: `case 0:` doesn't convey "charge status register = 0"
- **Maintainability**: Changing the mapping requires understanding the hardware spec
- **Error-prone**: Easy to confuse charge status values with other register values
- **Documentation**: Connection to BQ25895 datasheet is not explicit in code

## Evidence
```cpp
// components/cdc_hal/src/BQ25895Power.cpp:315-329
uint8_t chrgStat = 0;

if (readReg(BQ_REG_SYS_STATUS, &reg0b)) {
    // Magic: 0x03 = charge status mask (REG0B[4:3])
    chrgStat = (reg0b >> 3) & 0x03;
    // Magic: 0x01 = Power Good flag (REG0B[2])
    bool pgStat = (reg0b >> 2) & 0x01;

    cachedUsbConnected_ = pgStat;

    // Magic: 0=NOT_CHARGING, 1=PRE_CHARGE, 2=FAST_CHARGE, 3=CHARGE_DONE
    switch (chrgStat) {
        case 0: cachedChargeStatus_ = ChargeStatus::NOT_CHARGING; break;
        case 1: cachedChargeStatus_ = ChargeStatus::PRE_CHARGE; break;
        case 2: cachedChargeStatus_ = ChargeStatus::FAST_CHARGE; break;
        case 3: cachedChargeStatus_ = ChargeStatus::CHARGE_DONE; break;
    }
}

// components/cdc_hal/src/BQ25895Power.cpp:395
// Magic: 3 = CHARGE_DONE status
if (reg0c == 0x80 && chrgStat == 3 && cachedBatteryPresent_) {
    LOG_I(TAG, "Battery full");
}
```

## Recommended Fix
1. **Create charge status constants** in `components/cdc_hal/include/cdc_hal/BQ25895Power.h`:
    ```cpp
    // BQ25895 Charge Status (REG0B[4:3]) - Datasheet Section 9.3.3
    static constexpr uint8_t BQ_CHRG_NOT_CHARGING = 0;
    static constexpr uint8_t BQ_CHRG_PRE_CHARGE = 1;
    static constexpr uint8_t BQ_CHRG_FAST_CHARGE = 2;
    static constexpr uint8_t BQ_CHRG_CHARGE_DONE = 3;
    
    // BQ25895 Charge Status mask and shift
    static constexpr uint8_t BQ_CHRG_STAT_MASK = 0x03;
    static constexpr uint8_t BQ_CHRG_STAT_SHIFT = 3;
    
    // BQ25895 Power Good flag (REG0B[2])
    static constexpr uint8_t BQ_PG_MASK = 0x01;
    static constexpr uint8_t BQ_PG_SHIFT = 2;
    ```

2. **Refactor usage** to use constants:
    ```cpp
    // Before:
    chrgStat = (reg0b >> 3) & 0x03;
    bool pgStat = (reg0b >> 2) & 0x01;
    
    switch (chrgStat) {
        case 0: cachedChargeStatus_ = ChargeStatus::NOT_CHARGING; break;
        case 1: cachedChargeStatus_ = ChargeStatus::PRE_CHARGE; break;
        case 2: cachedChargeStatus_ = ChargeStatus::FAST_CHARGE; break;
        case 3: cachedChargeStatus_ = ChargeStatus::CHARGE_DONE; break;
    }
    
    // After:
    chrgStat = (reg0b >> BQ_CHRG_STAT_SHIFT) & BQ_CHRG_STAT_MASK;
    bool pgStat = (reg0b >> BQ_PG_SHIFT) & BQ_PG_MASK;
    
    switch (chrgStat) {
        case BQ_CHRG_NOT_CHARGING: cachedChargeStatus_ = ChargeStatus::NOT_CHARGING; break;
        case BQ_CHRG_PRE_CHARGE: cachedChargeStatus_ = ChargeStatus::PRE_CHARGE; break;
        case BQ_CHRG_FAST_CHARGE: cachedChargeStatus_ = ChargeStatus::FAST_CHARGE; break;
        case BQ_CHRG_CHARGE_DONE: cachedChargeStatus_ = ChargeStatus::CHARGE_DONE; break;
    }
    
    // Before:
    if (reg0c == 0x80 && chrgStat == 3 && cachedBatteryPresent_) {
    
    // After:
    if (reg0c == BQ_WDG_MASK && chrgStat == BQ_CHRG_CHARGE_DONE && cachedBatteryPresent_) {
    ```

3. **Consider using enum** for cleaner mapping:
    ```cpp
    enum class BqChargeStatus : uint8_t {
        NOT_CHARGING = 0,
        PRE_CHARGE = 1,
        FAST_CHARGE = 2,
        CHARGE_DONE = 3
    };
    
    // Then:
    BqChargeStatus hwStatus = static_cast<BqChargeStatus>(chrgStat);
    switch (hwStatus) {
        case BqChargeStatus::NOT_CHARGING:
        case BqChargeStatus::PRE_CHARGE:
        // ...
    }
    ```

4. **Add datasheet reference** in comments:
    ```cpp
    // BQ25895 REG0B System Status (datasheet Section 9.3.3)
    ```

## References
- [BQ25895 Datasheet - System Status Register (page 16)](https://www.ti.com/lit/ds/symlink/bq25895.pdf)
- [BQ25895 Register Map (page 23)](https://www.ti.com/lit/ds/symlink/bq25895.pdf)
