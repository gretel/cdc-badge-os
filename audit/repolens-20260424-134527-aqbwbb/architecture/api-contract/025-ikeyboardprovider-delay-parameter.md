---
title: "[LOW] IKeyboardProvider typeString delay parameter lacks bounds checking"
severity: LOW
domain: architecture/api-contract
lens: parameter-validation
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IKeyboardProvider::typeString` method accepts a `uint16_t delayMs` parameter with a default of 50ms, but no validation ensures the delay is reasonable. Very large delays (up to 65535ms) could cause UI hangs or unexpected behavior.

**Evidence location:** `components/cdc_core/include/cdc_core/IKeyboardProvider.h:38-43`

```cpp
/**
 * Type a string (UTF-8 encoded)
 * @param text Text to type (null-terminated)
 * @param delayMs Delay between keystrokes in milliseconds (default 50ms)
 * @return true if typing started successfully
 */
virtual bool typeString(const char* text, uint16_t delayMs = 50) = 0;
```

## Impact
1. **UI blocking**: A caller could pass `delayMs = 65535` (65 seconds per keystroke), making a 10-character string take ~10 minutes to type
2. **No timeout**: The typing operation may not be cancellable during long delays
3. **Inconsistent behavior**: Different implementations may handle large delays differently

## Evidence
The `isBusy()` and `cancel()` methods exist (lines 48-53):
```cpp
virtual bool isBusy() const = 0;
virtual void cancel() = 0;
```

But there's no guarantee that `cancel()` works reliably during long delays, and the interface doesn't specify timing constraints.

## Recommended Fix
Add documentation for valid delay ranges and optional validation:

```cpp
/**
 * \brief Type a string (UTF-8 encoded)
 * \param text Text to type (null-terminated)
 * \param delayMs Delay between keystrokes (10-1000ms, default 50ms)
 * \return true if typing started successfully
 *
 * \note Delays > 1000ms may cause UI timeout. Use cancel() to abort.
 */
virtual bool typeString(const char* text, uint16_t delayMs = 50) = 0;
```

Or add runtime validation:
```cpp
bool BleHidKeyboard::typeString(const char* text, uint16_t delayMs) {
    // Clamp delay to reasonable range
    constexpr uint16_t MIN_DELAY = 10;
    constexpr uint16_t MAX_DELAY = 1000;
    delayMs = std::max(MIN_DELAY, std::min(MAX_DELAY, delayMs));
    
    // Type string...
}
```

## References
- C++ Core Guidelines F.46: Don't use default arguments for non-obvious defaults
- C++ Core Guidelines E.3: Use `constexpr` for simple functions
