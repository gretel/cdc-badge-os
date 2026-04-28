---
title: "[LOW] USB HID callback may be called before queue is initialized"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "initialization-order"
  - "usb"
  - "callback"
---

## Summary
In `components/mod_fido2/src/Fido2Module.cpp`, the `onFidoSetReport()` callback is registered with the USB manager during `start()`, but the static RX queue `s_rx_queue` is created in `init()`. If the USB stack is initialized before the module's `init()` is called, or if `start()` is called before `init()`, the callback could fire with a null queue, causing silent packet drops.

**Location:** `components/mod_fido2/src/Fido2Module.cpp:84-92`

```cpp
static QueueHandle_t s_rx_queue = nullptr;  // Line 38

static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    (void)report_id;
    (void)report_type;

    if (!s_rx_queue || !buffer || bufsize != CTAPHID_PACKET_SIZE) {  // Line 86
        return;  // Silent return if queue not ready
    }
    // ...
}

bool Fido2Module::init() {
    // ...
    if (!s_rx_queue) {
        s_rx_queue = xQueueCreate(FIDO_QUEUE_SIZE, sizeof(FidoPacket));  // Line 120
        if (!s_rx_queue) {
            LOG_E(TAG, "Failed to create RX queue");
            return false;
        }
    }
    // ...
}

bool Fido2Module::start() {
    // ...
    spec.callbacks.onSetReport = onFidoSetReport;  // Line 172
    // ...
    if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
        // ...
    }
    // ...
}
```

## Impact
- **Race condition:** If USB is ready and sends a packet before `init()` creates the queue, packets are silently dropped
- **Initialization order dependency:** Assumes `init()` is always called before USB stack starts
- **Silent failure:** No warning logged when callback fires with null queue

## Evidence
File: `components/mod_fido2/src/Fido2Module.cpp`
- Line 38: `static QueueHandle_t s_rx_queue = nullptr;`
- Line 86: `if (!s_rx_queue || !buffer || bufsize != CTAPHID_PACKET_SIZE)` - checks for null queue
- Lines 120-128: Queue created in `init()`
- Lines 168-175: Callback registered in `start()`
- Line 285: `mod_fido2_register()` calls `module.init()` then `module.start()` - correct order

However, the USB stack can be triggered asynchronously. If `usb_cdc_start()` is called before `mod_fido2_register()`, or if the USB host sends packets immediately after enumeration, the callback could fire before the queue is ready.

## Recommended Fix
**Option 1: Initialize queue at module registration time**
```cpp
// Move queue creation to registration
extern "C" void mod_fido2_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        // Create queue first
        if (!s_rx_queue) {
            s_rx_queue = xQueueCreate(FIDO_QUEUE_SIZE, sizeof(FidoPacket));
            if (!s_rx_queue) {
                LOG_E(TAG, "Failed to create RX queue");
                return;
            }
        }

        auto& module = cdc::mod_fido2::Fido2Module::instance();
        if (module.init()) {
            module.start();
        }
    });
}

// Simplify init() to just validate queue exists
bool Fido2Module::init() {
    LOG_I(TAG, "Initializing FIDO2 module");

    if (!s_rx_queue) {
        s_rx_queue = xQueueCreate(FIDO_QUEUE_SIZE, sizeof(FidoPacket));
        if (!s_rx_queue) {
            LOG_E(TAG, "Failed to create RX queue");
            return false;
        }
    }
    // ... rest same
}
```

**Option 2: Add callback ready flag**
```cpp
static bool s_callback_ready = false;

static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    if (!s_callback_ready || !s_rx_queue || !buffer || bufsize != CTAPHID_PACKET_SIZE) {
        // Log warning for debugging
        static uint8_t warn_count = 0;
        if (warn_count++ < 5) {
            LOG_W(TAG, "Callback fired before ready (count=%d)", warn_count);
        }
        return;
    }
    // ...
}

bool Fido2Module::start() {
    // ...
    if (!core::UsbManager::instance().registerInterface(core::UsbHidInterface::Fido, getName(), spec)) {
        // ...
        return false;
    }
    
    // Mark callback ready after registration
    s_callback_ready = true;
    // ...
}
```

**Option 3: Use atomic queue check with count**
```cpp
static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    if (!buffer || bufsize != CTAPHID_PACKET_SIZE) {
        return;
    }

    // Use atomic check for queue validity
    QueueHandle_t q = s_rx_queue;
    if (!q) {
        // Queue not ready - drop silently (first few packets may be lost)
        return;
    }

    FidoPacket pkt;
    memcpy(pkt.data, buffer, CTAPHID_PACKET_SIZE);

    if (xQueueSend(q, &pkt, 0) != pdTRUE) {
        LOG_W(TAG, "RX queue full, dropping packet");
    }
}
```

## References
- [FreeRTOS Queue API](https://www.freertos.org/Embedded-RTOS-Queues.html)
- [USB HID Class Specification](https://www.usb.org/document-library/hid-111)
- [Initialization Order Patterns](https://www.embedded.com/design/programming-languages-and-compilers/4024877/Initialization-order-in-embedded-systems)
