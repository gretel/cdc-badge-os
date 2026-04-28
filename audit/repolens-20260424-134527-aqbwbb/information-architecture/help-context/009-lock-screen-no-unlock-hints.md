---
title: "[LOW] Lock screen provides minimal hints about unlock options"
severity: LOW
domain: information-architecture
lens: help-context
labels:
  - "audit:information-architecture/help-context"
---

## Summary
The lock screen (`components/cdc_os_ui/src/views/LockScreenView.cpp`) shows basic unlock instructions but doesn't explain all available options or what happens after unlock.

**Evidence** (`components/cdc_os_ui/src/views/LockScreenView.cpp:400-403`):
```cpp
const char* LockScreenView::getFooterHint() const {
    // Returns hint based on lock-screen mode
    // e.g., "[0-9] Input [Y] OK" or "Any key: unlock  [3]: menu"
}
```

Footer hints (`components/cdc_ui/src/I18n.cpp:351-363`):
```cpp
REG(HINT_PIN_INPUT,     "[0-9] Input [Y] OK",   "[0-9] Eingabe [Y] OK");
REG(PRESS_ANY_KEY,      "Any key: unlock  [3]: menu",  "Taste: entsperren  [3]: Menue");
```

The hints are minimal:
- No explanation of what the PIN unlocks (badge access, modules?)
- No hint about PIN length (4-8 digits based on code)
- No indication of retry limits
- No explanation of what "[3]: menu" does when locked

## Impact
New users may:
1. Not know if they need a default PIN or if they set one
2. Enter wrong PIN repeatedly and get locked out
3. Not understand what happens after unlock
4. Confuse the menu option (does it work when locked?)

## Evidence
- File: `components/cdc_os_ui/src/views/LockScreenView.cpp`
- Lines: 400-403 (footer hint function)
- Lines: 378-399 (PIN entry logic with 3-attempt limit)
- No help text about PIN requirements or recovery

## Recommended Fix
Add contextual help:

1. **Enhanced initial hint**:
   ```
   Enter PIN (4-8 digits)
   [0-9] Input  [Y] OK  [3] Help
   ```

2. **Help action** (key '3'):
   Shows:
   - Default PIN is "1234" (if not changed)
   - 3 attempts before lockout
   - Menu accessible with [3]

3. **Retry counter display**:
   ```
   PIN: ••••
   Attempts left: 3
   ```

## References
- Lock screen UX patterns: https://developer.apple.com/design/human-interface-guidelines/locks/
