---
title: "[LOW] No storage capacity metrics for secure element slots"
severity: LOW
domain: observability/metrics
lens: storage-metrics
labels:
  - "metrics"
  - "observability"
  - "storage"
---

## Summary
The firmware uses TROPIC01 secure element slots (ECC: 0-31, R-Memory: 0-511) but has no metrics tracking slot utilization. Users cannot query how many slots are used per module or predict when storage will be exhausted.

**Evidence:**
- `components/cdc_core/include/cdc_core/TropicStorage.h` - Storage management but no capacity metrics
- `components/cdc_core/src/TropicStorage.cpp` - Slot iteration but no utilization tracking
- `components/serial_cmd/src/SerialCmd.cpp:965-988` - `cmdTr01Slots()` shows slot usage but only on-demand, no metrics
- Module slot allocation in `components/cdc_core/include/cdc_core/IModule.h:53-70` but no tracking

## Impact
Without storage metrics:
- **Capacity planning missing**: Cannot predict when slots will be full
- **Module conflicts unclear**: Hard to debug slot allocation issues
- **User experience**: Users must manually check slots via serial command
- **No proactive alerts**: Cannot alert before storage exhaustion

## Evidence
**Slot allocation** (`components/cdc_core/include/cdc_core/IModule.h`):
```cpp
struct SlotRequest {
    const char* mapName = nullptr;
    uint8_t minEccSlots = 0;
    uint16_t minRmemSlots = 0;
};

struct SlotRange {
    bool hasEcc = false;
    bool hasRmem = false;
    uint8_t eccStart = 0;
    uint8_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t moduleId = 0;
};
```

**Manual slot check** (`components/serial_cmd/src/SerialCmd.cpp:965-988`):
```cpp
static void cmdTr01Slots(const char* args) {
    // Shows ECC and R-Memory slot usage
    // Only available via serial command
    // No metrics export
}
```

**Module slot usage** (from README):
| Module | ECC Slots | R-Memory Slots |
|--------|-----------|----------------|
| SYSTEM | 0 (Attestation) | 0 (PINs) |
| GPG | 1-3 | 1-3 |
| CA | 4 | 4 |
| FIDO2 | 5-31 | 5-31 |
| TOTP | - | 32-131 |
| Password | - | 150-511 |

## Recommended Fix
1. **Add storage metrics** (`components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   // Storage metrics
   #define METRIC_ECC_SLOTS_USED "ecc_slots_used"
   #define METRIC_ECC_SLOTS_TOTAL "ecc_slots_total"
   #define METRIC_RMEM_SLOTS_USED "rmem_slots_used"
   #define METRIC_RMEM_SLOTS_TOTAL "rmem_slots_total"
   #define METRIC_ECC_SLOTS_BY_MODULE "ecc_slots_by_module"
   #define METRIC_RMEM_SLOTS_BY_MODULE "rmem_slots_by_module"
   ```

2. **Create storage metrics collector** (`components/cdc_metrics/src/StorageMetrics.cpp`):
   ```cpp
   void collectStorageMetrics() {
       auto& storage = core::TropicStorage::instance();
       auto* se = hal::getSecureElementInstance();
       
       // Count used slots
       uint8_t eccUsed = 0;
       uint16_t rmemUsed = 0;
       
       for (uint8_t i = 0; i < hal::ISecureElement::ECC_SLOT_COUNT; i++) {
           if (se->eccSlotUsed(i)) eccUsed++;
       }
       
       for (uint16_t i = 0; i < hal::ISecureElement::RMEM_SLOT_COUNT; i++) {
           if (se->rmemSlotUsed(i)) rmemUsed++;
       }
       
       metrics_set_gauge(METRIC_ECC_SLOTS_USED, eccUsed);
       metrics_set_gauge(METRIC_ECC_SLOTS_TOTAL, hal::ISecureElement::ECC_SLOT_COUNT);
       metrics_set_gauge(METRIC_RMEM_SLOTS_USED, rmemUsed);
       metrics_set_gauge(METRIC_RMEM_SLOTS_TOTAL, hal::ISecureElement::RMEM_SLOT_COUNT);
   }
   ```

3. **Add to metrics collector task** (run every 60 seconds):
   ```cpp
   // In MetricsCollector.cpp
   collectStorageMetrics();
   ```

4. **Export in METRICS command**:
   ```
   ecc_slots_used 15
   ecc_slots_total 32
   rmem_slots_used 200
   rmem_slots_total 512
   ecc_slots_by_module{module="fido2"} 10
   ecc_slots_by_module{module="gpg"} 3
   rmem_slots_by_module{module="totp"} 100
   rmem_slots_by_module{module="password"} 100
   ```

## References
- Secure element: `components/cdc_hal/include/cdc_hal/ISecureElement.h`
- Storage management: `components/cdc_core/include/cdc_core/TropicStorage.h`
- Slot allocation: `main/tropic_slot_map.h` (if exists) or module registration
