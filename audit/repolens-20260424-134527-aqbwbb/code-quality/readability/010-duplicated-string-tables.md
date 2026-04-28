---
title: "[010] [MEDIUM] Duplicated translation tables without DRY extraction"
severity: MEDIUM
domain: Code Readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
The I18n translation tables repeat the same string IDs twice (once for English, once for German) in a verbose macro expansion pattern that makes the file hard to scan and maintain.

## Impact
- File is 370+ lines with 60% being repetitive string definitions
- Hard to see which strings have German translations at a glance
- Adding a new string requires editing two separate sections
- Easy to miss a string in one language when adding/updating

## Evidence
**File: `components/cdc_ui/src/I18n.cpp:201-370`**
```cpp
void I18n::initCoreStrings() {
    // Macro for cleaner registration
    #define REG(id, en, de) \
        strings_[0][static_cast<uint16_t>(StringId::id)] = en; \
        strings_[1][static_cast<uint16_t>(StringId::id)] = de

    // === System ===
    REG(MAIN_MENU,      "Main Menu",        "Hauptmenu");
    REG(SETTINGS,       "Settings",         "Einstellungen");
    REG(HARDWARE,       "Hardware",         "Hardware");
    // ... ~50 more REG() calls

    // === Lock Screen ===
    REG(LOCK,               "Lock",                 "Sperren");
    REG(UNLOCK,             "Unlock",               "Entsperren");
    // ... ~10 more REG() calls

    // === PIN ===
    // ... ~10 more REG() calls

    // === Settings ===
    // ... ~20 more REG() calls

    // === Hardware ===
    // ... ~40 more REG() calls

    // === Actions ===
    // ... ~5 more REG() calls

    // === Footer Hints ===
    // ... ~10 more REG() calls

    // === QR ===
    // ... ~2 more REG() calls

    #undef REG
}
```

The macro helps but still requires listing each string twice. A more readable approach would group by string ID.

## Recommended Fix
Restructure to use a data-driven approach with a table of translations:

```cpp
struct StringTranslation {
    StringId id;
    const char* en;
    const char* de;
};

static constexpr StringTranslation CORE_STRINGS[] = {
    // System
    {StringId::MAIN_MENU,     "Main Menu",        "Hauptmenu"},
    {StringId::SETTINGS,      "Settings",         "Einstellungen"},
    {StringId::HARDWARE,      "Hardware",         "Hardware"},
    {StringId::TOOLS,         "Tools",            "Werkzeuge"},
    // ... all strings in one table

    // Lock Screen
    {StringId::LOCK,          "Lock",             "Sperren"},
    {StringId::UNLOCK,        "Unlock",           "Entsperren"},
    // ...
};

void I18n::initCoreStrings() {
    for (const auto& entry : CORE_STRINGS) {
        strings_[0][static_cast<uint16_t>(entry.id)] = entry.en;
        strings_[1][static_cast<uint16_t>(entry.id)] = entry.de;
    }
}
```

Alternatively, use a compile-time approach with `constexpr` and `std::array` for better type safety.

## References
- Refactoring: "Replace Conditional with Table" - Martin Fowler
- Clean Code: "Duplicate code is the root of many bugs" - Robert C. Martin
