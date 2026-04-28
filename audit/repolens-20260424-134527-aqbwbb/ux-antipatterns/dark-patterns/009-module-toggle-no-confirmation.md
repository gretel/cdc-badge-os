---
title: "[LOW] Module toggle in expert menu executes immediately without confirmation"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary

In the Expert Menu's module management view, selecting a module toggles its state immediately without confirmation. This could lead to accidental disabling/enabling of modules, especially for users unfamiliar with the feature.

**Evidence:**

File: `components/cdc_os_ui/src/ExpertMenuUi.cpp`, lines 86-115

```cpp
// Normal toggle: enable/disable module
bool nowEnabled = moduleReg.toggleModuleEnabled(idx);

if (nowEnabled) {
    if (!moduleReg.startModule(idx)) {
        const char* error = moduleReg.getModuleSlotError(idx);
        if (error) {
            showToastError(error, TOAST_DURATION_MEDIUM_MS);
        } else {
            showToastError(tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
        }
    }
} else {
    if (module->getState() == core::ServiceState::STARTED) {
        module->stop();
    }
}

// If USB config changed by THIS module toggle, show sticky alert
bool needsReplugAfter = core::UsbManager::instance().showToastAlertSticky(tr(StringId::USB_REPLUG_REQUIRED));
```

**The pattern:**

1. **Select = Toggle**: Pressing Y on a module immediately toggles its state
2. **No confirmation**: Unlike the "retry" flow (which shows a confirm dialog for failed modules), the normal toggle has no confirmation step
3. **Side effects**: Toggling can trigger:
   - Module start/stop
   - USB replug requirements
   - State changes that may affect other modules

## Impact

1. **Accidental toggles**: Users navigating the list may accidentally toggle modules with simple selection
2. **USB replug surprise**: Users may not expect that toggling a module requires replugging USB
3. **No undo**: Once toggled, the user must select again to reverse (no explicit undo)
4. **Asymmetric UX**: Failed modules get a confirmation dialog, but normal toggles don't

## Recommended Fix

**Option 1: Add confirmation for toggles**

```cpp
static void onModuleSelect(uint16_t index, void* userData) {
    (void)userData;

    auto& moduleReg = core::ModuleRegistry::instance();
    if (index >= moduleReg.getModuleCount()) return;

    core::IModule* module = moduleReg.getModuleAt(index);
    if (!module) return;

    uint8_t idx = static_cast<uint8_t>(index);

    // If module has error, show retry dialog
    if (moduleReg.hasModuleSlotError(idx)) {
        const char* error = moduleReg.getModuleSlotError(idx);
        static char confirmMsg[128];
        snprintf(confirmMsg, sizeof(confirmMsg), "%s\n\nNochmal laden?",
                 error ? error : "Modul-Fehler");
        showConfirm(confirmMsg, onModuleRetryConfirm, nullptr,
                    ConfirmView::Icon::ERROR, reinterpret_cast<void*>(static_cast<uintptr_t>(idx)));
        return;
    }

    // Show toggle confirmation
    bool currentlyEnabled = moduleReg.isModuleEnabled(idx);
    static char toggleMsg[96];
    snprintf(toggleMsg, sizeof(toggleMsg), "%s %s?",
             module->getName(),
             currentlyEnabled ? "disable" : "enable");
    showConfirm(toggleMsg, onModuleToggleConfirm, nullptr,
                ConfirmView::Icon::QUESTION, reinterpret_cast<void*>(static_cast<uintptr_t>(idx)));
}

static void onModuleToggleConfirm(void* userData) {
    uint8_t idx = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(userData));
    // ... same toggle logic as before ...
}
```

**Option 2: Use a context menu for actions**

Instead of toggle-on-select, show a context menu with "Enable/Disable" options:

```cpp
case '3': // Context menu
    showContextMenu(module->getName(), toggleItems, 2);
    break;
```

**Option 3: Change the interaction model**

- Use a dedicated "Edit" mode where toggles require a second press
- Or use long-press (if supported) to toggle, short-press to view details

## References

- Nielsen Norman Group: [Action Confirmation](https://www.nngroup.com/articles/confirmation/) - when to ask before action
- Material Design: [Selection controls](https://material.io/components/selection-controls) - explicit vs implicit state changes
- Dark Patterns: [Visual Hierarchy Manipulation](https://www.darkpatterns.org/types-of-dark-pattern#visual-hierarchy-manipulation) - making important actions less prominent

</content>