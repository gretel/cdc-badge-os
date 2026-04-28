---
title: "[MEDIUM] I18n string registration creates temporal coupling during module initialization"
severity: MEDIUM
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
Modules register their i18n strings during `init()` using static `s_strIdBase` variables. This creates temporal coupling because:
1. String IDs must be registered before any views use them
2. If `mstr()` is called before `registerStrings()`, it returns invalid strings
3. Menu item factory functions may be called before strings are registered

**Evidence locations:**
- `components/mod_gpg/src/GpgModule.cpp:28` - `s_strIdBase` declaration
- `components/mod_gpg/src/GpgModule.cpp:56-105` - `registerStrings()` function
- `components/mod_gpg/src/GpgModule.cpp:632` - Menu item uses `mstr()` in lambda

## Impact
- **Initialization order sensitivity**: Wrong call order causes null/invalid strings
- **Race conditions**: Menu items may be created before strings registered
- **Hard to debug**: Missing translations appear as empty strings
- **Fragile refactors**: Moving code can break string registration order

## Evidence
```cpp
// components/mod_gpg/src/GpgModule.cpp:28
static uint16_t s_strIdBase = 0;

// Line 54-58: mstr() helper
static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

// Line 56-105: registerStrings() - must be called before mstr() is used
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_gpg", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }
    // ... register all translations
}

// Line 632: Menu item lambda uses mstr() - may be called BEFORE registerStrings()
items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_menuView.setOnSelect(onMenuSelect);
        s_viewsInitialized = true;
    }
    // ...
    rebuildMenu();  // Uses mstr()
    return &s_menuView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

**Problem flow:**
1. Module registered: `mod_gpg_register()` calls initializer
2. `GpgModule::init()` calls `registerStrings()` 
3. `getMenuItems()` returns menu with lambda
4. **If menu selected before init completes**: `mstr()` returns null/invalid

```cpp
// components/mod_password/src/PasswordModule.cpp:28
static uint16_t s_strIdBase = 0;

// Line 56-116: registerStrings()
static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_password", STR_COUNT);
    // ...
}

// Line 823: Similar pattern - lambda uses mstr()
items[0] = {mstr(STR_PASSWORDS), 55, []() -> ui::IView* {
    // ...
    rebuildList();  // Uses mstr()
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

## Recommended Fix
**Ensure string registration happens before any usage:**

1. **Register strings in module constructor:**
```cpp
// components/mod_gpg/include/mod_gpg/GpgModule.h
class GpgModule : public core::IModule {
public:
    GpgModule();  // Constructor registers strings
    // ...
};

// components/mod_gpg/src/GpgModule.cpp
static uint16_t s_strIdBase = 0;

GpgModule::GpgModule() {
    registerStrings();  // Called once at construction
}

bool GpgModule::init() {
    // No longer need to call registerStrings()
    // Strings already registered in constructor
    core::ModuleRegistry::instance().registerModule(this);
    // ...
}
```

2. **Or use lazy initialization with guard:**
```cpp
static const char* mstr(uint16_t offset) {
    if (s_strIdBase == 0) {
        // Lazy register if not done
        registerStrings();
    }
    return ui::tr(s_strIdBase + offset);
}

// Now safe to call mstr() anywhere
items[0] = {mstr(STR_GPG), 60, []() -> ui::IView* {
    // mstr() will auto-register if needed
    rebuildMenu();
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

3. **Store string IDs in module instance:**
```cpp
class GpgModule : public core::IModule {
public:
    GpgModule();
    const char* str(uint16_t offset) const { return ui::tr(strBase_ + offset); }
    
private:
    uint16_t strBase_ = 0;
};

GpgModule::GpgModule() {
    strBase_ = ui::I18n::instance().registerModule("mod_gpg", STR_COUNT);
}

static void rebuildMenu(GpgModule& module) {
    s_menuItems[0].label = module.str(STR_STATUS);  // Use instance method
}
```

## References
- [Initialization Order Fiasco](https://www.learncpp.com/cpp-tutorial/static-initialization-order-fiasco/)
- [Lazy Initialization Pattern](https://en.wikipedia.org/wiki/Lazy_evaluation)
