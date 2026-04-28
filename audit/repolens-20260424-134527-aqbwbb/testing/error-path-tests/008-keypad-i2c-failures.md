---
title: "[LOW] TCA9535 keypad I2C read failures return invalid data"
severity: LOW
domain: error-path-tests
lens: keypad
labels:
  - "i2c"
  - "keypad"
  - "hardware"
---

## Summary
In `components/cdc_hal/src/TCA9535Keypad.cpp` (lines 271-279), when I2C read fails, the `readInputs()` function returns `0xFFFF` (all keys pressed). This is confusing because it doesn't distinguish between "all keys pressed" and "I2C error".

**Files:**
- `components/cdc_hal/src/TCA9535Keypad.cpp:271-279`

## Impact
1. **False Key Events**: I2C error interpreted as all 16 keys pressed
2. **Debouncing Issues**: Invalid data disrupts key debouncing logic
3. **Debug Difficulty**: Hard to tell if user pressed all keys or I2C failed
4. **UI Confusion**: Display shows all keys active when hardware failed

## Evidence
From `components/cdc_hal/src/TCA9535Keypad.cpp`:

```cpp
// Lines 271-279: readInputs() - returns 0xFFFF on I2C failure
uint16_t TCA9535Keypad::readInputs() {
    SemaphoreGuard guard(semaphore);
    
    // Read from bank 0
    uint8_t data0 = 0;
    esp_err_t err = i2cManager->readByte(port, address, 0, &data0);
    if (err != ESP_OK) {
        LOG_E("Keypad", "I2C read failed: %s", esp_err_to_name(err));
        // ❌ Returns invalid data!
        return 0xFFFF;  // All keys "pressed"
    }
    
    // Read from bank 1
    uint8_t data1 = 0;
    err = i2cManager->readByte(port, address, 1, &data1);
    if (err != ESP_OK) {
        LOG_E("Keypad", "I2C read failed: %s", esp_err_to_name(err));
        return 0xFFFF;  // All keys "pressed"
    }
    
    // Combine
    return (data1 << 8) | data0;
}

// Usage in debouncer:
uint16_t raw = keypad->readInputs();  // 0xFFFF on I2C error
uint16_t pressed = debouncer.update(raw);  // Processes 0xFFFF as real input
// ❌ All keys appear pressed!
```

**Problem:**
- Returns `0xFFFF` which looks like valid "all keys pressed"
- No way for caller to distinguish I2C error from real input
- Debouncing logic treats it as real key events

## Recommended Fix
Return error indication separately from key data:

1. **Add error return to readInputs()**:
   ```cpp
   // Option 1: Return struct with status
   struct KeypadReadResult {
       uint16_t keys;
       bool error;
   };
   
   KeypadReadResult TCA9535Keypad::readInputs() {
       SemaphoreGuard guard(semaphore);
       
       uint8_t data0 = 0;
       esp_err_t err = i2cManager->readByte(port, address, 0, &data0);
       if (err != ESP_OK) {
           LOG_E("Keypad", "I2C read failed: %s", esp_err_to_name(err));
           return {0, true};  // No keys, error
       }
       
       uint8_t data1 = 0;
       err = i2cManager->readByte(port, address, 1, &data1);
       if (err != ESP_OK) {
           LOG_E("Keypad", "I2C read failed: %s", esp_err_to_name(err));
           return {0, true};
       }
       
       return {(data1 << 8) | data0, false};
   }
   ```

2. **Option 2: Use bit for error flag**:
   ```cpp
   // Define error bit in high byte
   #define KEYPAD_ERROR_FLAG 0x8000
   
   uint16_t TCA9535Keypad::readInputs() {
       // ... existing code ...
       if (err != ESP_OK) {
           return KEYPAD_ERROR_FLAG;  // Clear bits, set error flag
       }
       // ...
   }
   
   // Usage:
   uint16_t raw = keypad->readInputs();
   if (raw & KEYPAD_ERROR_FLAG) {
       LOG_W("Keypad", "I2C read error, ignoring");
       return;  // Skip debouncing
   }
   uint16_t keys = raw & 0x0FFF;  // Clear error bit
   ```

3. **Update debouncer to handle errors**:
   ```cpp
   void KeyDeboouncer::update(uint16_t raw) {
       // Check for error flag
       if (raw & KEYPAD_ERROR_FLAG) {
           // Reset debouncing state
           lastState = 0;
           debounceCount = 0;
           return;
       }
       
       // Normal debouncing...
   }
   ```

4. **Add test cases**:
   - Mock I2C to return error
   - Verify error flag is set
   - Verify debouncer handles error correctly
   - Verify no false key events on I2C error

## References
- TCA9535 Datasheet: https://www.ti.com/lit/ds/symlink/tca9535.pdf
- I2C Error Handling: https://www.nxp.com/docs/en/application-note/AN10279.pdf

</content>