---
title: "[MEDIUM] No throughput metrics for operations per second"
severity: MEDIUM
domain: observability/metrics
lens: throughput-metrics
labels:
  - "metrics"
  - "observability"
  - "throughput"
---

## Summary
The firmware has no throughput metrics to track operations per second. There is no way to measure the rate of FIDO2 authentications, TOTP generations, PIN verifications, or queue processing. This makes capacity planning and load monitoring impossible.

**Evidence:**
- `components/mod_fido2/src/Fido2Module.cpp` - CTAPHID packet processing with no rate tracking
- `components/mod_totp/src/TotpModule.cpp` - Code generation with no throughput metric
- `components/cdc_core/src/PinManager.cpp` - PIN verifications with no rate counter
- `components/cdc_log/src/cdc_log.cpp` - Log writes with no throughput metric
- No rate counters for HID packets, serial commands, or UI interactions

## Impact
Without throughput metrics:
- **Capacity planning blind**: Cannot determine max operations per second
- **Load monitoring missing**: Cannot detect sudden spikes or drops in activity
- **Performance baselines unknown**: Cannot establish normal vs abnormal rates
- **Scaling decisions hard**: No data to inform hardware or optimization needs

## Evidence
**FIDO2 processing** (`components/mod_fido2/src/Fido2Module.cpp`):
```cpp
static void onFidoSetReport(uint8_t report_id, uint8_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {
    // ... processes CTAPHID packet ...
    xQueueSend(s_rx_queue, &pkt, 0);
    // No rate tracking
}
```

**TOTP generation** (`components/mod_totp/src/TotpModule.cpp:520`):
```cpp
void updateCode() {
    int8_t rem = store.generateCode(slot_, code_);
    // No throughput metric
}
```

**Serial command processing** (`components/serial_cmd/src/SerialCmd.cpp`):
```cpp
void SerialCmd::process() {
    // ... processes commands ...
    // No rate tracking per command type
}
```

**Main loop** (`main/main.cpp:240`):
```cpp
while (true) {
    EventBus::instance().process();
    SerialCmd::process();
    ui_process(nowMs);
    // No throughput metrics for these operations
}
```

## Recommended Fix
1. **Add rate tracking API** (add to `components/cdc_metrics/include/cdc_metrics.h`):
   ```cpp
   // Rate tracking (counts per interval)
   #define METRIC_FIDO_AUTH_RATE "fido_auth_per_second"
   #define METRIC_TOTP_GEN_RATE "totp_gen_per_second"
   #define METRIC_PIN_VERIFY_RATE "pin_verify_per_second"
   #define METRIC_SERIAL_CMD_RATE "serial_cmd_per_second"
   
   // Simple rate counter
   void metrics_rate_increment(const char* name);
   float metrics_get_rate(const char* name);  // Returns ops/second
   ```

2. **Implement rate calculation** (`components/cdc_metrics/src/MetricsCollector.cpp`):
   ```cpp
   struct RateCounter {
       uint32_t count = 0;
       uint32_t lastResetMs = 0;
       float rate = 0.0f;
   };
   
   static RateCounter s_rates[10];  // Array of rate counters
   
   void metrics_rate_increment(const char* name) {
       // Find or create counter, increment
       RateCounter* c = findCounter(name);
       c->count++;
   }
   
   float metrics_get_rate(const char* name) {
       RateCounter* c = findCounter(name);
       uint32_t now = esp_timer_get_time() / 1000;
       
       // Calculate rate every second
       if (now - c->lastResetMs >= 1000) {
           c->rate = (float)c->count / ((now - c->lastResetMs) / 1000.0f);
           c->count = 0;
           c->lastResetMs = now;
       }
       return c->rate;
   }
   ```

3. **Instrument operations**:
   ```cpp
   // FIDO2 (`components/mod_fido2/src/Fido2Module.cpp`)
   static void onFidoSetReport(...) {
       metrics_rate_increment(METRIC_FIDO_AUTH_RATE);
       // ... rest of processing ...
   }
   
   // TOTP (`components/mod_totp/src/TotpModule.cpp`)
   void updateCode() {
       metrics_rate_increment(METRIC_TOTP_GEN_RATE);
       // ... rest of processing ...
   }
   
   // PIN (`components/cdc_core/src/PinManager.cpp`)
   bool verifyBadgePin(const char* pin) {
       metrics_rate_increment(METRIC_PIN_VERIFY_RATE);
       // ... rest of processing ...
   }
   ```

4. **Export in METRICS command**:
   ```
   fido_auth_per_second 5.2
   totp_gen_per_second 12.0
   pin_verify_per_second 3.5
   serial_cmd_per_second 8.0
   ```

5. **Add rolling window for historical rates** (optional):
   ```cpp
   // Track rates over longer periods
   #define METRIC_FIDO_AUTH_RATE_1MIN "fido_auth_per_minute"
   #define METRIC_FIDO_AUTH_RATE_1HOUR "fido_auth_per_hour"
   ```

## References
- [Prometheus rate() function](https://prometheus.io/docs/prometheus/latest/querying/operators/#rate)
- [Counter vs Rate](https://prometheus.io/docs/practices/counters/#rate)
- ESP timer: `esp_timer_get_time()` for timing
- Existing implementation locations:
  - FIDO2: `components/mod_fido2/src/Fido2Module.cpp`
  - TOTP: `components/mod_totp/src/TotpModule.cpp`
  - PIN: `components/cdc_core/src/PinManager.cpp`

</content>