---
title: "[LOW] Inconsistent component initialization patterns across views"
severity: LOW
domain: frontend
lens: component-architecture
labels:
  - "component-architecture"
  - "consistency"
  - "api-design"
---

## Summary

The UI components use **inconsistent initialization patterns**. Some components use `init()` methods with callbacks, while others use separate setter methods. This inconsistency increases cognitive load for developers and makes the API harder to learn.

**Files affected:**
- `components/cdc_views/include/cdc_views/ListView.h`
- `components/cdc_views/include/cdc_views/T9InputView.h`
- `components/cdc_views/include/cdc_views/SliderView.h`
- `components/cdc_views/include/cdc_views/PinEntryView.h`

## Impact

**Developer Experience:**
- Each view requires learning its specific API pattern
- Easy to forget to call a setter method after `init()`
- Code review requires checking each view's specific requirements

**Code Consistency:**
- Mixed patterns make refactoring harder
- New components may follow different patterns

## Evidence

**ListView (lines 70-108):**
```cpp
void init(const char* title, const ListItem* items, uint16_t count);
void setOnSelect(SelectCallback callback);
void setOnMenu(MenuCallback callback);
void setItemRenderer(ItemRenderCallback callback, void* userCtx = nullptr);
void setHint(const char* hint);
```

**T9InputView (from T9InputView.cpp:51-75):**
```cpp
void init(const char* title, const char* initialText, uint16_t maxLen);
void setOnSave(SaveCallback onSave);  // Separate setter
```

**PinEntryView (from PinEntryView.cpp):**
```cpp
void init(const char* title, uint8_t minDigits, uint8_t maxDigits);
void setOnVerify(VerifyCallback callback);
void setOnSuccess(Callback callback);
```

**Inconsistent patterns observed:**
1. Some use `init()` + setters (ListView, T9InputView, PinEntryView)
2. Some inline initialization in factory functions (showListView, showT9Input)
3. Callback parameter order varies (some use `onSave`, some use `onSelect`)

## Recommended Fix

**Standardize on builder pattern or fluent API (~30 min):**

**Option 1: Fluent builder pattern**
```cpp
ListView* showListView(const char* title, const ListItem* items, uint16_t count) {
    return new ListView()
        ->title(title)
        ->items(items, count)
        ->onSelect([](uint16_t idx, void* data) { ... })
        ->onMenu([](uint16_t idx, void* data) { ... })
        ->hint("OK=Select, N=Back");
}
```

**Option 2: Constructor with options struct**
```cpp
struct ListViewConfig {
    const char* title;
    const ListItem* items;
    uint16_t count;
    ListView::SelectCallback onSelect;
    ListView::MenuCallback onMenu;
    const char* hint;
};

ListView* showListView(ListViewConfig config);
```

**Option 3: Consistent init signature**
```cpp
// All views use same pattern:
void init(const char* title, Callbacks callbacks, Options options = {});
```

**Or at minimum, document the pattern:**
Add clear documentation to each view's header explaining the initialization sequence:
```cpp
/**
 * \brief Initialize ListView.
 * \note Must call init() before use. Callbacks can be set before or after init().
 * Example:
 *   auto* view = new ListView();
 *   view->init("Title", items, 5);
 *   view->setOnSelect([](uint16_t idx, void* data) { ... });
 */
```

## References

- [Fluent Interface](https://en.wikipedia.org/wiki/Fluent_interface)
- [Builder Pattern](https://en.wikipedia.org/wiki/Builder_pattern)
- [API Design Guidelines](https://abseil.io/tips/130)

</content>