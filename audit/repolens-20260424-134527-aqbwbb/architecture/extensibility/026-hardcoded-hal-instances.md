---
title: "[MEDIUM] Hardcoded HAL Instance Functions Limit Hardware Extension"
severity: MEDIUM
domain: architecture/extensibility
lens: extensibility-plugin
labels:
  - "hardcoded-behavior"
  - "hardware-abstraction"
  - "hal"
---

## Summary

The Hardware Abstraction Layer (HAL) uses hardcoded getter functions (`getI2cBus0()`, `getI2cBus1()`, `getPowerManagerInstance()`, `getKeypadInstance()`, etc.) that return specific singleton instances. Adding a new I2C bus, SPI bus, or hardware controller requires modifying the HAL source files directly.

The system supports exactly 2 I2C buses (0 and 1) with no runtime extensibility for additional buses or alternative implementations.

**Evidence:**

`components/cdc_hal/src/I2cBus.cpp:182-195`:
```cpp
/** \brief Singleton instances for both hardware I2C ports. */
static I2cBusImpl g_i2c0(I2C_NUM_0, I2C0_SDA_PIN, I2C0_SCL_PIN, "i2c0");
static I2cBus1 g_i2c1(I2C_NUM_1, I2C1_SDA_PIN, I2C1_SCL_PIN, "i2c1");

II2cBus* getI2cBus0() { return &g_i2c0; }
II2cBus* getI2cBus1() { return &g_i2c1; }
```

`components/cdc_hal/src/EpaperDisplay.cpp:508-519`:
```cpp
/** \brief Lazily created singleton display instance. */
static EpaperDisplay* s_display = nullptr;

IDisplay* getDisplayInstance() {
    if (!s_display) {
        s_display = new EpaperDisplay();
    }
    return s_display;
}
```

Each hardware type has exactly one implementation hardcoded:
- I2C: `I2cBusImpl` (2 instances)
- SPI: Shared SPI2_HOST only
- Keypad: `TCA9535Keypad` (1 instance)
- Power: `BQ25895Power` (1 instance)
- Display: `EpaperDisplay` (1 instance)

## Impact

**Hardware Flexibility:** Adding a third I2C bus (for additional sensors, expanders, etc.) requires:
1. Adding `g_i2c2` static instance
2. Adding `getI2cBus2()` function
3. Modifying `I2cBus.cpp` core file

**Driver Swapping:** Cannot easily swap implementations (e.g., software I2C for debugging, different keypad driver) without modifying source.

**Testing:** Difficult to inject mock implementations for unit testing since instances are hardcoded.

**Scalability:** Each new hardware variant requires core HAL modification.

## Recommended Fix

Implement a registry-based HAL discovery system:

**Option 1 - Simple registry:**
```cpp
class HalRegistry {
public:
    using BusId = uint8_t;
    static HalRegistry& instance();

    // Register a bus implementation
    BusId registerI2c(const char* name, II2cBus* impl);
    BusId registerSpi(const char* name, ISpiBus* impl);
    BusId registerKeypad(const char* name, IKeypad* impl);

    // Get by name or ID
    II2cBus* getI2c(const char* name);
    ISpiBus* getSpi(const char* name);

    // Get default (first registered)
    II2cBus* getDefaultI2c();
};

// Module registers its hardware at init:
void mod_sensor_init() {
    auto& hal = HalRegistry::instance();
    hal.registerI2c("sensor_bus", new I2cBusImpl(I2C_NUM_2, ...));
}
```

**Option 2 - Configuration-driven:**
Define hardware in Kconfig/build system:
```c
// Kconfig
config HAL_I2C_COUNT
    int "Number of I2C buses"
    default 2

config HAL_I2C_0_SDA
    int "I2C 0 SDA pin"
    default 8

// Generated hal_config.h
#define HAL_I2C_COUNT 2
#define HAL_I2C_0_SDA 8
#define HAL_I2C_1_SDA 9
```

Then create instances from generated config at boot.

**Option 3 - Keep current but add indirection:**
```cpp
// In header
II2cBus* getI2cBus(uint8_t index);

// In source
static II2cBus* g_i2cBuses[4] = {&g_i2c0, &g_i2c1};
II2cBus* getI2cBus(uint8_t index) {
    if (index >= 2) return nullptr;
    return g_i2cBuses[index];
}
```

**Recommended:** Option 1 for maximum flexibility (~1 hour to implement basic registry). Option 3 is the quickest fix (~30 min) if you just need more I2C buses without full registry.

## References

- Hardware Abstraction Layer: `components/cdc_hal/include/cdc_hal/II2cBus.h`
- Current I2C implementation: `components/cdc_hal/src/I2cBus.cpp:182-195`
- Display singleton: `components/cdc_hal/src/EpaperDisplay.cpp:508-519`
