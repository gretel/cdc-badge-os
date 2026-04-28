---
title: "[MEDIUM] Heavy reliance on singleton pattern creates tight coupling between modules"
severity: MEDIUM
domain: Architecture/Coupling
lens: architecture/coupling
labels:
  - "audit:architecture/coupling"
---

## Summary
The codebase uses approximately 20+ singleton classes that are accessed directly throughout the codebase. This creates tight coupling because:
1. Modules directly call `Singleton::instance()` methods (e.g., `ModuleRegistry::instance()`, `ServiceRegistry::instance()`, `ViewStack::instance()`)
2. No dependency injection - modules create their own dependencies internally
3. Testing requires all singletons to be available in correct initialization order

**Evidence locations:**
- `components/cdc_core/include/cdc_core/ServiceRegistry.h:28` - ServiceRegistry singleton
- `components/cdc_core/include/cdc_core/ModuleRegistry.h:18` - ModuleRegistry singleton
- `components/cdc_core/include/cdc_core/EventBus.h:79` - EventBus singleton
- `components/cdc_core/include/cdc_core/PinManager.h:58` - PinManager singleton
- `components/cdc_core/include/cdc_core/UsbManager.h:44` - UsbManager singleton
- `components/cdc_ui/include/cdc_ui/ViewStack.h:22` - ViewStack singleton
- `components/cdc_ui/include/cdc_ui/I18n.h:22` - I18n singleton
- `components/mod_gpg/include/mod_gpg/GpgModule.h:22` - GpgModule singleton
- `components/mod_password/include/mod_password/PasswordModule.h:22` - PasswordModule singleton

**Usage examples:**
```cpp
// In components/mod_gpg/src/GpgModule.cpp:554
core::ModuleRegistry::instance().registerModule(this);

// In components/mod_password/src/PasswordModule.cpp:442
auto* kb = core::getKeyboard();  // Calls ServiceRegistry::instance().request<>()

// In components/cdc_os_ui/src/AppUi.cpp:293
core::PinManager::instance().verifyBadgePin(pin);
```

## Impact
- **Testing difficulty**: Unit tests must set up entire singleton ecosystem even for simple tests
- **Rigid architecture**: Changing initialization order breaks multiple modules
- **Hidden dependencies**: Module A may implicitly depend on Module B's singleton being initialized first
- **Memory management**: Singletons live for entire program lifetime, no cleanup possible

## Evidence
**Singleton instances identified (20+):**
1. ServiceRegistry (`cdc_core/ServiceRegistry.h:28`)
2. ModuleRegistry (`cdc_core/ModuleRegistry.h:18`)
3. EventBus (`cdc_core/EventBus.h:79`)
4. PinManager (`cdc_core/PinManager.h:58`)
5. UsbManager (`cdc_core/UsbManager.h:44`)
6. TropicStorage (`cdc_core/TropicStorage.h`)
7. TropicSlotMap (`cdc_core/TropicSlotMap.h`)
8. ViewStack (`cdc_ui/ViewStack.h:22`)
9. I18n (`cdc_ui/I18n.h:22`)
10. GpgModule (`mod_gpg/GpgModule.h:22`)
11. PasswordModule (`mod_password/PasswordModule.h:22`)
12. TotpModule (`mod_totp/TotpModule.h`)
13. TotpStore (`mod_totp/TotpStore.h`)
14. PasswordStore (`mod_password/PasswordStore.h`)
15. HidModule (`mod_hid/HidModule.h`)
16. BleHidKeyboard (`mod_hid/BleHidKeyboard.h`)
17. AppUi (`cdc_os_ui/AppUi.h`)
18. SleepManager (`cdc_os_ui/SleepManager.h`)
19. WifiHandlers (`cdc_os_ui/WifiHandlers.h`)

**Direct singleton access patterns:**
```cpp
// components/mod_password/src/PasswordModule.cpp:442
auto* kb = core::getKeyboard();  // Static function calling ServiceRegistry::instance()

// components/cdc_os_ui/src/AppUi.cpp:644
I18n::instance().getLanguageName(Language::EN);

// components/cdc_os_ui/src/AppUi.cpp:293
core::PinManager::instance().verifyBadgePin(pin);
```

## Recommended Fix
Implement a **Dependency Injection Container** pattern:

1. **Create a `CoreDependencies` struct** that holds all core service pointers:
```cpp
struct CoreDependencies {
    cdc::core::ServiceRegistry* serviceRegistry;
    cdc::core::ModuleRegistry* moduleRegistry;
    cdc::core::EventBus* eventBus;
    cdc::core::PinManager* pinManager;
    cdc::core::UsbManager* usbManager;
    cdc::ui::ViewStack* viewStack;
    cdc::ui::I18n* i18n;
};
```

2. **Create a global dependency accessor** (initialized once in main):
```cpp
void initDependencies(const CoreDependencies& deps);
CoreDependencies& getDependencies();
```

3. **Create typed accessors** for common dependencies:
```cpp
inline cdc::core::ModuleRegistry* getModuleRegistry() {
    return getDependencies().moduleRegistry;
}

inline cdc::ui::ViewStack* getViewStack() {
    return getDependencies().viewStack;
}
```

4. **Refactor modules** to use dependency accessors instead of direct singleton calls:
```cpp
// Before:
core::ModuleRegistry::instance().registerModule(this);

// After:
getModuleRegistry()->registerModule(this);
```

5. **For testing**, create a test-specific dependency setup:
```cpp
void setupTestDependencies() {
    CoreDependencies deps = {
        .serviceRegistry = &testServiceRegistry,
        .moduleRegistry = &testModuleRegistry,
        // ...
    };
    initDependencies(deps);
}
```

**Priority order for refactoring:**
1. Start with most accessed singletons: `ModuleRegistry`, `ServiceRegistry`, `ViewStack`, `I18n`
2. Refactor core modules first: `GpgModule`, `PasswordModule`, `TotpModule`
3. Update UI components: `AppUi`, `ViewStack` users

## References
- [Dependency Injection on Wikipedia](https://en.wikipedia.org/wiki/Dependency_injection)
- [Loose Coupling on Wikipedia](https://en.wikipedia.org/wiki/Loose_coupling)
- [Martin Fowler: Inversion of Control Containers](https://martinfowler.com/articles/injection.html)
