---
title: "[LOW] Missing deployment and hardware topology documentation"
severity: LOW
domain: architecture
lens: architecture-docs
labels:
  - "audit:documentation/architecture-docs"
---

## Summary
The hardware deployment topology is not documented from an architectural perspective. While pin definitions exist in `hw_config.h`, there is no documentation of:

1. **Hardware block diagram** - How components connect to the ESP32-S3
2. **Bus topology** - I2C bus layout, SPI device sharing
3. **Power domains** - Which components share power rails
4. **Boot sequence** - Hardware initialization order
5. **Expansion interfaces** - SAO and Grove port capabilities

**Evidence:**
- `components/cdc_hal/include/cdc_hal/hw_config.h` - Has pin definitions but no topology diagram
- No hardware block diagram in `docs/`
- `docs/README.md` - Mentions hardware but no connection details
- No documentation of I2C bus sharing (BQ25895 + TCA9535 on same bus)

## Impact
- Hardware developers must reverse-engineer connections from code
- Difficult to understand bus contention or timing issues
- Expansion module developers lack reference
- Hard to troubleshoot hardware-software integration issues

## Evidence
**Current state (`hw_config.h`):**
```cpp
// I2C0: Charging IC (BQ25895) + IO Expander (TCA9535)
#define I2C0_SDA_PIN GPIO_NUM_17
#define I2C0_SCL_PIN GPIO_NUM_18

// I2C1: Expansion header
#define I2C1_SDA_PIN GPIO_NUM_47
#define I2C1_SCL_PIN GPIO_NUM_48

// SPI Bus (shared: Display + TROPIC01)
#define SPI_SCLK_PIN GPIO_NUM_12
#define SPI_MISO_PIN GPIO_NUM_11
#define SPI_MOSI_PIN GPIO_NUM_13
```

**Missing:**
- No diagram showing which devices are on which bus
- No explanation of SPI chip select sharing
- No power domain information
- No timing requirements for initialization order

## Recommended Fix
Add `docs/hardware-topology.md` with:

**1. Hardware Block Diagram**
```
                    ┌─────────────────────┐
                    │   ESP32-S3-WROOM-1  │
                    │  (16MB Flash, PSRAM)│
                    └──────────┬──────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
   ┌─────────┐          ┌─────────────┐       ┌───────────┐
   │  I2C0   │          │    SPI      │       │   I2C1    │
   │ 100kHz  │          │  Shared Bus │       │ Expansion │
   └────┬────┘          └──────┬──────┘       └─────┬─────┘
        │                      │                    │
   ┌────┴────┐            ┌────┴────┐          ┌───┴────┐
   │         │            │         │          │ SAO    │
   │BQ25895  │            │  E-Paper│          │ Grove  │
   │ TCA9535 │            │ TROPIC01│          │ Port   │
   └────┬────┘            └────┬────┘          └────────┘
        │                      │
   ┌────┴────┐            ┌────┴────┐
   │Battery  │            │Buttons  │
   │Charger  │            │(GPIO)   │
   └─────────┘            └─────────┘
```

**2. Bus Topology**
| Bus | Speed | Devices | Notes |
|-----|-------|---------|-------|
| I2C0 | 100kHz | BQ25895 (0x6A), TCA9535 (0x20) | Shared bus, distinct addresses |
| I2C1 | 100kHz | Expansion header | For SAO/Grove modules |
| SPI | ~1MHz | E-Paper (CS 41), TROPIC01 (CS 10) | Shared MISO/MOSI/SCLK |

**3. Power Domains**
| Domain | Source | Components |
|--------|--------|------------|
| 3.3V | LiPo + BQ25895 | ESP32-S3, TROPIC01, TCA9535 |
| VBUS | USB-C | Charging, USB device |
| Display Frontlight | GPIO 8 (PWM) | E-Paper backlight |

**4. Boot Sequence**
```
1. Power on (USB or LiPo)
2. ESP32-S3 boot (ROM bootloader)
3. NVS initialization
4. I2C0 scan (verify BQ25895, TCA9535)
5. SPI scan (verify TROPIC01)
6. Display init (E-Paper)
7. USB enumeration (CDC + HID + CCID)
8. BLE init (optional)
9. Module initialization
```

**5. Expansion Port Specifications**
| Port | Type | Signals | Power |
|------|------|---------|-------|
| SAO | I2C | SDA, SCL, GND, 3.3V | 3.3V @ 100mA |
| Grove | I2C | SDA, SCL, GND, 3.3V | 3.3V @ 200mA |

## References
- ESP32-S3 datasheet
- TROPIC01 datasheet: `third_party/libtropic_sdk/`
- Pin definitions: `components/cdc_hal/include/cdc_hal/hw_config.h`
