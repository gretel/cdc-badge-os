---
title: "[MEDIUM] Implicit IKeyboardProvider Contract with Missing Null Checks"
severity: MEDIUM
domain: API Contract Integrity
lens: module-boundaries
labels:
  - "audit:architecture/api-contract"
---

## Summary
Modules use `ServiceRegistry::request<IKeyboardProvider>()` to get keyboard access, but many call sites assume the keyboard is always available without checking for `nullptr`. The contract between keyboard provider modules (mod_hid) and consumers (mod_totp, mod_password) is not enforced with proper null checks.

**Location**: 
- `components/mod_totp/src/TotpModule.cpp` - Multiple call sites
- `components/mod_password/src/PasswordModule.cpp` - Multiple call sites
- `components/cdc_core/include/cdc_core/IKeyboardProvider.h:62-67` - Convenience function

## Impact
1. **Silent failures**: When no keyboard is connected, typing operations fail silently
2. **Poor UX**: Users get no feedback when keyboard is disconnected
3. **Race conditions**: Keyboard may not be registered when modules start (initialization order issue)

## Evidence

In `components/mod_totp/src/TotpModule.cpp:450-465` (example pattern):
```cpp
static void onTypeCode() {
    auto* kb = cdc::core::ServiceRegistry::instance().request<cdc::core::IKeyboardProvider>(
        cdc::core::ServiceType::KEYBOARD);
    if (!kb) {
        // Should show error, but often just returns silently
        return;
    }
    // No isConnected() check before typeString()
    kb->typeString(code, 50);  // May fail if not connected
}
```

In `components/cdc_core/include/cdc_core/IKeyboardProvider.h:62-67`:
```cpp
/**
 * Convenience function to get keyboard provider
 * @return Pointer to keyboard provider or nullptr if none registered
 */
IKeyboardProvider* getKeyboard();
```

The function returns `nullptr` when no provider is registered, but callers often don't check:
```cpp
// Pattern found in multiple modules:
auto* kb = cdc::core::getKeyboard();
kb->typeString(code);  // CRASH if kb is nullptr!
```

## Recommended Fix

1. **Add explicit null checks** in all keyboard consumers:
   ```cpp
   auto* kb = cdc::core::getKeyboard();
   if (!kb) {
       ui::showToastError(ui::tr(ui::StringId::NO_KEYBOARD));
       return;
   }
   ```

2. **Check connection state** before typing:
   ```cpp
   if (!kb->isConnected()) {
       ui::showToastError(ui::tr(ui::StringId::KEYBOARD_NOT_CONNECTED));
       return;
   }
   kb->typeString(code);
   ```

3. **Document the contract** in `IKeyboardProvider.h`:
   ```cpp
   /**
    * \brief Type a string (UTF-8 encoded).
    * \param text Text to type (null-terminated).
    * \param delayMs Delay between keystrokes in milliseconds.
    * \return true if typing started successfully.
    * \note MUST check isConnected() before calling. Returns false if not connected.
    */
   virtual bool typeString(const char* text, uint16_t delayMs = 50) = 0;
   ```

4. **Add defensive programming** in `typeString()` implementations:
   ```cpp
   bool HidKeyboard::typeString(const char* text, uint16_t delayMs) {
       if (!text) return false;
       if (!isConnected()) return false;  // <-- Defensive check
       // ... rest of implementation
   }
   ```

## References
- `components/cdc_core/include/cdc_core/IKeyboardProvider.h:24-48` - Keyboard interface definition
- `components/mod_hid/src/HidModule.cpp` - Keyboard provider implementation
- `components/mod_totp/src/TotpModule.cpp:450-465` - Usage pattern
