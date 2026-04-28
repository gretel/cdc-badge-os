---
title: "[MEDIUM] mod_hid: Internal BLE HID keyboard headers exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_hid` module exposes internal implementation headers in its public include directory:

- **`BleHidKeyboard.h`** - BLE HID keyboard implementation (internal class)
- **`KeyboardLayout.h`** - USB HID keycode definitions (internal constants)

These are implementation details. Only `HidModule.h` should be public.

## Impact

- **Implementation Leakage**: External modules can depend on internal BLE HID implementation
- **Tight Coupling**: Changes to keyboard implementation break external consumers
- **Poor Encapsulation**: No clear distinction between public API and internal implementation
- **Unnecessary Exposure**: `KeyboardLayout.h` exposes low-level HID constants that are implementation details

## Evidence

**Current public include structure:**
```
components/mod_hid/include/mod_hid/
├── HidModule.h         # Public API (correct)
├── BleHidKeyboard.h    # BLE HID implementation (internal - exposed!)
└── KeyboardLayout.h    # HID keycodes (internal - exposed!)
```

**Internal header contents:**

`BleHidKeyboard.h` (line 1-80):
```cpp
// BLE HID Keyboard Implementation
class BleHidKeyboard : public core::IKeyboardProvider {
public:
    static BleHidKeyboard& instance();
    
    // Lifecycle
    bool init();
    void deinit();
    
    // IKeyboardProvider implementation
    bool isConnected() const override;
    bool typeString(const char* text, uint16_t delayMs = 50) override;
    
    // Configuration
    void setUnicodeMethod(UnicodeMethod method);
    UnicodeMethod getUnicodeMethod() const;
    
    // BLE HID control (internal!)
    bool startAdvertising();
    void stopAdvertising();
    bool isAdvertising() const;
    
    // Connection callbacks (internal!)
    void onConnect(uint16_t connHandle);
    void onDisconnect(uint16_t connHandle, int reason);
    
private:
    // Internal typing helpers
    bool sendKeyReport(uint8_t modifier, uint8_t keycode);
    bool typeAsciiChar(char c);
    bool typeUnicodeChar(uint32_t codepoint);
    // ...
};
```

`KeyboardLayout.h` (line 1-60):
```cpp
// USB HID Keyboard Keycodes (internal constants)
namespace KeyCode {
    constexpr uint8_t KEY_A = 0x04;
    constexpr uint8_t KEY_B = 0x05;
    // ... 60+ keycodes
}

namespace Modifier {
    constexpr uint8_t LEFT_CTRL = 0x01;
    // ... modifier bits
}

struct KeyMapping {
    uint8_t keycode;
    uint8_t modifier;
};

KeyMapping getKeyMapping(char c);  // Internal lookup function
```

**Usage in module:**
```cpp
// From components/mod_hid/src/HidModule.cpp:
#include "mod_hid/BleHidKeyboard.h"
#include "mod_hid/KeyboardLayout.h"

// From components/mod_hid/src/BleHidKeyboard.cpp:
#include "mod_hid/BleHidKeyboard.h"
#include "mod_hid/KeyboardLayout.h"
```

## Recommended Fix

1. **Move internal headers to src/**:
   ```
   components/mod_hid/
   ├── include/mod_hid/
   │   └── HidModule.h     # Only public API
   └── src/
       ├── HidModule.cpp
       ├── BleHidKeyboard.h    # Move here (internal)
       ├── BleHidKeyboard.cpp
       ├── KeyboardLayout.h    # Move here (internal)
       └── KeyboardLayout.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/HidModule.cpp, change:
   #include "mod_hid/BleHidKeyboard.h"  # → #include "BleHidKeyboard.h"
   #include "mod_hid/KeyboardLayout.h"  # → #include "KeyboardLayout.h"
   ```

3. **Add internal markers**:
   ```cpp
   // In src/BleHidKeyboard.h:
   /**
    * @file BleHidKeyboard.h
    * @brief Internal BLE HID keyboard - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `HidModule.h`:
   ```cpp
   /**
    * @defgroup hid-public Public API
    * @brief HID module - Keyboard output
    * 
    * Public classes:
    * - @ref HidModule - Main module interface
    */
   ```

5. **Consider public interface**:
   - If external modules need keyboard functionality, expose only `IKeyboardProvider` interface
   - Keep `BleHidKeyboard` implementation details internal

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- IKeyboardProvider interface: `components/cdc_core/include/cdc_core/IKeyboardProvider.h`

(End of file - total 152 lines)
