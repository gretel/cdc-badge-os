---
title: "[LOW] Expert menu shows 3+ items without warning about scope"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - "expert-mode"
  - "information-architecture"
---

## Summary
The Expert menu in `components/cdc_os_ui/src/ExpertMenuUi.cpp` shows 3 fixed items plus dynamic module items (potentially 12+ total) with no indication of what "Expert" means or what each action does. A brief toast warning is shown but disappears quickly.

**File:** `components/cdc_os_ui/src/ExpertMenuUi.cpp`
**Lines:** 200-250

## Impact
- **Unclear Scope:** Users don't know what "Expert" entails or what risks they're taking
- **Brief Warning:** The toast at line 209 (`showToastInfo(tr(StringId::EXPERT_WARNING))`) auto-dismisses and may be missed
- **No Context:** Menu items like "TROPIC Cache Rebuild" have no description of what they do

## Evidence
The expert menu shows a brief toast then the list:

```cpp
// Line 209-218: Brief toast, then menu
void showExpertMenu() {
    showToastInfo(tr(StringId::EXPERT_WARNING), TOAST_DURATION_MEDIUM_MS);  // Disappears in 1.5s!
    if (!s_expertMenu) {
        s_expertMenu = new ListView();
        s_expertMenu->setOnSelect(onExpertMenuSelect);
    }

    rebuildExpertMenu();
    ViewStack::instance().push(s_expertMenu);
}

// Line 220-245: Menu items with no descriptions
static void rebuildExpertMenu() {
    s_expertItems[0] = {tr(StringId::HARDWARE_INFO), 0, false, nullptr};
    s_expertItems[1] = {tr(StringId::TR01_CACHE_REBUILD), 0, false, nullptr};
    s_expertItems[2] = {tr(StringId::TR01_CACHE_CLEANUP), 0, false, nullptr};

    // Module items...
}
```

The menu items are terse technical terms without context:
- "Hardware Info" - What does this show?
- "TR01 Cache Rebuild" - What is TR01? What does rebuilding do?
- "TR01 Cache Cleanup" - Will this delete data?

## Recommended Fix
**Option 1:** Show a confirmation dialog with more detail before entering expert menu:

```cpp
void showExpertMenu() {
    static char detailedWarning[256];
    snprintf(detailedWarning, sizeof(detailedWarning),
             "Expert menu contains advanced settings for debugging and maintenance.\n\n"
             "Actions may reset data or require technical knowledge.\n\n"
             "Continue?");

    showConfirm(detailedWarning,
                [](void*) { showExpertMenuInternal(); },
                nullptr,
                ConfirmView::Icon::WARNING);
}
```

**Option 2:** Add descriptions to each menu item using the `ListView` item renderer:

```cpp
// In rebuildExpertMenu, add description field to items
static bool renderExpertRow(Gdey029T94* gfx, const ListItem& item, ...) {
    // Draw main item text
    gfx->setCursor(x + 10, y + 5);
    gfx->print(item.label);

    // Draw smaller description below
    const char* desc = getDescriptionForItem(item.label);
    gfx->setTextSize(1);
    gfx->setCursor(x + 10, y + 12);
    gfx->print(desc);
}
```

**Option 3:** Show Hardware Info immediately (safe) and put risky actions in a submenu.

## References
- Progressive Disclosure: Show complexity gradually
- Error Prevention: Confirm before potentially destructive actions
- Expert vs Novice: Provide context for technical terms
