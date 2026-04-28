---
title: "[LOW] ContextMenuView lacks footer hint for navigation"
severity: LOW
domain: UI/UX
lens: cognitive-overload
labels:
  - "navigation-hint"
  - "discoverability"
---

## Summary
The `ContextMenuView` in `components/cdc_views/src/ContextMenuView.cpp` shows a modal popup menu but lacks a footer hint explaining which keys navigate and select. Unlike other views that use `getFooterHint()`, the context menu renders its own UI without showing key bindings.

**File:** `components/cdc_views/src/ContextMenuView.cpp`
**Lines:** 130-240 (render function)

## Impact
- **Discoverability:** Users may not know how to navigate the context menu without prior knowledge
- **Inconsistency:** Other views show footer hints (e.g., "Y=OK N=Back [2/8]") but context menu doesn't
- **Learning Curve:** First-time users might be confused about available actions

## Evidence
The `ContextMenuView::render()` function draws the menu but never calls `getFooterHint()` or displays key bindings:

```cpp
// Line 130-240: Renders menu but no footer hint
void ContextMenuView::render(bool partial) {
    // ... draws title, items, scroll indicators ...

    // Scroll indicators if needed
    if (itemCount_ > VISIBLE_ITEMS) {
        // ... up/down arrows ...
    }

    dirty_ = false;  // No footer with key hints!
}

// Line 105-125: Key handling exists but not documented in UI
InputResult ContextMenuView::onKey(char key) {
    switch (key) {
        case '2': // Up
            navigate(false);
            return InputResult::CONSUMED;
        case '8': // Down
            navigate(true);
            return InputResult::CONSUMED;
        case 'Y': // Select
            select();
            return InputResult::CONSUMED;
        case 'N': // Cancel
            hideContextMenu();
            return InputResult::CONSUMED;
    }
}
```

Compare to `ListView::render()` which shows footer:
```cpp
// ListView.cpp line 260-270
const char* hint = getFooterHint();
render::drawFooterBar(gfx, width, height, prefix, hint, true);
```

## Recommended Fix
Add a footer hint to `ContextMenuView::render()`:

```cpp
// After drawing scroll indicators, before dirty_ = false:
const char* hint = tr(StringId::HINT_CONTEXT_MENU);  // Add StringId for "Y=OK N=Cancel [2/8]"
render::drawFooterBar(gfx, screenWidth, screenHeight, nullptr, hint, false);
```

Or use a simpler inline hint at the bottom of the context menu box:
```cpp
// At bottom of context menu box (before closing border)
gfx->setCursor(boxX + 5, boxY + boxHeight - 10);
gfx->print("Y=OK N=Cancel");
```

## References
- HCI Heuristic #2: Match between system and real world (use familiar key labels)
- Nielsen Norman Group: [Visibility of System Status](https://www.nngroup.com/articles/visibility-system-status/)
- Consistency principle: All interactive views should show available actions
