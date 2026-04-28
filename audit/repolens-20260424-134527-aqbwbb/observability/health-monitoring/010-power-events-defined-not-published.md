---
title: "[MEDIUM] Power events defined but never published"
severity: MEDIUM
domain: observability
lens: health-monitoring
labels:
  - "audit:observability/health-monitoring"
---

## Summary

The `EventBus` defines multiple power-related event types (`POWER_USB_CONNECTED`, `POWER_USB_DISCONNECTED`, `POWER_CHARGING`, `POWER_BATTERY_LOW`, `POWER_BATTERY_CRITICAL`) but **none of these events are ever published** in the codebase.

**Event types defined** (`components/cdc_core/include/cdc_core/EventBus.h:18-22`):
```cpp
enum class EventType : uint8_t {
    // Power events
    POWER_USB_CONNECTED,
    POWER_USB_DISCONNECTED,
    POWER_CHARGING,
    POWER_BATTERY_LOW,
    POWER_BATTERY_CRITICAL,
    // ...
};
```

**Evidence of missing publications:**
- Only `MODULE_ERROR` event is published (in `ModuleRegistry.cpp:689`)
- `grep` search for `EventType::POWER` returns **0 matches**
- The `BQ25895Power::update()` method detects state changes but never publishes events

**Current power state tracking** (`components/cdc_hal/src/BQ25895Power.cpp:568-582`):
```cpp
void BQ25895Power::update() {
    // Proactive watchdog kick every 30s
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
    if ((nowMs - lastWdtKickMs_) >= 30000) {
        lastWdtKickMs_ = nowMs;
        kickWatchdog();
    }

    // Handle charger IRQ
    if (charger_irq_pending) {
        charger_irq_pending = false;
        readChargerStatus();  // Updates cached state but no events published
    }
}
```

## Impact

1. **No event-driven power monitoring**: Components that should react to power changes (e.g., save state on USB disconnect, reduce brightness on low battery) cannot do so automatically
2. **Missing observability signals**: External monitoring scripts cannot subscribe to power state changes
3. **Incomplete health dashboard**: Power status is only available via polling (STATUS command), not real-time events
4. **Lost opportunity for automation**: Features like "auto-save before deep sleep on low battery" require manual polling

## Evidence

**Definition location:**
- `components/cdc_core/include/cdc_core/EventBus.h:18-22` - Event types defined

**Power manager implementation:**
- `components/cdc_hal/src/BQ25895Power.cpp:568-582` - `update()` method detects changes
- `components/cdc_hal/src/BQ25895Power.cpp:128-136` - Cached state variables (no event publishing)

**Search results:**
```bash
grep -r "EventType::POWER" components/  # 0 matches
grep -r "POWER_USB_CONNECTED" components/  # 0 matches (only definition in EventBus.h)
```

**Event publishing locations (only MODULE_ERROR is published):**
- `components/cdc_core/src/ModuleRegistry.cpp:688-691` - Only `MODULE_ERROR` event

## Recommended Fix

Add event publishing to the `BQ25895Power::update()` method to notify subscribers of power state changes:

```cpp
#include "cdc_core/EventBus.h"

void BQ25895Power::update() {
    // Proactive watchdog kick every 30s
    uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000);
    if ((nowMs - lastWdtKickMs_) >= 30000) {
        lastWdtKickMs_ = nowMs;
        kickWatchdog();
    }

    // Handle charger IRQ
    if (charger_irq_pending) {
        charger_irq_pending = false;
        readChargerStatus();
    }

    // Publish power state change events
    bool usbConnected = isUsbConnected();
    if (usbConnected != prevUsbConnected_) {
        prevUsbConnected_ = usbConnected;
        auto evt = cdc::core::EventType::POWER_USB_CONNECTED;
        if (!usbConnected) evt = cdc::core::EventType::POWER_USB_DISCONNECTED;
        cdc::core::EventBus::instance().publish(evt);
        LOG_I(TAG, "Power event: USB %s", usbConnected ? "connected" : "disconnected");
    }

    ChargeStatus status = getChargeStatus();
    if (status != prevChargeStatus_) {
        prevChargeStatus_ = status;
        auto evt = cdc::core::EventType::POWER_CHARGING;
        cdc::core::EventBus::instance().publish(evt);
        LOG_I(TAG, "Power event: charging status changed");
    }

    if (isBatteryLow() && !prevBatteryLow_) {
        prevBatteryLow_ = true;
        cdc::core::EventBus::instance().publish(cdc::core::EventType::POWER_BATTERY_LOW);
        LOG_W(TAG, "Power event: battery low");
    }

    if (isBatteryCritical() && !prevBatteryCritical_) {
        prevBatteryCritical_ = true;
        cdc::core::EventBus::instance().publish(cdc::core::EventType::POWER_BATTERY_CRITICAL);
        LOG_W(TAG, "Power event: battery critical");
    }
}
```

**Add state tracking variables** to `BQ25895Power` class (private section):
```cpp
bool prevUsbConnected_ = false;
ChargeStatus prevChargeStatus_ = ChargeStatus::NOT_CHARGING;
bool prevBatteryLow_ = false;
bool prevBatteryCritical_ = false;
```

**Update `readChargerStatus()`** to also update the cached values that trigger events.

## References

- Event-driven architecture patterns: https://www.enterpriseintegrationpatterns.com/patterns/messaging/EventDrivenArchitecture.html
- ESP32 power management: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/power_management.html
- BQ25895 datasheet: https://www.ti.com/lit/ds/symlink/bq25895.pdf

</content>