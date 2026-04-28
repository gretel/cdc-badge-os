---
title: "[MEDIUM] GattCharacteristic Callbacks Use std::function - No Stack-Independent Guarantee"
severity: MEDIUM
domain: architecture/api-contract
lens: callback-interface-contract
labels:
  - "audit:architecture/api-contract"
---

## Summary
The `IBluetoothController` interface uses `std::function` for GATT callbacks (`GattWriteCallback`, `GattReadCallback`), which may not be truly stack-independent and could introduce heap allocation. The interface claims to be "stack-independent" but `std::function` typically uses heap.

## Impact
- **Memory overhead**: `std::function` may allocate on heap, contradicting "static allocation" goals.
- **Real-time performance**: Heap allocation in callbacks could cause unpredictable timing.
- **Stack size uncertainty**: `std::function` internal buffer size varies by implementation.
- **Interface contract violation**: Claims to be stack-independent but uses heap-allocating types.

## Evidence
**Interface definition - `components/cdc_hal/include/cdc_hal/IBluetoothController.h:80-98`:**
```cpp
/**
 * GATT write callback for characteristic writes
 */
using GattWriteCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                             const uint8_t* data, uint16_t len)>;

/**
 * GATT read callback for characteristic reads
 */
using GattReadCallback = std::function<int(uint16_t connHandle, uint16_t attrHandle,
                                            uint8_t* buf, uint16_t* len)>;

/**
 * GATT characteristic definition for service registration
 */
struct GattCharacteristic {
    BleUuid uuid;
    uint8_t properties;        // GattProp flags
    uint8_t permissions;       // GattPerm flags
    uint16_t* valueHandle;     // Output: handle assigned by stack
    GattWriteCallback onWrite;
    GattReadCallback onRead;
};
```

**Comment claims stack-independence - `components/cdc_hal/include/cdc_hal/IBluetoothController.h:1-6`:**
```cpp
/**
 * Stack-independent BLE UUID (no NimBLE/Bluedroid dependency)
 */
struct BleUuid { ... };

/**
 * Stack-independent GATT characteristic property flags
 */
namespace GattProp { ... }
```

**Usage in mod_hid - `components/mod_hid/src/BleHidKeyboard.cpp:108-168`:**
```cpp
// Lambda captures are stored in std::function
s_gattChars[0].onRead = [](uint16_t, uint16_t, uint8_t* buf, uint16_t* len) -> int {
    uint16_t copyLen = sizeof(HID_INFO);
    if (copyLen > *len) copyLen = *len;
    memcpy(buf, HID_INFO, copyLen);
    *len = copyLen;
    return 0;
};

// Another lambda with capture
s_gattChars[4].onWrite = [](uint16_t, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (len >= 1) s_protocolMode = data[0];
    return 0;
};
```

**std::function characteristics:**
- Typical implementation: 32 bytes small-buffer optimization + heap for larger captures
- Lambda with captures > 32 bytes will heap-allocate
- Not truly "static allocation" guaranteed

## Recommended Fix
**Use function pointers for truly stack-independent callbacks:**

1. **Update callback type definitions:**
```cpp
/**
 * GATT write callback for characteristic writes (stack-independent)
 */
using GattWriteCallback = int(*)(uint16_t connHandle, uint16_t attrHandle,
                                  const uint8_t* data, uint16_t len);

/**
 * GATT read callback for characteristic reads (stack-independent)
 */
using GattReadCallback = int(*)(uint16_t connHandle, uint16_t attrHandle,
                                 uint8_t* buf, uint16_t* len);
```

2. **Add userData context pointer to GattCharacteristic:**
```cpp
struct GattCharacteristic {
    BleUuid uuid;
    uint8_t properties;
    uint8_t permissions;
    uint16_t* valueHandle;
    GattWriteCallback onWrite;
    GattReadCallback onRead;
    void* userData;  // Context pointer for callbacks
};
```

3. **Update consumers to use function pointers with context.**

## References
- IBluetoothController: `components/cdc_hal/include/cdc_hal/IBluetoothController.h:80-98`
- GATT characteristics: `components/cdc_hal/include/cdc_hal/IBluetoothController.h:91-98`
