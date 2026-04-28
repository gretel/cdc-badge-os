---
title: "[LOW] No dashboard definitions (Grafana/Datadog) checked in"
severity: LOW
domain: observability/metrics
lens: dashboard-as-code
labels:
  - "metrics"
  - "observability"
  - "dashboard"
---

## Summary
The repository has no dashboard definitions checked in. There are no Grafana JSON files, Datadog monitors-as-code YAML, or any other dashboard-as-code definitions. This means dashboards would need to be manually recreated for each deployment environment.

**Evidence:**
- No `*.json` files in `docs/` or `dashboards/` directories
- No Grafana dashboard definitions found
- No Datadog monitor YAML files found
- No documentation of which metrics should be displayed on dashboards
- No alerting rules defined alongside metrics

## Impact
Without dashboard definitions:
- **Manual dashboard setup**: Each developer/ops person must recreate dashboards manually
- **Inconsistent monitoring**: Different teams may create different dashboards
- **No dashboard versioning**: Cannot track changes to dashboard layout or queries
- **Onboarding friction**: New team members don't know what to monitor
- **Alerts missing**: No alerting rules to define thresholds

## Evidence
**Directory structure** (from exploration):
```
/input/20260423-132359-oj8ayc/cdc-badge-os/
├── docs/              # No dashboard files
├── components/        # No dashboard definitions
└── main/             # No dashboard definitions
```

**No dashboard files found**:
- `glob("**/grafana*.json")` → 0 results
- `glob("**/dashboard*.yaml")` → 0 results
- `glob("**/*.json")` → Only library.json and test data files

**No documentation of metrics**:
- No `docs/metrics.md` or similar
- No README section explaining available metrics
- No alert threshold documentation

## Recommended Fix
1. **Create dashboards directory**:
   ```
   dashboards/
   ├── README.md              # Dashboard documentation
   ├── badge-overview.json    # Main overview dashboard
   ├── badge-alerts.yaml      # Alert definitions
   └── prometheus-rules.yaml  # Recording rules
   ```

2. **Create main dashboard** (`dashboards/badge-overview.json`):
   ```json
   {
     "dashboard": {
       "title": "CDC Badge OS Overview",
       "panels": [
         {
           "title": "Memory Usage",
           "targets": [
             { "expr": "heap_free_bytes" },
             { "expr": "psram_free_bytes" }
           ]
         },
         {
           "title": "Battery Level",
           "targets": [
             { "expr": "battery_percent" }
           ]
         },
         {
           "title": "Operations per Second",
           "targets": [
             { "expr": "fido_auth_per_second" },
             { "expr": "totp_gen_per_second" }
           ]
         },
         {
           "title": "Error Rate",
           "targets": [
             { "expr": "rate(errors_total[5m])" }
           ]
         }
       ]
     }
   }
   ```

3. **Create alert definitions** (`dashboards/badge-alerts.yaml`):
   ```yaml
   groups:
   - name: cdc-badge
     rules:
     - alert: LowMemory
       expr: heap_free_bytes < 50000
       for: 5m
       labels:
         severity: warning
       annotations:
         summary: "Low memory on badge"
         
     - alert: LowBattery
       expr: battery_percent < 20
       for: 2m
       labels:
         severity: warning
       annotations:
         summary: "Low battery on badge"
         
     - alert: HighErrorRate
       expr: rate(errors_total[5m]) > 0.1
       for: 5m
       labels:
         severity: info
       annotations:
         summary: "High error rate on badge"
   ```

4. **Add documentation** (`dashboards/README.md`):
   ```markdown
   # CDC Badge OS Dashboards
   
   ## Available Dashboards
   
   ### Overview Dashboard
   - File: `badge-overview.json`
   - Purpose: Main system health overview
   - Metrics: Memory, battery, operations, errors
   
   ## Installation
   
   1. Import `badge-overview.json` into Grafana
   2. Apply alert rules from `badge-alerts.yaml` to Prometheus
   ```

5. **Add metrics documentation** (`docs/metrics.md`):
   ```markdown
   # CDC Badge OS Metrics Reference
   
   ## Available Metrics
   
   ### Resource Metrics
   - `heap_free_bytes` - Free heap memory
   - `psram_free_bytes` - Free PSRAM
   - `battery_percent` - Battery level (0-100)
   - `uptime_ms` - Uptime in milliseconds
   
   ### Business Metrics
   - `fido_auth_success_total` - Successful FIDO2 authentications
   - `fido_auth_failure_total` - Failed FIDO2 authentications
   - `totp_generations_total` - TOTP code generations
   - `pin_attempts_total` - PIN verification attempts
   
   ### Error Metrics
   - `errors_total{category}` - Errors by category
   
   ## Exporting Metrics
   
   Use `METRICS` serial command to export in Prometheus format.
   ```

## References
- [Grafana dashboard JSON format](https://grafana.com/docs/grafana/latest/dashboards/export-import/)
- [Prometheus alerting rules](https://prometheus.io/docs/prometheus/latest/configuration/alerting_rules/)
- [Dashboard-as-code best practices](https://grafana.com/docs/grafana/latest/developers/dashboard-as-code/)
- [Prometheus recording rules](https://prometheus.io/docs/prometheus/latest/configuration/recording_rules/)

</content>