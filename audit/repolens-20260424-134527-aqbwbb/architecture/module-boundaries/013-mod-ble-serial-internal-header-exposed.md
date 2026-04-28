---
title: "[MEDIUM] mod_ble_serial: Internal BLE UART service header exposed in public include"
severity: MEDIUM
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_ble_serial` module exposes an internal BLE UART service header in its public include directory:

- **`BleUartService.h`** - Nordic UART Service GATT implementation (internal class)

Only `BleSerialModule.h` should be public. `BleUartService.h` is a low-level GATT service implementation.

## Impact

- **Implementation Leakage**: External modules can depend on internal GATT service
- **Tight Coupling**: Changes to BLE UART service break external consumers
- **Poor Encapsulation**: No clear distinction between public API and internal implementation
- **Implementation Exposure**: GATT handles, ring buffers, and BLE-specific details exposed publicly

## Evidence

**Current public include structure:**
```
components/mod_ble_serial/include/mod_ble_serial/
├── BleSerialModule.h     # Public API (correct)
└── BleUartService.h      # BLE UART service (internal - exposed!)
```

**Internal header contents:**

`BleUartService.h` (line 1-100):
```cpp
// BLE UART Service (Nordic UART Service compatible)
class BleUartService {
public:
    // Initialize the GATT service via IBluetoothController API
    bool init();
    void deinit();
    bool isInitialized() const;

    // TX (Badge -> Phone)
    size_t send(const uint8_t* data, size_t len);
    size_t send(const char* str);
    bool txReady() const;

    // RX (Phone -> Badge)
    size_t available() const;
    int getchar();
    size_t read(uint8_t* buf, size_t maxLen);

    // Connection State
    bool isConnected() const;

    // Callbacks (invoked by API)
    void onRxData(const uint8_t* data, size_t len);
    void onConnectionChange(bool connected);

    // Application Callbacks
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void()>;
    void setOnConnect(ConnectCallback cb);
    void setOnDisconnect(DisconnectCallback cb);

    // GATT handle for TX characteristic (internal!)
    uint16_t txCharHandle_ = 0;

    // RX ring buffer (internal!)
    static constexpr size_t RX_BUFFER_SIZE = 1024;
    uint8_t rxBuffer_[RX_BUFFER_SIZE] = {};
    volatile size_t rxHead_ = 0;
    volatile size_t rxTail_ = 0;

    // TX state (internal!)
    volatile bool txCongested_ = false;
    volatile bool txInProgress_ = false;
};
```

**Usage in module:**
```cpp
// From components/mod_ble_serial/src/BleSerialModule.cpp:
#include "mod_ble_serial/BleUartService.h"

// From components/mod_ble_serial/src/BleUartService.cpp:
#include "mod_ble_serial/BleUartService.h"
```

**Internal implementation details exposed:**
- GATT characteristic handles
- Ring buffer implementation
- TX congestion state
- BLE-specific callback signatures

## Recommended Fix

1. **Move BleUartService.h to src/**:
   ```
   components/mod_ble_serial/
   ├── include/mod_ble_serial/
   │   └── BleSerialModule.h     # Only public API
   └── src/
       ├── BleSerialModule.cpp
       └── BleUartService.h      # Move here (internal)
           BleUartService.cpp
   ```

2. **Update internal includes**:
   ```cpp
   // In src/BleSerialModule.cpp, change:
   #include "mod_ble_serial/BleUartService.h"  # → #include "BleUartService.h"
   ```

3. **Add internal marker**:
   ```cpp
   // In src/BleUartService.h:
   /**
    * @file BleUartService.h
    * @brief Internal BLE UART service - NOT part of public API
    */
   ```

4. **Document public API**:
   - Add Doxygen group to `BleSerialModule.h`:
   ```cpp
   /**
    * @defgroup ble-serial-public Public API
    * @brief BLE Serial module - UART over BLE
    * 
    * Public classes:
    * - @ref BleSerialModule - Main module interface
    */
   ```

5. **Consider public interface**:
   - If external modules need serial over BLE, expose via `BleSerialModule` methods
   - Keep `BleUartService` GATT details internal

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules are completely isolated and self-contained"
- IModule interface: `components/cdc_core/include/cdc_core/IModule.h`
- Nordic UART Service: https://devzone.nordicsemi.com/f/nordic-q-a/391/uart-over-ble-performance

(End of file - total 154 lines)
