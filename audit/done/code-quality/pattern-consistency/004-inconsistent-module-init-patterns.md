---
title: "[MEDIUM] Inconsistent module initialization patterns for i18n and views"
severity: MEDIUM
domain: architecture
lens: pattern-consistency
labels:
  - "audit:code-quality/pattern-consistency"
---

## Summary

Modules use two different patterns for initializing i18n strings and views during module startup, creating inconsistency in initialization order and timing:

1. **Standard pattern (mod_totp, mod_gpg, mod_password, mod_hid, mod_ble_serial)**: Call `registerStrings()` directly in the module's `init()` method
2. **Lazy pattern (mod_fido2)**: Defer string registration to first use via lazy initialization in helper functions

**Files affected:**
- `components/mod_fido2/src/Fido2Ui.cpp` - Lazy i18n registration
- `components/mod_fido2/src/Fido2Module.cpp` - Calls `fido2_ui_init()` without explicit string registration
- `components/mod_totp/src/TotpModule.cpp` - Standard pattern (reference)
- `components/mod_gpg/src/GpgModule.cpp` - Standard pattern (reference)

### Standard pattern (most modules)

```cpp
// In mod_totp/src/TotpModule.cpp
bool TotpModule::init() {
    LOG_I(TAG, "Initializing TOTP module");
    registerStrings();  // Strings registered immediately
    registerCommands();
    core::ModuleRegistry::instance().registerModule(this);
    // ...
}
```

Views are also initialized statically:
```cpp
static ui::ListView s_listView;
static ui::T9InputView s_t9Input;
static bool s_viewsInitialized = false;

// Used on first access:
items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_viewsInitialized = true;
    }
    rebuildList();
    return &s_listView;
}, ...};
```

### Lazy pattern (mod_fido2)

```cpp
// In mod_fido2/src/Fido2Ui.cpp
void fido2_ui_init() {
    registerStrings();  // Strings registered here, not in module init
    if (!s_listView) {
        s_listView = new ui::ListView();
        s_listView->setOnSelect(onListSelect);
    }
}

// Called from module init:
bool Fido2Module::init() {
    fido2_ui_init();  // Delegates to UI helper
    // ...
}

// Lazy registration on first label access:
const char* fido2_ui_get_label() {
    if (s_strIdBase == 0) {
        registerStrings();  // Deferred even further!
    }
    return ui::tr(s_strIdBase + STR_WEB_AUTHN);
}
```

## Impact

1. **Initialization order confusion**: Developers may not realize i18n strings are registered lazily in FIDO2
2. **Thread safety concerns**: Lazy initialization without synchronization could cause race conditions if accessed from multiple contexts
3. **Inconsistent error handling**: Standard pattern checks for registration failure immediately; lazy pattern may fail silently later
4. **Memory allocation timing**: FIDO2 uses dynamic allocation (`new ui::ListView()`) while other modules use static instances
5. **Debugging difficulty**: When strings aren't available, the cause differs between modules (immediate vs. deferred registration)

## Evidence

**Standard pattern (mod_totp):**
- `components/mod_totp/src/TotpModule.cpp:930` - `registerStrings();` called in `init()`
- `components/mod_totp/src/TotpModule.cpp:544` - Static views: `static ui::ListView s_listView;`
- `components/mod_totp/src/TotpModule.cpp:998` - View initialization on first access via lambda

**Standard pattern (mod_gpg):**
- `components/mod_gpg/src/GpgModule.cpp:553` - `registerStrings();` called in `init()`
- `components/mod_gpg/src/GpgModule.cpp:196` - Static views: `static ui::ListView s_menuView;`

**Lazy pattern (mod_fido2):**
- `components/mod_fido2/src/Fido2Ui.cpp:125` - `void fido2_ui_init() { registerStrings(); ... }`
- `components/mod_fido2/src/Fido2Ui.cpp:85` - Pointer views: `static ui::ListView* s_listView = nullptr;`
- `components/mod_fido2/src/Fido2Ui.cpp:273` - `const char* fido2_ui_get_label() { if (s_strIdBase == 0) registerStrings(); ... }`
- `components/mod_fido2/src/Fido2Module.cpp:126` - `fido2_ui_init();` called in module `init()`

## Recommended Fix

Standardize on the **immediate initialization pattern** used by most modules:

### For mod_fido2:

1. Move `registerStrings()` call to `Fido2Module::init()` directly:

```cpp
// In Fido2Module.cpp
bool Fido2Module::init() {
    LOG_I(TAG, "Initializing FIDO2 module");
    
    // Create RX queue
    if (!s_rx_queue) {
        s_rx_queue = xQueueCreate(FIDO_QUEUE_SIZE, sizeof(FidoPacket));
        if (!s_rx_queue) {
            LOG_E(TAG, "Failed to create RX queue");
            return false;
        }
    }
    
    fido2_ui_init();  // Now just initializes views, not strings
    core::ModuleRegistry::instance().registerModule(this);
    // ...
}
```

2. Add a separate `registerStrings()` call in `Fido2Module::init()`:

```cpp
// In Fido2Module.cpp
bool Fido2Module::init() {
    // ... queue creation ...
    
    // Register i18n strings immediately
    fido2_ui_register_strings();  // Extracted from fido2_ui_init()
    
    fido2_ui_init();  // Now just initializes views
    // ...
}
```

3. Remove lazy registration from `fido2_ui_get_label()`:

```cpp
// Before:
const char* fido2_ui_get_label() {
    if (s_strIdBase == 0) {
        registerStrings();
    }
    return ui::tr(s_strIdBase + STR_WEB_AUTHN);
}

// After (strings already registered in init()):
const char* fido2_ui_get_label() {
    return ui::tr(s_strIdBase + STR_WEB_AUTHN);
}
```

4. Consider converting pointer views to static instances for consistency:

```cpp
// Before:
static ui::ListView* s_listView = nullptr;

void fido2_ui_init() {
    if (!s_listView) {
        s_listView = new ui::ListView();
        s_listView->setOnSelect(onListSelect);
    }
}

// After:
static ui::ListView s_listView;
static bool s_viewsInitialized = false;

void fido2_ui_init() {
    if (!s_viewsInitialized) {
        s_listView.setOnSelect(onListSelect);
        s_viewsInitialized = true;
    }
}
```

## References

- CLAUDE.md: Module Architecture section - "Modules must be self-contained"
- `components/mod_totp/src/TotpModule.cpp` - Reference implementation of standard pattern
- `components/mod_gpg/src/GpgModule.cpp` - Another reference implementation
- `components/cdc_core/include/cdc_core/IModule.h` - Module interface definition

</content>