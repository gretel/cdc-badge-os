---
title: "[LOW] BLE vCard Peer Name Logged to Serial Output"
severity: LOW
domain: gdpr-dsgvo
lens: compliance/gdpr-dsgvo
labels:
  - "audit:compliance/gdpr-dsgvo"
---

## Summary
The BLE vCard module (`components/mod_vcard/src/ble_vcard.cpp`) logs the peer device name when discovering nearby vCard exchange partners. While less sensitive than the GPG cardholder data, this still exposes personal information (names of people whose badges are nearby) to anyone monitoring the serial output.

**Affected logging statement:**
- Line 592: `LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);`

## Impact
**Privacy Risk**: When performing BLE vCard exchange, the names of nearby devices (which typically contain the user's name) are logged to serial output. This could reveal:
- Who the badge owner exchanged vCards with
- Names of people in the physical vicinity
- Social connections (if analyzed over time)

**Context**: This is lower risk than the GPG module finding because:
1. Uses `LOG_I` from `cdc_log` system (not `ESP_LOGI`)
2. Data is transient (peer discovery, not permanent storage)
3. Only shows names of devices that broadcast vCard beacons

**Data Controller Responsibility**: Under Art. 5(1)(f) DSGVO, even incidental collection of personal data (names of nearby people) should be handled with appropriate logging levels.

**Evidence**:
```cpp
// components/mod_vcard/src/ble_vcard.cpp:592
static void onScanFound(hal::BleAdvertEvent& peer) {
    // ... peer processing ...
    if (!found && s_peer_count < MAX_PEERS) {
        s_peers[s_peer_count++] = peer;
        LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);  // PII exposed!
        s_nearby_peer = peer;
        s_nearby_available = true;
    }
}
```

## Recommended Fix

### Option 1: Lower Log Level to DEBUG (~15 min)
Change from INFO to DEBUG level:

```cpp
// Before:
LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);

// After:
LOG_D(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);
```

### Option 2: Mask Peer Name (~20 min)
Log only that a peer was found, not the actual name:

```cpp
// Before:
LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);

// After:
LOG_I(TAG, "Found vCard peer (name: %zu chars, RSSI: %d)", 
      strlen(peer.name), peer.rssi);
```

### Option 3: Truncate Peer Name (~15 min)
Show only first few characters:

```cpp
// Before:
LOG_I(TAG, "Found vCard peer: %s (RSSI %d)", peer.name, peer.rssi);

// After:
char short_name[16];
strncpy(short_name, peer.name, sizeof(short_name) - 1);
short_name[sizeof(short_name) - 1] = '\0';
LOG_I(TAG, "Found vCard peer: %s... (RSSI %d)", short_name, peer.rssi);
```

**Recommended**: Option 1 (change to LOG_D) is simplest and most appropriate since peer discovery is operational debug information.

## References
- **Art. 5(1)(f) DSGVO** - Integrity and confidentiality
- **Art. 32 DSGVO** - Security of processing
- **ENISA Logging Guidelines** - https://www.enisa.europa.eu/

</content>