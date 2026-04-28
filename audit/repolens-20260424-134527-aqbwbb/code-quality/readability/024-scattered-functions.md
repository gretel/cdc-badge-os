---
title: "[024] [LOW] File organization: related functions scattered"
severity: LOW
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In several files, related functions are not grouped together logically, making it harder to find related code.

## Impact
- Developers must search through the file to find related functions
- Harder to understand the overall structure of a class or module
- Increases time spent navigating the codebase

## Evidence
**File: `components/cdc_hal/src/BQ25895Power.cpp`**

The class definition (lines 68-132) declares methods in this order:
1. IService implementation (init, start, stop, getState, getName)
2. IPowerManager implementation (getBatteryVoltage, getBatteryPercent, isUsbConnected, etc.)
3. Private helpers (readReg, writeReg, updateRegBits)
4. Internal helpers (readChargerStatus, setChargeCurrentMa)
5. Kicks the charger watchdog timer (kickWatchdog) - declared mid-way through private section

But the implementation order is:
1. `init()` (line 186)
2. `start()` (line 287)
3. `stop()` (line 302)
4. `readReg()` (line 136) - helper before main methods
5. `writeReg()` (line 147)
6. `updateRegBits()` (line 158)
7. `readChargerStatus()` (line 313)
8. `setChargeCurrentMa()` (line 411)
9. `getBatteryVoltage()` (line 436)
10. `getBatteryPercent()` (line 458)
...and so on

The methods are not grouped by category (e.g., all getters together, all setters together).

**File: `components/cdc_hal/src/TCA9535Keypad.cpp`**

Class declaration (lines 88-149) lists:
- IService implementation
- IKeypad implementation
- Private methods in mixed order

Implementation order:
- `init()` (line 147)
- `start()` (line 243)
- `stop()` (line 254)
- `readInputs()` (line 263)
- `isKeyPressed()` (line 280)
- `getNextKey()` (line 291)
- `hasKey()` (line 298)
- `anyKeyDown()` (line 309)
- `setLongPressEnabled()` (line 318)
- `bufferAddKey()` (line 330)
- `bufferGetKey()` (line 347)
- `isrHandler()` (line 364) - interrupt handler in middle of buffer methods
- `taskFunc()` (line 376)
- `prepareForSleep()` (line 438)
- `recoverFromSleep()` (line 453)
- `clearBuffer()` (line 482)

The ISR and task are interspersed with buffer methods instead of being grouped together.

## Recommended Fix
Reorganize methods to group related functionality:

**For BQ25895Power.cpp:**
```cpp
// Implementation order:
// 1. IService methods (init, start, stop, getState, getName)
// 2. IPowerManager getters (getBatteryVoltage, getBatteryPercent, isUsbConnected, etc.)
// 3. IPowerManager setters (setChargingEnabled, enterShipMode)
// 4. IPowerManager status (getChargeStatus, isBatteryLow, isBatteryCritical, etc.)
// 5. Update methods (update, readChargerStatus, kickWatchdog)
// 6. Private helpers (readReg, writeReg, updateRegBits, setChargeCurrentMa)
```

**For TCA9535Keypad.cpp:**
```cpp
// Implementation order:
// 1. IService methods (init, start, stop, getState, getName)
// 2. IKeypad basic (poll, isKeyPressed, anyKeyDown)
// 3. IKeypad buffer (getNextKey, hasKey, clearBuffer, bufferAddKey, bufferGetKey)
// 4. IKeypad long-press (setLongPressEnabled, setLongPressCallback)
// 5. IKeypad sleep (prepareForSleep, recoverFromSleep)
// 6. Task and ISR (taskFunc, isrHandler)
// 7. Private helpers (readInputs, keyToMask, rawToKey)
```

Alternatively, use comments to separate sections within the file:
```cpp
// === IService Implementation ===
bool TCA9535Keypad::init() { ... }
bool TCA9535Keypad::start() { ... }
// === IKeypad Implementation ===
bool TCA9535Keypad::isKeyPressed(...) { ... }
// === Task and ISR ===
void TCA9535Keypad::taskFunc(...) { ... }
```

## References
- Clean Code: "Arrange code for maximum readability" - Robert C. Martin
- C++ Core Guidelines: C.34 - Group related functions together
