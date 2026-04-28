---
title: "[MEDIUM] Function Naming Convention Inconsistency"
severity: MEDIUM
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent naming conventions for member functions:

1. **cdc_core/cdc_ui**: camelCase (e.g., `stopAll()`, `resetBadgeRetries()`, `startLockout()`)
2. **CalEPD**: Mixed snake_case and camelCase with underscore prefix for private methods

### Evidence

**cdc_core (camelCase - consistent):**
```cpp
// components/cdc_core/include/cdc_core/IService.h
virtual void stop() = 0;

// components/cdc_core/include/cdc_core/ServiceRegistry.h
void stopAll();
void* getTypedService(ServiceType type) const;

// components/cdc_core/include/cdc_core/PinManager.h
void resetBadgeRetries();
void startLockout();
void resetPW1Retries();
void resetPW3Retries();
void generateSalt(uint8_t* salt);
void loadDefaults();

// components/cdc_core/include/cdc_core/EventBus.h
void unsubscribe(uint8_t id);
void process();
```

**CalEPD (mixed styles):**
```cpp
// components/CalEPD/include/epdParallel.h (camelCase)
virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;
virtual void init(bool debug = false) = 0;
virtual void powerOn() = 0;
virtual void powerOff() = 0;
void print(const std::string& text);
void println(const std::string& text);
void newline();

// components/CalEPD/include/gdep015OC1.h (snake_case with underscore prefix)
void _PowerOn();
void _writeCommandData(const uint8_t cmd, const uint8_t* pCommandData, uint8_t datalen);
void _setRamDataEntryMode(uint8_t em);
void _SetRamArea(uint8_t Xstart, uint8_t Xend, uint8_t Ystart, uint8_t Ystart1, uint8_t Yend, uint8_t Yend1);
void _SetRamPointer(uint8_t addrX, uint8_t addrY, uint8_t addrY1);
void _wakeUp();
void _sleep();
void _waitBusy(const char* message, uint16_t busy_time);

// components/CalEPD/include/gdew0213i5f.h (mixed)
void _PowerOn();
void _writeCommandData(const uint8_t cmd, const uint8_t* pCommandData, uint8_t datalen);
void _setRamDataEntryMode(uint8_t em);
void _SetRamArea(uint8_t Xstart, uint8_t Xend, uint8_t Ystart, uint8_t Ystart1, uint8_t Yend, uint8_t Yend1);
void _SetRamPointer(uint8_t addrX, uint8_t addrY, uint8_t addrY1);
void _setRamDataEntryMode(uint8_t em);
void _wakeup();
void _sleep();
```

**Key inconsistencies:**
- `powerOn()` vs `_PowerOn()` vs `_wakeUp()` (same concept, different naming)
- `_writeCommandData()` vs `_writeCommand()` (snake_case with different words)
- `_SetRamArea()` vs `_setRamDataEntryMode()` (inconsistent casing: SetRam vs setRam)
- `_wakeup()` vs `_wakeUp()` (camelCase inconsistency)
- Underscore prefix used inconsistently for private methods

## Impact
- **Search difficulty**: Harder to find related functions (e.g., all power-related)
- **Cognitive overhead**: Developers must remember multiple naming patterns
- **Maintenance friction**: New contributors unsure which convention to follow
- **Code clarity**: Underscore prefix doesn't reliably indicate private methods

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt camelCase for all methods** (consistent with cdc_core):
   - Public: `powerOn()`, `powerOff()`, `init()`
   - Private: `_powerOn()`, `_powerOff()`, `_init()` (underscore prefix, lowercase first)

2. **Rename CalEPD private methods** to match convention:
   - `_PowerOn()` → `_powerOn()`
   - `_writeCommandData()` → `_writeCommandData()` (already good)
   - `_setRamDataEntryMode()` → `_setRamDataEntryMode()` (already good)
   - `_SetRamArea()` → `_setRamArea()`
   - `_SetRamPointer()` → `_setRamPointer()`
   - `_wakeUp()` → `_wakeUp()` (already good)
   - `_wakeup()` → `_wakeUp()`
   - `_sleep()` → `_sleep()` (already good)

### Files to fix (scope for ~1 hour fix):
- `components/CalEPD/include/gdep015OC1.h` and corresponding `.cpp`
- `components/CalEPD/include/gdew0213i5f.h` and corresponding `.cpp`
- `components/CalEPD/include/gdew042t2Grays.h` and corresponding `.cpp`

### Consistent naming pattern:
```cpp
// PUBLIC METHODS (no prefix, camelCase)
void powerOn();
void powerOff();
void init(bool debug = false);
void drawPixel(int16_t x, int16_t y, uint16_t color);

// PRIVATE METHODS (underscore prefix, camelCase)
void _powerOn();
void _writeCommand(uint8_t cmd);
void _setData(uint8_t data);
void _setRamArea(uint8_t xStart, uint8_t xEnd, uint8_t yStart, uint8_t yEnd);
void _sleep();
void _wakeUp();
void _waitBusy(const char* message);
```

## References
- ESP-IDF Style Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/style-guide.html
- C++ Core Guidelines: Naming rules
- cdc-badge-os project convention: camelCase used in cdc_core, cdc_ui, cdc_hal

</content>