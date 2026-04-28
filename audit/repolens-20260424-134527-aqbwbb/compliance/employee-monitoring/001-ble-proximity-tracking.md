---
title: "[MEDIUM] BLE Proximity Tracking Without Temporal Limits or Works Council Configuration"
severity: MEDIUM
domain: employee-monitoring
lens: betriebsrat-compliance
labels:
  - proximity-tracking
  - ble-scanning
  - temporal-limits
---

## Summary

The **mod_vcard** component implements BLE-based proximity tracking that discovers, stores, and tracks nearby devices with their MAC addresses, names, and RSSI signal strength. The system stores up to **32 nearby peers** in runtime memory and up to **100 vCards** in persistent NVS storage.

**Files Affected:**
- `components/mod_vcard/src/ble_vcard.cpp` (lines 115-117, 537-593, 827-845)
- `components/mod_vcard/include/mod_vcard/ble_vcard.h` (lines 17-24, 52-54)
- `components/mod_vcard/include/mod_vcard/vcard_store.h` (lines 6-7)

**Evidence:**

```cpp
// components/mod_vcard/src/ble_vcard.cpp:115-117
static constexpr uint16_t MAX_PEERS = 32;
static vcard_peer_t s_peers[MAX_PEERS] = {};
static uint16_t s_peer_count = 0;

// components/mod_vcard/src/ble_vcard.cpp:537-593 (processScanResults)
static void processScanResults() {
    BleScanResult results[16];
    uint8_t count = ble->getScanResults(results, 16);

    for (uint8_t i = 0; i < count; i++) {
        const auto& r = results[i];
        vcard_peer_t peer = {};
        memcpy(peer.addr, r.mac, 6);        // MAC address stored
        peer.addr_type = r.addrType;
        peer.rssi = r.rssi;                 // RSSI signal strength stored
        strncpy(peer.name, r.name, sizeof(peer.name) - 1);
        
        // ... vCard service detection ...
        
        if (!found && s_peer_count < MAX_PEERS) {
            s_peers[s_peer_count++] = peer;
            LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);
        }
    }
}

// components/mod_vcard/include/mod_vcard/vcard_store.h:6-7
#define VCARD_MAX_LEN   768
#define VCARD_MAX_CARDS 100  // Persistent storage of up to 100 contacts
```

## Impact

Under **Betriebsverfassungsgesetz (BetrVG) §87 Abs. 1 Nr. 6**, the Works Council has co-determination rights for any system that monitors employee behavior or performance. This BLE proximity tracking has the following compliance implications:

1. **Proximity Data Collection**: MAC addresses and RSSI values can be used to track employee movement patterns, proximity to specific locations, and social interactions.

2. **Persistent Storage**: Up to 100 vCards are stored persistently in NVS, creating a historical record of contacts.

3. **No Temporal Limits**: Scanning can run continuously without configurable work-hour restrictions.

4. **No Automatic Data Expiry**: Stored peers and vCards have no automatic cleanup mechanism.

5. **Works Council Agreement Missing**: The module lacks configuration options for Works Council oversight (e.g., configurable scan intervals, data retention limits, work-hours-only mode).

## Recommended Fix

Implement the following within ~1 hour:

1. **Add temporal limits configuration** in `ble_vcard.h`:
   ```cpp
   // Add to ble_vcard.h
   void ble_vcard_set_work_hours_only(bool enabled);
   bool ble_vcard_is_work_hours_only(void);
   void ble_vcard_set_scan_window(uint8_t start_hour, uint8_t end_hour);
   ```

2. **Add data retention limits**:
   ```cpp
   // Add to ble_vcard.h
   void ble_vcard_set_peer_ttl_ms(uint32_t ttl_ms);
   void ble_vcard_prune_old_peers(uint32_t now_ms);
   ```

3. **Add Works Council oversight flag**:
   ```cpp
   // Add to ble_vcard.h
   void ble_vcard_set_betriebsrat_agreement_id(const char* agreement_id);
   bool ble_vcard_is_agreement_active(void);
   ```

4. **Implement scan pause during non-work hours** in `ble_vcard_tick()`:
   ```cpp
   // In ble_vcard_tick(), check if current time is within configured work hours
   // before allowing scan to continue
   ```

## References

- **Betriebsverfassungsgesetz §87 Abs. 1 Nr. 6**: Co-determination right for "technische Einrichtungen, mit denen das Verhalten oder die Leistung der Arbeitnehmer überwacht werden können"
- **BAG Urteil vom 1.12.2015 (Az.: 1 ABR 10/14)**: BLE tracking qualifies as employee monitoring
- **GDPR Article 5(1)(c)**: Data minimization principle - only collect what is necessary
- **GDPR Article 5(1)(e)**: Storage limitation - data should not be kept longer than necessary
