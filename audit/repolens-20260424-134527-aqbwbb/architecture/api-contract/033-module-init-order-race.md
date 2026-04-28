---
title: "[MEDIUM] Module Initialization Order Race Condition with IBluetoothController Callbacks"
severity: MEDIUM
domain: API Contract Integrity
lens: module-boundaries
labels:
  - "audit:architecture/api-contract"
---

## Summary
Modules register GATT service callbacks and connection listeners during `init()` phase, but `IBluetoothController` may not be fully initialized at that time. This creates a race condition where callbacks can be called before the receiving module is ready, or callbacks may capture uninitialized state.

**Location**: Multiple module initializers call `registerInitializer()` which runs before the module registry itself is ready:
- `components/mod_ble_serial/src/BleSerialModule.cpp:327`
- `components/mod_gpg/src/GpgModule.cpp:655`
- `components/mod_fido2/src/Fido2Module.cpp:291`
- `components/mod_totp/src/TotpModule.cpp:1014`
- `components/mod_password/src/PasswordModule.cpp:850`

## Impact
1. **Callback invocation on uninitialized modules**: If BLE connects during boot, callbacks may fire before module state is ready
2. **Missing service discovery**: GATT service registration happens during `init()`, but if `IBluetoothController::enable()` hasn't completed, service may not be advertised
3. **Non-deterministic behavior**: Module initialization order is not guaranteed, leading to intermittent failures

## Evidence

In `mod_ble_serial/src/BleSerialModule.cpp:58-108`:
```cpp
bool BleUartService::init() {
    auto* ble = cdc::hal::getBluetoothControllerInstance();
    if (!ble || !ble->isEnabled()) {  // <-- May return false if BLE not enabled yet
        LOG_E(TAG, "BLE not enabled");
        return false;
    }

    // Register GATT service
    if (!ble->registerGattService(svcDef)) {
        LOG_E(TAG, "Failed to register NUS GATT service");
        return false;
    }

    // Register connection callbacks
    ble->addConnectionCallback([](uint16_t /*connHandle*/) {
        BleUartService::instance().onConnectionChange(true);  // <-- May fire before module ready
    });
```

Module registration pattern in `mod_ble_serial/src/BleSerialModule.cpp:327-336`:
```cpp
extern "C" void mod_ble_serial_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_ble_serial::BleSerialModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}
```

The `registerInitializer()` stores the function but doesn't guarantee execution order relative to other modules.

## Recommended Fix

1. **Two-phase initialization**: Split module initialization into:
   - Phase 1 (`init()`): Register interfaces, allocate resources
   - Phase 2 (`start()`): Enable services, register callbacks

2. **Deferral for callback-heavy modules**: Modules that depend on `IBluetoothController` should:
   - Check `isEnabled()` in `init()` and defer callback registration
   - Register callbacks in `start()` after confirming BLE is enabled

3. **Document initialization dependencies**: Add a dependency graph or ordering hints to `ModuleRegistry`:
   ```cpp
   struct ModuleDependency {
       const char* moduleName;
       const char* dependsOn[];  // NULL-terminated array
   };
   ```

4. **Add callback guard**: In `mod_ble_serial/src/BleUartService.cpp`, add initialization guard to callbacks:
   ```cpp
   ble->addConnectionCallback([](uint16_t connHandle) {
       auto& svc = BleUartService::instance();
       if (svc.isInitialized()) {  // <-- Guard
           svc.onConnectionChange(true);
       }
   });
   ```

## References
- `components/cdc_core/include/cdc_core/ModuleRegistry.h:17-33` - Module initializer registration
- `components/cdc_hal/include/cdc_hal/IBluetoothController.h:308-320` - Callback registration API
- `components/mod_ble_serial/src/BleUartService.cpp:92-108` - Connection callback registration
