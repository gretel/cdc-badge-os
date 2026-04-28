---
title: "[MEDIUM] No automated dependency vulnerability scanning"
severity: MEDIUM
domain: compliance/product-liability
lens: product-liability
labels:
  - dependency-management
  - vulnerability-scanning
---

## Summary
The project has no automated dependency vulnerability scanning configured. With firmware depending on multiple external libraries (ESP-IDF, TinyUSB, libtropic, CalEPD, Adafruit-GFX), known CVEs in these dependencies may go unnoticed.

**Evidence:**
- No Dependabot, Renovate, or Snyk configuration found
- No Trivy or similar container/image scanning in GitHub Actions
- `dependencies.lock` exists but no automated update mechanism

## Impact
- Known CVEs in dependencies may not be detected until after they affect users
- Firmware distributed to users may contain known vulnerabilities
- Under Product Liability Directive, "state of the art" security is expected
- Manual dependency tracking is error-prone and may miss critical updates

## Evidence
No dependency update tools found:
```bash
grep -rn "dependabot\|renovate\|snyk\|trivy\|audit" /input/20260423-132359-oj8ayc/cdc-badge-os --include="*.yml" --include="*.json"
# No results
```

Dependencies in use:
- `espressif/tinyusb@0.19.0` (from dependencies.lock)
- `espressif/led_strip@2.5.5`
- `espressif/qrcode@0.2.0`
- `idf@5.5.0`
- Submodules: libtropic, CalEPD, Adafruit-GFX

GitHub Actions workflow (`build.yml`) only builds, no security scanning.

## Recommended Fix
Add automated vulnerability scanning:

1. **Enable GitHub Dependabot** (for GitHub-hosted dependencies):
   Create `.github/dependabot.yml`:
   ```yaml
   version: 2
   updates:
     - package-ecosystem: "github-actions"
       directory: "/"
       schedule:
         interval: "weekly"
   ```

2. **Add Trivy scanning** to GitHub Actions for container/firmware images:
   Add step to `build.yml`:
   ```yaml
   - name: Run Trivy scanner
     uses: aquasecurity/trivy-action@master
     with:
       scan-type: 'fs'
       scan-ref: '.'
       format: 'table'
   ```

3. **Manual process for submodules**: Document in SECURITY.md that submodules (libtropic, CalEPD, Adafruit-GFX) should be checked regularly for releases with security fixes.

## References
- OWASP Dependency-Check: https://owasp.org/www-project-dependency-check/
- Trivy GitHub Action: https://github.com/aquasecurity/trivy-action
- GitHub Dependabot: https://docs.github.com/en/code-security/dependabot
