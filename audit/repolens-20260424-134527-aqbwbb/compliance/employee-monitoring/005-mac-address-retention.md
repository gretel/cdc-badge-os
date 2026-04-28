---
title: "[LOW] MAC Addresses Stored in RAM Without Automatic Expiry"
severity: LOW
domain: employee-monitoring
lens: betriebsrat-compliance
labels:
  - mac-address
  - data-retention
  - ram-storage
---

## Summary

The **mod_vcard** component stores MAC addresses of discovered peers in RAM (`s_peers` array) without automatic expiry. While the data is volatile (lost on reboot), it persists for the entire device uptime. Under GDPR data minimization principles, proximity data should have automatic cleanup.

**Files Affected:**
- `components/mod_vcard/src/ble_vcard.cpp` (lines 115-124, 583-594)

**Evidence:**

```cpp
// components/mod_vcard/src/ble_vcard.cpp:115-124
static constexpr uint16_t MAX_PEERS = 32;
static vcard_peer_t s_peers[MAX_PEERS] = {};  // RAM storage, no expiry
static uint16_t s_peer_count = 0;

static vcard_peer_t s_nearby_peer = {};       // Most recent peer snapshot
static bool s_nearby_available = false;

// components/mod_vcard/src/ble_vcard.cpp:583-594
static void processScanResults() {
    // ...
    for (uint8_t i = 0; i < count; i++) {
        const auto& r = results[i];
        vcard_peer_t peer = {};
        memcpy(peer.addr, r.mac, 6);  // MAC address stored
        peer.addr_type = r.addrType;
        peer.rssi = r.rssi;
        
        // ...
        
        if (!found && s_peer_count < MAX_PEERS) {
            s_peers[s_peer_count++] = peer;  // Stored until device reboot or manual clear
        }
    }
}

// components/mod_vcard/src/ble_vcard.cpp:837-838
void ble_vcard_set_scan_enabled(bool enabled) {
    // Only clears on scan restart
    if (enabled) {
        memset(s_peers, 0, sizeof(s_peers));  // Manual clear only
    }
}
```

The RAM storage contains:
- **MAC addresses**: Unique device identifiers (can be linked to specific employees)
- **RSSI values**: Signal strength for proximity tracking
- **Device names**: Potentially employee names or location identifiers
- **No automatic cleanup**: Data persists until device reboot or manual scan restart

## Impact

Under **GDPR Article 5(1)(c)** (data minimization) and **Article 5(1)(e)** (storage limitation):

1. **No Automatic Expiry**: MAC addresses stored indefinitely during device uptime
2. **No Age Tracking**: Cannot determine when a peer was last seen
3. **Memory Leak Risk**: Up to 32 peers accumulate without cleanup
4. **Privacy Risk**: Historical proximity data available for analysis

## Recommended Fix

Implement the following within ~1 hour:

1. **Add timestamp to peer structure** in `ble_vcard.h`:
   ```cpp
   typedef struct {
       char name[VCARD_BLE_NAME_MAX];
       char slogan[VCARD_BLE_SLOGAN_MAX];
       int8_t rssi;
       uint8_t addr[6];
       uint8_t addr_type;
       bool exchange_ready;
       uint32_t last_seen_ms;  // Add timestamp
   } vcard_peer_t;
   ```

2. **Add peer expiry configuration** in `ble_vcard.h`:
   ```cpp
   void ble_vcard_set_peer_ttl_ms(uint32_t ttl_ms);
   uint32_t ble_vcard_get_peer_ttl_ms(void);
   void ble_vcard_prune_old_peers(uint32_t now_ms);
   ```

3. **Implement pruning in tick()** in `ble_vcard.cpp`:
   ```cpp
   static uint32_t s_peer_ttl_ms = 3600000;  // Default: 1 hour
   
   void ble_vcard_set_peer_ttl_ms(uint32_t ttl_ms) {
       s_peer_ttl_ms = ttl_ms;
   }
   
   void ble_vcard_prune_old_peers(uint32_t now_ms) {
       if (xSemaphoreTake(s_peer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
           uint16_t write_idx = 0;
           for (uint16_t read_idx = 0; read_idx < s_peer_count; read_idx++) {
               // Keep peer if within TTL
               if (now_ms - s_peers[read_idx].last_seen_ms < s_peer_ttl_ms) {
                   s_peers[write_idx++] = s_peers[read_idx];
               }
           }
           s_peer_count = write_idx;
           xSemaphoreGive(s_peer_mutex);
       }
   }
   
   void ble_vcard_tick(uint32_t now_ms) {
       // Prune old peers
       ble_vcard_prune_old_peers(now_ms);
       
       // ... rest of tick logic
   }
   
   // Update processScanResults to set timestamp
   static void processScanResults() {
       // ...
       for (uint8_t i = 0; i < count; i++) {
           // ...
           peer.last_seen_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;  // Add timestamp
           // ...
       }
   }
   ```

4. **Add serial command for TTL configuration** in `VcardModule.cpp`:
   ```cpp
   static void cmdVcardTtl(const char* args) {
       // Usage: VCARD_TTL <seconds>
       char* endptr;
       long seconds = strtol(args, &endptr, 10);
       if (endptr != args) {
           ble_vcard_set_peer_ttl_ms(seconds * 1000);
           serial::Console::printf("Peer TTL set to %lu seconds\r\n", seconds);
       }
   }
   
   reg.registerCommand({"VCARD_TTL", "Set peer TTL in seconds", cmdVcardTtl, "vcard", false});
   ```

## References

- **GDPR Article 5(1)(c)**: Data minimization - only what is necessary
- **GDPR Article 5(1)(e)**: Storage limitation - data kept no longer than necessary
- **BetrVG §87 Abs. 1 Nr. 6**: Works Council co-determination for monitoring systems
- **BAG Urteil vom 1.12.2015 (Az.: 1 ABR 10/14)**: Proximity data should have automatic cleanup
