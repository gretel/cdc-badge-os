---
title: "[MEDIUM] PII logged in vCard BLE discovery - peer names printed with RSSI"
severity: MEDIUM
domain: Privacy by Design
lens: PII-in-Logs
labels:
  - "audit:compliance/privacy-by-design"
---

## Summary
The vCard BLE module logs peer device names during discovery at line 592 of `components/mod_vcard/src/ble_vcard.cpp`. The log statement includes the peer's vCard name field, which may contain personally identifiable information (PII) such as full names, emails, or other contact details.

**Location:** `components/mod_vcard/src/ble_vcard.cpp:592`

```cpp
LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);
```

The `peer.name` field comes from BLE advertising data and can contain up to 32 characters (see `VCARD_BLE_NAME_MAX` in `ble_vcard.h:10`). This name is extracted from the BLE device name and may include personal information depending on how users configure their devices.

## Impact
- **PII Exposure in Logs:** Device names containing personal information are written to the serial log output, which may be captured during debugging, production logging, or when users connect the badge to a computer.
- **Audit Trail Contamination:** If logs are stored or forwarded to external systems, PII may be retained beyond the device's local storage.
- **Debug/Development Environments:** Serial logs are commonly captured during development, testing, and troubleshooting, potentially exposing user data to developers.

## Evidence
**File:** `components/mod_vcard/src/ble_vcard.cpp`
**Line 592:**
```cpp
LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);
```

**File:** `components/mod_vcard/include/mod_vcard/ble_vcard.h`
**Line 10-18:**
```cpp
#define VCARD_BLE_NAME_MAX   32

typedef struct {
    char name[VCARD_BLE_NAME_MAX];
    char slogan[VCARD_BLE_SLOGAN_MAX];
    int8_t rssi;
    uint8_t addr[6];
    uint8_t addr_type;
    bool exchange_ready;
} vcard_peer_t;
```

The `vcard_peer_t` struct stores the peer name in a 32-character buffer, which is then passed to the LOG_I macro without any filtering or truncation for log output.

## Recommended Fix
1. **Log only a generic identifier** instead of the full peer name. Use a hash of the BLE address or a simple counter:
   ```cpp
   LOG_I(TAG, "Found vCard peer (RSSI %d)", peer.rssi);
   ```

2. **Add DEBUG_MODE guard** if peer name logging is needed for development:
   ```cpp
   #ifdef DEBUG_MODE
   LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);
   #else
   LOG_I(TAG, "Found vCard peer (RSSI %d)", peer.rssi);
   #endif
   ```

3. **Truncate to reasonable length** if name must be logged:
   ```cpp
   LOG_I(TAG, "Found vCard peer: %.16s... (RSSI %d)", peer.name, peer.rssi);
   ```

## References
- GDPR Article 5(1)(c) - Data minimization: "Personal data shall be adequate, relevant and limited to what is necessary"
- Privacy by Design Principle 3 - Data minimization: "Collect only what you need, keep it only as long as necessary"
- Common logging best practices: Avoid logging PII unless absolutely necessary for debugging
