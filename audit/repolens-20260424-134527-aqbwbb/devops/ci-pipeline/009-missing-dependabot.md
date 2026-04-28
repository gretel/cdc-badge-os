---
title: "[LOW] Missing Dependabot configuration for automated dependency updates"
severity: LOW
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The repository does not have Dependabot configured to automatically update dependencies and scan for vulnerabilities. ESP-IDF components, PlatformIO packages, and Python dependencies need regular updates for security patches.

**Evidence:**
- No `.github/dependabot.yml` file in the repository
- No automated dependency update workflow

## Impact
- Manual tracking of dependency updates required
- Security patches for dependencies may be delayed
- Version drift as dependencies age
- Missed minor/patch updates that could be applied automatically

## Evidence
```bash
# Check for dependabot config:
ls -la .github/dependabot.yml
# File does not exist
```

Dependencies that need tracking:
- `espressif/led_strip` v2.5.5
- `espressif/qrcode` v0.2.0
- `espressif/tinyusb` v0.19.0~2
- `idf` v5.5.0
- Python packages (PlatformIO)

## Recommended Fix
Create `.github/dependabot.yml`:

```yaml
version: 2
updates:
  # ESP-IDF components
  - package-ecosystem: "gradle"  # Or appropriate for ESP-IDF
    directory: "/"
    schedule:
      interval: "weekly"
    open-pull-requests-limit: 5

  # Python dependencies (for PlatformIO)
  - package-ecosystem: "pip"
    directory: "/"
    schedule:
      interval: "weekly"
    open-pull-requests-limit: 5

  # GitHub Actions
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
    open-pull-requests-limit: 3
```

Note: ESP-IDF components may not be directly supported by Dependabot. Consider using a script to check for newer versions.

## References
- [Dependabot configuration](https://docs.github.com/en/code-security/dependabot/dependabot-version-updates/configuration-options-for-the-dependabot.yml-file)
- [ESP-IDF component updates](https://components.espressif.com/)
