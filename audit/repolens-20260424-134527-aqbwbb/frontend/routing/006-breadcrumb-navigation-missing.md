---
title: "[LOW] No Breadcrumb Navigation for Deep Hierarchies"
severity: LOW
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The app uses a view stack for navigation but doesn't show breadcrumbs or a clear hierarchy in the UI. Users navigating deep into menus (e.g., Settings → Language → select, or Tools → WiFi → Setup → enter SSID) don't have a clear visual indication of where they are in the hierarchy.

**Files affected:**
- `components/cdc_views/src/ListView.cpp` (render method)
- All views that use the view stack

## Impact
User experience issues:
1. No visual indicator of current location in hierarchy
2. Users may get "lost" in deep navigation paths
3. Hard to know how many back presses to get to a specific level
4. No way to jump back multiple levels at once

## Evidence
From `ListView.cpp:170-175` (getFooterHint):
```cpp
const char* ListView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    return tr(StringId::HINT_OK_BACK);  // Just "[Y] OK [N] Back"
}
```

From `ViewStack.cpp:56-63` (push):
```cpp
void ViewStack::push(IView* view, void* context) {
    // ...
    stack_[depth_++] = view;
    view->onEnter(context);
    // No breadcrumb tracking
}
```

The footer only shows generic hints, not the navigation path.

## Recommended Fix
Add breadcrumb support to the view stack:

**Option 1: Simple breadcrumb in footer**
```cpp
class ViewStack {
    // Add breadcrumb tracking
    char breadcrumbs_[MAX_DEPTH][32];
    
    void updateBreadcrumbs() {
        // Build path from root to current
        for (uint8_t i = 0; i < depth_; i++) {
            snprintf(breadcrumbs_[i], sizeof(breadcrumbs_[i]), 
                     "%s", stack_[i]->getName());
        }
    }
    
    const char* getBreadcrumbs() const {
        // Return formatted path like "Main > Tools > WiFi"
    }
};

// In ListView footer:
const char* getFooterHint() const {
    // Show breadcrumbs + action hints
    static char fullHint[64];
    snprintf(fullHint, sizeof(fullHint), "%s  [N] Back", 
             ViewStack::instance().getBreadcrumbs());
    return fullHint;
}
```

**Option 2: Breadcrumb in header**
Modify `render::drawHeaderLeft()` to show breadcrumbs:
```cpp
void render::drawHeaderLeft(Gdey029T94* gfx, const char* title, 
                            int x, int y, int width) {
    // Show breadcrumbs above title
    const char* path = ViewStack::instance().getBreadcrumbs();
    gfx->setCursor(x, y - 8);
    gfx->print(path);
    
    // Then title
    gfx->setCursor(x, y);
    gfx->print(title);
}
```

**Option 3: Jump-to-root feature**
Add long-press to jump to root:
```cpp
void ViewStack::dispatchLongPress(char key) {
    if (key == 'N') {
        if (modal_) {
            hideModal();
        } else {
            pop();  // Short press: go back one
        }
        return;
    }
    // Long press N: jump to root
    if (key == 'N') {  // Could use different key
        popToRoot();
    }
}
```

## References
- ListView header rendering: `components/cdc_views/src/ListView.cpp:188-195`
- ViewStack navigation methods: `components/cdc_ui/include/cdc_ui/ViewStack.h:18-48`
