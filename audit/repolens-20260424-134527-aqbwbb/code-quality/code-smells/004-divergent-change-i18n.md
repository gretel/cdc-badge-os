---
title: "[MEDIUM] Divergent Change: I18n::initCoreStrings() is the only place to add translations but grows too large"
severity: MEDIUM
domain: ui
lens: code-smells
labels:
  - "divergent-change"
  - "cdc_ui"
---

## Summary
`components/cdc_ui/src/I18n.cpp:199-369` contains `initCoreStrings()` which registers all ~100 core UI strings in a single monolithic function. Every time a new UI element is added, this function must be modified. The function is 170 lines long and contains strings from many different UI areas (system, lock screen, PIN, settings, hardware, WiFi, Bluetooth, etc.).

## Impact
**High churn**: This file is modified for every UI addition, increasing merge conflict probability.

**Single responsibility violation**: The function handles translations for 10+ different UI areas.

**Discoverability**: Finding where to add a new string requires scanning through 170 lines of macro calls.

**Testing difficulty**: Hard to test specific string groups without testing the whole function.

## Evidence
`components/cdc_ui/src/I18n.cpp:199-369` (170 lines):
```cpp
void I18n::initCoreStrings() {
    #define REG(id, en, de) \
        strings_[0][static_cast<uint16_t>(StringId::id)] = en; \
        strings_[1][static_cast<uint16_t>(StringId::id)] = de

    // === System ===
    REG(MAIN_MENU,      "Main Menu",        "Hauptmenu");
    REG(SETTINGS,       "Settings",         "Einstellungen");
    // ... more system strings

    // === Lock Screen ===
    REG(LOCK,               "Lock",                 "Sperren");
    REG(UNLOCK,             "Unlock",               "Entsperren");
    // ... more lock screen strings

    // === PIN ===
    // ...

    // === Settings ===
    // ...

    // === Hardware ===
    // ...

    // === Actions ===
    // ...

    // === Footer Hints ===
    // ...

    // === QR ===
    // ...

    #undef REG
}
```

All UI areas are in one function with no separation of concerns.

## Recommended Fix
1. **Split into domain-specific functions**:
```cpp
void I18n::initSystemStrings();
void I18n::initLockScreenStrings();
void I18n::initPinStrings();
void I18n::initSettingsStrings();
void I18n::initHardwareStrings();
void I18n::initFooterHints();
```

2. **Update `initCoreStrings()` to delegate**:
```cpp
void I18n::initCoreStrings() {
    initSystemStrings();
    initLockScreenStrings();
    initPinStrings();
    initSettingsStrings();
    initHardwareStrings();
    initFooterHints();
    // ...
}
```

3. **Consider grouping by feature**: WiFi strings, Bluetooth strings could be in their own modules.

**Estimated effort**: ~1 hour to split the function and update `initCoreStrings()`.

## References
- Refactoring.com: "Divergent Change" - https://refactoring.com/catalog/extractClass
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
