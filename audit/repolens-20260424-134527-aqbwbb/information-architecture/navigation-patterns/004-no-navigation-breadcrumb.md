---
title: "[LOW] No navigation history/breadcrumb for deep view stacks"
severity: LOW
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The ViewStack supports a maximum depth of 8 views but provides no visual indication of the current navigation path. Users navigating deep into module-specific views (e.g., TOTP -> Add Account -> Secret -> Issuer -> Digits -> Algorithm -> Period) lose context of their location in the hierarchy.

**Evidence locations:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h:18` - `MAX_DEPTH = 8`
- `components/mod_totp/src/TotpModule.cpp:330-360` - 6-level wizard navigation
- `components/cdc_os_ui/src/WifiMenuUi.cpp` - Multi-level WiFi setup wizard

## Impact
**User disorientation:** Deep navigation paths (5+ levels) make it hard for users to know where they are.
**Error recovery:** Users may need to navigate back multiple levels to correct a mistake.
**Learning curve:** New users may find deep hierarchies confusing without visual cues.

## Evidence
```cpp
// ViewStack has no breadcrumb tracking
class ViewStack {
    IView* stack_[MAX_DEPTH] = {};  // Just stores views
    uint8_t depth_ = 0;
    // No path tracking!
};

// Deep wizard navigation (6 levels deep)
static void wizardStart() {
    pushT9WizardStep(mstr(STR_ACCOUNT_NAME), ...);  // Level 1
}
static void onWizardName(const char* text) {
    pushT9WizardStep(mstr(STR_SECRET), ...);  // Level 2
}
static void onWizardSecret(const char* text) {
    pushT9WizardStep(mstr(STR_ISSUER), ...);  // Level 3
}
static void onWizardIssuer(const char* text) {
    // Push digits menu  // Level 4
}
static void onWizardDigits(uint16_t index, void* userData) {
    // Push algo menu    // Level 5
}
static void onWizardAlgo(uint16_t index, void* userData) {
    // Push period menu  // Level 6
}
```

## Recommended Fix
Add navigation breadcrumbs or path indicator:
1. **Add `getTitle()`** to IView interface (already partially exists in ViewBase)
2. **Display current path** in header (e.g., "TOTP > Add > Secret")
3. **Show depth indicator** (e.g., "Step 3 of 6")
4. **Add keyboard shortcut** to pop to specific level (e.g., long-press N)

Example:
```cpp
// In render()
gfx->setTextSize(1);
gfx->setCursor(8, 6);
gfx->print(getBreadcrumb());  // "TOTP > Add Account > Secret"

// Helper to build breadcrumb
std::string getBreadcrumb() {
    std::string path;
    for (int i = 0; i < ViewStack::instance().depth(); i++) {
        if (i > 0) path += " > ";
        path += ViewStack::instance().at(i)->getTitle();
    }
    return path;
}
```

## References
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Navigation stack
- `components/cdc_ui/include/cdc_ui/IView.h` - View interface with `getTitle()`
