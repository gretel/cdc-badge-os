---
title: "[MEDIUM] Module initialization errors shown only via toast, no persistent feedback"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When modules fail to initialize (e.g., TOTP slot allocation failure in `components/mod_totp/src/TotpModule.cpp:918-926`), the error is reported via `reportModuleError()` which publishes an event but **only shows a sticky toast**. The module's menu item may still appear in menus (depending on timing), and there's no clear loading/progress state showing that modules are being initialized.

**Evidence:**
- `ModuleRegistry::reportModuleError()` (`components/cdc_core/src/ModuleRegistry.cpp:668-689`) calls `showToastAlertSticky()`
- Module initialization happens during startup without any "Loading modules..." indicator
- Users see the main menu immediately without knowing modules are still initializing

## Impact
**User Experience:** During startup, users see the main menu appear but modules may not be ready yet. If a module fails, they see a toast but may miss it if they're interacting with the menu. No loading state shows "Modules initializing..." during the critical startup phase.

**Technical:** Module initialization can take several seconds (especially with TROPIC01 secure element). Users should see a splash or loading screen during this phase.

## Evidence
**File: `components/cdc_core/src/ModuleRegistry.cpp`**

```cpp
// Line 668-689: Error reporting with toast only
void ModuleRegistry::reportModuleError(const char* name, const char* message) {
    if (!name) return;

    // Find module by name
    for (uint8_t i = 0; i < count_; i++) {
        if (strcmp(modules_[i]->getName(), name) == 0) {
            // Stop the module if it's running
            if (modules_[i]->getState() == ServiceState::STARTED) {
                modules_[i]->stop();
                LOG_W(TAG, "Module '%s' stopped due to error", name);
            }
            setModuleError(i, message);
            LOG_E(TAG, "Module '%s' error: %s", name, message ? message : "(null)");

            // Publish error event for UI notification
            Event evt;
            evt.type = EventType::MODULE_ERROR;
            evt.data.value = i;
            EventBus::instance().publish(evt);  // Toast shown via subscription
            return;
        }
    }
}
```

**File: `components/mod_totp/src/TotpModule.cpp`**

```cpp
// Line 918-926: Module init with error reporting
bool TotpModule::init() {
    LOG_I(TAG, "Initializing TOTP module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);
    if (slotRange_.hasRmem) {
        TotpStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "TOTP slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;  // Error reported, but no loading state shown
    }
    state_ = core::ServiceState::INITIALIZED;
    return true;
}
```

**File: `main/main.cpp`** (typical startup sequence)
- Modules are initialized in sequence
- No "Loading..." screen is shown during this phase
- Main menu appears before modules are ready

## Recommended Fix
1. **Show a loading screen during module initialization:**
```cpp
void ModuleRegistry::runAllInitializers() {
    // Show loading screen
    auto* display = hal::getDisplayInstance();
    if (display) {
        display->showSplash("Loading modules...");
    }
    
    LOG_I(TAG, "Running %d module initializers", initCount_);
    for (uint8_t i = 0; i < initCount_; i++) {
        if (initializers_[i]) {
            initializers_[i]();
        }
    }
    
    // Hide loading screen
    if (display) {
        display->backlightOn();
    }
}
```

2. **Or use a task toast that persists until initialization completes:**
```cpp
// In main.cpp during startup
showToastTask("Initializing modules...");
ViewStack::instance().render();

// After all modules initialized
ViewStack::instance().hideModal();
```

## References
- Splash screen already exists in `EpaperDisplay::showSplash()` (`components/cdc_hal/src/EpaperDisplay.cpp:353-399`)
- ToastView has `Icon::TASK` for loading states
