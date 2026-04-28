---
title: "[MEDIUM] FIDO2 USB response retry lacks exponential backoff"
severity: MEDIUM
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "fido2"
  - "usb"
  - "retry"
  - "backoff"
---

## Summary
The FIDO2 transport layer (`components/mod_fido2/src/fido2.cpp`) implements retry logic for USB writes but uses fixed-interval delays without exponential backoff or jitter. This can cause retry storms when USB is temporarily congested.

**Evidence:**
- File: `components/mod_fido2/src/fido2.cpp:88-110` - Fixed-interval retry loop:
  ```cpp
  int retry_count = 0;
  while (ctaphid_has_response()) {
      if (fido2_usb_ready()) {
          // ... send response ...
          retry_count = 0;
          vTaskDelay(pdMS_TO_TICKS(1));
      } else {
          retry_count++;
          if (retry_count > 100) {  // ~1 second timeout
              LOG_W("FIDO2", "USB not ready timeout, aborting response");
              break;
          }
          vTaskDelay(pdMS_TO_TICKS(10));  // Fixed 10ms delay
      }
  }
  ```

- File: `components/mod_fido2/src/fido2.cpp:58-78` - Another fixed-interval loop:
  ```cpp
  int inner_retry = 0;
  while (ctaphid_has_response()) {
      if (fido2_usb_ready()) {
          // ...
          inner_retry = 0;
          vTaskDelay(pdMS_TO_TICKS(1));
      } else {
          inner_retry++;
          if (inner_retry > 10) {
              LOG_D("FIDO2", "USB not ready, deferring to outer loop");
              break;
          }
          vTaskDelay(pdMS_TO_TICKS(5));  // Fixed 5ms delay
      }
  }
  ```

## Impact
- **Retry storms**: Fixed 5-10ms delays can overwhelm USB when it's recovering
- **No jitter**: Synchronized retries from multiple operations cause contention
- **Inefficient**: Linear retry pattern doesn't adapt to USB congestion level

## Recommended Fix
Implement exponential backoff with jitter for USB retry logic:

1. Add backoff constants:
   ```cpp
   static constexpr uint8_t USB_MAX_RETRY = 100;
   static constexpr uint32_t USB_MIN_DELAY_MS = 5;
   static constexpr uint32_t USB_MAX_DELAY_MS = 100;
   ```

2. Replace fixed delays with exponential backoff:
   ```cpp
   uint32_t delayMs = USB_MIN_DELAY_MS;
   int retry_count = 0;
   while (ctaphid_has_response()) {
       if (fido2_usb_ready()) {
           // ... send response ...
           retry_count = 0;
           delayMs = USB_MIN_DELAY_MS;  // Reset backoff on success
           vTaskDelay(pdMS_TO_TICKS(1));
       } else {
           retry_count++;
           if (retry_count > USB_MAX_RETRY) {
               LOG_W("FIDO2", "USB not ready timeout, aborting response");
               break;
           }
           vTaskDelay(pdMS_TO_TICKS(delayMs));
           // Exponential backoff with jitter
           delayMs = min(delayMs * 2 + (esp_random() % 10), USB_MAX_DELAY_MS);
       }
   }
   ```

## References
- Exponential backoff patterns: https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/
- FIDO2 specification timeout requirements: https://fidoalliance.org/specs/fido-v2.0-rd-20180330/fido-client-to-authenticator-protocol-v2.0-rd-20180330.html
