---
title: "[MEDIUM] TROPIC01 Secure Element Session Start Blocks Boot Sequence"
severity: MEDIUM
domain: startup-perf
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
In `main/main.cpp:165-170`, the TROPIC01 secure element session is started synchronously during boot. The `sessionStart()` call can take significant time (typically 10-50ms for secure element handshake) and blocks the entire boot sequence. This is called before modules are initialized, meaning all 10 modules wait for this single operation.

**Evidence:**
```cpp
// main/main.cpp:165-170
LOG_I(TAG, "Initializing Secure Element...");
s_secureElement = cdc::hal::getSecureElementInstance();
if (s_secureElement && s_secureElement->init() && s_secureElement->start()) {
    if (s_secureElement->sessionStart()) {  // BLOCKING HERE
        LOG_I(TAG, "Secure Element ready (TROPIC01, session active)");
    } else {
        LOG_W(TAG, "Secure Element initialized but session start failed");
    }
}
```

## Impact
- **Boot Delay**: Each boot waits ~10-50ms for secure element session establishment
- **Serializes Dependencies**: All 10 modules (GPG, FIDO2, TOTP, Password, etc.) depend on this single blocking call
- **No Timeout Handling**: If secure element is slow to respond or momentarily unavailable, boot waits indefinitely
- **User Experience**: Slower perceived boot time, especially relevant for deep-sleep wake scenarios

## Evidence
File: `main/main.cpp` lines 165-180
- `s_secureElement->sessionStart()` is called synchronously
- No timeout or async mechanism
- Session start happens before module initialization (line 224)

Related code in `components/cdc_hal/src/Tropic01Element.cpp` shows the session start involves SPI communication with the TROPIC01 chip, which can be variable in timing.

## Recommended Fix
Defer the secure element session start to lazy initialization:

1. **Initialize but don't start session at boot:**
   ```cpp
   // In main.cpp, change to:
   if (s_secureElement && s_secureElement->init() && s_secureElement->start()) {
       LOG_I(TAG, "Secure Element ready (TROPIC01, session on-demand)");
       // Don't call sessionStart() here - defer to first use
   }
   ```

2. **Add lazy session start to TROPIC01Element:**
   ```cpp
   // In Tropic01Element.h:
   bool ensureSession();  // Start session if not active, with timeout
   
   // In Tropic01Element.cpp:
   bool Tropic01Element::ensureSession() {
       if (isSessionActive()) return true;
       // Add timeout logic
       uint32_t start = esp_timer_get_time();
       while (!isSessionActive()) {
           if ((esp_timer_get_time() - start) > 100000) {  // 100ms timeout
               return false;
           }
           vTaskDelay(pdMS_TO_TICKS(5));
       }
       return true;
   }
   ```

3. **Update modules to use `ensureSession()` on first access:**
   - GPG module: Before first ECC operation
   - FIDO2 module: Before first credential operation
   - TOTP module: Before first R-MEM read

This change allows boot to complete while the secure element session is established in the background or on first use.

## References
- ESP32-S3 TROPIC01 integration typically shows 10-50ms session establishment time
- Similar pattern already used in codebase: USB CDC is initialized (`usb_cdc_init()`) but not started (`usb_cdc_start()`) until line 233
- FreeRTOS task notification for async secure element ready signal could further improve this
