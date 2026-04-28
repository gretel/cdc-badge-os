---
title: "[MEDIUM] Hardcoded bit masks and register field values"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
Bit masks and register field values are used directly in bitwise operations throughout the BQ25895 power manager implementation. While register addresses are well-defined, the bit-level operations use magic numbers that require the datasheet for interpretation.

**Files affected:**
- `components/cdc_hal/src/BQ25895Power.cpp:224,242,318,319,338,378,395,397,423,439,440,450`

**Magic values found:**
- `0x07` - Part number mask (REG14[5:3])
- `0x0E` - SYS_MIN bit mask (REG03[3:1])
- `0x7F` - Charge current mask (REG04[6:0]) and ADC value mask
- `0x03` - Charge status mask (REG0B[4:3])
- `0x01` - Power Good flag (REG0B[2])
- `0x80` - ADC conversion start (REG02[7]) and watchdog fault (REG0C[7])

## Impact
- **Maintainability**: Understanding bit operations requires cross-referencing the BQ25895 datasheet
- **Error-prone**: Easy to use wrong bit position or mask when modifying code
- **Documentation**: Bit field meanings are not captured in the code
- **Discoverability**: No way to find all uses of a specific bit field without searching

## Evidence
```cpp
// components/cdc_hal/src/BQ25895Power.cpp:224
uint8_t partNumber = (vendor >> 3) & 0x07;  // Magic: REG14[5:3] = Part number

// components/cdc_hal/src/BQ25895Power.cpp:242
if (!updateRegBits(BQ_REG_CHG_CTRL, 0x0E, (uint8_t)(sysMinCode << 1), "SYS_MIN=3.3V")) {
    // Magic: REG03[3:1] = System minimum voltage
}

// components/cdc_hal/src/BQ25895Power.cpp:318,319
chrgStat = (reg0b >> 3) & 0x03;  // Magic: REG0B[4:3] = Charge status
bool pgStat = (reg0b >> 2) & 0x01;  // Magic: REG0B[2] = Power Good

// components/cdc_hal/src/BQ25895Power.cpp:338,378
uint16_t chargeMa = (ichgr & 0x7F) * 50;  // Magic: REG12[6:0] * 50mA

// components/cdc_hal/src/BQ25895Power.cpp:395,397
if (reg0c == 0x80 && chrgStat == 3 && cachedBatteryPresent_) {
    // Magic: REG0C[7] = Watchdog fault
}

// components/cdc_hal/src/BQ25895Power.cpp:439,440
if (readReg(BQ_REG_ADC_CTRL, &reg02) && !(reg02 & 0x80)) {
    const_cast<BQ25895Power*>(this)->writeReg(BQ_REG_ADC_CTRL, reg02 | 0x80);
    // Magic: REG02[7] = CONV_START
}

// components/cdc_hal/src/BQ25895Power.cpp:450
uint8_t code = batv & 0x7F;  // Magic: REG0E[6:0] = Battery voltage
```

## Recommended Fix
1. **Create bit field constants** in `components/cdc_hal/include/cdc_hal/BQ25895.h` (header file):
   ```cpp
   // BQ25895 Register Bit Masks and Field Definitions
   // Based on Texas Instruments BQ25895 Datasheet
   
   // REG00: Input Source Control
   static constexpr uint8_t BQ_REG00_ILIM_MASK = (1 << 6);
   
   // REG02: ADC Control
   static constexpr uint8_t BQ_REG02_CONV_START = (1 << 7);
   static constexpr uint8_t BQ_REG02_DPDM_MASK = (1 << 0);
   
   // REG03: Charge Control
   static constexpr uint8_t BQ_REG03_SYS_MIN_MASK = 0x0E;  // [3:1]
   static constexpr uint8_t BQ_REG03_OTG_MASK = (1 << 5);
   static constexpr uint8_t BQ_REG03_WDT_RST = (1 << 6);
   
   // REG04: Fast Charge Current
   static constexpr uint8_t BQ_REG04_ICHG_MASK = 0x7F;  // [6:0]
   
   // REG0B: System Status
   static constexpr uint8_t BQ_REG0B_CHRG_STAT_MASK = (0x03 << 3);  // [4:3]
   static constexpr uint8_t BQ_REG0B_PG_MASK = (1 << 2);  // Power Good
   
   // REG0C: Fault Status
   static constexpr uint8_t BQ_REG0C_WDG_MASK = (1 << 7);  // Watchdog
   
   // REG12: Charge Current ADC
   static constexpr uint8_t BQ_REG12_ICHG_ADC_MASK = 0x7F;  // [6:0]
   
   // REG14: Vendor/Part Info
   static constexpr uint8_t BQ_REG14_PART_MASK = (0x07 << 3);  // [5:3]
   ```

2. **Use helper macros** for bit field extraction:
   ```cpp
   #define BQ_GET_FIELD(reg, mask, shift) (((reg) & (mask)) >> (shift))
   #define BQ_SET_FIELD(reg, mask, shift, val) (((reg) & ~(mask)) | (((val) << (shift)) & (mask)))
   ```

3. **Refactor existing code**:
   ```cpp
   // Before:
   uint8_t partNumber = (vendor >> 3) & 0x07;
   
   // After:
   uint8_t partNumber = BQ_GET_FIELD(vendor, BQ_REG14_PART_MASK, 3);
   
   // Before:
   chrgStat = (reg0b >> 3) & 0x03;
   
   // After:
   chrgStat = BQ_GET_FIELD(reg0b, BQ_REG0B_CHRG_STAT_MASK, 3);
   ```

## References
- [BQ25895 Datasheet (Texas Instruments)](https://www.ti.com/lit/ds/symlink/bq25895.pdf)
- [Register map summary (page 23)](https://www.ti.com/lit/ds/symlink/bq25895.pdf)
