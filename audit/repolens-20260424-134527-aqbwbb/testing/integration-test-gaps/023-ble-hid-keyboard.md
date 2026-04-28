---
title: "[LOW] BleHidKeyboard and KeyboardLayout integration lacks tests"
severity: LOW
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:mod_hid"
  - "area:keyboard"
---

## Summary
The `BleHidKeyboard` component (`components/mod_hid/src/BleHidKeyboard.cpp`) with `KeyboardLayout` (`components/mod_hid/src/KeyboardLayout.cpp`) handles Bluetooth HID keyboard transmission, but **no integration tests** verify that keys are sent correctly with proper modifiers and layout support.

## Impact
- **Key transmission**: Keys may not be sent correctly
- **Modifiers**: Shift, Ctrl may not work
- **Layout**: Different keyboard layouts may not work
- **Report format**: HID reports may not match spec

## Evidence

**BleHidKeyboard API** (`components/mod_hid/src/BleHidKeyboard.cpp`):
```cpp
class BleHidKeyboard {
    bool sendKey(uint8_t key, uint8_t modifiers = 0);
    bool sendString(const char* str);
    void sendModifiers(uint8_t modifiers, bool pressed);
    void releaseAll();
};
```

**KeyboardLayout** (`components/mod_hid/src/KeyboardLayout.cpp`):
```cpp
class KeyboardLayout {
    static uint8_t getKeyCode(char c);
    static uint8_t getModifiers(char c);
    // Maps ASCII to HID key codes
};
```

**Key mapping** (`components/mod_hid/src/KeyboardLayout.cpp:20-100`):
```cpp
uint8_t KeyboardLayout::getKeyCode(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 4;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 4;
    if (c >= '0' && c <= '9') return c - '0' + 39;
    // ... special keys ...
}

uint8_t KeyboardLayout::getModifiers(char c) {
    if (isupper(c)) return KEYMOD_LEFT_SHIFT;
    // ...
}
```

**Usage in modules**:
- `PasswordModule` - Type passwords
- `TOTP module` - Type TOTP codes

**Current test coverage**: None

## Recommended Fix

Create integration test `test_ble_hid_keyboard/` that verifies:

1. **Key transmission**: Single keys sent correctly
2. **String transmission**: Strings sent correctly
3. **Modifiers**: Shift, Ctrl work
4. **Layout**: Uppercase, numbers, symbols work
5. **Release**: releaseAll() clears keys

**Test structure** (example):
```cpp
// test/test_ble_hid_keyboard/test_keyboard.cpp
#include "mod_hid/BleHidKeyboard.h"
#include "mod_hid/KeyboardLayout.h"

void test_send_key() {
    BleHidKeyboard keyboard;
    keyboard.init();
    
    // Send 'A' (requires shift)
    keyboard.sendKey('A');
    
    // Verify HID report sent
    // (hardware-dependent verification)
}

void test_send_string() {
    BleHidKeyboard keyboard;
    keyboard.init();
    
    keyboard.sendString("Hello World");
    
    // Verify all keys sent
}

void test_modifiers() {
    BleHidKeyboard keyboard;
    keyboard.init();
    
    // Send shift
    keyboard.sendModifiers(KEYMOD_LEFT_SHIFT, true);
    
    // Send 'A'
    keyboard.sendKey('a');
    
    // Release shift
    keyboard.sendModifiers(KEYMOD_LEFT_SHIFT, false);
}
```

## References
- [BleHidKeyboard implementation](components/mod_hid/src/BleHidKeyboard.cpp)
- [KeyboardLayout](components/mod_hid/src/KeyboardLayout.cpp)
- [HID module](components/mod_hid/src/HidModule.cpp)

</content>