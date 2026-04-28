---
title: "[HIGH] Shared view instances create hidden coupling in ListView and T9InputView"
severity: HIGH
domain: frontend
lens: component-architecture
labels:
  - "component-architecture"
  - "state-management"
  - "reusability"
---

## Summary

The `ListView` and `T9InputView` components use **static shared instances** (`s_sharedListView` and `s_sharedT9Input`) with factory functions (`showListView` and `showT9Input`). This creates hidden global state that limits component reusability and introduces coupling.

**Files affected:**
- `components/cdc_views/src/ListView.cpp:278-298`
- `components/cdc_views/src/T9InputView.cpp:336-355`

## Impact

**State Pollution:**
- Only one instance of each view can exist at a time
- Navigating back and re-opening the view reuses stale state from the previous instance
- Cannot open multiple instances simultaneously (e.g., nested lists, modal input)

**Coupling:**
- Components are not truly reusable; they're "single-use" by design
- The shared pattern leaks into calling code, which assumes single-instance behavior
- Hard to test in isolation without resetting global state

**Memory vs Flexibility Trade-off:**
- While shared instances save memory (important for embedded), the pattern should be explicit
- Current implementation hides the limitation in convenience functions

## Evidence

**ListView.cpp:278-298:**
```cpp
static ListView s_sharedListView;

ListView* showListView(const char* title, const ListItem* items, uint16_t count,
                       ListView::SelectCallback onSelect, const char* hint) {
    s_sharedListView.init(title, items, count);
    s_sharedListView.setOnSelect(onSelect);
    if (hint) {
        s_sharedListView.setHint(hint);
    }
    ViewStack::instance().push(&s_sharedListView);
    return &s_sharedListView;
}
```

**T9InputView.cpp:336-355:**
```cpp
static T9InputView s_sharedT9Input;

T9InputView* showT9Input(const char* title, const char* initialText,
                         T9InputView::SaveCallback onSave, uint16_t maxLen) {
    s_sharedT9Input.init(title, initialText, maxLen);
    s_sharedT9Input.setOnSave(onSave);
    ViewStack::instance().push(&s_sharedT9Input);
    return &s_sharedT9Input;
}
```

**Usage in AppUi.cpp:58-90:**
```cpp
static ListView* s_mainMenu = nullptr;
static ListView* s_toolsMenu = nullptr;
static ListView* s_settingsMenu = nullptr;
static ListView* s_languageMenu = nullptr;
// ...and more static view pointers
```

Note: AppUi.cpp creates its own static instances for main views, but uses `showListView`/`showT9Input` for temporary views, creating two different patterns.

## Recommended Fix

**Option 1: Explicit Shared Instance Pattern (~30 min)**
Add documentation and a clear API to reset/clear state:

```cpp
/**
 * \brief Shows a shared list view instance.
 * \note Uses a single shared instance. Previous state is cleared on init.
 * \warning Only one ListView can be active at a time when using this helper.
 */
ListView* showListView(const char* title, const ListItem* items, uint16_t count,
                       ListView::SelectCallback onSelect, const char* hint = nullptr);

/**
 * \brief Clears the shared ListView state for reuse.
 */
void clearSharedListView();
```

**Option 2: Instance Factory with Pool (~1 hour)**
Create a simple view pool that manages a fixed number of instances:

```cpp
class ViewPool {
public:
    static ListView* acquireListView();
    static void releaseListView(ListView* view);
    static T9InputView* acquireT9Input();
    static void releaseT9Input(T9InputView* view);
};

// Usage:
void showListView(const char* title, const ListItem* items, uint16_t count,
                  ListView::SelectCallback onSelect, const char* hint = nullptr) {
    auto* view = ViewPool::acquireListView();
    view->init(title, items, count);
    view->setOnSelect(onSelect);
    if (hint) view->setHint(hint);
    ViewStack::instance().push(view);
}
```

**Option 3: Constructor-based Instantiation (~30 min)**
For views that need multiple instances, allow direct construction:

```cpp
// In header:
ListView();  // Public constructor
void init(...);

// Usage in AppUi.cpp:
s_mainMenu = new ListView();  // Already done for main views
s_mainMenu->init(...);
```

Keep the shared helper for temporary views but document the limitation clearly.

## References

- [Singleton Pattern](https://en.wikipedia.org/wiki/Singleton_pattern) - When to use and when to avoid
- [State Management in UI Components](https://reactjs.org/docs/state-and-lifecycle.html) - React's approach to component state
- [Object Pool Pattern](https://en.wikipedia.org/wiki/Object_pool_pattern) - For memory-constrained environments

</content>