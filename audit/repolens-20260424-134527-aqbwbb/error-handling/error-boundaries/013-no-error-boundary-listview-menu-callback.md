---
title: "[LOW] No error boundary in ListView menu callback dispatch"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "ui-framework"
---

## Summary
The `ListView::onKey()` function in `components/cdc_views/src/ListView.cpp:155-158` dispatches the menu callback (key '3') without error isolation. A single menu callback throwing will crash the entire input handling for the list view.

**Evidence:**
- `components/cdc_views/src/ListView.cpp:155-158`:
```cpp
case '3': // Context menu
    if (onMenu_ && items_ && selection_ < itemCount_) {
        onMenu_(selection_, items_[selection_].userData);  // No error boundary!
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
```

- Called from `ViewStack::dispatchKey()` with no recovery:
```cpp
void ViewStack::dispatchKey(char key) {
    // ...
    IView* view = current();
    if (view) {
        InputResult result = view->onKey(key);  // ListView::onKey() called here
        // ...
    }
}
```

- Usage pattern (from modules):
```cpp
listView->setOnMenu([](uint16_t index, void* userData) {
    // Menu action logic that could throw
    TotpStore::instance().editAccount(index);
});
```

## Impact
- **Input lockup**: One crashing menu callback freezes list navigation
- **No graceful recovery**: User must navigate back to recover
- **Silent failures**: Menu errors not logged
- **Debug difficulty**: Hard to identify which menu action failed

## Recommended Fix
Add error boundary around menu callback:

```cpp
InputResult ListView::onKey(char key) {
    switch (key) {
        case '2': // Up
            navigate(false);
            return InputResult::CONSUMED;

        case '8': // Down
            navigate(true);
            return InputResult::CONSUMED;

        case 'Y': // Select
            if (onSelect_ && items_ && selection_ < itemCount_) {
                try {
                    onSelect_(selection_, items_[selection_].userData);
                } catch (const std::exception& e) {
                    LOG_E(TAG, "List select exception: %s", e.what());
                } catch (...) {
                    LOG_E(TAG, "List select exception (unknown)");
                }
            }
            return InputResult::CONSUMED;

        case '3': // Context menu
            if (onMenu_ && items_ && selection_ < itemCount_) {
                try {
                    onMenu_(selection_, items_[selection_].userData);
                } catch (const std::exception& e) {
                    LOG_E(TAG, "List menu exception: %s", e.what());
                } catch (...) {
                    LOG_E(TAG, "List menu exception (unknown)");
                }
                return InputResult::CONSUMED;
            }
            return InputResult::IGNORED;

        case 'N': // Back
            return InputResult::REQUEST_POP;

        default:
            return InputResult::IGNORED;
    }
}
```

## References
- [UI Input Patterns](https://opencode.ai/guides/ui/input-patterns/)
- [Callback Error Handling](https://en.cppreference.com/w/cpp/utility/functional)
