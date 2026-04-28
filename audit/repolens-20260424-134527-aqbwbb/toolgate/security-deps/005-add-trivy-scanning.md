---
title: "[SETUP] Add Trivy for ESP-IDF component vulnerability scanning"
severity: MEDIUM
domain: security-deps
lens: toolgate/security-deps
labels:
  - "audit:toolgate/security-deps"
---

## Summary
The ESP-IDF components (led_strip, qrcode, tinyusb) used in this project lack automated vulnerability scanning. Trivy can scan ESP-IDF components for known CVEs.

**File Reference:** `dependencies.lock`

**Affected Dependencies:**
- `espressif/led_strip` v2.5.5
- `espressif/qrcode` v0.2.0
- `espressif/tinyusb` v0.19.0~2
- `idf` v5.5.0

## Impact
- **Security Risk:** ESP-IDF components may contain known vulnerabilities
- **Maintenance Burden:** Manual tracking of ESP-IDF component CVEs required
- **Supply Chain Risk:** Third-party ESP-IDF components may have untracked vulnerabilities

ESP-IDF components are regularly updated with security fixes. Without automated scanning, the project may miss critical updates.

## Evidence
**Current ESP-IDF Dependencies (`dependencies.lock`):**
```yaml
dependencies:
  espressif/led_strip:
    version: 2.5.5
  espressif/qrcode:
    version: 0.2.0
  espressif/tinyusb:
    version: 0.19.0~2
  idf:
    version: 5.5.0
```

**Known CVEs in ESP-IDF:**
- CVE-2025-66409 (CRITICAL) - AVRCP stack
- CVE-2025-68474 (HIGH) - AVRCP vendor command
- CVE-2025-68473 (HIGH) - SDP service discovery
- CVE-2024-24746 (HIGH) - Apache NimBLE (if enabled)

No automated scanning is currently configured.

## Recommended Fix
**Set up Trivy for ESP-IDF component scanning**

1. **Install Trivy**:
   ```bash
   # On Ubuntu/Debian
   curl -sfL https://raw.githubusercontent.com/aquasecurity/trivy/main/contrib/install.sh | sh -s -- -b /usr/local/bin
   
   # Or via Homebrew
   brew install trivy
   ```

2. **Add scanning script** (`tools/scan_deps.sh`):
   ```bash
   #!/bin/bash
   # Scan ESP-IDF dependencies for vulnerabilities
   
   echo "Scanning ESP-IDF dependencies..."
   
   # Scan dependencies.lock
   trivy fs --format json \
     --scanners vuln \
     --ignore-unfixed \
     -o dependencies-report.json \
     dependencies.lock
   
   # Also scan managed components
   trivy fs --format json \
     --scanners vuln \
     --ignore-unfixed \
     -o components-report.json \
     managed_components/
   
   echo "Reports generated:"
   echo "  - dependencies-report.json"
   echo "  - components-report.json"
   ```

3. **Add to CI workflow** (`.github/workflows/security.yml`):
   ```yaml
   name: ESP-IDF Dependency Scan
   
   on:
     push:
       paths:
         - 'dependencies.lock'
         - 'managed_components/**'
     schedule:
       - cron: '0 6 * * 1'  # Weekly
   
   jobs:
     trivy-scan:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4
         - name: Run Trivy
           uses: aquasecurity/trivy-action@master
           with:
             scan-type: 'fs'
             scan-ref: '.'
             format: 'table'
             output: 'trivy-results.json'
             exit-code: '1'  # Fail CI on vulnerabilities
   ```

4. **Alternative: Use ESP-IDF's built-in component check**:
   ```bash
   # Check for component updates
   idf.py reconfigure
   
   # Check specific component versions
   cat managed_components/espressif__tinyusb/library.json | grep version
   ```

## References
- Trivy: https://trivy.dev/
- Trivy GitHub Action: https://github.com/aquasecurity/trivy-action
- ESP-IDF Components: https://components.espressif.com/
- ESP-IDF Security Advisories: https://github.com/espressif/esp-idf/security/advisories
