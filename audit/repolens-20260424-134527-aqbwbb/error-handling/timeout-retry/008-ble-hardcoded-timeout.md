---
title: "[LOW] BLE connection uses hardcoded timeout without configurability"
severity: LOW
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "ble"
  - "timeout"
  - "connection"
  - "gatt"
---

## Summary
The Bluetooth controller (`components/cdc_hal/src/BluetoothController.cpp`) uses a hardcoded 10-second connection timeout that may be too short for congested environments or too long for responsive user feedback.

**Evidence:**

1. File: `components/cdc_hal/src/BluetoothController.cpp:1482` - Hardcoded 10-second timeout:
   ```cpp
   int rc = ble_gap_connect(ownAddrType_, &bleAddr, 10000, nullptr,
                            bleGapEventCallback, nullptr);
   ```

2. File: `components/cdc_hal/src/BluetoothController.cpp:150-151` - No timeout configuration in class:
   ```cpp
   private:
       core::ServiceState state_ = core::ServiceState::UNINITIALIZED;
       bool enabled_ = false;
       // No connection timeout configurable
   ```

3. File: `components/cdc_hal/src/BluetoothController.cpp:1514-1517` - GATT discovery has no timeout:
   ```cpp
   int rc = ble_gattc_disc_svc_by_uuid(connHandle, &nimbleUuid.u,
                                         gattcSvcDiscCb, nullptr);
   if (rc != 0) {
       LOG_E(TAG, "ble_gattc_disc_svc_by_uuid failed: %d", rc);
   }
   // No timeout for discovery to complete!
   ```

4. File: `components/cdc_hal/src/BluetoothController.cpp:1567-1571` - GATT read has no timeout:
   ```cpp
   int rc = ble_gattc_read(connHandle, attrHandle, gattcReadCb, nullptr);
   if (rc != 0) {
       LOG_E(TAG, "ble_gattc_read failed: %d", rc);
   }
   // No timeout for read to complete!
   ```

## Impact
- **Connection failures**: 10s may be too short in crowded BLE environments
- **Poor UX**: 10s wait may feel slow for simple connections
- **Blocking operations**: GATT discovery and read operations have no timeout, potentially hanging forever
- **Resource leaks**: Operations without timeouts can leave resources allocated indefinitely

## Recommended Fix
Add configurable timeout constants and per-operation timeouts:

1. Add timeout constants:
   ```cpp
   static constexpr uint32_t BLE_CONNECT_TIMEOUT_MS = 10000;  // Configurable
   static constexpr uint32_t BLE_DISCOVERY_TIMEOUT_MS = 5000;
   static constexpr uint32_t BLE_READ_TIMEOUT_MS = 3000;
   static constexpr uint32_t BLE_WRITE_TIMEOUT_MS = 3000;
   ```

2. Add timeout state tracking:
   ```cpp
   private:
       // Timeout configuration
       uint32_t connectTimeoutMs_ = BLE_CONNECT_TIMEOUT_MS;
       uint32_t discoveryTimeoutMs_ = BLE_DISCOVERY_TIMEOUT_MS;
       
       // Operation tracking
       uint32_t lastDiscoveryStartMs_ = 0;
       uint32_t lastReadStartMs_ = 0;
   ```

3. Add timeout checking for GATT operations:
   ```cpp
   // In gattcSvcDiscCb - check for timeout
   int gattcSvcDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                       const struct ble_gatt_svc* service, void* arg) {
       (void)arg;
       auto* ctrl = BluetoothController::instance_;
       if (!ctrl) return 0;
       
       // Check for timeout
       if (error->status == BLE_HS_ETIMEOUT || 
           (error->status == 0 && service && 
            (xTaskGetTickCount() * portTICK_PERIOD_MS - ctrl->lastDiscoveryStartMs_ > 
             ctrl->discoveryTimeoutMs_))) {
           LOG_E(TAG, "Service discovery timeout");
           if (ctrl->svcDiscoveryCb_) {
               ctrl->svcDiscoveryCb_(connHandle, nullptr, true);
           }
           return 0;
       }
       // ... rest of callback
   }
   ```

4. Add timeout configuration method:
   ```cpp
   /**
    * \brief Sets connection timeout in milliseconds.
    * \param timeoutMs Connection timeout.
    */
   void setConnectTimeout(uint32_t timeoutMs) {
       connectTimeoutMs_ = timeoutMs;
   }
   
   bool connect(const uint8_t* addr, uint8_t addrType) {
       if (!enabled_ || !synced_ || !addr) return false;
       
       if (advertising_) {
           ble_gap_adv_stop();
           advertising_ = false;
       }
       
       ble_addr_t bleAddr;
       bleAddr.type = addrType;
       memcpy(bleAddr.val, addr, 6);
       
       int rc = ble_gap_connect(ownAddrType_, &bleAddr, connectTimeoutMs_, nullptr,
                                bleGapEventCallback, nullptr);
       // ...
   }
   ```

5. Consider adding retry logic for transient BLE failures with exponential backoff

## References
- NimBLE connection parameters: https://nimble.apache.org/api/latest/group__group__gatt__client.html
- BLE connection best practices: https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/protocols/ble/ble_optimized.html
- Timeout configuration patterns: https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/client-config.html

</content>