---
title: "[LOW] Inconsistent view storage patterns across modules"
severity: LOW
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

Modules use two different patterns for storing UI view instances: some use **static instances** (stack/segment allocation), while others use **pointers** (with lazy initialization). Both patterns work correctly, but the inconsistency can be confusing for developers.

**Files affected:**
- `components/mod_fido2/src/Fido2Ui.cpp` - Uses pointer pattern
- `components/mod_password/src/PasswordModule.cpp` - Uses pointer pattern
- `components/mod_totp/src/TotpModule.cpp` - Uses static instance pattern
- `components/mod_gpg/src/GpgModule.cpp` - Uses static instance pattern

### Pattern 1: Static instances (mod_totp, mod_gpg, mod_sao)

```cpp
// In TotpModule.cpp
static ui::ListView s_listView;
static ui::T9InputView s_t9Input;
static ui::ListView s_digitsMenu;

// Usage:
s_listView.setOnSelect(onListSelect);
s_listView.init(mstr(STR_TOTP), s_listItems, count);
ui::ViewStack::instance().push(&s_listView);
```

### Pattern 2: Pointers with lazy initialization (mod_fido2, mod_password)

```cpp
// In Fido2Ui.cpp
static ui::ListView* s_listView = nullptr;
static ui::InfoView* s_detailView = nullptr;

// Lazy initialization in a setup function:
static void ensureViews() {
    if (!s_listView) {
        s_listView = new ui::ListView();
        s_listView->setOnSelect(onListSelect);
    }
}

// Usage:
ensureViews();
s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, count);
ui::ViewStack::instance().push(s_listView);
```

## Impact

1. **Memory allocation timing**: Static instances are allocated at compile time; pointers are allocated at runtime (first use)
2. **Different syntax**: `&s_listView` vs `s_listView`, `s_listView.init()` vs `s_listView->init()`
3. **Potential memory leaks**: Pointer pattern requires explicit cleanup (though none found in current code)
4. **Cognitive overhead**: Developers need to remember which pattern each module uses

## Evidence

**mod_totp (static instances):**
- `components/mod_totp/src/TotpModule.cpp:544` - `static ui::ListView s_listView;`
- `components/mod_totp/src/TotpModule.cpp:545` - `static ui::T9InputView s_t9Input;`
- `components/mod_totp/src/TotpModule.cpp:719` - `s_listView.init(mstr(STR_TOTP), s_listItems, ...);`

**mod_gpg (static instances):**
- `components/mod_gpg/src/GpgModule.cpp:196` - `static ui::ListView s_menuView;`
- `components/mod_gpg/src/GpgModule.cpp:197` - `static ui::ListView s_settingsView;`
- `components/mod_gpg/src/GpgModule.cpp:266` - `s_menuView.init(mstr(STR_GPG), s_menuItems, 5);`

**mod_fido2 (pointers):**
- `components/mod_fido2/src/Fido2Ui.cpp:85` - `static ui::ListView* s_listView = nullptr;`
- `components/mod_fido2/src/Fido2Ui.cpp:86` - `static ui::InfoView* s_detailView = nullptr;`
- `components/mod_fido2/src/Fido2Ui.cpp:132` - `s_listView->init(mstr(STR_WEB_AUTHN), s_listItems, 1);`

**mod_password (pointers):**
- `components/mod_password/src/PasswordModule.cpp:277` - `static ui::ListView* s_listView = nullptr;`
- `components/mod_password/src/PasswordModule.cpp:278` - `static ui::ListView* s_menuView = nullptr;`
- `components/mod_password/src/PasswordModule.cpp:433` - `s_listView->init(mstr(STR_PASSWORDS), s_listItems, ...);`

## Recommended Fix

Standardize on one pattern across all modules. The **static instance pattern** is preferred because:

1. No dynamic allocation (no heap, no leaks)
2. Simpler syntax (no pointers, no `->`)
3. Predictable initialization order
4. Matches the ESP32's static-heavy design philosophy

### For modules using pointers (mod_fido2, mod_password):

Refactor to use static instances:

```cpp
// Before:
static ui::ListView* s_listView = nullptr;

static void ensureViews() {
    if (!s_listView) {
        s_listView = new ui::ListView();
    }
}

// After:
static ui::ListView s_listView;

static void ensureViews() {
    // No allocation needed
}
```

Update all usages:
```cpp
// Before:
s_listView->init(...);
ui::ViewStack::instance().push(s_listView);

// After:
s_listView.init(...);
ui::ViewStack::instance().push(&s_listView);
```

## References

- CLAUDE.md: Memory Considerations section - "Static allocation preferred over dynamic where possible"
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - View stack API
