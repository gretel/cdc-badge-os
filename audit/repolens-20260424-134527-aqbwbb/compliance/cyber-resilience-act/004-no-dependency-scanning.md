---
title: "[MEDIUM] No dependency vulnerability scanning in CI/CD"
severity: MEDIUM
domain: cyber-resilience-act
lens: dependency-scanning
labels:
  - "dependency-scanning"
  - "ci-cd"
  - "cra-2026"
---

## Summary
The CI/CD pipeline lacks automated dependency vulnerability scanning. No tools like Dependabot, Renovate, Trivy, Grype, or Snyk are configured to scan for known vulnerabilities in dependencies.

**Files affected:**
- `.github/workflows/build.yml` - No vulnerability scanning step
- `.github/workflows/deploy-pages.yml` - No vulnerability scanning step
- Repository root - No `dependabot.yml` or `renovate.json`

## Impact
Under CRA (Article 11), manufacturers must monitor and patch vulnerabilities:
- **Unknown vulnerabilities**: Dependencies may contain known CVEs that go undetected
- **Delayed patching**: No automated alerts when new vulnerabilities are discovered
- **Compliance gap**: CRA requires active monitoring of supply chain vulnerabilities

**Current dependencies** (from `dependencies.lock`):
- `espressif/led_strip` v2.5.5
- `espressif/qrcode` v0.2.0
- `espressif/tinyusb` v0.19.0~2
- `idf` v5.5.0

## Recommended Fix
Add dependency vulnerability scanning:

1. **Create `.github/dependabot.yml`**:
```yaml
version: 2
updates:
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    labels:
      - "dependencies"
      - "security"

  - package-ecosystem: "gomod"  # If using Go modules
    directory: "/"
    schedule:
      interval: "weekly"
```

2. **Add Trivy or Grype to build.yml**:
```yaml
- name: Scan dependencies with Trivy
  uses: aquasecurity/trivy-action@master
  with:
    scan-type: 'fs'
    scan-ref: '.'
    format: 'sarif'
    output: 'trivy-results.sarif'

- name: Upload Trivy results
  uses: github/codeql-action/upload-sarif@v2
  with:
    sarif_file: 'trivy-results.sarif'
```

3. **Consider ESP-IDF component scanning**:
   - Check `managed_components/` directory for updates
   - Script to compare against ESP-IDF component registry for known CVEs

## References
- [EU CRA Article 11 - Vulnerability monitoring](https://digital-strategy.ec.europa.eu/en/library/cyber-resilience-act)
- [Dependabot configuration](https://docs.github.com/en/code-security/dependabot/dependabot-version-updates/configuration-options-for-the-dependabot.yml-file)
- [Trivy GitHub Action](https://github.com/aquasecurity/trivy-action)
