---
title: "[LOW] BLE connection timeout hardcoded without configurability"
severity: LOW
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/cdc_hal/src/BluetoothController.cpp`, the BLE connection timeout is hardcoded to `10000` (10 seconds) in the `connect()` function at line 1482. This timeout value is passed directly to `ble_gap_connect()` without any configuration option or documentation explaining why this specific value was chosen.

**Location:** `components/cdc_hal/src/BluetoothController.cpp:1482`

## Impact
1. **Limited flexibility**: Different use cases may require different timeout values:
   - Quick testing: shorter timeout (3-5 seconds)
   - Poor RF environment: longer timeout (20-30 seconds)
   - Power-critical applications: shorter timeout to save power

2. **Debugging difficulty**: When connection issues occur, developers cannot easily adjust the timeout to diagnose whether the problem is timing-related.

3. **Suboptimal performance**: A single timeout value may not be optimal for all scenarios, potentially causing unnecessary connection failures or excessive wait times.

## Evidence
**File: `components/cdc_hal/src/BluetoothController.cpp`**

1. **Hardcoded timeout in connect() (line 1469-1493):**
```cpp
bool BluetoothController::connect(const uint8_t* addr, uint8_t addrType) {
    if (!enabled_ || !synced_ || !addr) return false;

    // Must stop advertising to connect as central
    if (advertising_) {
        ble_gap_adv_stop();
        advertising_ = false;
    }

    ble_addr_t bleAddr;
    bleAddr.type = addrType;
    memcpy(bleAddr.val, addr, 6);

    int rc = ble_gap_connect(ownAddrType_, &bleAddr, 10000, nullptr,  // Hardcoded!
                              bleGapEventCallback, nullptr);
    if (rc != 0) {
        LOG_E(TAG, "ble_gap_connect failed: %d", rc);
        startAdvertising();
        return false;
    }

    LOG_I(TAG, "Connecting to %02X:%02X:%02X:%02X:%02X:%02X",
          addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    return true;
}
```

2. **NimBLE API documentation (from header):**
The `ble_gap_connect()` function signature is:
```cpp
int ble_gap_connect(uint8_t own_addr_type, const ble_addr_t *peer_addr,
                    int32_t duration_ms, struct ble_gap_event *event,
                    ble_gap_event_fn *cb, void *cb_arg);
```

Where `duration_ms`:
- `0`: Use default duration
- `> 0`: Specific duration in milliseconds
- The duration limits both connection establishment and the subsequent connection event

## Recommended Fix
Add a configurable timeout parameter with a sensible default:

### Option 1: Add method overload
```cpp
class BluetoothController {
public:
    // Existing method with default timeout
    bool connect(const uint8_t* addr, uint8_t addrType = 0) {
        return connect(addr, addrType, 10000);  // Default 10s
    }
    
    // New method with configurable timeout
    bool connect(const uint8_t* addr, uint8_t addrType, int32_t durationMs);
    
private:
    static constexpr int32_t DEFAULT_CONNECT_TIMEOUT_MS = 10000;
};

bool BluetoothController::connect(const uint8_t* addr, uint8_t addrType, int32_t durationMs) {
    if (!enabled_ || !synced_ || !addr) return false;

    if (advertising_) {
        ble_gap_adv_stop();
        advertising_ = false;
    }

    ble_addr_t bleAddr;
    bleAddr.type = addrType;
    memcpy(bleAddr.val, addr, 6);

    int rc = ble_gap_connect(ownAddrType_, &bleAddr, durationMs, nullptr,
                              bleGapEventCallback, nullptr);
    // ... rest of implementation
}
```

### Option 2: Add configuration struct
```cpp
struct BleConnectConfig {
    int32_t timeoutMs = 10000;          // Default 10 seconds
    uint8_t ownAddrType = 0;
    // Future: connection parameters, retries, etc.
};

bool BluetoothController::connect(const uint8_t* addr, uint8_t addrType,
                                   const BleConnectConfig& config = {}) {
    // Use config.timeoutMs instead of hardcoded value
    int rc = ble_gap_connect(ownAddrType_, &bleAddr, config.timeoutMs, nullptr,
                              bleGapEventCallback, nullptr);
    // ...
}
```

### Option 3: Add constant with documentation
```cpp
class BluetoothController {
private:
    /** \brief BLE connection timeout in milliseconds.
     *  10 seconds provides a balance between:
     *  - Quick failure detection for unreachable devices
     *  - Sufficient time for devices to respond in noisy RF environments
     *  - Reasonable power consumption for connection attempts
     */
    static constexpr int32_t CONNECT_TIMEOUT_MS = 10000;

public:
    bool connect(const uint8_t* addr, uint8_t addrType = 0) {
        // ...
        int rc = ble_gap_connect(ownAddrType_, &bleAddr, CONNECT_TIMEOUT_MS, nullptr,
                                  bleGapEventCallback, nullptr);
        // ...
    }
};
```

## References
- [NimBLE Gap API](https://docs.nimble-mcu.org/group__group-nimble-group-gap/) - ble_gap_connect documentation
- [BLE Connection Establishment](https://www.bluetooth.com/specifications/bluetooth-core-specification/) - Link layer connection timing
- [ESP-IDF BLE](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/ble_gap.html) - Connection timeout considerations
