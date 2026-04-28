---
title: "[MEDIUM] Silent packet dropping in FIDO2 RX queue without backpressure handling"
severity: MEDIUM
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "queue-handling"
  - "fido2"
---

## Summary
In `components/mod_fido2/src/Fido2Module.cpp:88`, the `onFidoSetReport()` callback silently drops packets when the RX queue is full using `xQueueSend(s_rx_queue, &pkt, 0)` with zero timeout. When the queue is full, the packet is dropped with only a debug log, but no backpressure mechanism signals the USB HID stack to slow down transmission.

**Location:** `components/mod_fido2/src/Fido2Module.cpp:88`

```cpp
static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    // ...
    if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
        LOG_W(TAG, "RX queue full, dropping packet");  // Silent drop
    }
}
```

## Impact
- **Data loss:** CTAPHID packets may be dropped during high-traffic periods, causing FIDO2 operations to fail silently
- **No flow control:** The USB HID stack continues sending packets at full speed even when the queue is saturated
- **User experience:** FIDO2 credential operations (register, authenticate) may fail intermittently under load
- **Debug difficulty:** Silent drops make it hard to diagnose performance issues

## Evidence
File: `components/mod_fido2/src/Fido2Module.cpp`
- Line 88: `xQueueSend(s_rx_queue, &pkt, 0)` - zero timeout means "don't wait"
- Line 89: `LOG_W(TAG, "RX queue full, dropping packet")` - only a warning, no recovery
- Queue size: `FIDO_QUEUE_SIZE = 8` (line 37) - small buffer for burst traffic

The callback is invoked from TinyUSB's HID report handler, which runs in task context (not ISR), but the immediate return and packet drop provides no feedback to the USB stack.

## Recommended Fix
Implement one of the following backpressure strategies:

**Option 1: Blocking send with timeout**
```cpp
// Wait up to 10ms for queue space
TickType_t ticks = pdMS_TO_TICKS(10);
if (xQueueSend(s_rx_queue, &pkt, ticks) != pdTRUE) {
    LOG_W(TAG, "RX queue full, blocking send timed out");
    // Optionally signal USB stack to pause
}
```

**Option 2: Track dropped packets for metrics**
```cpp
static uint32_t s_dropped_packets = 0;
// ...
if (xQueueSend(s_rx_queue, &pkt, 0) != pdTRUE) {
    s_dropped_packets++;
    LOG_W(TAG, "RX queue full, dropping packet (total dropped: %lu)", s_dropped_packets);
}
```

**Option 3: Increase queue size**
```cpp
// Increase from 8 to 16 or 32 to handle burst traffic
static constexpr size_t FIDO_QUEUE_SIZE = 16;
```

**Option 4: Hybrid approach (recommended)**
```cpp
// Use blocking send with timeout, but limit retries
static uint8_t s_consecutive_drops = 0;
TickType_t ticks = pdMS_TO_TICKS(5);
if (xQueueSend(s_rx_queue, &pkt, ticks) != pdTRUE) {
    s_consecutive_drops++;
    if (s_consecutive_drops > 10) {
        LOG_E(TAG, "RX queue saturated, consecutive drops: %d", s_consecutive_drops);
    }
} else {
    s_consecutive_drops = 0;  // Reset on success
}
```

## References
- [FreeRTOS Queue Documentation](https://www.freertos.org/Embedded-RTOS-Queues.html)
- [CTAPHID Protocol Specification](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html)
- ESP32-S3 USB CDC/HID integration patterns in `components/usb_badge/usb_hid.cpp`
