---
title: "[LOW] IKeyboardProvider interface missing documentation"
severity: LOW
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IKeyboardProvider` interface (components/cdc_core/include/cdc_core/IKeyboardProvider.h) has minimal documentation:

```cpp
#pragma once
#include <cstdint>

namespace cdc::core {

class IKeyboardProvider {
public:
    virtual ~IKeyboardProvider() = default;
    virtual void setOnKeyPress(std::function<void(uint8_t)> cb) = 0;
    virtual void setOnKeyComplete(std::function<void(const char*)> cb) = 0;
    virtual void simulateKeyPress(uint8_t key) = 0;
};

}
```

No documentation explains:
- What `setOnKeyPress` vs `setOnKeyComplete` means
- When each callback is invoked
- Expected key codes
- Thread safety requirements
- Callback lifetime expectations

## Impact
- **Confusing API**: Developers must guess callback semantics
- **Incorrect usage**: May confuse press vs complete events
- **Maintenance burden**: Future changes lack context

## Evidence
- Interface: components/cdc_core/include/cdc_core/IKeyboardProvider.h

## Recommended Fix
Add comprehensive documentation:

```cpp
/**
 * \brief Keyboard input provider interface for character entry.
 *
 * Implementations provide keyboard input from various sources
 * (USB HID, BLE HID, on-screen keypad, etc.)
 */
class IKeyboardProvider {
public:
    virtual ~IKeyboardProvider() = default;

    /**
     * \brief Set callback for individual key presses.
     * \param cb Callback function, called for each key press.
     * \param key Key code (ASCII value, e.g., 'A', '1', etc.)
     *
     * Called immediately when a key is pressed, before the complete string is available.
     * Use this for visual feedback (e.g., showing asterisks for PIN entry).
     */
    virtual void setOnKeyPress(std::function<void(uint8_t)> cb) = 0;

    /**
     * \brief Set callback for complete string entry.
     * \param cb Callback function, called when entry is complete.
     * \param str Complete string (null-terminated).
     *
     * Called when the user signals entry is complete (e.g., presses ENTER).
     * The string is valid until the next call or until the callback returns.
     */
    virtual void setOnKeyComplete(std::function<void(const char*)> cb) = 0;

    /**
     * \brief Simulate a key press (for testing or programmatic input).
     * \param key Key code to simulate.
     *
     * Triggers the same callbacks as a physical key press.
     * Thread-safe: can be called from any thread.
     */
    virtual void simulateKeyPress(uint8_t key) = 0;
};
```

## References
- IKeyboardProvider: components/cdc_core/include/cdc_core/IKeyboardProvider.h
