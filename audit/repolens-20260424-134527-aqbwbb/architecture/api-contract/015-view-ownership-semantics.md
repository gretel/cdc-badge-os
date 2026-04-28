---
title: "[MEDIUM] IView interface uses raw pointers without ownership semantics"
severity: MEDIUM
domain: architecture/api-contract
lens: api-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IView` interface (components/cdc_ui/include/cdc_ui/IView.h) uses raw pointers throughout without clarifying ownership:

```cpp
class IView {
public:
    virtual void init(const char* title, ui::ListItem* items, uint8_t itemCount) = 0;
    virtual void setOnSelect(std::function<void(uint16_t, void*)> cb) = 0;
    // ...
};
```

Usage in ListView (components/cdc_views/include/cdc_views/ListView.h):
```cpp
void init(const char* title, ListItem* items, uint8_t itemCount);
```

It's unclear:
1. Does the view own the `items` array?
2. Does the view copy the `title` string?
3. How long must the `cb` function remain valid?
4. Can the caller reuse the `items` array after calling `init()`?

## Impact
- **Memory errors**: May double-free or use-after-free
- **Dangling pointers**: Caller may reuse memory too soon
- **Confusing API**: Must guess ownership semantics

## Evidence
- IView: components/cdc_ui/include/cdc_ui/IView.h
- ListView: components/cdc_views/include/cdc_views/ListView.h
- Usage in GpgModule.cpp:264-268

## Recommended Fix
Document ownership clearly:

```cpp
/**
 * \brief Initialize the list view.
 * \param title View title (view copies the string).
 * \param items Array of list items (view copies the array, not the labels).
 * \param itemCount Number of items.
 *
 * Ownership:
 * - title: View makes a copy (caller can free after call)
 * - items: View copies the array (caller can reuse after call)
 * - items[].label: View stores pointer only (must outlive view)
 */
virtual void init(const char* title, ListItem* items, uint8_t itemCount) = 0;

/**
 * \brief Set selection callback.
 * \param cb Callback function (view stores pointer, caller must ensure validity).
 *
 * The callback is stored by the view and called later.
 * Caller must ensure the std::function remains valid until view is destroyed.
 */
virtual void setOnSelect(std::function<void(uint16_t, void*)> cb) = 0;
```

Or use clearer semantics:
```cpp
// Take ownership
virtual void init(std::string title, std::vector<ListItem> items) = 0;

// Or borrow with lifetime
virtual void init(std::string_view title, const ListItem* items, uint8_t itemCount) = 0;
```

## References
- IView: components/cdc_ui/include/cdc_ui/IView.h
- ListView: components/cdc_views/include/cdc_views/ListView.h
