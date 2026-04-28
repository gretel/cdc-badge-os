---
title: "[MEDIUM] IBluetoothController callback storage without lifetime guarantees"
severity: MEDIUM
domain: architecture/api-contract
lens: callback-lifetime
labels:
  - "audit:architecture/api-contract"
---

## Summary
In `IBluetoothController.h` (lines 80-98), callback types use `std::function` without documenting the expected lifetime of the callback object. The interface stores callbacks directly but doesn't specify whether callbacks must remain valid indefinitely or if they're copied.

**Evidence location:** `components/cdc_hal/include/cdc_hal/IBluetoothController.h:80-98`

```cpp
using GattWriteCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                            const uint8_t* data, uint16_t len)>;
using GattReadCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                            uint8_t* buf, uint16_t* len)>;

// ... later in class ...
virtual bool registerGattService(const GattServiceDef& service) {
    (void)service;
    return false;
}
```

The `GattServiceDef` struct (lines 103-108) stores these callbacks:

```cpp
struct GattCharacteristic {
    BleUuid uuid;
    uint8_t properties;
    uint8_t permissions;
    uint16_t* valueHandle;
    GattWriteCallback onWrite;
    GattReadCallback onRead;
};
```

## Impact
Developers may pass temporary lambdas or local function objects that go out of scope, leading to:
- Dangling function objects stored in the service definition
- Undefined behavior when callbacks are invoked after the defining scope exits
- Hard-to-debug crashes in GATT event handling

## Evidence
The `registerGattService` function takes `const GattServiceDef& service` (line 323), suggesting the struct is copied. However, `std::function` copy semantics depend on the underlying callable. If a lambda captures by reference to a local variable, the copy will still hold the dangling reference.

**Example problematic usage pattern:**
```cpp
void setupService() {
    uint16_t handle = 0;
    GattCharacteristic charDef = {
        .uuid = BleUuid::from16(0x2A00),
        .onWrite = [localVar](uint16_t h, uint16_t k, const uint8_t* d, uint16_t l) {
            // localVar may go out of scope
            return process(localVar);
        }
    };
    GattServiceDef service = { .uuid = ..., .characteristics = &charDef, ... };
    controller->registerGattService(service);  // Callback may dangle!
}
```

## Recommended Fix
Document callback lifetime requirements explicitly in the interface:

1. **Option A (Copy semantics)**: Add documentation:
```cpp
/**
 * Register a GATT service with characteristics.
 * ...
 * @note Callbacks are copied into internal storage. Lambdas should capture
 *       by value or use shared pointers for long-lived data.
 */
virtual bool registerGattService(const GattServiceDef& service)
```

2. **Option B (Pointer semantics)**: Change to explicit ownership model:
```cpp
struct GattCharacteristic {
    // ...
    GattWriteCallback* onWrite;  // Pointer, owner manages lifetime
    GattReadCallback* onRead;
};
```

3. **Option C (Builder pattern)**: Use a builder that ensures callbacks are stored before returning:
```cpp
class GattServiceBuilder {
public:
    GattServiceBuilder& withWriteCallback(GattWriteCallback cb);
    GattServiceDef build();  // Returns fully owned service definition
};
```

## References
- C++ Core Guidelines F.23: Use a `not_null<T*>` type to indicate that a pointer is never null
- C++ Core Guidelines F.54: Avoid passing 'T' by reference-to-const if 'T' is cheaply copyable (for callbacks)
- ISO C++ Standard §20.9.11.2.1 (std::function): Copy construction copies the target object
