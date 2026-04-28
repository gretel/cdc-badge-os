---
title: "[MEDIUM] Missing audit events for USB/BLE connection lifecycle"
severity: MEDIUM
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

USB and BLE connection/disconnection events are not audited with structured records. The system logs these events but without audit-specific fields for tracking connection lifecycle, which is important for understanding when the device was accessible to hosts.

**Files affected:**
- `components/cdc_core/src/ModuleRegistry.cpp:281-299` - `dispatchUsbConnect()`, `dispatchUsbDisconnect()`
- `components/cdc_core/src/UsbManager.cpp` - USB interface registration
- `components/mod_hid/src/BleHidKeyboard.cpp:262-279` - BLE connection handlers
- `components/cdc_os_ui/src/AppUi.cpp` - System lock/unlock events

## Impact

1. **Session Tracking**: Cannot reconstruct when device was connected to hosts.
2. **Security Analysis**: No audit trail for USB/BLE session start/end times.
3. **Forensic Analysis**: Hard to determine if unauthorized connections occurred.
4. **Compliance**: FIDO2 requires tracking of authenticator sessions.

## Evidence

### USB disconnect event not audited

In `ModuleRegistry.cpp:291-299`:
```cpp
void ModuleRegistry::dispatchUsbDisconnect() {
    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbDisconnect();
        }
    }
}
```

**Missing audit data:**
- No record of USB disconnect event
- No record of which modules were notified
- No timestamp for session end tracking
- No record of modules that were active

### BLE connection events not audited

In `mod_hid/src/BleHidKeyboard.cpp:262-279`:
```cpp
void BLEOnConnect(esp_gatts_cb_event_t event, esp_gatts_attr_db_t* gatts_db) {
    LOG_I(TAG, "HID device connected (handle=%d)", connHandle);
    // ...
}

void BLEOnDisconnect(esp_gatts_cb_event_t event, esp_gatts_attr_db_t* gatts_db) {
    LOG_I(TAG, "HID device disconnected (reason=%d)", reason);
    // ...
}
```

**Missing audit data:**
- No structured record of BLE connection
- No record of connection handle or peer device
- No timestamp for session tracking
- No record of disconnect reason

### System lock/unlock not audited

In `AppUi.cpp` (search for lock/unlock):
```cpp
static void onUnlockRequested() {
    // ... unlock logic ...
    core::ModuleRegistry::instance().dispatchUnlock();
}
```

**Missing audit data:**
- No audit record of lock event
- No audit record of unlock event
- No record of lockout duration (if applicable)
- No record of which modules were notified

## Recommended Fix

### Step 1: Add USB connection audit (10 min)

Update `ModuleRegistry.cpp`:
```cpp
void ModuleRegistry::dispatchUsbConnect() {
    // Audit USB connect
    core::audit_record(core::AuditEventType::USB_CONNECT,
                      0, 0, 0, 1, count_);  // 1 = USB context, count_ = active modules

    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbConnect();
        }
    }
}

void ModuleRegistry::dispatchUsbDisconnect() {
    // Audit USB disconnect with before-state
    core::audit_record(core::AuditEventType::USB_CONNECT,  // Reuse as USB_DISCONNECT
                      0, 0, 0, 1, count_);

    for (uint8_t i = 0; i < count_; i++) {
        if (modules_[i]->getState() == ServiceState::STARTED) {
            modules_[i]->onUsbDisconnect();
        }
    }
}
```

### Step 2: Add BLE connection audit (10 min)

Update `mod_hid/src/BleHidKeyboard.cpp`:
```cpp
void BLEOnConnect(esp_gatts_cb_event_t event, esp_gatts_attr_db_t* gatts_db) {
    LOG_I(TAG, "HID device connected (handle=%d)", connHandle);

    // Audit BLE connect
    core::audit_record(core::AuditEventType::MODULE_EVENT,
                      0, 0, 0, 2, connHandle);  // 2 = BLE context

    // ... existing code ...
}

void BLEOnDisconnect(esp_gatts_cb_event_t event, esp_gatts_attr_db_t* gatts_db) {
    LOG_I(TAG, "HID device disconnected (reason=%d)", reason);

    // Audit BLE disconnect
    core::audit_record(core::AuditEventType::MODULE_EVENT,
                      0, 0, 0, 2, (reason << 8) | connHandle);

    // ... existing code ...
}
```

### Step 3: Add lock/unlock audit (10 min)

Update `AppUi.cpp`:
```cpp
static void onUnlockRequested() {
    // Audit unlock
    core::audit_record(core::AuditEventType::SYSTEM_UNLOCK,
                      0, 0, 0, 0, 0);

    // ... existing unlock logic ...
    core::ModuleRegistry::instance().dispatchUnlock();
}

// Add to lock function (find where lock happens):
static void lockDevice() {
    // Audit lock
    core::audit_record(core::AuditEventType::SYSTEM_LOCK,
                      0, 0, 0, 0, 0);

    // ... existing lock logic ...
    core::ModuleRegistry::instance().dispatchLock();
}
```

## References

- FIDO2 CTAP2 Spec Section 4 (Transport Session)
- NIST SP 800-63B - Session management for authentication
- Common Criteria EAL2+ - FAU_GEN.1 audit data generation
