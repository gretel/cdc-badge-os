---
title: "[MEDIUM] WiFi reconnection uses fixed delay without exponential backoff"
severity: MEDIUM
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "wifi"
  - "retry"
  - "backoff"
  - "connection"
---

## Summary
The WiFi controller (`components/cdc_hal/src/WifiController.cpp`) uses a fixed retry count (5 attempts) with no exponential backoff when reconnecting after disconnection. This can cause rapid reconnection attempts that may overwhelm the AP or WiFi stack.

**Evidence:**

1. File: `components/cdc_hal/src/WifiController.cpp:99-100` - Fixed retry count without backoff:
   ```cpp
   uint8_t retryCount_ = 0;
   static constexpr uint8_t MAX_RETRY = 5;
   ```

2. File: `components/cdc_hal/src/WifiController.cpp:667-676` - Reconnection logic with no delay between attempts:
   ```cpp
   case WIFI_EVENT_STA_DISCONNECTED: {
       wifiState_ = WifiState::DISCONNECTED;
       if (retryCount_ < MAX_RETRY) {
           retryCount_++;
           LOG_I(TAG, "Reconnecting (attempt %d)", retryCount_);
           esp_wifi_connect();  // Immediate reconnection!
       } else {
           xEventGroupSetBits(eventGroup_, WIFI_FAIL_BIT);
           wifiState_ = WifiState::CONNECTION_FAILED;
       }
       break;
   }
   ```

3. File: `components/cdc_hal/src/WifiController.cpp:419-425` - Connection timeout but no backoff:
   ```cpp
   // Wait for connection result
   EventBits_t bits = xEventGroupWaitBits(eventGroup_,
                                          WIFI_GOT_IP_BIT | WIFI_FAIL_BIT,
                                          pdFALSE, pdFALSE,
                                          pdMS_TO_TICKS(timeoutMs));
   ```

## Impact
- **Retry storms**: Rapid reconnection attempts can trigger AP rate-limiting
- **WiFi stack stress**: Continuous immediate retries can congest the WiFi stack
- **Power inefficiency**: Unnecessary radio activity drains battery
- **AP disorganization**: Some APs may ban clients that retry too aggressively
- **User experience**: Device may appear stuck retrying instead of reporting failure

## Recommended Fix
Add exponential backoff to WiFi reconnection logic:

1. Add backoff constants:
   ```cpp
   static constexpr uint8_t MAX_RETRY = 5;
   static constexpr uint32_t INITIAL_BACKOFF_MS = 500;
   static constexpr uint32_t MAX_BACKOFF_MS = 8000;
   static constexpr float BACKOFF_MULTIPLIER = 2.0f;
   ```

2. Add backoff state tracking:
   ```cpp
   private:
       uint8_t retryCount_ = 0;
       uint32_t currentBackoffMs_ = INITIAL_BACKOFF_MS;
   ```

3. Modify reconnection logic:
   ```cpp
   case WIFI_EVENT_STA_DISCONNECTED: {
       wifiState_ = WifiState::DISCONNECTED;
       if (retryCount_ < MAX_RETRY) {
           retryCount_++;
           LOG_I(TAG, "Reconnecting (attempt %d, backoff %lu ms)", 
                 retryCount_, (unsigned long)currentBackoffMs_);
           
           // Wait with exponential backoff
           vTaskDelay(pdMS_TO_TICKS(currentBackoffMs_));
           
           // Double backoff for next attempt (capped at max)
           currentBackoffMs_ = MIN(currentBackoffMs_ * BACKOFF_MULTIPLIER, MAX_BACKOFF_MS);
           
           esp_wifi_connect();
       } else {
           xEventGroupSetBits(eventGroup_, WIFI_FAIL_BIT);
           wifiState_ = WifiState::CONNECTION_FAILED;
           currentBackoffMs_ = INITIAL_BACKOFF_MS;  // Reset for next connection
       }
       break;
   }
   ```

4. Reset backoff on successful connection:
   ```cpp
   case IP_EVENT_STA_GOT_IP: {
       auto* event = (ip_event_got_ip_t*)eventData;
       currentIp_ = event->ip_info.ip;
       wifiState_ = WifiState::GOT_IP;
       retryCount_ = 0;
       currentBackoffMs_ = INITIAL_BACKOFF_MS;  // Reset backoff
       xEventGroupSetBits(eventGroup_, WIFI_GOT_IP_BIT);
       LOG_I(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
       break;
   }
   ```

5. Optional: Add jitter to prevent synchronized retries
   ```cpp
   // Add 0-25% random jitter to backoff
   uint32_t jitter = (esp_random() % (currentBackoffMs_ / 4));
   vTaskDelay(pdMS_TO_TICKS(currentBackoffMs_ + jitter));
   ```

## References
- Exponential backoff patterns: https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/
- WiFi reconnection best practices: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html
- IEEE 802.11 reconnection guidelines

</content>