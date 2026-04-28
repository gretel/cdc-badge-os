---
title: "[MEDIUM] ModuleMenuItem factory function returns raw pointer with unclear ownership"
severity: MEDIUM
domain: architecture/api-contract
lens: ownership-semantics
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `ModuleMenuItem` struct defines `ui::IView* (*getView)()` as a factory function that returns a raw pointer. The contract doesn't specify whether the caller takes ownership, if the view is stack-allocated, or if it's a singleton. This ambiguity can lead to use-after-free bugs or memory leaks.

**Evidence location:** `components/cdc_core/include/cdc_core/IModule.h:26-36`

```cpp
struct ModuleMenuItem {
    const char* label;              // Display label (use I18n for translation)
    uint8_t priority;               // Sort order (lower = higher in list)
    ui::IView* (*getView)();        // Factory function to push view on select
    bool (*isVisible)();            // Optional visibility check (nullptr = always visible)
    const char* moduleName;         // Owner module name (set automatically)
    MenuLocation location;          // Where to show this item
    void (*onSelect)();             // Toggle/action callback (used when getView is nullptr)
};
```

## Impact
1. **Ownership ambiguity**: Does `getView()` return a new allocation (caller deletes), a singleton (never delete), or a stack object (dangling pointer)?
2. **Memory leaks**: If views are allocated but never freed, memory accumulates
3. **Use-after-free**: If views are deleted by the wrong owner, crashes occur
4. **Inconsistent patterns**: Different modules may use different conventions, making the codebase harder to maintain

## Evidence
The comment "Factory function to push view on select" (line 30) suggests the view is pushed onto a view stack, but doesn't clarify:
- Who allocates the view?
- Who frees the view?
- Is the view reused or recreated each time?

Looking at `IView` interface (`IDisplay.h`), it has a virtual destructor but no reference counting or shared pointer support, suggesting raw pointer ownership.

**Typical usage pattern (from module implementations):**
```cpp
// Pattern 1: Stack allocation (may dangle!)
ui::IView* getListView() {
    ListView view;  // Stack object!
    return &view;   // Dangling pointer after return!
}

// Pattern 2: Heap allocation (memory leak if never freed)
ui::IView* getListView() {
    return new ListView();  // Who deletes this?
}

// Pattern 3: Singleton (shared ownership)
static ListView s_listView;
ui::IView* getListView() {
    return &s_listView;  // Works but unclear lifetime
}
```

## Recommended Fix
Clarify ownership semantics in the interface definition:

**Option A (Explicit ownership transfer):**
```cpp
/**
 * \brief Factory function to create a new view instance.
 * \return Newly allocated view (caller takes ownership, view stack will delete)
 */
using ViewFactory = ui::IView* (*)();
```

**Option B (Singleton/managed lifetime):**
```cpp
/**
 * \brief Get the module's main view (singleton, lifetime managed by module)
 * \return View instance (never null, never delete)
 */
using ViewGetter = ui::IView* (*)();
```

**Option C (Modern C++ smart pointers):**
```cpp
using ViewFactory = std::unique_ptr<ui::IView> (*)();
// Clear ownership: view stack takes unique_ptr, deletes when popped
```

**Option D (View registry pattern):**
```cpp
struct ModuleMenuItem {
    // ...
    const char* viewId;  // Reference to registered view
    ui::IView* (*resolveView)(const char* id);  // Look up by ID
};
```

Recommend **Option A** with explicit documentation:
```cpp
/**
 * \brief Factory function for menu item view.
 *
 * Called when menu item is selected. The returned view is pushed onto
 * the view stack and will be deleted by the stack when popped.
 *
 * Example:
 *   static ui::IView* getMyView() {
 *       return new MyView();  // Stack takes ownership
 *   }
 */
ui::IView* (*getView)();
```

## References
- C++ Core Guidelines C.30: Use `unique_ptr` to indicate exclusive ownership
- C++ Core Guidelines C.31: Use `shared_ptr` to indicate shared ownership
- C++ Core Guidelines C.32: Use `unique_ptr` for factory functions that return ownership
