---
title: "[LOW] WiFi reconnection uses fixed retry count without exponential backoff"
severity: LOW
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "wifi"
  - "retry"
  - "backoff"
---

## Summary
The WiFi controller (`components/cdc_hal/src/WifiController.cpp`) implements a simple retry mechanism for reconnection but uses a fixed maximum retry count (5) without exponential backoff between attempts. This can lead to rapid reconnection attempts that may overwhelm the WiFi stack or access point.

**Evidence:**
- File: `components/cdc_hal/src/WifiController.cpp:94-96` - Retry configuration:
  ```cpp
  uint8_t retryCount_ = 0;
  static constexpr uint8_t MAX_RETRY = 5;
  ```

- File: `components/cdc_hal/src/WifiController.cpp:670-680` - Simple retry without backoff:
  ```cpp
  case WIFI_EVENT_STA_DISCONNECTED: {
      wifiState_ = WifiState::DISCONNECTED;
      if (retryCount_ < MAX_RETRY) {
          retryCount_++;
          LOG_I(TAG, "Reconnecting (attempt %d)", retryCount_);
          esp_wifi_connect();  // Immediate retry, no delay
      } else {
          xEventGroupSetBits(eventGroup_, WIFI_FAIL_BIT);
          wifiState_ = WifiState::CONNECTION_FAILED;
      }
      break;
  }
  ```

Note: The retry happens immediately via `esp_wifi_connect()` with no delay between attempts.

## Impact
- **Access point stress**: Rapid reconnection attempts (potentially within milliseconds) can stress the AP
- **WiFi stack contention**: Multiple immediate retries may conflict with WiFi driver state machine
- **Power efficiency**: Rapid retries consume more power than spaced attempts

## Recommended Fix
Add exponential backoff between WiFi reconnection attempts:

1. Add backoff tracking:
   ```cpp
   uint8_t retryCount_ = 0;
   uint32_t lastRetryTimeMs_ = 0;
   static constexpr uint8_t MAX_RETRY = 5;
   static constexpr uint32_t MIN_RETRY_DELAY_MS = 100;
   static constexpr uint32_t MAX_RETRY_DELAY_MS = 2000;
   ```

2. Modify retry logic in `onWifiEvent`:
   ```cpp
   case WIFI_EVENT_STA_DISCONNECTED: {
       wifiState_ = WifiState::DISCONNECTED;
       if (retryCount_ < MAX_RETRY) {
           uint32_t nowMs = esp_timer_get_time() / 1000;
           uint32_t elapsed = nowMs - lastRetryTimeMs_;
           uint32_t backoff = min(MIN_RETRY_DELAY_MS * (1 << retryCount_), MAX_RETRY_DELAY_MS);
           
           if (elapsed >= backoff) {
               retryCount_++;
               lastRetryTimeMs_ = nowMs;
               LOG_I(TAG, "Reconnecting (attempt %d, backoff %lu ms)", retryCount_, (unsigned long)backoff);
               esp_wifi_connect();
           }
       } else {
           xEventGroupSetBits(eventGroup_, WIFI_FAIL_BIT);
           wifiState_ = WifiState::CONNECTION_FAILED;
       }
       break;
   }
   ```

## References
- WiFi reconnection best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html
- Exponential backoff patterns: https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/
