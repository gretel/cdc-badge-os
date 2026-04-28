---
title: "[LOW] IKeyboardProvider Interface Missing Error Handling Contract"
severity: LOW
domain: architecture/api-contract
lens: error-handling-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IKeyboardProvider` interface returns `bool` for success/failure but provides no way to distinguish between different error conditions (keyboard disconnected mid-typing, buffer full, timeout, etc.). Consumers cannot implement appropriate error recovery.

## Impact
- **Poor error handling**: Consumers only know "failed" but not "why".
- **No retry strategy**: Can't decide whether to retry based on error type.
- **No user feedback**: UI can only show generic "failed" message.
- **Debugging difficulty**: Can't log specific error conditions.

## Evidence
**Interface definition - `components/cdc_core/include/cdc_core/IKeyboardProvider.h:23-63`:**
```cpp
class IKeyboardProvider {
public:
    virtual ~IKeyboardProvider() = default;
    
    virtual bool isConnected() const = 0;
    virtual bool typeString(const char* text, uint16_t delayMs = 50) = 0;
    virtual bool typeChar(char c) = 0;
    virtual bool isBusy() const = 0;
    virtual void cancel() = 0;
    virtual const char* getStatusText() const { return isConnected() ? "Connected" : "Disconnected"; }
};
```

**Usage pattern with limited error info - `components/mod_totp/src/TotpModule.cpp:463-471`:**
```cpp
if (key == 'Y') {
    auto* kb = core::getKeyboard();
    if (kb && kb->isConnected()) {
        if (timeValid_ && code_[0] != '-') {
            kb->typeString(code_);  // Returns bool, no error detail
            ui::showToastSuccess("Typed");
        }
    } else {
        ui::showToastError(mstr(STR_NO_KEYBOARD));  // Only knows "not connected"
    }
}
```

**BleHidKeyboard implementation - `components/mod_hid/src/BleHidKeyboard.cpp:316-342`:**
```cpp
bool BleHidKeyboard::typeString(const char* text, uint16_t delayMs) {
    if (!text || !isConnected()) return false;  // Can't distinguish these!
    if (busy_) return false;  // Why busy?

    busy_ = true;
    cancelRequested_ = false;

    const char* p = text;
    while (*p && !cancelRequested_) {
        // ...
        if (codepoint < 128) {
            typeAsciiChar(static_cast<char>(codepoint));
        } else {
            typeUnicodeChar(codepoint);
        }
        // ...
    }

    releaseAllKeys();
    busy_ = false;
    return !cancelRequested_;  // Could be timeout, disconnect, or cancel!
}
```

**No error codes defined:**
```cpp
// Missing:
enum class KeyboardError {
    NONE = 0,
    NOT_CONNECTED,
    BUSY,
    TIMEOUT,
    CANCELLED,
    BUFFER_FULL,
    INVALID_CHAR,
};
```

## Recommended Fix
**Add error handling to interface:**

1. **Define error codes:**
```cpp
namespace cdc::core {

enum class KeyboardError {
    NONE = 0,
    NOT_CONNECTED,
    BUSY,
    TIMEOUT,
    CANCELLED,
    BUFFER_FULL,
    INVALID_CHAR,
    UNKNOWN
};

class IKeyboardProvider {
public:
    virtual KeyboardError getLastError() const = 0;
    virtual const char* getLastErrorString() const = 0;
};

} // namespace cdc::core
```

2. **Update implementation to track and return errors.**

3. **Update consumers to handle errors appropriately.**

## References
- IKeyboardProvider: `components/cdc_core/include/cdc_core/IKeyboardProvider.h`
- BleHidKeyboard: `components/mod_hid/include/mod_hid/BleHidKeyboard.h`
