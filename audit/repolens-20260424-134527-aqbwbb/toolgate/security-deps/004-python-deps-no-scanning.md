---
title: "[MEDIUM] No automated vulnerability scanning for Python dependencies"
severity: MEDIUM
domain: security-deps
lens: toolgate/security-deps
labels:
  - "audit:toolgate/security-deps"
---

## Summary
The project has Python dependencies defined in `tools/requirements.txt` but lacks automated vulnerability scanning to detect known CVEs in these packages.

**File Reference:** `tools/requirements.txt`

**Affected Dependencies:**
- `esptool>=4.7` - ESP32 flashing utility
- `requests>=2.28` - HTTP library
- `bleak>=0.21` - BLE library
- `esp-coredump>=1.5` - ESP core dump analysis

## Impact
- **Maintenance Burden:** Manual tracking of CVEs required
- **Security Risk:** Known vulnerabilities in transitive dependencies may go undetected
- **Supply Chain Risk:** No automated check for dependency updates with security fixes

Python packages are particularly vulnerable to supply chain attacks and have frequent CVE announcements. Without automated scanning, the team may miss critical security updates.

## Evidence
**Current Python Dependencies (`tools/requirements.txt`):**
```
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

**No vulnerability scanning tools configured:**
- No `pip-audit` in CI/CD
- No `safety` checks
- No `pipenv` or `poetry` lock files for deterministic builds

## Recommended Fix
**Set up automated Python dependency vulnerability scanning**

1. **Install pip-audit** (recommended):
   ```bash
   pip install pip-audit
   ```

2. **Add scanning to CI workflow** (`.github/workflows/security.yml`):
   ```yaml
   name: Python Dependency Audit
   
   on:
     push:
       paths:
         - 'tools/requirements.txt'
     schedule:
       - cron: '0 6 * * 1'  # Weekly
   
   jobs:
     audit:
       runs-on: ubuntu-latest
       steps:
         - uses: actions/checkout@v4
         - name: Set up Python
           uses: actions/setup-python@v5
           with:
             python-version: '3.x'
         - name: Install dependencies
           run: pip install -r tools/requirements.txt
         - name: Run pip-audit
           run: pip-audit --format json --output audit-results.json
         - name: Upload results
           uses: actions/upload-artifact@v4
           with:
             name: vulnerability-report
             path: audit-results.json
   ```

3. **Alternative: Use safety** (free tier available):
   ```bash
   pip install safety
   safety check -r tools/requirements.txt
   ```

4. **Consider using pip-tools for lock files**:
   ```bash
   pip install pip-tools
   pip-compile tools/requirements.txt  # Creates requirements.txt with pinned versions
   ```

## References
- pip-audit: https://pypi.org/project/pip-audit/
- safety: https://pypi.org/project/safety/
- OWASP Dependency Check: https://owasp.org/www-project-dependency-check/
- Python Security Advisories: https://pyup.io/advisories/
