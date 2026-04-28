---
title: "[LOW] Callback functions use void* userData without type safety"
severity: LOW
domain: type-safety
lens: c++-callbacks
labels:
  - "audit:code-quality/type-safety"
---

## Summary
Multiple callback patterns use `void* userData` parameters, requiring callers to cast to the expected type. This C-style pattern loses type information and can lead to incorrect casts at callback sites.

**Locations:**
- `components/cdc_core/include/cdc_core/TropicStorage.h:43` - `SlotCallback` uses `void* ctx`
- `components/cdc_views/include/cdc_views/ConfirmView.h` - Callbacks use `void* userData`
- `components/cdc_views/include/cdc_views/InfoView.h` - Callbacks use `void* userData`
- `components/cdc_views/include/cdc_views/ListView.h:44, 51` - Callbacks use `void* userData`
- `components/cdc_ui/include/cdc_ui/IView.h:41` - `onEnter(void* context)`

## Impact
**Type confusion**: A callback expecting `MyData*` might receive a different type if the caller passes the wrong data.

**No compile-time checking**: The compiler cannot detect mismatches between the type stored and the type expected in the callback.

**Example:**
```cpp
// Caller passes wrong type
view.setOnConfirm([](void* userData) {
    auto* data = static_cast<MyData*>(userData);  // Might be wrong type!
}, &someOtherData);
```

## Evidence
```cpp
// components/cdc_views/include/cdc_views/ConfirmView.h
using ConfirmCallback = void(*)(void* userData);
using CancelCallback = void(*)(void* userData);
void setOnConfirm(ConfirmCallback callback, void* userData = nullptr);

// components/cdc_core/include/cdc_core/TropicStorage.h
using SlotCallback = void(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);

// components/cdc_ui/include/cdc_ui/IView.h
virtual void onEnter(void* context = nullptr) = 0;
```

## Recommended Fix
Use `std::function` with captured context for type-safe callbacks:

```cpp
#include <functional>

class ConfirmView {
public:
    using ConfirmCallback = std::function<void()>;
    using CancelCallback = std::function<void()>;

    void setOnConfirm(ConfirmCallback callback) {
        onConfirm_ = callback;
    }

    void triggerConfirm() {
        if (onConfirm_) onConfirm_();  // No cast needed!
    }

private:
    ConfirmCallback onConfirm_;
};

// Usage:
view.setOnConfirm([]() {
    // Capture context via lambda, no void* needed
    auto* data = &myData;
    data->confirm();
});
```

For callbacks that need parameters:
```cpp
using SelectCallback = std::function<void(uint16_t index)>;
void setOnSelect(SelectCallback callback) { onSelect_ = callback; }

// Usage captures context automatically
list.setOnSelect([this](uint16_t index) {
    processItem(index);  // 'this' captured, no void*
});
```

## References
- C++ Core Guidelines, F.23: "Use `std::function<>` for type-erased callables"
- C++ Core Guidelines, F.24: "Use `std::bind_front` or lambdas for simple callbacks"
