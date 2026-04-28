---
title: "[MEDIUM] BLE vCard exchange has no rate limiting or flood protection"
severity: MEDIUM
domain: rate-abuse
lens: rate-abuse-ble
labels:
  - "audit:security/rate-abuse"
---

## Summary
The BLE vCard exchange implementation in `components/mod_vcard/src/ble_vcard.cpp` accepts incoming exchange requests and vCard writes without any rate limiting, deduplication, or flood protection. An attacker with a BLE transmitter can:
1. Send unlimited exchange requests to trigger consent callbacks
2. Flood the GATT RX characteristic with vCard data
3. Exhaust the 32-peer discovery list (`MAX_PEERS = 32`)
4. Cause repeated NVS writes to store peer vCards

**Location:** `components/mod_vcard/src/ble_vcard.cpp:660-690` (CONTROL characteristic write handler)

```cpp
// CONTROL - Exchange commands
s_gattChars[2].onWrite = [](uint16_t connHandle, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (len < 1) return 0;

    if (data[0] == CMD_REQUEST_EXCHANGE) {
        if (s_consent_pending || s_exchange_in_progress) {
            sendStatusNotification(connHandle, STATUS_BUSY);
            return 0;
        }

        snprintf(s_consent_peer_name, sizeof(s_consent_peer_name), "BLE Device");
        s_consent_pending = true;
        s_consent_conn_handle = connHandle;
        s_state_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        LOG_I(TAG, "Exchange request from connected device");

        if (s_consent_callback) {
            s_consent_callback(s_consent_peer_name);  // Callback every request!
        }
    }
    return 0;
};
```

**Location:** `components/mod_vcard/src/ble_vcard.cpp:630-658` (RX characteristic write handler)

```cpp
// RX - Receive vCard from peer
s_gattChars[1].onWrite = [](uint16_t, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (!s_receive_enabled) return 0x03;

    if (len > 0 && s_rx_offset + len < sizeof(s_rx_vcard)) {
        memcpy(s_rx_vcard + s_rx_offset, data, len);
        s_rx_offset += len;

        if (strstr(s_rx_vcard, "END:VCARD") != nullptr) {
            char err[64];
            if (vcard_store_add(s_rx_vcard, s_rx_offset, err, sizeof(err))) {  // NVS write!
                s_exchange_result = true;
            }
            s_exchange_result_ready = true;
            s_rx_offset = 0;
        }
    }
    return 0;
};
```

## Impact
**Consent callback flooding:**
- Each `CMD_REQUEST_EXCHANGE` triggers the user consent callback
- No rate limiting means callbacks can fire at BLE throughput (~100ms intervals)
- User can be bombarded with consent prompts (UI disruption)
- If callback is slow (UI render), it can block BLE processing

**NVS storage exhaustion:**
- `vcard_store_add()` writes to NVS for each received vCard
- NVS has limited write endurance (~100,000 writes per sector)
- An attacker can send 100 vCards/second to wear out NVS
- With 1000 exchanges/minute, NVS could be degraded in ~2 hours

**Peer list exhaustion:**
- `MAX_PEERS = 32` in `processScanResults()` (line 115)
- Each unique BLE MAC address adds to the peer list
- Once full, no new peers can be discovered until list is cleared
- Attack: flood with random MACs to fill the list

**Memory pressure:**
- Each vCard up to `VCARD_MAX_LEN` bytes stored in RAM
- Repeated writes without proper cleanup can fragment heap
- `s_rx_vcard` buffer is reused but NVS cache may grow

## Evidence
- **File:** `components/mod_vcard/src/ble_vcard.cpp`
- **Line 115:** `static constexpr uint16_t MAX_PEERS = 32;` - No limit on discovery rate
- **Lines 660-690:** CONTROL write handler - no rate limiting on exchange requests
- **Lines 630-658:** RX write handler - no rate limiting on vCard writes
- **Lines 529-589:** `vcard_store_add()` in `vcard_store.cpp` - NVS write per vCard
- **Line 679:** `vcard_store.cpp` - `nvs_set_str()` called for every vCard

**No protection mechanisms found:**
- No timestamp tracking for last exchange request
- No debounce on consent callback
- No exponential backoff on failed exchanges
- No deduplication of rapid-fire requests from same peer
- No maximum exchanges per minute/hour

## Recommended Fix
Implement rate limiting and flood protection at multiple levels:

**1. Rate limit exchange requests (per connection)**
```cpp
// Add to runtime state (around line 170)
static constexpr uint32_t EXCHANGE_RATE_LIMIT_MS = 5000;  // 1 per 5 seconds
static uint32_t s_last_exchange_ms = 0;
static uint16_t s_exchange_count_min = 0;
static uint32_t s_exchange_window_start = 0;

// In CONTROL write handler (line 660)
uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
if (now - s_last_exchange_ms < EXCHANGE_RATE_LIMIT_MS) {
    sendStatusNotification(connHandle, STATUS_BUSY);
    return 0;  // Rate limited
}
s_last_exchange_ms = now;
```

**2. Limit exchanges per minute**
```cpp
// Track exchanges per minute
if (now - s_exchange_window_start >= 60000) {
    s_exchange_window_start = now;
    s_exchange_count_min = 0;
}
if (s_exchange_count_min >= 10) {  // Max 10 exchanges/minute
    sendStatusNotification(connHandle, STATUS_BUSY);
    return 0;
}
s_exchange_count_min++;
```

**3. Debounce consent callback**
```cpp
static uint32_t s_last_consent_ms = 0;
static constexpr uint32_t CONSENT_DEBOUNCE_MS = 3000;

if (now - s_last_consent_ms >= CONSENT_DEBOUNCE_MS) {
    s_last_consent_ms = now;
    if (s_consent_callback) {
        s_consent_callback(s_consent_peer_name);
    }
}
```

**4. NVS write throttling**
- Buffer vCard writes and batch to once per 5 seconds
- Add maximum NVS writes per minute (e.g., 20)
- Return STATUS_BUSY if NVS budget exceeded

**5. Peer list protection**
- Add timestamp to each peer entry
- Evict oldest peers when list is full (LRU)
- Limit discovery rate (e.g., max 1 new peer per 10 seconds)

**Implementation steps:**
1. Add rate limit constants and state variables to `ble_vcard.cpp` (lines 170-185)
2. Modify CONTROL write handler to check rate limits before processing (lines 660-690)
3. Add consent callback debouncing (lines 680-690)
4. Add exchange counter with per-minute reset (lines 660-690)
5. Add peer list LRU eviction in `processScanResults()` (lines 530-600)
6. Test with rapid-fire BLE requests to verify rate limiting works

## References
- OWASP: [Rate Limiting Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Rate_Limiting_Cheat_Sheet.html)
- BLE Specification: [GATT Server characteristics](https://www.bluetooth.com/specifications/specs/generic-attribute-profile-2-0/)
- NVS endurance: [ESP32 NVS documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
