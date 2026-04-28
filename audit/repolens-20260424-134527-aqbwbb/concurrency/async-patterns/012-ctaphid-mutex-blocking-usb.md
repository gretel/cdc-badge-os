---
title: "[MEDIUM] Blocking USB call while holding CTAPHID mutex"
severity: MEDIUM
domain: concurrency/async-patterns
lens: freeRTOS-synchronization
labels:
  - "blocking-in-critical-section"
  - "mutex-contention"
---

## Summary
The CTAPHID keepalive function `ctaphid_send_keepalive()` calls `fido2_usb_write()` which can block waiting for USB endpoint availability, but this is called while holding the CTAPHID mutex. This creates a window where the mutex is held during a potentially blocking I/O operation.

**Location:** `components/mod_fido2/src/ctaphid.cpp:593-600`, `components/mod_fido2/src/ctap2.cpp:1079,1138`

## Impact
When `ctap2_process_command()` processes a long-running operation (like key generation or signing):

1. `process_complete_message()` acquires `g_ctaphid.mutex` (line 412 in ctaphid.cpp)
2. Calls `handle_cbor()` -> `ctap2_process_command()` -> `create_credential_and_respond()`
3. `create_credential_and_respond()` calls `ctap2_send_keepalive()` (lines 1079, 1138)
4. Which calls `ctaphid_send_keepalive()` -> `fido2_usb_write()` -> `usb_hid_send_report()`
5. `usb_hid_send_report()` can block if USB endpoint is congested

**Potential issues:**
- **Extended mutex hold time**: The mutex is held while USB transmission completes
- **Starvation**: Other CTAPHID packet processing waits for the mutex
- **Priority inversion**: Lower-priority USB task might need to run while high-priority CTAPHID task holds mutex

While the actual blocking is usually short (USB endpoint buffer availability), this pattern is fragile:
- If USB disconnects during operation, the wait could be longer
- If the USB task has lower priority, the mutex is held longer
- Future code changes might add more operations while holding the mutex

## Evidence
```cpp
// ctaphid.cpp:412 - Mutex acquired
bool ctaphid_process_packet(const uint8_t *packet) {
    xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);  // Line 412
    
    // ... packet parsing ...
    
    if (ch->offset >= ch->bcnt) {
        process_complete_message(ch);  // Line 533 - calls handle_cbor
    }
    
    xSemaphoreGive(g_ctaphid.mutex);  // Line 538 - released AFTER processing
    return true;
}

// ctaphid.cpp:593-600 - Keepalive sends via USB
void ctaphid_send_keepalive(uint32_t cid, uint8_t status) {
    uint8_t packet[CTAPHID_PACKET_SIZE];
    uint8_t data = status;
    build_init_packet(packet, cid, CTAPHID_KEEPALIVE, 1, &data, 1);
    
    // Send directly via USB - can block if endpoint congested
    fido2_usb_write(packet);  // Line 599
}

// Fido2Module.cpp:280-282 - USB send
bool fido2_usb_write(const uint8_t* buffer) {
    if (!buffer) return false;
    return usb_hid_send_report(s_hid_instance, 0, buffer, CTAPHID_PACKET_SIZE);
}

// ctap2.cpp:1078-1079 - Called during long operation
static uint8_t create_credential_and_respond(const MakeCredentialParams *p, ...) {
    // Send KEEPALIVE before long operation (key generation takes ~200-500ms)
    ctap2_send_keepalive(CTAPHID_STATUS_PROCESSING);  // Line 1079
    // ... key generation ...
}
```

The call chain while holding mutex:
```
ctaphid_process_packet (mutex held)
  -> process_complete_message
    -> handle_cbor
      -> ctap2_process_command
        -> create_credential_and_respond
          -> ctap2_send_keepalive
            -> ctaphid_send_keepalive
              -> fido2_usb_write
                -> usb_hid_send_report (can block)
```

## Recommended Fix
Release the mutex before calling `ctaphid_send_keepalive()`. Since keepalive packets are informational and not critical for message ordering, they don't need to be inside the critical section.

Option 1: Capture the CID before releasing mutex, send keepalive after:

```cpp
// In process_complete_message(), modify to return CID for keepalive:
static void process_complete_message(ctaphid_channel_t *ch) {
    uint32_t cid = ch->cid;
    uint8_t cmd = ch->cmd;
    uint8_t *data = ch->buffer;
    uint16_t len = ch->bcnt;
    
    // ... process command ...
    
    // Clear channel state
    ch->active = false;
    ch->offset = 0;
    
    // Return CID for post-mutex keepalive
    return cid;  // Or use a struct return
}

// In ctaphid_process_packet():
if (ch->offset >= ch->bcnt) {
    uint32_t cid = process_complete_message(ch);  // Returns CID
    xSemaphoreGive(g_ctaphid.mutex);  // Release BEFORE keepalive
    
    // Send keepalive outside critical section if needed
    if (cid) {
        // Check if keepalive was queued during processing
        ctaphid_send_keepalive(cid, CTAPHID_STATUS_PROCESSING);
    }
} else {
    xSemaphoreGive(g_ctaphid.mutex);
}
```

Option 2 (simpler): Make `ctaphid_send_keepalive()` idempotent and call it both inside and outside:

```cpp
// In handle_cbor() or create_credential_and_respond():
// Keep the current call for logic, but add one after mutex release

// In ctaphid_process_packet():
xSemaphoreTake(g_ctaphid.mutex, portMAX_DELAY);
// ... processing ...
uint32_t current_cid = g_ctaphid.current_cid;  // Capture before release
xSemaphoreGive(g_ctaphid.mutex);

// Send keepalive after releasing mutex
if (current_cid && ctaphid_needs_keepalive(current_cid)) {
    ctaphid_send_keepalive(current_cid, CTAPHID_STATUS_PROCESSING);
}
```

Option 3 (minimal change): Add a quick check to avoid blocking:

```cpp
void ctaphid_send_keepalive(uint32_t cid, uint8_t status) {
    uint8_t packet[CTAPHID_PACKET_SIZE];
    uint8_t data = status;
    build_init_packet(packet, cid, CTAPHID_KEEPALIVE, 1, &data, 1);
    
    // Non-blocking send - drop if not ready
    if (!fido2_usb_ready()) {
        LOG_D("CTAPHID", "USB not ready, skipping keepalive");
        return;
    }
    fido2_usb_write(packet);
}
```

Option 1 is the cleanest architectural fix. Option 3 is the quickest to implement.

## References
- [FreeRTOS critical sections best practices](https://www.freertos.org/short-critical-sections.html)
- [ESP32-S3 USB HID timing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/usb.html)
- [CTAPHID keepalive specification](https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-v2.1-ps-20210615.html#keepalive)
