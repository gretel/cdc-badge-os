---
title: "[MEDIUM] ViewStack singleton creates temporal coupling in UI navigation"
severity: MEDIUM
domain: architecture/coupling
lens: viewstack-temporal
labels:
  - "audit:architecture/coupling"
---

## Summary
`ViewStack::instance()` is a singleton that all views use for navigation. Views call `ViewStack::instance().push()`, `pop()`, etc. This creates temporal coupling: views must be pushed after ViewStack is initialized, and the order of view pushes matters.

**Evidence:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h` (lines 23-26): Singleton instance
- `components/cdc_os_ui/src/AppUi.cpp` - All navigation uses ViewStack
- `components/mod_gpg/src/GpgModule.cpp` - Uses ViewStack for navigation
- `components/cdc_views/src/ListView.cpp` - Uses ViewStack for shared views

## Impact
**Temporal coupling:** Views must be initialized in a specific order. If `showGpgMenu()` is called before `ui_init()`, the app crashes.

**Global state:** ViewStack is a global singleton. Any view can push/pop any other view.

**Testing complexity:** To test a view, you need ViewStack initialized with proper dependencies.

**Example:**
```cpp
// Anywhere in code
ViewStack::instance().push(gpgMenuView);  // What if ViewStack not initialized?
ViewStack::instance().pop();              // What if stack empty?
```

## Evidence
**File: `components/cdc_ui/include/cdc_ui/ViewStack.h` (lines 22-27)**
```cpp
/**
 * Get singleton instance
 */
static ViewStack& instance();
```

**File: `components/cdc_os_ui/src/AppUi.cpp` (lines 297-350)**
```cpp
static void showMainMenu() {
    ViewStack::instance().push(s_mainMenu);  // Uses singleton
}

static void showToolsMenu() {
    ViewStack::instance().push(s_toolsMenu);
}
```

**File: `components/cdc_views/src/ListView.cpp` (lines 278-285)**
```cpp
// Shared ListView instance
static ListView s_sharedListView;

void ListView::show(const char* title, ListItem* items, uint8_t count) {
    auto* list = &s_sharedListView;
    list->init(title, items, count);
    ViewStack::instance().push(list);  // Uses singleton
}
```

## Recommended Fix
**Option 1: Dependency injection**
Pass ViewStack to views that need it:
```cpp
class ListView {
    void show(ViewStack* stack, const char* title, ...);
};
```

**Option 2: View factory**
Use a factory that manages ViewStack:
```cpp
class ViewFactory {
    static void push(ViewStack* stack, IView* view);
};
```

**Option 3: Event-based navigation**
Views emit "push view" events. ViewStack listens and handles.

## References
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - ViewStack interface
- `components/cdc_os_ui/src/AppUi.cpp` - View navigation
- `components/cdc_views/src/ListView.cpp` - Shared views
- `components/cdc_ui/include/cdc_ui/IView.h` - View interface
