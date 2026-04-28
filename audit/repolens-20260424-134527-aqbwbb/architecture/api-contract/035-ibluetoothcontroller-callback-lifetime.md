---
title: "[HIGH] IBluetoothController Callback Lifetime and Capture Issues"
severity: HIGH
domain: API Contract Integrity
lens: callback-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
`IBluetoothController` stores callbacks as `std::function` objects that capture module state, but these callbacks can outlive their capturing context. The contract doesn't specify callback lifetime guarantees, leading to potential use-after-free or dangling captures.

**Location**: `components/cdc_hal/include/cdc_hal/IBluetoothController.h:79-100` (callback definitions and registration)

## Impact
1. **Use-after-free**: Module callbacks captured in `std::function` may reference destroyed module state
2. **Memory leaks**: `std::function` with captures allocates on heap; no cleanup mechanism documented
3. **Non-deterministic behavior**: Callbacks fire with stale data after module restart

## Evidence

In `components/cdc_hal/include/cdc_hal/IBluetoothController.h:79-98`:
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

struct GattCharacteristic {
    BleUuid uuid;
    uint8_t properties;
    uint8_t permissions;
    uint16_t* valueHandle;
    GattWriteCallback onWrite;
    GattReadCallback onRead;
};
```

In `mod_ble_serial/src/BleUartService.cpp:65-78`:
```cpp
// Capture by reference to singleton instance
chars[0].onWrite = [](uint16_t /*connHandle*/, uint16_t /*attrHandle*/,
                      const uint8_t* data, uint16_t len) -> int {
    BleUartService::instance().onRxData(data, len);  // <-- Captures singleton
    return 0;
};
```

The issue:
1. `registerGattService()` stores the `std::function` in the BLE stack
2. If the service is unregistered and re-registered (e.g., module restart), old callbacks may still fire
3. No mechanism to clear callbacks on service unregistration

In `components/cdc_hal/include/cdc_hal/IBluetoothController.h:321-328`:
```cpp
virtual bool registerGattService(const GattServiceDef& service) {
    (void)service;
    return false;
}
```

No corresponding `unregisterGattService()` method exists in the interface!

## Recommended Fix

1. **Add unregister method** to `IBluetoothController`:
   ```cpp
   /**
    * Unregister a previously registered GATT service
    * @param uuid Service UUID to unregister
    */
   virtual void unregisterGattService(const BleUuid& uuid) {}
   ```

2. **Document callback lifetime** in interface:
   ```cpp
   /**
    * \brief Register a GATT service with characteristics.
    * \param service Service definition (struct must remain valid until unregistered).
    * \return true if successfully registered.
    * \note Callbacks are stored by value; avoid capturing large objects.
    * \note Callbacks may fire until unregisterGattService() is called.
    */
   virtual bool registerGattService(const GattServiceDef& service) = 0;
   ```

3. **Add callback clearing** on module stop:
   ```cpp
   void BleUartService::deinit() {
       auto* ble = cdc::hal::getBluetoothControllerInstance();
       if (ble) {
           ble->unregisterGattService(BleUuid::from128(NUS_SVC_UUID));
           // Clear connection callbacks
           ble->clearConnectionCallbacks();  // New method needed
       }
       initialized_ = false;
   }
   ```

4. **Use weak reference pattern** for captures:
   ```cpp
   // Instead of:
   ble->addConnectionCallback([this](uint16_t connHandle) {
       this->onConnected(connHandle);
   });

   // Use:
   ble->addConnectionCallback([weakSelf = std::weak_ptr<Module>(shared_from_this())](uint16_t connHandle) {
       if (auto strongSelf = weakSelf.lock()) {
           strongSelf->onConnected(connHandle);
       }
   });
   ```

## References
- `components/cdc_hal/include/cdc_hal/IBluetoothController.h:321-328` - Service registration (no unregister)
- `components/mod_ble_serial/src/BleUartService.cpp:65-78` - Callback captures
- `components/mod_gpg/src/GpgModule.cpp:575-587` - Module start/stop pattern
