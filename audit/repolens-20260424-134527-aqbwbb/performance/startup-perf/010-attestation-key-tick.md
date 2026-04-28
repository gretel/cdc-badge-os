---
title: "[LOW] Attestation key check deferred but runs on main loop"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The `AttestationKeyService` uses a deferred initialization pattern (good!), but its `onTick()` method runs on the **main loop** and performs blocking secure-element operations. This can cause intermittent stalls during normal operation.

**Location:** `components/cdc_core/src/AttestationKeyService.cpp:50-60`

## Impact
- **Intermittent blocking**: Every 3 seconds, the main loop may stall for secure-element access
- **UI lag**: Display updates and keypad scanning may be delayed
- **Unpredictable timing**: User experiences random pauses

## Evidence
From `components/cdc_core/src/AttestationKeyService.cpp:50-60`:
```cpp
void AttestationKeyService::onTick(uint32_t nowMs) {
    if (state_ != ServiceState::STARTED || ready_) return;
    if (nowMs - lastAttemptMs_ < RETRY_INTERVAL_MS) return;
    lastAttemptMs_ = nowMs;
    if (ensureKey()) {
        ready_ = true;
        LOG_I(TAG, "Attestation key ready");
    }
}
```

Called from main loop (`main/main.cpp:246`):
```cpp
while (true) {
    EventBus::instance().process();
    cdc::serial::SerialCmd::process();

    if (s_powerManager) {
        s_powerManager->update();
    }

    uint32_t nowMs = esp_timer_get_time() / 1000;
    cdc::ui::ui_process(nowMs);
    s_attestationService.onTick(nowMs);  // Runs in main loop!

    // Dispatch tick to all modules
    cdc::core::ModuleRegistry::instance().dispatchTick(nowMs);

    vTaskDelay(pdMS_TO_TICKS(10));
}
```

From `ensureKey()`:
```cpp
bool AttestationKeyService::ensureKey() {
    // ...
    hal::SeResult res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
    // ... SPI read from TROPIC01 (~5-10ms) ...
    
    if (res == hal::SeResult::SLOT_EMPTY) {
        // Generate key (~50-100ms!)
        secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256);
    }
    // ...
}
```

## Recommended Fix
**Run attestation key check in a dedicated task**:

```cpp
class AttestationKeyService {
    TaskHandle_t checkTask_ = nullptr;
    
    bool init() {
        // ... existing init ...
        xTaskCreate(attestationCheckTask, "attest", 2048, this, 5, &checkTask_);
    }
    
    static void attestationCheckTask(void* arg) {
        auto* self = static_cast<AttestationKeyService*>(arg);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(3000));  // 3 second interval
            self->ensureKey();  // Runs in dedicated task
        }
    }
};
```

**Alternative**: Move to background priority
```cpp
// In main loop, run at lower priority
void mainLoop() {
    while (true) {
        // ... high-priority work ...
        
        // Process attestation at end of loop
        s_attestationService.onTick(nowMs);  // Already at low frequency
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

**Alternative**: Cache with longer interval
```cpp
// Only check at boot, then once per day
static constexpr uint32_t CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000;  // 24 hours
```

## References
- ESP-IDF FreeRTOS: [Task priorities](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos.html#task-priority)
- TROPIC01: [ECC generation time](https://www.tropicdevices.com/) - ~100ms for key generation
