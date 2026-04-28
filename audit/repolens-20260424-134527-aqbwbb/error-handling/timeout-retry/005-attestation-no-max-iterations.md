---
title: "[LOW] Attestation key service retry has no maximum iteration limit"
severity: LOW
domain: error-handling/timeout-retry
lens: timeout-retry
labels:
  - "attestation"
  - "retry"
  - "secure-element"
---

## Summary
The `AttestationKeyService` (`components/cdc_core/src/AttestationKeyService.cpp`) retries key generation on each tick but has no maximum iteration limit. If the secure element consistently fails (e.g., hardware issue), the service will retry indefinitely on every 3-second tick.

**Evidence:**
- File: `components/cdc_core/src/AttestationKeyService.cpp:11-12` - Retry interval but no max attempts:
  ```cpp
  static constexpr uint32_t RETRY_INTERVAL_MS = 3000;
  ```

- File: `components/cdc_core/src/AttestationKeyService.cpp:47-58` - Retry loop without max iterations:
  ```cpp
  void AttestationKeyService::onTick(uint32_t nowMs) {
      if (state_ != ServiceState::STARTED || ready_) return;
      if (nowMs - lastAttemptMs_ < RETRY_INTERVAL_MS) return;
      lastAttemptMs_ = nowMs;
      if (ensureKey()) {
          ready_ = true;
          LOG_I(TAG, "Attestation key ready");
      }
      // No attempt counter, no max limit - retries forever if ensureKey() fails
  }
  ```

- File: `components/cdc_core/src/AttestationKeyService.cpp:99-161` - `ensureKey()` can fail for various reasons:
  ```cpp
  bool AttestationKeyService::ensureKey() {
      if (!secureElement_) {
          LOG_W(TAG, "Secure element not set");
          return false;  // Will retry forever
      }
      if (!secureElement_->isSessionActive()) {
          if (!secureElement_->sessionStart()) {
              LOG_W(TAG, "Secure element session not active");
              return false;  // Will retry forever
          }
      }
      // ... multiple other failure points ...
  }
  ```

## Impact
- **Resource waste**: Continuous retry attempts consume CPU and secure element resources
- **Error masking**: Persistent failures are never surfaced as permanent errors
- **Debugging difficulty**: No clear indication when a failure becomes permanent

## Recommended Fix
Add attempt tracking and maximum iteration limit:

1. Add attempt counter and max constant:
   ```cpp
   static constexpr uint32_t RETRY_INTERVAL_MS = 3000;
   static constexpr uint8_t MAX_ATTESTATION_ATTEMPTS = 10;  // ~30 seconds
   ```

2. Add state tracking:
   ```cpp
   private:
       ServiceState state_ = ServiceState::UNINITIALIZED;
       bool ready_ = false;
       uint32_t lastAttemptMs_ = 0;
       uint8_t attemptCount_ = 0;  // Track attempts
   ```

3. Modify `onTick()` to enforce max attempts:
   ```cpp
   void AttestationKeyService::onTick(uint32_t nowMs) {
       if (state_ != ServiceState::STARTED || ready_) return;
       if (nowMs - lastAttemptMs_ < RETRY_INTERVAL_MS) return;
       
       lastAttemptMs_ = nowMs;
       attemptCount_++;
       
       if (ensureKey()) {
           ready_ = true;
           LOG_I(TAG, "Attestation key ready");
       } else if (attemptCount_ >= MAX_ATTESTATION_ATTEMPTS) {
           LOG_E(TAG, "Max attestation attempts reached, service may be stuck");
           // Optionally publish error event or set error state
       }
   }
   ```

4. Add reset mechanism (e.g., via serial command or power cycle)

## References
- Retry patterns with bounded attempts: https://docs.aws.amazon.com/sdk-for-java/v1/developer-guide/retries-backoff.html
- ESP32 secure element best practices
